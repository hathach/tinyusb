import assert from 'node:assert/strict'
import { readFileSync } from 'node:fs'

const AsyncFunction = Object.getPrototypeOf(async function () {}).constructor
const workflowBody = readFileSync(new URL('../pr-babysit.js', import.meta.url), 'utf8').replace(/^export /m, '')

const GREEN = { status: 'green', infraRerun: [], realFailures: [] }
const clone = (v) => (v === undefined ? v : structuredClone(v))
const finding = (over = {}) => ({
  source: 'codex', commentId: 1, file: 'src/a.c', line: 1,
  claim: 'bad', verdict: 'valid', reason: '', fixHint: 'fix it', ...over,
})
const oneValid = { findings: [finding()], replies: [], done: true }
const SHA = 'a1b2c3d4e5f60718293a4b5c6d7e8f9012345678'

// Drive the workflow against stub agents. `reviews`/`ci` accept an array to
// answer successive cycles; `fix`/`push` patch (or null out) those replies.
async function run(opts = {}) {
  const logs = []
  const labels = []
  const napPoints = [] // logs.length when a backoff started, to prove ordering
  const reviewsQueue = [].concat(opts.reviews ?? { findings: [], replies: [], done: true })
  const ciQueue = [].concat(opts.ci ?? GREEN)
  const next = (q) => (q.length > 1 ? q.shift() : q[0])

  const agent = async (prompt, options) => {
    const label = options.label
    labels.push(label)
    if (label.startsWith('ci#')) return clone(next(ciQueue))
    if (label.startsWith('reviews#')) {
      const r = next(reviewsQueue)
      if (r instanceof Error) throw r
      return clone(r)
    }
    if (label.startsWith('scope')) return { files: [] }
    if (label.startsWith('fix:')) {
      if (opts.fix === null) return null
      return {
        item: label.slice(4), diffstat: `stat:${label.slice(4)}`,
        buildOk: true, board: 'stm32f407disco', notes: '', ...opts.fix,
      }
    }
    if (label.startsWith('replies#') || label.startsWith('resolve#')) {
      return {
        pass: true, detail: 'posted',
        doneIds: [...String(prompt).matchAll(/"commentId":(\d+)/g)].map(m => Number(m[1])),
      }
    }
    if (label.startsWith('push#')) {
      if (opts.push === null) return { pass: false, detail: 'push rejected', sha: '' }
      if (opts.push instanceof Error) throw opts.push
      return { pass: true, detail: 'pushed to claude/foo', sha: SHA, ...opts.push }
    }
    throw new Error(`unstubbed agent label ${label}`)
  }
  const pipeline = async (items, first, second) => {
    const out = []
    for (const item of items) out.push(await second(await first(item), item))
    return out
  }
  const parallel = (thunks) => Promise.all(thunks.map(fn => fn()))
  const workflow = async () => clone(opts.verify ?? { addresses: true, reason: 'verified' })

  // nap()'s real delay is minutes; fire it immediately and record where in the
  // log stream it happened.
  const realTimeout = globalThis.setTimeout
  globalThis.setTimeout = (fn) => { napPoints.push(logs.length); realTimeout(fn, 0); return 0 }
  try {
    const fn = new AsyncFunction(
      'args', 'agent', 'pipeline', 'parallel', 'phase', 'log', 'workflow', 'budget', workflowBody)
    const result = await fn(
      { pr: 3888, maxCycles: 1, autoPush: true, ...opts.args },
      agent, pipeline, parallel, () => {}, (m) => logs.push(String(m)), workflow, null)
    return { result, logs, labels, napPoints }
  } finally {
    globalThis.setTimeout = realTimeout
  }
}

const summaries = (logs) => logs.filter(l => l.startsWith('cycle ') && l.includes(' summary '))
// Split on the padded delimiter, not on a bare pipe: an escaped `\|` inside a
// cell must stay part of that cell.
const rowsOf = (summary) => summary.split('\n').slice(3)
  .map(l => l.replace(/^\| /, '').replace(/ \|$/, '').split(' | ').map(c => c.trim()))

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

