import assert from 'node:assert/strict'
import { readdirSync, readFileSync } from 'node:fs'

const AsyncFunction = Object.getPrototypeOf(async function () {}).constructor
// Read the workflow tree once: every check scans or runs from it.
const WORKFLOW_DIR = new URL('../', import.meta.url)
const WORKFLOW_SRC = readdirSync(WORKFLOW_DIR).filter(n => n.endsWith('.js'))
  .map(name => [name, readFileSync(new URL(name, WORKFLOW_DIR), 'utf8')])
const sourceOf = name => new Map(WORKFLOW_SRC).get(name)
// A workflow body runs as an async function over the runtime globals.
const compile = name => new AsyncFunction(
  'args', 'agent', 'pipeline', 'parallel', 'phase', 'log', 'workflow', 'budget',
  sourceOf(name).replace(/^export /m, ''))
const parallelAll = thunks => Promise.all(thunks.map(run => run()))

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

await check('no workflow nests another verification workflow or dispatches a Codex bridge', async () => {
  for (const [name, src] of WORKFLOW_SRC) {
    assert.doesNotMatch(src, /workflow\('code-verify'/, `${name} still nests code-verify`)
    assert.doesNotMatch(src, /codex-agent|reviewProvider/, `${name} still names the retired bridge`)
  }
})

await check('validate gates the review on confirmed P0/P1 findings and fails a dead reviewer', async () => {
  const fn = compile('validate.js')
  const workflow = async () => { throw new Error('validate cannot nest a workflow') }
  const runValidate = async review => {
    const calls = []
    const agent = async (prompt, options) => {
      calls.push(options.agentType)
      if (options.agentType === 'builder') return { board: 'test', pass: true, builtCount: 1, failures: [] }
      assert.equal(options.agentType, 'code-verifier')
      assert.match(prompt, /git diff master\.\.\.HEAD/)
      return review
    }
    const result = await fn({ boards: ['test'], skip: ['unit', 'size', 'pvs'], maxCycles: 1 },
      agent, null, parallelAll, () => {}, () => {}, workflow, null)
    assert.deepEqual(calls.sort(), ['builder', 'code-verifier'])
    return result.stages.find(s => s.stage === 'review')
  }
  const finding = severity => ({ file: 'a.c', line: 1, severity, summary: 'bug' })
  const clean = await runValidate({ pass: true, detail: 'ok', findings: [finding('CONFIRMED P2 correctness')] })
  assert.equal(clean.pass, true)
  const blocked = await runValidate({ pass: true, detail: 'ok', findings: [finding('CONFIRMED P1 safety')] })
  assert.equal(blocked.pass, false)
  const dead = await runValidate(null)
  assert.equal(dead.pass, false)
  assert.equal(dead.detail, 'stage agent died')
})

await check('validate runs no review only when review is skipped', async () => {
  const fn = compile('validate.js')
  const calls = []
  const logs = []
  const agent = async (prompt, options) => {
    calls.push(options.agentType)
    return { board: 'test', pass: true, builtCount: 1, failures: [] }
  }
  const result = await fn({ boards: ['test'], skip: ['unit', 'size', 'pvs', 'review'], maxCycles: 1 },
    agent, null, parallelAll, () => {}, l => logs.push(l), null, null)
  assert.deepEqual(calls, ['builder'])
  assert.equal(result.pass, true)
  assert.ok(logs.some(l => /no diff review will run/.test(l)))
})

await check('full-check forwards skip and maxCycles to validate', async () => {
  const fn = compile('full-check.js')
  const seen = []
  const workflow = async (name, workflowArgs) => {
    seen.push({ name, args: workflowArgs })
    return { pass: true }
  }
  await fn({ boards: ['test'], skip: ['review'], maxCycles: 1 },
    null, null, null, () => {}, () => {}, workflow, null)
  assert.deepEqual(seen.map(s => s.name), ['validate'])
  assert.deepEqual(seen[0].args.boards, ['test'])
  assert.deepEqual(seen[0].args.skip, ['review'])
  assert.equal(seen[0].args.maxCycles, 1)
})

await check('fanout simplifies once after all writers and before verification', async () => {
  const fn = compile('fanout-dev.js')
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
        assert.match(prompt, new RegExp(`build\\.py --scope ${item}\\b`))
        assert.doesNotMatch(prompt, /-e device\/cdc_msc/)
        if (item === 'a') await new Promise(resolve => setImmediate(resolve))
        events.push(`wrote:${item}`)
        if (deadWriter && item === 'a') return null
        assert.equal(options.isolation, worktree ? 'worktree' : undefined)
        // board is what code-writer.md returns when the prompt named none: empty
        return { item, board: '', buildOk: true, diffstat: '', notes: `note:${item}` }
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
      if (options.agentType === 'code-verifier') {
        assert.ok(events.includes(options.label.replace('review:', 'verify:')))
        assert.match(prompt, /git diff HEAD/)
        events.push(options.label)
        return { findings: [] }
      }
      assert.equal(options.agentType, 'builder')
      assert.equal(events.filter(e => e === 'simplified').length, 1)
      // the workflow names the scope itself; a writer that reports no board must not
      // leave the verifier building "for board ."
      assert.match(prompt, /--scope (a|b)\b/)
      assert.doesNotMatch(prompt, /--board\s*(;|$)/m)
      events.push(options.label)
      return deadBuilder ? null : { pass: true }
    }
    const workflow = async () => { throw new Error('fanout-dev cannot nest a workflow') }
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

console.log(failed ? `\n${failed} FAILED` : '\nall checks passed')
process.exit(failed ? 1 : 0)
