import assert from 'node:assert/strict'
import { readdirSync, readFileSync } from 'node:fs'

const AsyncFunction = Object.getPrototypeOf(async function () {}).constructor
const source = readFileSync(new URL('./code-verify.js', import.meta.url), 'utf8')
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
      : options.model === 'haiku' ? 'codex' : 'unknown'
    calls.push({ provider, prompt, options })
    const answer = provided[provider]
    if (answer instanceof Error) throw answer
    return structuredClone(answer)
  }
  const parallel = async (thunks) => {
    parallelCalls++
    return Promise.all(thunks.map(fn => fn()))
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
  assert.equal(calls[0].options.effort, 'low')
  assert.deepEqual(calls[0].options.schema, RESULT)
  assert.match(calls[0].prompt, /\.codex\/agents\/code-verifier\.toml/)
  assert.match(calls[0].prompt, /tomllib/)
  assert.match(calls[0].prompt, /timeout 600s codex exec/)
  assert.match(calls[0].prompt, /--sandbox read-only/)
  assert.match(calls[0].prompt, /--output-schema/)
  assert.match(calls[0].prompt, /--output-last-message/)
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
  await assert.rejects(
    run({ prompt: 'review', schema: RESULT, provider: 'auto' }),
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
})

await check('all code-verifier calls use the router', async () => {
  const dir = new URL('.', import.meta.url)
  const offenders = readdirSync(dir)
    .filter(name => name.endsWith('.js') && name !== 'code-verify.js')
    .filter(name => /agentType:\s*['"]code-verifier['"]/.test(
      readFileSync(new URL(name, dir), 'utf8')))
  assert.deepEqual(offenders, [])
})

await check('validate uses one dual-provider router call', async () => {
  const src = readFileSync(new URL('./validate.js', import.meta.url), 'utf8')
  assert.equal((src.match(/workflow\(['"]code-verify['"]/g) || []).length, 1)
  assert.match(src, /const reviewProvider = reviewStageNames\.length === 2 \? 'both'/)
  assert.match(src, /provider:\s*reviewProvider/)
  assert.doesNotMatch(src, /codex review --base/)
  assert.doesNotMatch(src, /model:\s*['"]opus['"][^}]*schema:\s*REVIEW/)
})

await check('validate keeps provider results and gates separate', async () => {
  const src = readFileSync(new URL('./validate.js', import.meta.url), 'utf8').replace(/^export /m, '')
  const calls = []
  const fn = new AsyncFunction(
    'args', 'agent', 'pipeline', 'parallel', 'phase', 'log', 'workflow', 'budget', src)
  const agent = async (prompt, options) => {
    assert.equal(options.agentType, 'builder')
    return { board: 'test', pass: true, builtCount: 1, failures: [] }
  }
  const workflow = async (name, args) => {
    calls.push({ name, args })
    return {
      claude: {
        pass: true, detail: 'claude',
        findings: [{ file: 'a.c', line: 1, severity: 'CONFIRMED P2 correctness', summary: 'bug' }],
      },
      codex: {
        pass: true, detail: 'codex',
        findings: [{ file: 'b.c', line: 2, severity: 'CONFIRMED P2 correctness', summary: 'bug' }],
      },
    }
  }
  const parallel = thunks => Promise.all(thunks.map(run => run()))
  const result = await fn(
    { boards: ['test'], skip: ['unit', 'size', 'pvs'], maxCycles: 1 },
    agent, null, parallel, () => {}, () => {}, workflow, null,
  )
  assert.equal(calls.length, 1)
  assert.equal(calls[0].name, 'code-verify')
  assert.equal(calls[0].args.provider, 'both')
  assert.equal(result.stages.find(s => s.stage === 'review').pass, false)
  assert.equal(result.stages.find(s => s.stage === 'codex').pass, true)
})

console.log(failed ? `\n${failed} FAILED` : '\nall checks passed')
process.exit(failed ? 1 : 0)
