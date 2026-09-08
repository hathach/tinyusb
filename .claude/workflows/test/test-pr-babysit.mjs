import assert from 'node:assert/strict'
import { readFileSync } from 'node:fs'

const AsyncFunction = Object.getPrototypeOf(async function () {}).constructor
const WORKFLOW_SRC = readFileSync(new URL('../pr-babysit.js', import.meta.url), 'utf8')
const workflowBody = readFileSync(new URL('../pr-babysit.js', import.meta.url), 'utf8').replace(/^export /m, '')

const GREEN = { status: 'green', infraRerun: [], realFailures: [] }
const finding = (over = {}) => {
  const f = {
    source: 'codex', commentId: 1, file: 'src/a.c', line: 1,
    claim: 'bad', verdict: 'valid', reason: '', fixHint: 'fix it', ...over,
  }
  return { findingId: `${f.commentId}#${f.line}`, commentDigest: `d${f.commentId}`, ...f } // overridable
}
const oneValid = { findings: [finding()], replies: [], done: true }
const SHA = 'a1b2c3d4e5f60718293a4b5c6d7e8f9012345678'

// Drive the workflow against stub agents. Every cycle gets the same `reviews`
// and `ci` answer; `fix`/`push` patch (or null out) those replies.
async function run(opts = {}) {
  const logs = []
  const calls = []
  const napPoints = [] // logs.length when a backoff started, to prove ordering
  const reviews = opts.reviews ?? { findings: [], replies: [], done: true }
  const ci = opts.ci ?? GREEN

  const agent = async (prompt, options) => {
    const label = options.label
    calls.push({ label, prompt: String(prompt), agentType: options.agentType })
    if (opts.throwOn && label.startsWith(opts.throwOn)) throw new Error(`${label} exploded`)
    if (label.startsWith('ci#')) return structuredClone(ci)
    if (label.startsWith('reviews#')) {
      if (opts.reviewsPerCycle) return opts.reviewsPerCycle()
      if (reviews instanceof Error) throw reviews
      return structuredClone(reviews)
    }
    if (label.startsWith('scope')) return { files: [] }
    if (label.startsWith('fix:')) {
      if (opts.fix === null) return null // a dead code-writer
      return {
        item: label.slice(4), diffstat: `stat:${label.slice(4)}`,
        buildOk: true, board: 'stm32f407disco', notes: '', ...opts.fix,
      }
    }
    if (label.startsWith('replies#') || label.startsWith('resolve#')) {
      if (opts.dropDoneIds && opts.dropDoneIds(label)) return { pass: false, detail: 'posting failed', doneIds: [] }
      return {
        pass: true, detail: 'posted',
        doneIds: [...[...String(prompt).matchAll(/"commentId":(\d+)/g)].map(m => Number(m[1])),
          ...(opts.strayDoneIds || [])],
      }
    }
    if (label.startsWith('push#')) {
      if (opts.push === null) return { pass: false, committed: true, detail: 'push rejected', sha: '' }
      return { pass: true, committed: true, detail: 'pushed to claude/foo', sha: SHA, ...opts.push }
    }
    throw new Error(`unstubbed agent label ${label}`)
  }
  // Match the host's pipeline: every item's first stage runs concurrently, each
  // second stage starts as soon as its own first stage lands, results in order.
  const pipeline = (items, first, second) =>
    Promise.all(items.map(async item => second(await first(item), item)))
  const parallel = (thunks) => Promise.all(thunks.map(fn => fn()))
  const workflowCalls = []
  const workflow = async (name, wargs) => {
    workflowCalls.push({
      name,
      label: (wargs && wargs.label) || '',
      prompt: (wargs && wargs.prompt) || '',
      provider: wargs && wargs.provider,
    })
    if (String((wargs && wargs.label) || '').startsWith('challenge#')) {
      if (opts.challengePerCycle) return opts.challengePerCycle()
      if (!('challenge' in opts)) {
        // default: uphold every submitted dismissal, i.e. today's behaviour
        const ids = [...String(wargs.prompt).matchAll(/"id":(\d+)/g)].map(m => Number(m[1]))
        return { verdicts: ids.map(id => ({ id, upheld: true, reason: 'stands' })) }
      }
      return opts.challenge === null ? null : structuredClone(opts.challenge)
    }
    return structuredClone(opts.verify ?? { addresses: true, reason: 'verified' })
  }

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
    return { result, logs, labels: calls.map(c => c.label), calls, napPoints, workflowCalls }
  } finally {
    globalThis.setTimeout = realTimeout
  }
}

const summaries = (logs) => logs.filter(l => l.startsWith('cycle ') && l.includes(' summary '))
// Split on the padded delimiter, not on a bare pipe: an escaped `\|` inside a
// cell must stay part of that cell.
const rowsOf = (summary) => summary.split('\n').slice(3)
  .map(l => l.replace(/^\| /, '').replace(/ \|$/, '').split(' | ').map(c => c.trim()))
