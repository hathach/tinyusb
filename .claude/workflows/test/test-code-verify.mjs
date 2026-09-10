import assert from 'node:assert/strict'
import { readdirSync, readFileSync } from 'node:fs'

const AsyncFunction = Object.getPrototypeOf(async function () {}).constructor
const source = readFileSync(new URL('../code-verify.js', import.meta.url), 'utf8')
// Read the workflow tree once: four separate checks scan it.
const WORKFLOW_DIR = new URL('../', import.meta.url)
const WORKFLOW_SRC = readdirSync(WORKFLOW_DIR).filter(n => n.endsWith('.js'))
  .map(name => [name, readFileSync(new URL(name, WORKFLOW_DIR), 'utf8')])
const workflowBody = source.replace(/^export /m, '')

const RESULT = {
  type: 'object', additionalProperties: false,
  required: ['answer'],
  properties: { answer: { type: 'string' } },
}
const answers = {
  codex: { answer: 'codex' },
  claude: { answer: 'claude' },
}

// The launcher wraps every Codex answer as { job, thread, result }; `wrap`
// lets a test hand back something else to prove the router rejects it.
const ENVELOPE = result => ({ job: '/tmp/tinyusb-codex/20260910T000000Z-1', thread: 't-1', result })
async function run(args, provided = answers, wrap = ENVELOPE) {
  const calls = []
  let parallelCalls = 0
  const agent = async (prompt, options) => {
    const provider = options.agentType === 'code-verifier' ? 'claude'
      : options.agentType === 'codex-agent' ? 'codex' : 'unknown'
    calls.push({ provider, prompt, options })
    const answer = provided[provider]
    if (answer instanceof Error) throw answer
    if (answer && typeof answer.answer !== 'string') throw new Error('schema mismatch')
    if (answer && provider === 'codex') return wrap(structuredClone(answer))
    return structuredClone(answer)
  }
  const parallel = async (thunks) => {
    parallelCalls++
    return Promise.all(thunks.map(fn => Promise.resolve().then(fn).catch(() => null)))
  }
  const fn = new AsyncFunction(
    'args', 'agent', 'pipeline', 'parallel', 'phase', 'log', 'workflow', 'budget',
    workflowBody,
  )
  const result = await fn(args, agent, null, parallel, () => {}, () => {}, null, null)
  return { result, calls, parallelCalls }
}

let failed = 0
async function check(name, fn) {
  try {
    await fn()
    console.log(`  ok   ${name}`)
  } catch (error) {
    failed++
    console.log(`  FAIL ${name}\n       ${error.stack || error}`)
  }
}

await check('defaults to codex', async () => {
  const { result, calls, parallelCalls } = await run({ prompt: 'review', schema: RESULT })
  assert.deepEqual(result, answers.codex)
  assert.deepEqual(calls.map(c => c.provider), ['codex'])
  assert.equal(parallelCalls, 0)
  assert.equal(calls[0].options.agentType, 'codex-agent')
  // the bridge is asked for the launcher's envelope around the caller's schema
  assert.deepEqual(calls[0].options.schema.required, ['job', 'thread', 'result'])
  assert.deepEqual(calls[0].options.schema.properties.result.anyOf, [RESULT, { type: 'null' }])
  assert.deepEqual(JSON.parse(calls[0].prompt), { prompt: 'review', schema: RESULT })
})

await check('a codex reply without provenance means codex did not run', async () => {
  // A bridge that answered the prompt itself returns the bare schema shape;
  // only the launcher adds where Codex ran. Such an answer is never used.
  for (const wrap of [r => r, r => ({ result: r }), r => ({ job: '', thread: 't', result: r })]) {
    const { result, calls } = await run({ prompt: 'review', schema: RESULT }, answers, wrap)
    assert.deepEqual(result, answers.claude)
    assert.deepEqual(calls.map(c => c.options.label), ['code-verifier:codex', 'code-verifier:claude-fallback'])
  }
})