await check('args validation', async () => {
  await assert.rejects(run({ args: { pr: undefined } }), /args must be/)
  await assert.rejects(run({ args: { pr: 0 } }), /args must be/)
  await assert.rejects(run({ args: { pr: -3 } }), /positive integer/)
  await assert.rejects(run({ args: { pr: 'abc' } }), /positive integer/)
  await assert.rejects(run({ args: { maxCycles: 0 } }), /maxCycles must be/)
  await assert.rejects(run({ args: { checkoutDir: "/tmp/it's" } }), /plain path string/)
})

await check('a clean green PR passes and still logs a summary', async () => {
  const { result, logs } = await run()
  assert.equal(result.pass, true)
  assert.deepEqual(summaries(logs).length, 1)
  assert.match(summaries(logs)[0], /^cycle 1 summary — CI green, all bots settled\n\(no bot findings/)
  assert.equal(result.history[0].summary, summaries(logs)[0])
})

await check('the summary tables every verdict, fix and pushed SHA', async () => {
  const { result, logs } = await run({
    reviews: {
      findings: [
        finding({ commentId: 3, file: 'src/c.c', line: 3, claim: 'refuted | with a pipe', verdict: 'invalid' }),
        finding({ commentId: 1, file: 'src/a.c', line: 1, claim: 'real bug' }),
        finding({ commentId: 2, file: 'src/b.c', line: 2, claim: 'already gone', verdict: 'stale' }),
      ],
      // the validator drafts a reply for every invalid AND stale finding
      replies: [{ commentId: 3, body: 'refuted because…' }, { commentId: 2, body: 'already fixed in…' }],
      done: true,
    },
    ci: { status: 'green', infraRerun: [], realFailures: [] },
  })
  const rows = rowsOf(summaries(logs)[0])
  assert.deepEqual(rows.map(r => r[2]), ['valid', 'stale', 'invalid'], 'valid first, then stale, then invalid')
  assert.match(rows[0][3], /^fixed \+ pushed/)
  assert.equal(rows[0][4], SHA.slice(0, 8))
  assert.match(rows[1][3], /already fixed, replied \+ resolved/)
  assert.match(rows[2][3], /refuted, replied \+ resolved/)
  assert.deepEqual([rows[1][4], rows[2][4]], ['-', '-'], 'only fixed findings carry a commit')
  assert.match(rows[2][1], /refuted \\\| with a pipe/, 'a pipe in a claim is escaped, not table-breaking')
  assert.equal(result.history[0].reviewPush.sha, SHA)
})

await check('a fix whose build failed is never pushed', async () => {
  const { result, logs, labels } = await run({ reviews: oneValid, fix: { buildOk: false } })
  assert.equal(result.pass, false)
  assert.equal(result.reason, 'fix-verification-failed')
  assert.equal(labels.some(l => l.startsWith('push#')), false, 'no push may be attempted')
  assert.ok(logs.some(l => /targeted build FAILED/.test(l)))
  assert.match(rowsOf(summaries(logs)[0])[0][3], /BUILD FAILED/)
})

await check('a fix the verifier rejects is reported unverified, not pushed', async () => {
  const { result, logs, labels } = await run({
    reviews: oneValid, verify: { addresses: false, reason: 'does not address the claim' },
  })
  assert.equal(result.reason, 'fix-verification-failed')
  assert.equal(labels.some(l => l.startsWith('push#')), false)
  assert.match(rowsOf(summaries(logs)[0])[0][3], /unverified: does not address the claim/)
})

await check('a dry run leaves the fix uncommitted', async () => {
  const { result, logs, labels } = await run({ reviews: oneValid, args: { autoPush: false } })
  assert.equal(result.dryRun, true)
  assert.equal(labels.some(l => l.startsWith('push#') || l.startsWith('replies#')), false)
  assert.match(rowsOf(summaries(logs)[0])[0][3], /fixed, uncommitted/)
})

await check('a HIL roster edit is withheld rather than committed', async () => {
  const { result, logs, labels } = await run({
    reviews: { findings: [finding({ file: 'test/hil/tinyusb.json' })], replies: [], done: true },
  })
  assert.equal(result.reason, 'fix-verification-failed')
  assert.equal(labels.some(l => l.startsWith('fix:')), false, 'no fixer may be dispatched for a rig config')
  assert.ok(logs.some(l => /HIL rig config/.test(l)))
  assert.match(rowsOf(summaries(logs)[0])[0][3], /withheld/)
})

await check('two matrix legs of one check name keep separate fixes', async () => {
  const { logs } = await run({
    ci: {
      status: 'red', infraRerun: [],
      realFailures: [
        { check: 'build / arm', firstError: 'error in stm32f4', files: ['hw/bsp/stm32f4/family.c'], rigSide: false },
        { check: 'build / arm', firstError: 'error in nrf', files: ['hw/bsp/nrf/family.c'], rigSide: false },
      ],
    },
  })
  const rows = rowsOf(summaries(logs)[0])
  assert.match(rows[0][3], /stat:hw\/bsp\/stm32f4/)
  assert.match(rows[1][3], /stat:hw\/bsp\/nrf/, 'the second leg must not inherit the first fix')
})

await check('a rig-side CI failure is left red, with no fix and no commit', async () => {
  const { result, logs, labels } = await run({
    reviews: { findings: [], replies: [], done: true },
    ci: {
      status: 'red', infraRerun: [],
      realFailures: [{ check: 'hil / pico', firstError: 'board did not enumerate', files: [], rigSide: true }],
    },
  })
  assert.equal(result.reason, 'ci-red-rig-side')
  assert.equal(labels.some(l => l.startsWith('fix:')), false)
  const row = rowsOf(summaries(logs)[0])[0]
  assert.deepEqual([row[2], row[3], row[4]], ['rig-side', 'left red for the rig', '-'])
})

await check('a mislabeled push SHA is not shown as a commit', async () => {
  const { logs } = await run({ reviews: oneValid, push: { sha: 'committed 1234567 insertions' } })
  assert.equal(rowsOf(summaries(logs)[0])[0][4], '-')
})

await check('a dead review validator still settles the CI lane', async () => {
  const { result, logs, labels } = await run({ reviews: new Error('validator exploded') })
  assert.equal(result.reason, 'review-validator-died')
  assert.equal(result.history[0].error, 'pr-review-validator died')
  assert.equal(labels[0], 'ci#1', 'the CI lane was launched')
  assert.equal(result.history[0].ci.status, 'green', 'and awaited, so no agent outlives the workflow')
  assert.equal(summaries(logs).length, 1)
})

await check('a failed push stops the loop after a summary', async () => {
  const { result, logs } = await run({ reviews: oneValid, push: null })
  assert.equal(result.reason, 'push-failed')
  assert.equal(summaries(logs).length, 1)
})

await check('the pending-bot backoff is taken after the cycle summary', async () => {
  const { result, logs, napPoints } = await run({
    args: { maxCycles: 2 },
    reviews: { findings: [], replies: [], done: false },
  })
  assert.equal(result.reason, 'maxCycles reached')
  assert.equal(summaries(logs).length, 2, 'every cycle reports')
  assert.equal(napPoints.length, 1, 'no backoff after the last cycle')
  const firstSummaryAt = logs.findIndex(l => l.startsWith('cycle 1 summary'))
  assert.ok(napPoints[0] > firstSummaryAt, 'cycle 1 reported before the wait, not after it')
})

console.log(failed ? `\n${failed} FAILED` : '\nall checks passed')
process.exit(failed ? 1 : 0)
