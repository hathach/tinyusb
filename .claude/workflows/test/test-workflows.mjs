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

await check('every workflow compiles as a workflow body; a malformed one does not', async () => {
  for (const [name] of WORKFLOW_SRC) compile(name)
  assert.throws(() => new AsyncFunction('args', 'return }broken'), SyntaxError)
})

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

console.log(failed ? `\n${failed} FAILED` : '\nall checks passed')
process.exit(failed ? 1 : 0)