await check('the launcher owns the Codex command-line contract', async () => {
  const launcher = readFileSync(new URL('../../codex-agent.py', import.meta.url), 'utf8')
  assert.match(launcher, /tomllib/, 'model, effort and role text come from the Codex adapter')
  assert.match(launcher, /'codex', 'exec', 'review', '-c', 'sandbox_mode=read-only'/)
  assert.match(launcher, /'codex', 'exec', '-C', str\(root\), '--sandbox', 'read-only'/)
  assert.match(launcher, /--output-schema/)
  assert.match(launcher, /TIMEOUT = 1800/)
  assert.match(launcher, /'job': str\(job\), 'thread':/, 'the envelope carries provenance')
  assert.doesNotMatch(launcher, /workspace-write|danger-full-access|--add-dir/)

  const bridge = readFileSync(new URL('../../agents/codex-agent.md', import.meta.url), 'utf8')
  assert.match(bridge, /model: haiku/)
  assert.match(bridge, /effort: low/)
  assert.match(bridge, /tools: Bash/)
  assert.match(bridge, /python3 \.claude\/codex-agent\.py <<'CODEX_AGENT_INPUT'/)
  assert.doesNotMatch(bridge, /codex exec/, 'the bridge must not run codex itself')
})

await check('selects claude', async () => {
  const { result, calls, parallelCalls } = await run(
    { prompt: 'review', schema: RESULT, provider: 'claude', label: 'check' })
  assert.deepEqual(result, answers.claude)
  assert.deepEqual(calls.map(c => c.provider), ['claude'])
  assert.equal(parallelCalls, 0)
  assert.equal(calls[0].prompt, 'review')
  assert.equal(calls[0].options.label, 'check:claude')
  assert.deepEqual(calls[0].options.schema, RESULT)
})

await check('runs all providers independently', async () => {
  const { result, calls, parallelCalls } = await run(
    { prompt: 'review', schema: RESULT, provider: 'all' })
  assert.deepEqual(result, { codex: answers.codex, claude: answers.claude })
  assert.deepEqual(calls.map(c => c.provider).sort(), ['claude', 'codex'])
  assert.equal(parallelCalls, 1)
})

await check('rejects invalid input before dispatch', async () => {
  for (const args of [null, {}, { prompt: '', schema: RESULT }, { prompt: 'x', schema: [] }]) {
    await assert.rejects(run(args), /args must be/)
  }
  for (const provider of ['', 'auto']) await assert.rejects(
    run({ prompt: 'review', schema: RESULT, provider }),
    /provider must be codex, claude, or all/,
  )
})

await check('reviews with claude when codex cannot run', async () => {
  for (const codex of [null, new Error('broken'), { answer: 1 }]) {
    const { result, calls } = await run({ prompt: 'review', schema: RESULT }, { ...answers, codex })
    assert.deepEqual(result, answers.claude)
    assert.deepEqual(calls.map(c => c.provider), ['codex', 'claude'])
    assert.equal(calls[1].options.label, 'code-verifier:claude-fallback')
    assert.equal(calls[1].options.agentType, 'code-verifier')
    assert.deepEqual(calls[1].options.schema, RESULT)
  }
  // both verifiers dead: the fallback's failure is the one reported
  await assert.rejects(
    run({ prompt: 'review', schema: RESULT }, { codex: null, claude: null }),
    /claude verifier failed/,
  )
})

await check('a codex timeout is a failure, not a fallback', async () => {
  const timedOut = () => ({ ...ENVELOPE(null), error: 'codex timed out after 30 min' })
  // a fallback would surface as the claude fake's failure instead
  await assert.rejects(
    run({ prompt: 'review', schema: RESULT },
      { codex: { answer: 'x' }, claude: new Error('claude must not run') }, timedOut),
    /^Error: codex verifier timed out: codex timed out after 30 min$/,
  )
})