// GFM's own row rule: a backslash escapes the next character, so only an
// unescaped pipe splits cells. Applying it is the only way to prove a claim
// carrying `\|` renders inside one cell instead of spilling into extra columns.
const gfmCells = (row) => {
  const cells = ['']
  for (let i = 0; i < row.length; i++) {
    if (row[i] === '\\') cells[cells.length - 1] += row[++i] ?? ''
    else if (row[i] === '|') cells.push('')
    else cells[cells.length - 1] += row[i]
  }
  return cells.slice(1, -1).map(c => c.trim()) // the framing pipes leave an empty cell at each end
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

await check('a claim already containing a backslash-pipe stays one cell', async () => {
  const { logs } = await run({
    reviews: { findings: [finding({ claim: 'the regex \\| splits the row' })], replies: [], done: true },
  })
  const cells = gfmCells(summaries(logs)[0].split('\n')[3])
  assert.equal(cells.length, 5, 'the row keeps exactly its five columns')
  assert.equal(cells[1], 'src/a.c:1 the regex \\| splits the row', 'and renders the backslash and pipe literally')
})

await check('a fix whose build failed is never pushed, and skips the verifier', async () => {
  const { result, logs, labels } = await run({ reviews: oneValid, fix: { buildOk: false } })
  assert.equal(result.pass, false)
  assert.equal(result.reason, 'fix-verification-failed')
  assert.equal(labels.some(l => l.startsWith('push#')), false, 'no push may be attempted')
  assert.ok(logs.some(l => /targeted build FAILED/.test(l)))
  assert.match(rowsOf(summaries(logs)[0])[0][3], /unverified: targeted build failed/,
    'reported as unverified, and the verifier is not paid for a broken build')
})

await check('a dead code-writer withholds the fix', async () => {
  const { result, logs, labels } = await run({ reviews: oneValid, fix: null })
  assert.equal(result.reason, 'fix-verification-failed')
  assert.equal(labels.some(l => l.startsWith('push#')), false)
  assert.ok(logs.some(l => /lost to dead workers/.test(l)))
  assert.match(rowsOf(summaries(logs)[0])[0][3], /withheld/)
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
  const row = rowsOf(summaries(logs)[0])[0]
  assert.match(row[3], /fixed \+ committed, PUSH FAILED: push rejected/,
    'the fix is committed locally — the row must say so, and say the push failed')
  assert.equal(row[4], '-', 'a failed push carries no commit SHA')
  assert.equal(result.history[0].reviewPushFailed.detail, 'push rejected')
  const dry = await run({ reviews: oneValid, args: { autoPush: false } })
  assert.notEqual(row[3], rowsOf(summaries(dry.logs)[0])[0][3], 'and reads differently from a dry run')
})

await check('a push that failed before committing is not reported as committed', async () => {
  const { result, logs } = await run({
    reviews: oneValid,
    push: { pass: false, committed: false, detail: 'pre-commit hook rejected', sha: '' },
  })
  assert.equal(result.reason, 'push-failed')
  const row = rowsOf(summaries(logs)[0])[0]
  assert.match(row[3], /fixed, COMMIT FAILED: pre-commit hook rejected/,
    'nothing landed in git — the row must not send the reader after a nonexistent commit')
  assert.doesNotMatch(row[3], /PUSH FAILED|\+ committed/, 'and must not claim a commit to recover')
  assert.equal(row[4], '-')
})

await check('a worker rejection outside the guarded lanes still reports the cycle', async () => {
  const { result, logs } = await run({ reviews: oneValid, throwOn: 'push#' })
  assert.equal(result.pass, false)
  assert.equal(result.reason, 'cycle-threw')
  assert.equal(result.history.length, 1, 'the verdict keeps the history it was built from')
  assert.match(result.history[0].error, /cycle threw: push#1-review exploded/)
  assert.equal(summaries(logs).length, 1, 'the scoreboard survives an agent-failure cycle')
  const row = rowsOf(summaries(logs)[0])[0]
  assert.match(row[3], /fixed, uncommitted/, 'the push never landed, so the fix is only in the tree')
  assert.equal(row[4], '-')
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

await check('reviews never route to the Codex bridge', async () => {
  // The validator's whole procedure is `gh`, and the launcher's read-only
  // sandbox has no network: a Codex-hosted run reports an empty harvest as a
  // settled one rather than failing. It must not be reachable from here.
  const { calls } = await run()
  const reviews = calls.find(c => c.label.startsWith('reviews#'))
  assert.equal(reviews.agentType, 'pr-review-validator')
  assert.equal(reviews.prompt.includes('"role"'), false)
  assert.equal(calls.some(c => c.agentType === 'codex-agent'), false)
  assert.doesNotMatch(WORKFLOW_SRC, /pr-review-validator['"]?\s*,\s*prompt|role: 'pr-review-validator'/)
})

const invalidFinding = (over = {}) => finding({ verdict: 'invalid', ...over })

await check('a Claude validator is challenged by Codex, not left unchecked', async () => {
  // The challenge protects the act of publicly refuting a reviewer. Validation
  // is Claude's, so the check on it has to be someone else's - and code-verifier
  // is the one role that works on Codex, reading the checkout with no network.
  const { workflowCalls } = await run({
    reviews: { findings: [invalidFinding()], replies: [{ commentId: 1, body: 'no' }], done: true },
  })
  const ch = workflowCalls.find(w => w.label.startsWith('challenge#'))
  assert.ok(ch, 'a Claude-validated refutation must still be challenged')
  assert.equal(ch.provider, 'codex')
})

await check('the challenge routes through code-verify', async () => {
  const { workflowCalls } = await run({
    reviews: { findings: [invalidFinding()], replies: [{ commentId: 1, body: 'no' }], done: true },
  })
  const ch = workflowCalls.find(w => w.label.startsWith('challenge#'))
  assert.ok(ch, 'expected a challenge stage')
  assert.equal(ch.name, 'code-verify')
})

await check('an upheld refutation still replies and resolves', async () => {
  const { calls } = await run({
    reviews: { findings: [invalidFinding()], replies: [{ commentId: 1, body: 'no' }], done: true },
    challenge: { verdicts: [{ id: 0, upheld: true, reason: 'stands' }] },
    args: { autoPush: true },
  })
  const posted = calls.find(c => c.label.startsWith('replies#'))
  assert.ok(posted, 'expected the refutation to be posted')
  assert.match(posted.prompt, /"commentId":1/)
})

await check('an overturned finding is fixed, replied to, and carries the challenger reason', async () => {
  const { calls } = await run({
    reviews: { findings: [invalidFinding()], replies: [{ commentId: 1, body: 'no' }], done: true },
    challenge: { verdicts: [{ id: 0, upheld: false, reason: 'the NAK path is real' }] },
    args: { autoPush: true },
  })
  assert.equal(calls.some(c => c.label.startsWith('replies#')), false, 'no refutation is posted')
  const fix = calls.find(c => c.label.startsWith('fix:'))
  assert.match(fix.prompt, /the NAK path is real/)
  // every finding on the comment was overturned, so the fix note is NOT withheld
  const resolve = calls.find(c => c.label.startsWith('resolve#'))
  assert.ok(resolve, 'expected the fix note to be posted')
  assert.match(resolve.prompt, /"commentId":1/)
})

await check('a mixed comment defers its reply and blocks the green exit', async () => {
  const { calls, result } = await run({
    reviews: {
      findings: [invalidFinding({ commentId: 7 }), invalidFinding({ commentId: 7, line: 9 })],
      replies: [{ commentId: 7, body: 'both wrong' }],
      done: true,
    },
    challenge: { verdicts: [
      { id: 0, upheld: false, reason: 'real' },
      { id: 1, upheld: true, reason: 'stands' },
    ] },
    args: { autoPush: true, maxCycles: 1 },
  })
  assert.equal(calls.some(c => c.label.startsWith('replies#')), false)
  const resolve = calls.find(c => c.label.startsWith('resolve#'))
  if (resolve) assert.doesNotMatch(resolve.prompt, /"commentId":7/)
  assert.equal(result.pass, false)
  assert.equal(result.reason, 'deferred-replies-unresolved')
  assert.deepEqual(result.deferred, [7])
})

await check('a broken challenge response fails the cycle', async () => {
  for (const challenge of [
    null,
    { verdicts: [] },
    { verdicts: [{ id: 9, upheld: true, reason: 'x' }] },
    { verdicts: [{ id: 0, upheld: true, reason: 'x' }, { id: 0, upheld: false, reason: 'y' }] },
  ]) {
    const { result } = await run({
      reviews: { findings: [invalidFinding()], replies: [{ commentId: 1, body: 'no' }], done: true },
      challenge, args: { autoPush: true, maxCycles: 1 },
    })
    assert.equal(result.reason, 'review-challenger-died')
  }
})

await check('the summary marks an overturned finding', async () => {
  const { logs } = await run({
    reviews: { findings: [invalidFinding()], replies: [{ commentId: 1, body: 'no' }], done: true },
    challenge: { verdicts: [{ id: 0, upheld: false, reason: 'real' }] },
    args: { autoPush: true, maxCycles: 1 },
  })
  assert.ok(logs.some(l => l.includes('overturned')), 'cycle table must show the overturn')
})

await check('a deferred obligation is cleared only by a posted reply', async () => {
  // Cycle 1 defers comment 7 (one overturned, one upheld). A later cycle sees
  // only the upheld one, posts it, and that posting is what clears the deferral.
  let cycle = 0
  const { result } = await run({
    args: { autoPush: true, maxCycles: 4 },
    reviewsPerCycle: () => {
      cycle++
      return cycle === 1
        ? { findings: [invalidFinding({ commentId: 7 }), invalidFinding({ commentId: 7, line: 9 })],
          replies: [{ commentId: 7, body: 'both wrong' }], done: true }
        : { findings: [invalidFinding({ commentId: 7, line: 9 })],
          replies: [{ commentId: 7, body: 'still wrong' }], done: true }
    },
    challengePerCycle: () => cycle === 1
      ? { verdicts: [{ id: 0, upheld: false, reason: 'real' }, { id: 1, upheld: true, reason: 'stands' }] }
      : { verdicts: [{ id: 0, upheld: true, reason: 'stands' }] },
  })
  assert.equal(result.pass, true, `the posted reply must discharge the deferral (got ${result.reason})`)
})

await check('an orphan reply is never posted unchallenged', async () => {
  // REVIEWS does not tie replies to findings, so a validator can emit a reply
  // for a commentId that has no non-valid finding. Nothing challenges it, and
  // posting it publicly refutes a reviewer on no one's authority.
  const { calls } = await run({
    reviews: {
      findings: [finding({ commentId: 1, verdict: 'valid' })],
      replies: [{ commentId: 99, body: 'you are wrong' }],
      done: true,
    },
    args: { autoPush: true, maxCycles: 1 },
  })
  const posted = calls.find(c => c.label.startsWith('replies#'))
  if (posted) assert.doesNotMatch(posted.prompt, /"commentId":99/, 'orphan reply was posted')
})

await check('a stray doneId cannot discharge an unrelated deferral', async () => {
  // The reply/resolve agents return doneIds. Trusting an id that was never in
  // the payload lets one clear an obligation nobody answered.
  let cycle = 0
  const { result } = await run({
    args: { autoPush: true, maxCycles: 2 },
    strayDoneIds: [7],
    reviewsPerCycle: () => {
      cycle++
      return cycle === 1
        ? { findings: [invalidFinding({ commentId: 7 }), invalidFinding({ commentId: 7, line: 9 })],
          replies: [{ commentId: 7, body: 'both wrong' }], done: true }
        : { findings: [], replies: [], done: true }
    },
    challengePerCycle: () => ({ verdicts: [
      { id: 0, upheld: false, reason: 'real' }, { id: 1, upheld: true, reason: 'stands' }] }),
  })
  assert.equal(result.reason, 'deferred-replies-unresolved',
    'a stray doneId discharged comment 7 without any reply being posted')
})

await check('a refutation with no drafted reply is not silently dropped', async () => {
  // REVIEWS lets a validator report a non-valid finding and draft no reply for
  // it. Nothing else accounts for that answer, so the cycle could go green with
  // the reviewer's thread untouched.
  const { result } = await run({
    reviews: { findings: [invalidFinding({ commentId: 5 })], replies: [], done: true },
    challenge: { verdicts: [{ id: 0, upheld: true, reason: 'stands' }] },
    args: { autoPush: true, maxCycles: 1 },
  })
  assert.equal(result.pass, false, 'an unanswered refutation must not pass')
  assert.deepEqual(result.deferred, [5])
})

await check('a comment mixing valid and refuted findings is not resolved early', async () => {
  // Posting the refutation resolves the whole thread, hiding the valid finding
  // until its fix lands - and burying it for good if the fixer then fails.
  const { calls, result } = await run({
    reviews: {
      findings: [finding({ commentId: 3, verdict: 'valid' }), invalidFinding({ commentId: 3, line: 9 })],
      replies: [{ commentId: 3, body: 'the second one is wrong' }],
      done: true,
    },
    challenge: { verdicts: [{ id: 0, upheld: true, reason: 'stands' }] },
    args: { autoPush: true, maxCycles: 1 },
  })
  const posted = calls.find(c => c.label.startsWith('replies#'))
  if (posted) assert.doesNotMatch(posted.prompt, /"commentId":3/, 'thread resolved before the fix')
  assert.equal(result.pass, false)
  assert.deepEqual(result.deferred, [3])
})

await check('a fix note does not discharge a deferred refutation', async () => {
  // The fix note says "fixed in commit X"; it is not the refutation that was
  // owed. Letting its doneIds clear the deferral answers the thread with the
  // wrong content and lets the next green cycle pass.
  let cycle = 0
  const { result } = await run({
    args: { autoPush: true, maxCycles: 2 },
    reviewsPerCycle: () => {
      cycle++
      return cycle === 1
        ? { findings: [finding({ commentId: 4, verdict: 'valid' }), invalidFinding({ commentId: 4, line: 9 })],
          replies: [{ commentId: 4, body: 'the second is wrong' }], done: true }
        : { findings: [finding({ commentId: 4, verdict: 'valid' })], replies: [], done: true }
    },
    challengePerCycle: () => ({ verdicts: [{ id: 0, upheld: true, reason: 'stands' }] }),
  })
  assert.equal(result.reason, 'deferred-replies-unresolved',
    'a fix note discharged a refutation it never made')
})

await check('a comment is never replied to twice in one cycle', async () => {
  const { calls } = await run({
    reviews: {
      findings: [invalidFinding({ commentId: 8 })],
      replies: [{ commentId: 8, body: 'wrong' }, { commentId: 8, body: 'also wrong' }],
      done: true,
    },
    challenge: { verdicts: [{ id: 0, upheld: true, reason: 'stands' }] },
    args: { autoPush: true, maxCycles: 1 },
  })
  const posted = calls.find(c => c.label.startsWith('replies#'))
  assert.ok(posted)
  assert.equal([...posted.prompt.matchAll(/"commentId":8/g)].length, 1, 'duplicate reply posted')
})

await check('a deferred refutation can still be posted after a fix note', async () => {
  // The fix note puts the comment in repliedIds. If that also blocks the reply
  // stage, the refutation it still owes can never go out and the deferral is
  // permanent.
  let cycle = 0
  const { result } = await run({
    args: { autoPush: true, maxCycles: 4 },
    reviewsPerCycle: () => {
      cycle++
      if (cycle === 1) {
        return { findings: [finding({ commentId: 4, verdict: 'valid' }), invalidFinding({ commentId: 4, line: 9 })],
          replies: [{ commentId: 4, body: 'the second is wrong' }], done: true }
      }
      if (cycle === 2) return { findings: [finding({ commentId: 4, verdict: 'valid' })], replies: [], done: true }
      return { findings: [invalidFinding({ commentId: 4, line: 9 })],
        replies: [{ commentId: 4, body: 'still wrong' }], done: true }
    },
    challengePerCycle: () => ({ verdicts: [{ id: 0, upheld: true, reason: 'stands' }] }),
  })
  assert.equal(result.pass, true, `the owed refutation must be postable (got ${result.reason})`)
})

await check('an already-answered comment is not deferred for a missing draft', async () => {
  // repliedIds means the thread was answered and resolved. Deferring it again
  // because this harvest drafted no reply creates an obligation nothing can
  // discharge.
  let cycle = 0
  const { result } = await run({
    args: { autoPush: true, maxCycles: 3 },
    reviewsPerCycle: () => {
      cycle++
      return cycle === 1
        ? { findings: [invalidFinding({ commentId: 7 })],
          replies: [{ commentId: 7, body: 'wrong' }], done: true }
        : { findings: [invalidFinding({ commentId: 7 })], replies: [], done: true }
    },
    challengePerCycle: () => ({ verdicts: [{ id: 0, upheld: true, reason: 'stands' }] }),
  })
  assert.equal(result.pass, true, `an answered comment must not be re-deferred (got ${result.reason})`)
})

await check('an obligation dies with the finding that created it', async () => {
  // Cycle 1 defers a mixed comment. Cycle 2 overturns its refuted half, so
  // nothing is owed but a fix note. A stored obligation would outlive its
  // cause here and end the run as unresolved with nothing actually owed.
  let cycle = 0
  const { result } = await run({
    args: { autoPush: true, maxCycles: 4 },
    reviewsPerCycle: () => {
      cycle++
      return { findings: [finding({ commentId: 6, verdict: 'valid' }),
        invalidFinding({ commentId: 6, line: 9 })],
      replies: [{ commentId: 6, body: 'the second is wrong' }], done: true }
    },
    challengePerCycle: () => cycle === 1
      ? { verdicts: [{ id: 0, upheld: true, reason: 'stands' }] }
      : { verdicts: [{ id: 0, upheld: false, reason: 'actually real' }] },
  })
  assert.notEqual(result.reason, 'deferred-replies-unresolved',
    'the deferral outlived the refuted finding that created it')
  assert.equal(result.deferred, undefined)
})

await check('overturning one refutation does not excuse a vanished sibling', async () => {
  // Comment 7 owes two refutations. The next harvest drops one and the
  // challenger overturns the other; the fix note then resolves the thread. The
  // dropped dismissal was never answered, so it still holds the loop open.
  let cycle = 0
  const { result } = await run({
    args: { autoPush: true, maxCycles: 4 },
    reviewsPerCycle: () => {
      cycle++
      if (cycle === 1) {
        return { findings: [invalidFinding({ commentId: 7 }), invalidFinding({ commentId: 7, line: 9 })],
          replies: [], done: true }
      }
      if (cycle === 2) return { findings: [invalidFinding({ commentId: 7, line: 9 })], replies: [], done: true }
      return { findings: [], replies: [], done: true }
    },
    challengePerCycle: () => cycle === 1
      ? { verdicts: [{ id: 0, upheld: true, reason: 'stands' }, { id: 1, upheld: true, reason: 'stands' }] }
      : { verdicts: [{ id: 0, upheld: false, reason: 'real' }] },
  })
  assert.equal(result.reason, 'deferred-replies-unresolved',
    'one overturn discharged an unrelated unanswered dismissal')
  assert.deepEqual(result.deferred, [7])
})

await check('a retried fix note discharges its own carried obligation', async () => {
  // The first attempt posts nothing, so the fix note is owed into the next
  // cycle. Only that same answer can clear it - no refutation was ever owed.
  let cycle = 0
  const { result } = await run({
    args: { autoPush: true, maxCycles: 4 },
    dropDoneIds: (label) => cycle === 1 && label.startsWith('resolve#'),
    reviewsPerCycle: () => {
      cycle++
      return cycle <= 2 ? structuredClone(oneValid) : { findings: [], replies: [], done: true }
    },
  })
  assert.equal(result.pass, true, `the retried fix note must clear its deferral (got ${result.reason})`)
})

await check('a stale re-report of a fixed finding owes nothing', async () => {
  // The thread was answered and resolved by the fix note. Reporting the same
  // finding stale afterwards is a consequence of our own fix, not a dismissal
  // owed to the reviewer - and no reply is ever drafted for it.
  let cycle = 0
  const { result } = await run({
    args: { autoPush: true, maxCycles: 3 },
    reviewsPerCycle: () => {
      cycle++
      return cycle === 1
        ? structuredClone(oneValid)
        : { findings: [finding({ verdict: 'stale' })], replies: [], done: true }
    },
  })
  assert.equal(result.pass, true, `a stale re-report reopened a settled comment (got ${result.reason})`)
})

await check('an edit to an answered comment owes an answer again', async () => {
  // The fix note answered and resolved comment 1. The reviewer then edits the
  // body: the point now being made is not the one we answered, so it accrues a
  // dismissal the run must not pass without.
  let cycle = 0
  const { result, logs } = await run({
    args: { autoPush: true, maxCycles: 3 },
    reviewsPerCycle: () => {
      cycle++
      return cycle === 1
        ? structuredClone(oneValid)
        : { findings: [invalidFinding({ commentDigest: 'edited' })], replies: [], done: true }
    },
  })
  assert.ok(logs.some(l => l.includes('comment 1 was edited after we answered it')), 'the edit must be reported')
  assert.notEqual(result.pass, true, 'an edit to an answered comment went unnoticed')
  assert.deepEqual(result.deferred, [1])
})

await check('a fix note reads as deferred only while it still owes a dismissal', async () => {
  // Nothing is outstanding here: the fix note closed the thread, so calling it
  // deferred names a next cycle that has nothing to do.
  let cycle = 0
  const paid = await run({
    args: { autoPush: true, maxCycles: 2 },
    reviewsPerCycle: () => {
      cycle++
      return cycle === 1
        ? structuredClone(oneValid)
        : { findings: [finding({ verdict: 'stale' })], replies: [], done: true }
    },
  })
  assert.match(rowsOf(summaries(paid.logs)[1])[0][3], /already fixed, answered by fix note/)

  // Comment 4's refuted half is never replied to, so the fix note that answered
  // its valid half leaves that dismissal owed - and deferred is the right word.
  let mixed = 0
  const owing = await run({
    args: { autoPush: true, maxCycles: 3 },
    reviewsPerCycle: () => {
      mixed++
      if (mixed === 1) {
        return { findings: [finding({ commentId: 4, verdict: 'valid' }), invalidFinding({ commentId: 4, line: 9 })],
          replies: [], done: true }
      }
      if (mixed === 2) return { findings: [finding({ commentId: 4, verdict: 'valid' })], replies: [], done: true }
      return { findings: [invalidFinding({ commentId: 4, line: 9 })], replies: [], done: true }
    },
  })
  assert.match(rowsOf(summaries(owing.logs)[2])[0][3], /refuted, deferred to next cycle/)
})

await check('a dry run reports refutations it would post rather than deferring them', async () => {
  // Without autoPush nothing is posted, so an obligation would spin the loop to
  // exhaustion and call it unresolved. The review lane already says dryRun.
  const { calls, result } = await run({
    args: { autoPush: false, maxCycles: 3 },
    reviews: { findings: [invalidFinding()], replies: [{ commentId: 1, body: 'no' }], done: true },
  })
  assert.equal(calls.some(c => c.label.startsWith('replies#')), false, 'a dry run must post nothing')
  assert.equal(result.dryRun, true)
  assert.equal(result.reason, undefined)
})

await check('a dropped dismissal survives a later harvest that reports fewer', async () => {
  // Comment 7 owes two refutations. The next harvest reports only one, and the
  // one after overturns and fixes it. Taking that shrinking harvest as the
  // standing debt would forget the dismissal nobody ever answered.
  let cycle = 0
  const { result } = await run({
    args: { autoPush: true, maxCycles: 5 },
    reviewsPerCycle: () => {
      cycle++
      if (cycle === 1) {
        return { findings: [invalidFinding({ commentId: 7 }), invalidFinding({ commentId: 7, line: 9 })],
          replies: [], done: true }
      }
      if (cycle <= 3) return { findings: [invalidFinding({ commentId: 7, line: 9 })], replies: [], done: true }
      return { findings: [], replies: [], done: true }
    },
    challengePerCycle: () => cycle === 3
      ? { verdicts: [{ id: 0, upheld: false, reason: 'real' }] }
      : { verdicts: [{ id: 0, upheld: true, reason: 'stands' }, { id: 1, upheld: true, reason: 'stands' }]
        .slice(0, cycle === 1 ? 2 : 1) },
  })
  assert.equal(result.reason, 'deferred-replies-unresolved',
    'a shrinking harvest discharged a dismissal nobody answered')
  assert.deepEqual(result.deferred, [7])
})

await check('a stale reply settles the fix note its own failed attempt owed', async () => {
  // The fix note fails to post, then the finding comes back stale and its
  // "already fixed" reply posts and resolves the thread. That answers the
  // comment; holding the earlier note type against it defers it forever.
  let cycle = 0
  const { result } = await run({
    args: { autoPush: true, maxCycles: 4 },
    dropDoneIds: (label) => cycle === 1 && label.startsWith('resolve#'),
    reviewsPerCycle: () => {
      cycle++
      if (cycle === 1) return structuredClone(oneValid)
      if (cycle === 2) return { findings: [finding({ verdict: 'stale' })], replies: [{ commentId: 1, body: 'already fixed' }], done: true }
      return { findings: [], replies: [], done: true }
    },
  })
  assert.equal(result.pass, true, `the stale reply must settle the comment (got ${result.reason})`)
})

await check('an answered comment is not replied to again for a stale re-report', async () => {
  // The fix note already resolved the thread. A drafted "already fixed" reply
  // for the same finding is a second answer to a closed thread.
  let cycle = 0
  const { calls } = await run({
    args: { autoPush: true, maxCycles: 2 },
    reviewsPerCycle: () => {
      cycle++
      return cycle === 1
        ? structuredClone(oneValid)
        : { findings: [finding({ verdict: 'stale' })], replies: [{ commentId: 1, body: 'already fixed' }], done: true }
    },
  })
  assert.equal(calls.some(c => c.label.startsWith('replies#')), false,
    'a resolved thread was answered twice')
})

await check('a dry run still runs the fixers it is allowed to run', async () => {
  // Withholding the refutation must not also withhold the local fix work the
  // review lane already promises to leave uncommitted.
  const { calls, result } = await run({
    args: { autoPush: false, maxCycles: 2 },
    reviews: {
      findings: [invalidFinding({ commentId: 1 }), finding({ commentId: 2, verdict: 'valid' })],
      replies: [{ commentId: 1, body: 'no' }],
      done: true,
    },
  })
  assert.ok(calls.some(c => c.label.startsWith('fix:')), 'the dry run skipped the fixer')
  assert.equal(calls.some(c => c.label.startsWith('replies#')), false, 'a dry run must post nothing')
  assert.equal(result.dryRun, true)
})

await check('re-overturning a finding does not retire a different dismissal', async () => {
  // Comment 7 owes A and B. B is overturned and fixed, then reported and
  // overturned again in a later cycle. Retiring by count would spend that
  // second overturn on A, which nobody ever answered.
  let cycle = 0
  const { result } = await run({
    args: { autoPush: true, maxCycles: 5 },
    reviewsPerCycle: () => {
      cycle++
      if (cycle === 1) {
        return { findings: [invalidFinding({ commentId: 7 }), invalidFinding({ commentId: 7, line: 9 })],
          replies: [], done: true }
      }
      if (cycle <= 3) return { findings: [invalidFinding({ commentId: 7, line: 9 })], replies: [], done: true }
      return { findings: [], replies: [], done: true }
    },
    challengePerCycle: () => cycle === 1
      ? { verdicts: [{ id: 0, upheld: true, reason: 'stands' }, { id: 1, upheld: true, reason: 'stands' }] }
      : { verdicts: [{ id: 0, upheld: false, reason: 'real' }] },
  })
  assert.equal(result.reason, 'deferred-replies-unresolved',
    'a repeated overturn discharged an unrelated dismissal')
  assert.deepEqual(result.deferred, [7])
})

await check('a dry run runs the CI fixer before reporting withheld replies', async () => {
  // The refutation is withheld either way; returning on it before the CI lane
  // would skip fix work the dry run is allowed to do and leave uncommitted.
  const { calls, result } = await run({
    args: { autoPush: false, maxCycles: 2 },
    reviews: { findings: [invalidFinding()], replies: [{ commentId: 1, body: 'no' }], done: true },
    ci: {
      status: 'red',
      infraRerun: [],
      realFailures: [{ check: 'build-arm', firstError: 'undefined reference', files: ['src/a.c'], rigSide: false }],
    },
  })
  assert.ok(calls.some(c => c.label.startsWith('fix:')), 'the dry run skipped the CI fixer')
  assert.equal(calls.some(c => c.label.startsWith('replies#')), false, 'a dry run must post nothing')
  assert.equal(result.dryRun, true)
})

await check('a dismissal survives the fix that moves its line', async () => {
  // Comment 7 owes A and B. B is overturned and fixed; A comes back at a new
  // line after that fix, is overturned and fixed too. Keying the dismissal on
  // the location would leave A's original key outstanding forever.
  let cycle = 0
  const A = (over) => invalidFinding({ commentId: 7, findingId: '7#1', line: 10, ...over })
  const { result } = await run({
    args: { autoPush: true, maxCycles: 5 },
    reviewsPerCycle: () => {
      cycle++
      if (cycle === 1) return { findings: [A(), invalidFinding({ commentId: 7, findingId: '7#2', line: 20 })], replies: [], done: true }
      if (cycle === 2) return { findings: [invalidFinding({ commentId: 7, findingId: '7#2', line: 20 })], replies: [], done: true }
      if (cycle === 3) return { findings: [A({ line: 11 })], replies: [], done: true }
      return { findings: [], replies: [], done: true }
    },
    challengePerCycle: () => cycle === 1
      ? { verdicts: [{ id: 0, upheld: true, reason: 'stands' }, { id: 1, upheld: true, reason: 'stands' }] }
      : { verdicts: [{ id: 0, upheld: false, reason: 'real' }] },
  })
  assert.equal(result.pass, true, `a shifted line stranded a retired dismissal (got ${result.reason})`)
})

await check('the validator contract defines the id the debt is keyed on', async () => {
  // pr-babysit keys every unanswered dismissal on findingId. If the validator
  // stops emitting a stable one, the debt silently never retires.
  const md = readFileSync(new URL('../../agents/pr-review-validator.md', import.meta.url), 'utf8')
  assert.match(md, /findingId/, 'the validator no longer documents findingId')
  assert.match(md, /"findingId":/, 'the output contract example omits findingId')
  assert.match(workflowBody, /required: \[[^\]]*'findingId'/, 'REVIEWS no longer requires findingId')
})

await check('an edited comment stops the run instead of retiring by a reused id', async () => {
  // Editing a review comment renumbers the positions its ids are built from, so
  // 7#1 can name a different point than the one that debt belongs to.
  let cycle = 0
  const { result, logs } = await run({
    args: { autoPush: true, maxCycles: 4 },
    reviewsPerCycle: () => {
      cycle++
      if (cycle === 1) return { findings: [invalidFinding({ commentId: 7, findingId: '7#1', commentDigest: 'before' })], replies: [], done: true }
      if (cycle === 2) return { findings: [invalidFinding({ commentId: 7, findingId: '7#1', commentDigest: 'after' })], replies: [], done: true }
      return { findings: [], replies: [], done: true }
    },
    challengePerCycle: () => cycle === 1
      ? { verdicts: [{ id: 0, upheld: true, reason: 'stands' }] }
      : { verdicts: [{ id: 0, upheld: false, reason: 'real' }] },
  })
  assert.ok(logs.some(l => l.includes('comment 7 was edited')), 'the edit must be reported')
  assert.equal(result.reason, 'deferred-replies-unresolved',
    'a reused id retired a dismissal after the comment was edited')
})

await check('an edited comment can still be answered by a later refutation', async () => {
  // Blocking retirement must not also block recovery: once the comment is
  // renumbered, the dismissal it owes stays owed, and the drafted reply for it
  // must still be postable.
  let cycle = 0
  const { calls, result } = await run({
    args: { autoPush: true, maxCycles: 5 },
    reviewsPerCycle: () => {
      cycle++
      if (cycle === 1) return { findings: [invalidFinding({ commentId: 7, findingId: '7#1', commentDigest: 'before' })], replies: [], done: true }
      if (cycle === 2) return { findings: [invalidFinding({ commentId: 7, findingId: '7#1', commentDigest: 'after' })], replies: [], done: true }
      if (cycle === 3) {
        return { findings: [invalidFinding({ commentId: 7, findingId: '7#2', commentDigest: 'after' })],
          replies: [{ commentId: 7, body: 'still wrong' }], done: true }
      }
      return { findings: [], replies: [], done: true }
    },
    challengePerCycle: () => cycle === 2
      ? { verdicts: [{ id: 0, upheld: false, reason: 'real' }] }
      : { verdicts: [{ id: 0, upheld: true, reason: 'stands' }] },
  })
  assert.ok(calls.some(c => c.label.startsWith('replies#')), 'the refutation was withheld forever')
  assert.equal(result.pass, true, `an edited comment could not recover (got ${result.reason})`)
})

await check('a harvest that reuses a findingId is rejected', async () => {
  // Two dismissals under one id collapse into a single obligation, so answering
  // one would silently answer both.
  const { result } = await run({
    args: { autoPush: true, maxCycles: 1 },
    reviews: {
      findings: [invalidFinding({ commentId: 7, findingId: '7#1', line: 10 }),
        invalidFinding({ commentId: 7, findingId: '7#1', line: 20 })],
      replies: [], done: true,
    },
  })
  assert.equal(result.reason, 'duplicate-finding-ids')
})

console.log(failed ? `\n${failed} FAILED` : '\nall checks passed')
process.exit(failed ? 1 : 0)
