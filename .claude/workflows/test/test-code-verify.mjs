import assert from 'node:assert/strict'
import { readdirSync, readFileSync } from 'node:fs'

const AsyncFunction = Object.getPrototypeOf(async function () {}).constructor
const source = readFileSync(new URL('../code-verify.js', import.meta.url), 'utf8')
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

async function run(args, provided = answers) {
  const calls = []
  let parallelCalls = 0
  const agent = async (prompt, options) => {
    const provider = options.agentType === 'code-verifier' ? 'claude'
      : options.agentType === 'codex-code-verifier' ? 'codex' : 'unknown'
    calls.push({ provider, prompt, options })
    const answer = provided[provider]
    if (answer instanceof Error) throw answer
    if (answer && typeof answer.answer !== 'string') throw new Error('schema mismatch')
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
  assert.equal(calls[0].options.agentType, 'codex-code-verifier')
  assert.deepEqual(calls[0].options.schema, RESULT)
  assert.deepEqual(JSON.parse(calls[0].prompt), { prompt: 'review', schema: RESULT })
})

await check('bridge owns the Codex subprocess contract', async () => {
  const bridge = readFileSync(new URL('../../agents/codex-code-verifier.md', import.meta.url), 'utf8')
  assert.match(bridge, /model: haiku/)
  assert.match(bridge, /effort: low/)
  assert.match(bridge, /\.codex\/agents\/code-verifier\.toml/)
  assert.match(bridge, /tomllib/)
  assert.match(bridge, /timeout 600s codex exec/)
  assert.match(bridge, /--sandbox read-only/)
  assert.match(bridge, /--output-schema/)
  assert.match(bridge, /--output-last-message/)
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

await check('runs both independently', async () => {
  const { result, calls, parallelCalls } = await run(
    { prompt: 'review', schema: RESULT, provider: 'both' })
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
    /provider must be codex, claude, or both/,
  )
})

await check('fails closed when codex dies', async () => {
  await assert.rejects(
    run({ prompt: 'review', schema: RESULT }, { ...answers, codex: null }),
    /codex verifier failed/,
  )
  await assert.rejects(
    run({ prompt: 'review', schema: RESULT }, { ...answers, codex: new Error('broken') }),
    /codex verifier failed: broken/,
  )
  await assert.rejects(
    run({ prompt: 'review', schema: RESULT, provider: 'both' },
      { ...answers, codex: new Error('broken') }),
    /codex verifier failed/,
  )
  await assert.rejects(
    run({ prompt: 'review', schema: RESULT }, { ...answers, codex: { answer: 1 } }),
    /codex verifier failed: schema mismatch/,
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
  const result = await fn(
    { dirs: ['src/portable/test'], dimensions: ['correctness'] },
    null, pipeline, null, () => {}, () => {}, async () => { throw new Error('verifier died') }, null,
  )
  assert.deepEqual(result, [])
})

await check('validate dispatches directly to stay within one workflow level', async () => {
  const src = readFileSync(new URL('../validate.js', import.meta.url), 'utf8')
  assert.equal((src.match(/workflow\(['"]code-verify['"]/g) || []).length, 0)
  assert.match(src, /const reviewProvider = reviewStageNames\.length === 2 \? 'both'/)
  assert.match(src, /agentType:\s*'codex-code-verifier'/)
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
    if (options.agentType === 'codex-code-verifier') return failCodex ? null : {
      pass: true, detail: 'codex',
      findings: [{ file: 'b.c', line: 2,
        severity: blocking ? 'CONFIRMED P1 safety' : 'CONFIRMED P1 quality', summary: 'bug' }],
    }
    assert.equal(options.agentType, 'builder')
    return { board: 'test', pass: true, builtCount: 1, failures: [] }
  }
  const workflow = async () => { throw new Error('validate cannot nest a workflow') }
  const parallel = thunks => Promise.all(thunks.map(run => run()))
  const result = await fn(
    { boards: ['test'], skip: ['unit', 'size', 'pvs'], reviewProvider: 'both', maxCycles: 1 },
    agent, null, parallel, () => {}, () => {}, workflow, null,
  )
  assert.deepEqual(calls.sort(), ['builder', 'code-verifier', 'codex-code-verifier'])
  assert.equal(result.stages.find(s => s.stage === 'review').pass, true)
  assert.equal(result.stages.find(s => s.stage === 'codex').pass, true)

  blocking = true
  const blocked = await fn(
    { boards: ['test'], skip: ['unit', 'size', 'pvs'], reviewProvider: 'both', maxCycles: 1 },
    agent, null, parallel, () => {}, () => {}, workflow, null,
  )
  assert.equal(blocked.stages.find(s => s.stage === 'review').pass, false)
  assert.equal(blocked.stages.find(s => s.stage === 'codex').pass, false)

  failCodex = true
  const partial = await fn(
    { boards: ['test'], skip: ['unit', 'size', 'pvs'], reviewProvider: 'both', maxCycles: 1 },
    agent, null, parallel, () => {}, () => {}, workflow, null,
  )
  assert.equal(partial.stages.find(s => s.stage === 'review').detail, 'claude')
  assert.equal(partial.stages.find(s => s.stage === 'codex').detail, 'stage agent died')
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
      return { pass: true, detail: options.agentType, findings: [] }
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
    assert.deepEqual(calls.sort(), ['builder', 'codex-code-verifier'])
    assert.equal(result.stages.some(s => s.stage === 'review'), false)
    assert.equal(result.stages.find(s => s.stage === 'codex').pass, true)
  }

  const claudeOnly = await runValidate({ reviewProvider: 'claude' })
  assert.deepEqual(claudeOnly.calls.sort(), ['builder', 'code-verifier'])
  assert.equal(claudeOnly.result.stages.some(s => s.stage === 'codex'), false)

  const both = await runValidate({ reviewProvider: 'both' })
  assert.deepEqual(both.calls.sort(), ['builder', 'code-verifier', 'codex-code-verifier'])

  // skip still turns a selected reviewer off
  const skipped = await runValidate({ reviewProvider: 'both', skip: ['unit', 'size', 'pvs', 'review'] })
  assert.deepEqual(skipped.calls.sort(), ['builder', 'codex-code-verifier'])

  // selecting a reviewer and skipping it is contradictory input, not silence
  await assert.rejects(
    runValidate({ reviewProvider: 'claude', skip: ['unit', 'size', 'pvs', 'review'] }),
    /reviewProvider "claude" is cancelled by skip/,
  )
  await assert.rejects(
    runValidate({ reviewProvider: 'both', skip: ['unit', 'size', 'pvs', 'review', 'codex'] }),
    /reviewProvider "both" is cancelled by skip/,
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
    await assert.rejects(runValidate({ reviewProvider }), /reviewProvider must be codex, claude, or both/)
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
  await fn({ boards: ['test'], reviewProvider: 'both' },
    null, null, null, () => {}, () => {}, workflow, null)
  assert.deepEqual(seen.map(s => s.name), ['validate'])
  assert.equal(seen[0].args.reviewProvider, 'both')
  assert.deepEqual(seen[0].args.boards, ['test'])
})

await check('fanout simplifies once after all writers and before verification', async () => {
  const src = readFileSync(new URL('../fanout-dev.js', import.meta.url), 'utf8').replace(/^export /m, '')
  const fn = new AsyncFunction('args', 'agent', 'pipeline', 'log', 'workflow', src)
  const pipeline = (items, ...stages) => Promise.all(items.map(async item => {
    let value = item
    for (const stage of stages) value = await stage(value, item)
    return value
  }))
  const run = async ({ worktree = false, deadWriter = false, deadSimplifier = false,
    throwingSimplifier = false, changed = false, deadBuilder = false } = {}) => {
    const events = []
    const logs = []
    const simplification = { changed, files: changed ? ['a/file.c'] : [], summary: 'done' }
    const agent = async (prompt, options) => {
      if (options.agentType === 'code-writer') {
        const item = options.label.slice(4)
        if (item === 'a') await new Promise(resolve => setImmediate(resolve))
        events.push(`wrote:${item}`)
        if (deadWriter && item === 'a') return null
        assert.equal(options.isolation, worktree ? 'worktree' : undefined)
        return { item, board: item, buildOk: true, diffstat: '', notes: '' }
      }
      if (options.agentType === 'code-simplifier') {
        assert.deepEqual([...events].sort(), ['wrote:a', 'wrote:b'])
        assert.match(prompt, /Assigned scopes: \["a","b"\]/)
        events.push('simplified')
        if (throwingSimplifier) throw new Error('simplifier crashed')
        return deadSimplifier ? null : simplification
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
      agent, pipeline, message => logs.push(message), workflow)
    return { result, events, logs, simplification }
  }
  for (const changed of [false, true]) {
    const { result, events, simplification } = await run({ changed })
    assert.equal(events.filter(e => e === 'simplified').length, 1)
    assert.equal(result.length, 2)
    for (const row of result) {
      assert.equal(row.verifyBuild, true)
      assert.deepEqual(row.review, [])
      assert.deepEqual(row.simplification, simplification)
    }
  }
  await assert.rejects(run({ deadWriter: true }), /writer failed/)
  await assert.rejects(run({ deadSimplifier: true }), /simplifier failed/)
  await assert.rejects(run({ throwingSimplifier: true }), /simplifier crashed/)
  const isolated = await run({ worktree: true })
  assert.deepEqual(isolated.events.sort(), ['wrote:a', 'wrote:b'])
  assert.match(isolated.logs[0], /deferred until integration/)
  const unverified = await run({ deadBuilder: true })
  assert.ok(unverified.result.every(row => row.verifyBuild === null))
  assert.match(unverified.logs.at(-1), /0 build-clean/)
})

console.log(failed ? `\n${failed} FAILED` : '\nall checks passed')
process.exit(failed ? 1 : 0)