await check('all providers stays strict when codex dies', async () => {
  await assert.rejects(
    run({ prompt: 'review', schema: RESULT, provider: 'all' },
      { ...answers, codex: new Error('broken') }),
    /codex verifier failed/,
  )
})

await check('only validate bypasses the router for one-level nesting', async () => {
  const dir = new URL('../', import.meta.url)
  assert.ok(readdirSync(dir).includes('code-verify.js'), 'workflow scan must target its parent directory')
  const offenders = readdirSync(dir)
    .filter(name => name.endsWith('.js') && !['code-verify.js', 'validate.js'].includes(name))
    .filter(name => /agentType:\s*['"](?:codex-)?code-verifier['"]/.test(
      readFileSync(new URL(name, dir), 'utf8')))
  assert.deepEqual(offenders, [])
})

await check('router rejections preserve caller null-result contracts', async () => {
  for (const name of ['fanout-dev.js', 'pr-babysit.js']) {
    const src = readFileSync(new URL(`../${name}`, import.meta.url), 'utf8')
    assert.match(src, /workflow\('code-verify',[\s\S]*?\}\)\.catch\(\(\) => null\)\s*\.then/)
  }
})

await check('driver review drops a failed routed scanner', async () => {
  const src = readFileSync(new URL('../driver-review.js', import.meta.url), 'utf8').replace(/^export /m, '')
  const fn = new AsyncFunction(
    'args', 'agent', 'pipeline', 'parallel', 'phase', 'log', 'workflow', 'budget', src)
  const pipeline = async (items, ...stages) => Promise.all(items.map(async item => {
    let value = item
    for (const stage of stages) value = await stage(value, item)
    return value
  }))
  const parallel = thunks => Promise.all(thunks.map(run => run()))
  const unit = { dir: 'src/portable/test', dim: 'correctness' }
  const finding = { file: 'a.c', line: 1, snippet: 's', why: 'w', severity: 'major', confidence: 'high' }

  // scanner dies: the unit is reported as dropped, never as clean
  const dead = await fn(
    { dirs: [unit.dir], dimensions: [unit.dim] },
    null, pipeline, parallel, () => {}, () => {}, async () => { throw new Error('verifier died') }, null,
  )
  assert.deepEqual(dead, { confirmed: [], dropped: [unit], unverified: [] })

  // scanner reports, every verifier dies: the finding is reported as unverified
  const lost = await fn(
    { dirs: [unit.dir], dimensions: [unit.dim] },
    null, pipeline, parallel, () => {}, () => {}, async (name, a) => {
      if (/Adversarially verify/.test(a.prompt)) throw new Error('verifier died')
      return { scope: unit.dir, dimension: unit.dim, findings: [finding] }
    }, null,
  )
  assert.deepEqual(lost, { confirmed: [], dropped: [], unverified: [{ ...unit, findings: [finding] }] })

  // one verifier confirms, one refutes: only the survivor is confirmed
  const mixed = await fn(
    { dirs: [unit.dir], dimensions: [unit.dim] },
    null, pipeline, parallel, () => {}, () => {}, async (name, a) => {
      if (/Adversarially verify/.test(a.prompt)) return { real: /line":1,/.test(a.prompt), reason: 'r' }
      return { scope: unit.dir, dimension: unit.dim, findings: [finding, { ...finding, line: 2 }] }
    }, null,
  )
  assert.deepEqual(mixed, {
    confirmed: [{ ...unit, findings: [{ ...finding, verdict: { real: true, reason: 'r' } }] }],
    dropped: [], unverified: [],
  })
})

await check('validate dispatches directly to stay within one workflow level', async () => {
  const src = readFileSync(new URL('../validate.js', import.meta.url), 'utf8')
  assert.equal((src.match(/workflow\(['"]code-verify['"]/g) || []).length, 0)
  assert.match(src, /const reviewProvider = reviewStageNames\.length === 2 \? 'all'/)
  assert.match(src, /agentType:\s*'codex-agent'/)
  assert.match(src, /agentType:\s*'code-verifier'/)
  assert.doesNotMatch(src, /codex review --base/)
  assert.doesNotMatch(src, /model:\s*['"]opus['"][^}]*schema:\s*REVIEW/)
})

await check('validate keeps provider results and gates separate', async () => {
  const src = readFileSync(new URL('../validate.js', import.meta.url), 'utf8').replace(/^export /m, '')
  const calls = []
  let failCodex = false
  let blocking = false
  const fn = new AsyncFunction(
    'args', 'agent', 'pipeline', 'parallel', 'phase', 'log', 'workflow', 'budget', src)
  const agent = async (prompt, options) => {
    calls.push(options.agentType)
    if (options.agentType === 'code-verifier') return {
      pass: true, detail: 'claude',
      findings: [{ file: 'a.c', line: 1,
        severity: blocking ? 'CONFIRMED P1 safety' : 'CONFIRMED P2 correctness', summary: 'bug' }],
    }
    if (options.agentType === 'codex-agent') return failCodex ? null : ENVELOPE({
      pass: true, detail: 'codex',
      findings: [{ file: 'b.c', line: 2,
        severity: blocking ? 'CONFIRMED P1 safety' : 'CONFIRMED P1 quality', summary: 'bug' }],
    })
    assert.equal(options.agentType, 'builder')
    return { board: 'test', pass: true, builtCount: 1, failures: [] }
  }
  const workflow = async () => { throw new Error('validate cannot nest a workflow') }
  const parallel = thunks => Promise.all(thunks.map(run => run()))
  const result = await fn(
    { boards: ['test'], skip: ['unit', 'size', 'pvs'], reviewProvider: 'all', maxCycles: 1 },
    agent, null, parallel, () => {}, () => {}, workflow, null,
  )
  assert.deepEqual(calls.sort(), ['builder', 'code-verifier', 'codex-agent'])
  assert.equal(result.stages.find(s => s.stage === 'review').pass, true)
  assert.equal(result.stages.find(s => s.stage === 'codex').pass, true)

  blocking = true
  const blocked = await fn(
    { boards: ['test'], skip: ['unit', 'size', 'pvs'], reviewProvider: 'all', maxCycles: 1 },
    agent, null, parallel, () => {}, () => {}, workflow, null,
  )
  assert.equal(blocked.stages.find(s => s.stage === 'review').pass, false)
  assert.equal(blocked.stages.find(s => s.stage === 'codex').pass, false)

  failCodex = true
  const partial = await fn(
    { boards: ['test'], skip: ['unit', 'size', 'pvs'], reviewProvider: 'all', maxCycles: 1 },
    agent, null, parallel, () => {}, () => {}, workflow, null,
  )
  assert.equal(partial.stages.find(s => s.stage === 'review').detail, 'claude')
  assert.equal(partial.stages.find(s => s.stage === 'codex').detail, 'stage agent died')
})

await check('validate reviews with claude when codex cannot run, but not after a timeout', async () => {
  const src = readFileSync(new URL('../validate.js', import.meta.url), 'utf8').replace(/^export /m, '')
  const fn = new AsyncFunction(
    'args', 'agent', 'pipeline', 'parallel', 'phase', 'log', 'workflow', 'budget', src)
  const parallel = thunks => Promise.all(thunks.map(run => run()))
  const runValidate = async codex => {
    const calls = []
    const logs = []
    const agent = async (prompt, options) => {
      calls.push(options.label)
      if (options.agentType === 'builder') return { board: 'test', pass: true, builtCount: 1, failures: [] }
      if (options.agentType === 'code-verifier') return { pass: true, detail: 'claude', findings: [] }
      if (codex instanceof Error) throw codex
      return codex
    }
    const result = await fn(
      { boards: ['test'], skip: ['unit', 'size', 'pvs'], maxCycles: 1 },
      agent, null, parallel, () => {}, l => logs.push(l), null, null,
    )
    return { calls, logs, row: result.stages.find(s => s.stage === 'codex') }
  }

  for (const codex of [null, new Error('bridge died'), { pass: true, detail: 'bare', findings: [] }]) {
    const { calls, logs, row } = await runValidate(codex)
    assert.deepEqual(calls.filter(l => /codex|claude/.test(l)), ['reviews:codex', 'reviews:claude-fallback'])
    assert.equal(row.pass, true)
    assert.equal(row.detail, 'claude fallback: claude')
    assert.ok(logs.some(l => /reviewing with claude instead/.test(l)))
  }

  const timedOut = await runValidate({ ...ENVELOPE(null), error: 'codex timed out after 30 min' })
  assert.deepEqual(timedOut.calls.filter(l => /codex|claude/.test(l)), ['reviews:codex'])
  assert.equal(timedOut.row.pass, false)
  assert.equal(timedOut.row.detail, 'stage agent died')
  assert.ok(timedOut.logs.some(l => /timed out after 30 min/.test(l)))
})

await check('validate reviews with codex unless claude is explicitly selected', async () => {
  const src = readFileSync(new URL('../validate.js', import.meta.url), 'utf8').replace(/^export /m, '')
  const fn = new AsyncFunction(
    'args', 'agent', 'pipeline', 'parallel', 'phase', 'log', 'workflow', 'budget', src)
  const parallel = thunks => Promise.all(thunks.map(run => run()))
  const runValidate = async extra => {
    const calls = []
    const agent = async (prompt, options) => {
      calls.push(options.agentType)
      if (options.agentType === 'builder') return { board: 'test', pass: true, builtCount: 1, failures: [] }
      const review = { pass: true, detail: options.agentType, findings: [] }
      return options.agentType === 'codex-agent' ? ENVELOPE(review) : review
    }
    const result = await fn(
      { boards: ['test'], skip: ['unit', 'size', 'pvs'], maxCycles: 1, ...extra },
      agent, null, parallel, () => {}, () => {}, null, null,
    )
    return { calls, result }
  }

  // the default must never spend a Claude review
  for (const extra of [{}, { reviewProvider: 'codex' }]) {
    const { calls, result } = await runValidate(extra)
    assert.deepEqual(calls.sort(), ['builder', 'codex-agent'])
    assert.equal(result.stages.some(s => s.stage === 'review'), false)
    assert.equal(result.stages.find(s => s.stage === 'codex').pass, true)
  }

  const claudeOnly = await runValidate({ reviewProvider: 'claude' })
  assert.deepEqual(claudeOnly.calls.sort(), ['builder', 'code-verifier'])
  assert.equal(claudeOnly.result.stages.some(s => s.stage === 'codex'), false)

  const all = await runValidate({ reviewProvider: 'all' })
  assert.deepEqual(all.calls.sort(), ['builder', 'code-verifier', 'codex-agent'])

  // skip still turns a selected reviewer off
  const skipped = await runValidate({ reviewProvider: 'all', skip: ['unit', 'size', 'pvs', 'review'] })
  assert.deepEqual(skipped.calls.sort(), ['builder', 'codex-agent'])

  // selecting a reviewer and skipping it is contradictory input, not silence
  await assert.rejects(
    runValidate({ reviewProvider: 'claude', skip: ['unit', 'size', 'pvs', 'review'] }),
    /reviewProvider "claude" is cancelled by skip/,
  )
  await assert.rejects(
    runValidate({ reviewProvider: 'all', skip: ['unit', 'size', 'pvs', 'review', 'codex'] }),
    /reviewProvider "all" is cancelled by skip/,
  )

  // skip:['codex'] used to mean "review with Claude" — it must not silently
  // become an unreviewed green now that Codex is the only default reviewer
  await assert.rejects(
    runValidate({ skip: ['unit', 'size', 'pvs', 'codex'] }),
    /leaves no diff reviewer/,
  )
  // skipping every reviewer is unmistakable, so it stays legal
  const noReview = await runValidate({ skip: ['unit', 'size', 'pvs', 'review', 'codex'] })
  assert.deepEqual(noReview.calls.sort(), ['builder'])
  assert.equal(noReview.result.stages.some(s => s.stage === 'review' || s.stage === 'codex'), false)

  for (const reviewProvider of ['', 'auto', 'CODEX']) {
    await assert.rejects(runValidate({ reviewProvider }), /reviewProvider must be codex, claude, or all/)
  }
})

await check('full-check forwards the reviewer selection to validate', async () => {
  const src = readFileSync(new URL('../full-check.js', import.meta.url), 'utf8').replace(/^export /m, '')
  const fn = new AsyncFunction(
    'args', 'agent', 'pipeline', 'parallel', 'phase', 'log', 'workflow', 'budget', src)
  const seen = []
  const workflow = async (name, workflowArgs) => {
    seen.push({ name, args: workflowArgs })
    return { pass: true }
  }
  await fn({ boards: ['test'], reviewProvider: 'all' },
    null, null, null, () => {}, () => {}, workflow, null)
  assert.deepEqual(seen.map(s => s.name), ['validate'])
  assert.equal(seen[0].args.reviewProvider, 'all')
  assert.deepEqual(seen[0].args.boards, ['test'])
})

await check('fanout simplifies once after all writers and before verification', async () => {
  const src = readFileSync(new URL('../fanout-dev.js', import.meta.url), 'utf8').replace(/^export /m, '')
  const fn = new AsyncFunction(
    'args', 'agent', 'pipeline', 'parallel', 'phase', 'log', 'workflow', 'budget', src)
  const pipeline = (items, ...stages) => Promise.all(items.map(async (item, index) => {
    let value = item
    for (const stage of stages) value = await stage(value, item, index)
    return value
  }))
  const runFanout = async ({ worktree = false, deadWriter = false, deadSimplifier = false,
    deadBuilder = false } = {}) => {
    const events = []
    const logs = []
    const scopes = []
    const agent = async (prompt, options) => {
      if (options.agentType === 'code-writer') {
        const item = options.label.slice(4)
        if (item === 'a') await new Promise(resolve => setImmediate(resolve))
        events.push(`wrote:${item}`)
        if (deadWriter && item === 'a') return null
        assert.equal(options.isolation, worktree ? 'worktree' : undefined)
        return { item, board: item, buildOk: true, diffstat: '', notes: `note:${item}` }
      }
      if (options.label === 'simplify') {
        // inline general subagent: /simplify via Skill, no agent definition
        assert.equal(options.agentType, undefined)
        assert.match(prompt, /\/simplify skill exactly once through the Skill tool/)
        assert.match(prompt, /Do not stage, commit or push/)
        assert.deepEqual([...events].sort(), ['wrote:a', 'wrote:b'])
        scopes.push(prompt.match(/Assigned scopes: (\[[^\]]*\])/)[1])
        // writer results reach the simplifier as notes only, not build metadata
        assert.match(prompt, /Writer notes: .*note:b/)
        assert.doesNotMatch(prompt, /buildOk/)
        events.push('simplified')
        return deadSimplifier ? null : { changed: true, files: ['b/file.c'], summary: 'tidied' }
      }
      assert.equal(options.agentType, 'builder')
      assert.equal(events.filter(e => e === 'simplified').length, 1)
      events.push(options.label)
      return deadBuilder ? null : { pass: true }
    }
    const workflow = async (name, args) => {
      assert.equal(name, 'code-verify')
      assert.ok(events.includes(args.label.replace('review:', 'verify:')))
      assert.match(args.prompt, /git diff HEAD/)
      events.push(args.label)
      return { findings: [] }
    }
    const result = await fn({ task: 'fix', items: ['a', 'b'], review: true, worktree },
      agent, pipeline, null, () => {}, message => logs.push(message), workflow, null)
    return { result, events, logs, scopes }
  }

  const { result, events, logs, scopes } = await runFanout()
  assert.equal(events.filter(e => e === 'simplified').length, 1)
  assert.deepEqual(scopes, ['["a","b"]'])
  assert.equal(result.length, 2)
  for (const row of result) {
    assert.equal(row.verifyBuild, true)
    assert.deepEqual(row.review, [])
    // the run-level simplification is logged once, not stamped on every row
    assert.equal('simplification' in row, false)
  }
  assert.ok(logs.some(message => /^simplify: b\/file\.c — tidied$/.test(message)))
  assert.match(logs.at(-1), /2\/2 items completed; 2 build-clean/)

  // a dead writer drops its own item; the survivors are still simplified and verified
  const partial = await runFanout({ deadWriter: true })
  assert.deepEqual(partial.scopes, ['["b"]'])
  assert.deepEqual(partial.result.map(row => row.item), ['b'])
  assert.ok(partial.logs.some(message => /1 item\(s\) dropped/.test(message)))
  assert.match(partial.logs.at(-1), /1\/2 items completed; 1 build-clean/)

  await assert.rejects(runFanout({ deadSimplifier: true }), /simplifier failed/)

  const isolated = await runFanout({ worktree: true })
  assert.deepEqual(isolated.events.sort(), ['wrote:a', 'wrote:b'])
  assert.match(isolated.logs[0], /deferred until integration/)
  assert.match(isolated.logs.at(-1), /2\/2 items completed; 2 build-clean/)

  const unverified = await runFanout({ deadBuilder: true })
  assert.ok(unverified.result.every(row => row.verifyBuild === null))
  assert.match(unverified.logs.at(-1), /0 build-clean/)
})

await check('the retired bridge is referenced nowhere', async () => {
  for (const [name, src] of WORKFLOW_SRC) assert.doesNotMatch(src, /codex-code-verifier/, name)
})

await check('every codex-agent dispatch sends the bridge shape and nothing that steers it', async () => {
  // The bridge runs one fixed command line for the code-verifier role, so a
  // dispatch may carry only prompt, schema and the review switch. A `role`
  // or `sandbox` field would be silently ignored today and would invite the
  // bridge to grow an input it must not have.
  const dispatches = []
  for (const [name, src] of WORKFLOW_SRC) {
    if (!/agentType:\s*['"]codex-agent['"]/.test(src)) continue
    dispatches.push(name)
    assert.doesNotMatch(src, /\brole:\s*['"`A-Za-z_$]/, `${name} passes a role to codex-agent`)
    assert.doesNotMatch(src, /sandbox/i, `${name} mentions a sandbox near a codex-agent dispatch`)
  }
  assert.deepEqual(dispatches.sort(), ['code-verify.js', 'validate.js'])
})

await check('the retired provider value is accepted nowhere', async () => {
  // Case-insensitive rather than a [Pp] class: codespell rewrites the bare
  // `rovider` fragment such a class leaves behind, silently breaking the scan.
  const offersBoth = /provider[^\n]*['"]both['"]/i
  // The scan is a negative assertion, so a typo in it passes silently forever.
  assert.match("provider: 'both'", offersBoth)
  assert.match('reviewProvider: "both"', offersBoth)
  for (const [name, src] of WORKFLOW_SRC) {
    // scoped to the provider enum: 'both' is an ordinary English word elsewhere
    assert.doesNotMatch(src, offersBoth, `${name} still offers 'both'`)
  }
  await assert.rejects(
    run({ prompt: 'review', schema: RESULT, provider: 'both' }),
    /provider must be codex, claude, or all/,
  )
})

console.log(failed ? `\n${failed} FAILED` : '\nall checks passed')
process.exit(failed ? 1 : 0)
