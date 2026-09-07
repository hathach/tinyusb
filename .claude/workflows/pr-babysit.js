export const meta = {
  name: 'pr-babysit',
  description: 'Drive a PR to green: a fast review lane (validate bot findings, fix, push without waiting on CI) overlapped with a CI-watch lane; code-writer fixes, code-verifier verification, at most one push per lane per cycle, and a bot/finding/outcome/commit table logged per cycle',
  whenToUse: 'After opening a PR, from a checkout of the PR branch. Default is a dry run (fixes left uncommitted, nothing posted); passing autoPush: true is the explicit authorization for pushes and PR comments.',
  phases: [{ title: 'Triage' }, { title: 'Fix' }, { title: 'Verify' }, { title: 'Push' }],
}

// args: { pr: number, maxCycles?: number, autoPush?: boolean (default false = dry run),
//          checkoutDir?: string (PR branch checkout; default: the session working dir) }
if (typeof args === 'string') { try { args = JSON.parse(args) } catch { /* not JSON: shape check below reports it */ } }
if (!args || !args.pr) {
  throw new Error('args must be { pr: number, maxCycles?, autoPush?, checkoutDir? }; run from the PR branch checkout or point checkoutDir at it')
}
args.pr = Number(args.pr)
if (!Number.isInteger(args.pr) || args.pr <= 0) {
  throw new Error('args.pr must be a positive integer PR number')
}
const checkoutDir = args.checkoutDir || '.'
if (typeof checkoutDir !== 'string' || checkoutDir.includes("'")) {
  throw new Error('checkoutDir must be a plain path string')
}
const IN_CHECKOUT = checkoutDir === '.' ? 'The working tree IS the PR checkout. '
  : `The PR branch checkout is at ${checkoutDir} - run every git/build/file command there, not in the session directory. `
const maxCycles = args.maxCycles ?? 3
if (!Number.isInteger(maxCycles) || maxCycles < 1) {
  throw new Error('maxCycles must be an integer >= 1')
}

const CI = {
  type: 'object', additionalProperties: false,
  required: ['status', 'infraRerun', 'realFailures'],
  properties: {
    status: { type: 'string', enum: ['green', 'red', 'running'] },
    infraRerun: { type: 'array', items: { type: 'string' } },
    realFailures: {
      type: 'array',
      items: {
        type: 'object', additionalProperties: false,
        required: ['check', 'firstError', 'files', 'rigSide'],
        properties: {
          check: { type: 'string' }, firstError: { type: 'string' },
          files: { type: 'array', items: { type: 'string' } },
          rigSide: { type: 'boolean' },
        },
      },
    },
  },
}
const REVIEWS = {
  type: 'object', additionalProperties: false,
  required: ['findings', 'replies', 'done'],
  properties: {
    findings: {
      type: 'array',
      items: {
        type: 'object', additionalProperties: false,
        required: ['source', 'commentId', 'file', 'line', 'claim', 'verdict', 'reason', 'fixHint'],
        properties: {
          source: { type: 'string' }, commentId: { type: 'integer' },
          file: { type: 'string' }, line: { type: 'integer' }, claim: { type: 'string' },
          verdict: { type: 'string', enum: ['valid', 'invalid', 'stale'] },
          reason: { type: 'string' }, fixHint: { type: 'string' },
        },
      },
    },
    replies: {
      type: 'array',
      items: {
        type: 'object', additionalProperties: false,
        required: ['commentId', 'body'],
        properties: { commentId: { type: 'integer' }, body: { type: 'string' } },
      },
    },
    done: { type: 'boolean' },
  },
}
const DEV = {
  type: 'object', additionalProperties: false,
  required: ['item', 'diffstat', 'buildOk', 'board', 'notes'],
  properties: {
    item: { type: 'string' }, diffstat: { type: 'string' }, buildOk: { type: 'boolean' },
    board: { type: 'string' }, notes: { type: 'string' },
  },
}
const CHECK = {
  type: 'object', additionalProperties: false,
  required: ['addresses', 'reason'],
  properties: { addresses: { type: 'boolean' }, reason: { type: 'string' } },
}
// `committed` separates the two ways pass=false happens: the commit exists and
// only the push was rejected, or no commit was ever created (hook rejection,
// missing git identity) and the work is still only in the working tree.
const PUSH = {
  type: 'object', additionalProperties: false,
  required: ['pass', 'committed', 'detail', 'sha'],
  properties: {
    pass: { type: 'boolean' }, committed: { type: 'boolean' },
    detail: { type: 'string' }, sha: { type: 'string' },
  },
}
const SCOPE = {
  type: 'object', additionalProperties: false,
  required: ['files'],
  properties: { files: { type: 'array', items: { type: 'string' } } },
}
const OPIDS = {
  type: 'object', additionalProperties: false,
  required: ['pass', 'detail', 'doneIds'],
  properties: {
    pass: { type: 'boolean' }, detail: { type: 'string' },
    doneIds: { type: 'array', items: { type: 'integer' } },
  },
}

// Marking a review thread resolved has no REST endpoint — it needs the
// GraphQL resolveReviewThread mutation. Shared recipe handed to the posting
// agents so a fixed/refuted comment ends up both answered AND resolved.
const RESOLVE_RECIPE =
  'To resolve the review thread for an inline review comment (its integer databaseId is the commentId): ' +
  'get owner/repo via `gh repo view --json nameWithOwner -q .nameWithOwner`; find the thread node id with ' +
  '`gh api graphql -f query=\'query($o:String!,$r:String!,$p:Int!,$c:String){repository(owner:$o,name:$r){pullRequest(number:$p){reviewThreads(first:100,after:$c){pageInfo{hasNextPage endCursor}nodes{id isResolved comments(first:50){nodes{databaseId}}}}}}}\' -F o=OWNER -F r=REPO -F p=' + args.pr + '` ' +
  '(while hasNextPage is true and the comment is not found yet, re-run with -F c=<endCursor>), pick the thread whose comments contain that databaseId, then resolve it with ' +
  '`gh api graphql -f query=\'mutation($id:ID!){resolveReviewThread(input:{threadId:$id}){thread{isResolved}}}\' -F id=THREAD_ID`. ' +
  'Issue comments (the 404 fallback case) have no thread — do not try to resolve those.'

// Mechanical reply skeleton shared by the refuted-replies and fixed-resolve
// steps — kept in one place because the two copies drifted once already
// (the 404 fallback was missing from one of them).
const postReplyRecipe = (noun) =>
  `post a threaded reply to its inline comment via gh api repos/{owner}/{repo}/pulls/${args.pr}/comments/{commentId}/replies -f body=<body> ` +
  '(valid for inline review comments); if that 404s, the id is an issue comment — post a regular PR comment instead ' +
  `(gh pr comment ${args.pr} --body <quote the original point, then the ${noun}>) and skip resolving. ` +
  `After replying to an inline comment, mark its thread resolved. ${RESOLVE_RECIPE} `

const history = []
const repliedIds = new Set() // issue comments can't be thread-resolved, so they re-harvest every cycle — never reply twice

// Backoff between cycles that have nothing to do but wait. Degrades to a no-op
// rather than throwing if the workflow host has no timer.
const nap = (ms) => new Promise(res => { if (typeof setTimeout === 'function') setTimeout(res, ms); else res() })

// Canonicalize a repo-relative path for set/collision comparison: resolve ./..
// segments, unify separators; '' for anything that escapes the repo or uses
// characters no repo path does (also makes the path shell-safe to interpolate).
const canon = (p) => {
  const s = String(p).trim().replace(/\\/g, '/')
  // Absolute (CI-runner) paths: reject rather than corrupt into a bogus relative
  // path — the file-less group then routes through the scoper, which recovers the
  // real repo path and is existence-checked.
  if (s.startsWith('/')) return ''
  const out = []
  for (const seg of s.split('/')) {
    if (!seg || seg === '.') continue
    if (seg === '..') { if (out.pop() === undefined) return '' } else out.push(seg)
  }
  const c = out.join('/')
  return /^[A-Za-z0-9._+/-]+$/.test(c) ? c : ''
}

// Group actionable notes by top-level scope (plain JS — no model tokens).
// A note keeps its id alongside its text so the cycle summary can still map a
// finding to the fix that handled it after grouping and merging.
const groupWork = (notes) => {
  const groups = new Map()
  for (const n of notes) {
    const key = (canon(n.scopeFile) || n.scopeFile).split('/').slice(0, 3).join('/')
    if (!groups.has(key)) groups.set(key, { key, files: new Set(), notes: [] })
    const g = groups.get(key)
    n.files.forEach(f => { const c = canon(f); if (c) g.files.add(c) })
    g.notes.push({ id: n.id, text: n.text })
  }
  return [...groups.values()]
}

// Fix + verify one work list; returns { ok, fixes } — ok only if every group
// was scoped, fixed by a live worker, AND passed code-verifier verification.
const fixAndVerify = async (workIn) => {
  const textOf = (w) => w.notes.map(n => n.text).join('\n- ')
  // The note ids ride along on the fix so the cycle summary can say which
  // finding each fix answered, after grouping and the overlap merge.
  const verdictOf = (fix, w, addresses, checkReason) =>
    ({ ...fix, ids: w.notes.map(n => n.id), addresses, checkReason })
  // code-writer's contract needs an explicit file set: a group whose notes named no
  // files (a CI failure whose log yielded no paths) is scoped by a dedicated agent
  // first; if that fails too, the group is withheld (ok=false → human review) rather
  // than dispatched with an invalid scope.
  const fileless = workIn.filter(w => w.files.size === 0)
  await parallel(fileless.map(w => () =>
    agent(
      `${IN_CHECKOUT}Determine which repo files must change to address these notes (read the code; if a note is a CI failure, read its CI log too):\n- ${textOf(w)}\n` +
      'files = repo-relative paths; empty only if genuinely undeterminable.',
      { label: `scope:${w.key}`, phase: 'Fix', model: 'sonnet', schema: SCOPE },
    ).then(s => s && s.files.forEach(f => { const c = canon(f); if (c) w.files.add(c) }))))
  // Scoped paths are model output: keep only what git ls-files confirms exists.
  // The check is executed (by a mechanical agent) and intersected here — a dead
  // checker drops every candidate, so unconfirmed groups fall through to withheld.
  const candidates = [...new Set(fileless.flatMap(w => [...w.files]))]
  if (candidates.length > 0) {
    const v = await agent(
      `${IN_CHECKOUT}Run exactly: git ls-files -- ${candidates.join(' ')}\nReturn files = the paths that command printed, verbatim — no additions, no substitutions.`,
      { label: 'scope:verify', phase: 'Fix', model: 'haiku', schema: SCOPE },
    )
    const exists = new Set((v ? v.files : []).map(canon))
    for (const w of fileless) for (const f of [...w.files])
      if (!exists.has(f)) { w.files.delete(f); log(`scope:${w.key}: dropped ${f} — not confirmed as a repo file`) }
  }
  const unscoped = workIn.filter(w => w.files.size === 0)
  for (const w of unscoped) log(`fix for ${w.key}: no file scope determinable — withheld for human review`)
  // Scoping can make groups overlap (two checks resolving to the same file); merge
  // intersecting groups (to closure) so two fixers never edit one file concurrently.
  const work = []
  for (let g of workIn.filter(w => w.files.size > 0)) {
    for (let i; (i = work.findIndex(m => [...g.files].some(f => m.files.has(f)))) >= 0;) {
      const [m] = work.splice(i, 1)
      g.files.forEach(f => m.files.add(f)); m.notes.push(...g.notes); m.key = `${m.key}+${g.key}`
      g = m
    }
    work.push(g)
  }
  // HIL rig rosters (test/hil/*.json) describe physical hardware the user owns:
  // never edit them autonomously — skipping/reshaping tests there papers over a
  // failing fixture. A failure that needs hardware swapped or re-cabled stays RED
  // for the user; roster edits happen only with the user's explicit approval.
  const withheld = []
  for (const w of work) {
    for (const f of [...w.files]) if (/^test\/hil\/[^/]+\.json$/.test(f)) {
      w.files.delete(f)
      log(`fix for ${w.key}: ${f} is a HIL rig config — edits need user approval, dropped from scope`)
    }
    if (w.files.size === 0) {
      withheld.push(w)
      log(`fix for ${w.key}: only a HIL rig config edit would address it — leaving red for the user`)
    }
  }
  for (const w of withheld) work.splice(work.indexOf(w), 1)
  const scopeOf = (w) => [...w.files].join(', ')
  const fixes = await pipeline(
    work,
    w => agent(
      `Fix the following issues on the PR branch. ${IN_CHECKOUT}\n` +
      'Constraint: never modify test/hil/*.json (HIL rig hardware config) — a failure that needs hardware swapped/changed stays red for the user.\n' +
      `Scope: ${scopeOf(w)}\nIssues:\n- ${textOf(w)}`,
      { label: `fix:${w.key}`, phase: 'Fix', agentType: 'code-writer', schema: DEV },
    ),
    (fix, w) => {
      if (!fix) return null
      // A broken build is already fatal below, so skip the verifier: its verdict
      // could not change the outcome and it is the expensive step here.
      if (fix.buildOk === false) return verdictOf(fix, w, false, 'targeted build failed')
      return workflow('code-verify', {
        prompt: `${IN_CHECKOUT}Verify the uncommitted changes for ${scopeOf(w)} (use git diff -- <the files above>, and read any newly created untracked files directly) address these issues:\n- ${textOf(w)}\n` +
        'Return {"addresses": bool, "reason": string}.',
        label: `check:${w.key}`,
        schema: CHECK,
      }).catch(() => null)
        .then(v => verdictOf(fix, w, !!(v && v.addresses), v ? v.reason : 'verifier died'))
    },
  )
  const alive = fixes.filter(Boolean)
  if (alive.length < work.length) log(`${work.length - alive.length} fix group(s) lost to dead workers`)
  const unverified = alive.filter(f => f.addresses !== true)
  for (const f of unverified) log(`fix for ${f.item}: failed verification — ${f.checkReason}`)
  // A fix whose own targeted build failed is not pushable, however well it reads
  // against the finding: CI would only rediscover the break a cycle later.
  const broken = alive.filter(f => f.buildOk === false)
  for (const f of broken) log(`fix for ${f.item}: targeted build FAILED — not pushable`)
  return {
    ok: unscoped.length === 0 && withheld.length === 0 && alive.length === work.length
      && unverified.length === 0 && broken.length === 0,
    fixes: alive,
  }
}

// ---- per-cycle scoreboard ----
// One markdown row per validated bot finding (and real CI failure): what the bot
// claimed, the verdict, what happened to it, and the commit carrying the fix.
// A finding is identified by its commentId (as it already is for replies); a CI
// failure gets an `id` stamped on it where the watcher's list arrives, because
// two matrix legs of one job report the same `check`.

// Markdown cells break on newlines and bare pipes; long claims need a cap.
// Backslashes go first: escaping pipes in `\|` without it yields `\\|`, whose
// doubled backslash GFM eats, leaving the pipe live to split the row.
const cell = (s, max = 90) => {
  const t = String(s ?? '').replace(/\s+/g, ' ').replace(/\\/g, '\\\\').replace(/\|/g, '\\|').trim()
  if (!t) return '-'
  return t.length > max ? `${t.slice(0, max - 1)}…` : t
}
const mdTable = (headers, rows) => {
  const w = headers.map((h, i) => Math.max(h.length, ...rows.map(r => r[i].length)))
  const line = (cells) => `| ${cells.map((c, i) => c.padEnd(w[i])).join(' | ')} |`
  return [line(headers), line(w.map(n => '-'.repeat(n))), ...rows.map(line)].join('\n')
}
// Validate the pushed SHA rather than trusting it: it is model output, and a
// mislabeled commit in the table is worse than no commit at all.
const shaOf = (push) => {
  const s = push && push.sha && String(push.sha).trim()
  return s && /^[0-9a-f]{7,40}$/.test(s) ? s.slice(0, 8) : '-'
}
const fixCell = (fixes, id, push, pushFailed) => {
  // No fixes array at all = that lane never got to dispatch this cycle (a dead
  // agent, or a push in the other lane that superseded it).
  if (!fixes) return 'no fix attempted this cycle'
  const fix = fixes.find(x => x.ids.includes(id))
  if (!fix) return 'withheld (no fix dispatched)'
  // A broken build reports as unverified: fixAndVerify makes buildOk === false
  // fail verification with that reason, so it never reaches the pushable text.
  if (fix.addresses !== true) return `unverified: ${fix.checkReason}`
  const stat = fix.diffstat ? ` — ${fix.diffstat}` : ''
  // Two different recoveries, so never infer one from the other: a rejected push
  // leaves the fix committed locally, while a failed commit leaves it only in the
  // working tree with nothing in git to recover.
  if (pushFailed) {
    const detail = pushFailed.detail || 'no detail'
    return pushFailed.committed
      ? `fixed + committed, PUSH FAILED: ${detail}${stat}`
      : `fixed, COMMIT FAILED: ${detail}${stat}`
  }
  return `${push ? 'fixed + pushed' : 'fixed, uncommitted'}${stat}`
}
const VERDICT_ORDER = { valid: 0, stale: 1, invalid: 2 }
const cycleSummary = (entry) => {
  const rows = []
  const findings = [...((entry.reviews && entry.reviews.findings) || [])]
    .sort((a, b) => (VERDICT_ORDER[a.verdict] ?? 3) - (VERDICT_ORDER[b.verdict] ?? 3))
  for (const f of findings) {
    const valid = f.verdict === 'valid'
    rows.push([
      cell(f.source, 16),
      cell(`${f.file}:${f.line} ${f.claim}`),
      cell(f.verdict, 8),
      valid ? cell(fixCell(entry.reviewFixes, f.commentId, entry.reviewPush, entry.reviewPushFailed), 60)
        : cell(`${f.verdict === 'stale' ? 'already fixed' : 'refuted'}, ${repliedIds.has(f.commentId) ? 'replied + resolved' : 'reply pending'}`, 60),
      valid ? shaOf(entry.reviewPush) : '-',
    ])
  }
  for (const rf of ((entry.ci && entry.ci.realFailures) || [])) {
    rows.push([
      cell(`ci:${rf.check}`, 24),
      cell(rf.firstError),
      rf.rigSide ? 'rig-side' : 'ci-real',
      rf.rigSide ? 'left red for the rig' : cell(fixCell(entry.ciFixes, rf.id, entry.ciPush, entry.ciPushFailed), 60),
      rf.rigSide ? '-' : shaOf(entry.ciPush),
    ])
  }
  const head = `cycle ${entry.cycle} summary — CI ${entry.ci ? entry.ci.status : 'unknown'}, ` +
    `${entry.reviews ? (entry.reviews.done ? 'all bots settled' : 'bots still pending') : 'no review data'}` +
    `${entry.error ? `, ERROR: ${entry.error}` : ''}`
  return rows.length === 0
    ? `${head}\n(no bot findings, no real CI failures)`
    : `${head}\n${mdTable(['Bot', 'Finding', 'Verdict', 'Outcome', 'Commit'], rows)}`
}

// Verification gates every push: never push unverified or partial edits.
// Returns the agent's verdict as-is (pass=false and all) so the caller can tell
// the summary whether the fix is sitting committed-but-unpushed; a dead agent
// becomes a pass=false verdict of its own.
const commitAndPush = async (cycle, what) => {
  const push = await agent(
    `${IN_CHECKOUT}On the PR branch: commit ALL working-tree changes as ONE commit (imperative message summarizing the cycle-${cycle} ${what} fixes for PR #${args.pr}, repo commit conventions), ` +
    "then push to the PR's remote branch. pass=true only if commit AND push succeeded; " +
    'committed = a commit was created, even if the push then failed (false if the commit itself never landed); ' +
    'sha = the pushed commit SHA, `git rev-parse HEAD` verbatim and nothing else; detail = one line on what was pushed.',
    { label: `push#${cycle}-${what}`, phase: 'Push', model: 'sonnet', schema: PUSH },
  )
  return push || { pass: false, committed: false, detail: 'push agent died', sha: '' }
}

let napMs = 0 // backoff owed from the previous cycle, taken after its summary

// One cycle: returns null to re-arm, or the workflow's final result to stop.
// Records what happened on `entry` as it goes, so the caller can report a cycle
// that ended early.
const runCycle = async (cycle, entry) => {
  let ciPromise = null
  // Every early return below can leave the CI lane still running: settle it in a
  // finally so no CI agent outlives the workflow, even on a throw.
  try {
    // Two independent lanes, launched together. The review lane never waits on
    // CI: it validates, fixes, and pushes while the CI lane is still watching.
    ciPromise = agent(
      `Watch CI for PR #${args.pr} per your procedure; wait for pending checks.`,
      { label: `ci#${cycle}`, phase: 'Triage', agentType: 'pr-ci-watcher', schema: CI },
    ).catch(e => { log(`cycle ${cycle}: pr-ci-watcher errored — ${e && e.message}`); return null })

    const r = await agent(
      `Validate the bot review findings on PR #${args.pr} per your procedure. ${IN_CHECKOUT}`,
      { label: `reviews#${cycle}`, phase: 'Triage', agentType: 'pr-review-validator', schema: REVIEWS },
    ).catch(e => { log(`cycle ${cycle}: pr-review-validator errored — ${e && e.message}`); return null })
    if (!r) {
      entry.error = 'pr-review-validator died'
      return { pass: false, cycles: cycle, history, reason: 'review-validator-died' }
    }
    entry.reviews = r
    // Outward reply/resolve attempts this cycle that did not fully complete; a green
    // PR must not terminate the loop while any remain, or the retry never happens.
    let pendingReplies = 0

    // Post drafted replies to REFUTED findings immediately. Outward-facing,
    // so gated on autoPush.
    const freshReplies = r.replies.filter(x => !repliedIds.has(x.commentId))
    if (freshReplies.length > 0 && args.autoPush === true) {
      const posted = await agent(
        `Reply to and resolve these refuted review comments on PR #${args.pr}. For each: ${postReplyRecipe('reply')}` +
        'If a thread already carries an identical reply of ours (a prior attempt that posted but failed to resolve), do not repost — just resolve it. ' +
        `Replies: ${JSON.stringify(freshReplies)}. pass=true only if every reply was posted and every inline thread resolved; detail = what went where. ` +
        'doneIds = the commentIds fully handled: reply posted (or already present) AND (thread resolved, or an issue comment with no thread to resolve).',
        { label: `replies#${cycle}`, phase: 'Push', model: 'sonnet', schema: OPIDS },
      )
      // Per-id accounting, matching the resolve path: only fully handled ids are marked
      // replied; a failed reply/resolve stays fresh and retries next cycle (the prompt's
      // already-present check keeps the retry from duplicating the reply).
      for (const id of (posted && posted.doneIds) || []) repliedIds.add(id)
      pendingReplies += freshReplies.filter(x => !repliedIds.has(x.commentId)).length
      if (!posted || !posted.pass) log(`cycle ${cycle}: refuted reply/resolve incomplete — ${posted ? posted.detail : 'agent died'}`)
    }

    // ---- review lane: fix + push without waiting for CI ----
    const validFindings = r.findings.filter(x => x.verdict === 'valid')
    let reviewPushed = false
    if (validFindings.length > 0) {
      const work = groupWork(validFindings.map(f => ({
        id: f.commentId, scopeFile: f.file, files: [f.file],
        text: `${f.file}:${f.line} [${f.source}] ${f.claim} — hint: ${f.fixHint}`,
      })))
      const { ok, fixes } = await fixAndVerify(work)
      entry.reviewFixes = fixes
      if (args.autoPush !== true) {
        log('autoPush not set: review-lane fixes left uncommitted (dry run)')
        return { pass: false, cycles: cycle, history, dryRun: true }
      }
      if (!ok) {
        log(`cycle ${cycle}: review-lane fixes left uncommitted for human review — not pushing unverified changes`)
        return { pass: false, cycles: cycle, history, reason: 'fix-verification-failed' }
      }
      const push = await commitAndPush(cycle, 'review')
      if (!push.pass) {
        entry.reviewPushFailed = push
        log(`cycle ${cycle}: review-lane push failed (${push.detail}) — stopping`)
        return { pass: false, cycles: cycle, history, reason: 'push-failed' }
      }
      entry.reviewPush = push
      reviewPushed = true
      const resolved = await agent(
        `The fixes for PR #${args.pr}'s valid review findings were just committed and pushed (${push.sha}). ` +
        `For each finding below: ${postReplyRecipe('fix note')}` +
        'Each reply states the finding is fixed in the pushed commit, with one line on the change. ' +
        `Findings: ${JSON.stringify(validFindings.map(f => ({ commentId: f.commentId, file: f.file, line: f.line, claim: f.claim, fixHint: f.fixHint })))}. ` +
        'pass=true only if every reply was posted and every thread resolved; detail = what went where. ' +
        'doneIds = the commentIds fully handled: reply posted AND (thread resolved, or an issue comment with no thread to resolve).',
        { label: `resolve#${cycle}`, phase: 'Push', model: 'sonnet', schema: OPIDS },
      )
      // Per-id accounting: a fully handled finding never re-replies (an issue comment
      // has no thread to resolve, so it re-harvests as stale next cycle and would get
      // a duplicate "fixed" note); an unfinished one stays out of repliedIds so its
      // reply/resolve is retried next cycle instead of silently abandoned.
      for (const id of (resolved && resolved.doneIds) || []) repliedIds.add(id)
      pendingReplies += validFindings.filter(f => !repliedIds.has(f.commentId)).length
      if (!resolved || !resolved.pass) log(`cycle ${cycle}: fixed reply/resolve incomplete — ${resolved ? resolved.detail : 'agent died'}`)
    }

    // ---- CI lane result ----
    const c = await ciPromise
    if (!c) {
      log(`cycle ${cycle}: pr-ci-watcher died — re-arming`)
      return null
    }
    if (reviewPushed) {
      // The push restarted CI: this cycle's CI verdict is superseded. Re-arm;
      // next cycle's ci#N watches the fresh run.
      log(`cycle ${cycle}: review-lane push superseded the CI run — re-arming`)
      return null
    }
    c.realFailures.forEach((rf, i) => { rf.id = `ci:${i}:${rf.check}` })
    const rigSide = c.realFailures.filter(rf => rf.rigSide)
    for (const rf of rigSide) log(`cycle ${cycle}: rig-side CI failure (not fixing): ${rf.check} — ${rf.firstError.slice(0, 120)}`)
    const fixable = c.realFailures.filter(rf => !rf.rigSide)
    if (fixable.length > 0) {
      const work = groupWork(fixable.map(rf => ({
        id: rf.id, scopeFile: rf.files[0] || rf.check, files: rf.files,
        text: `CI ${rf.check}: ${rf.firstError}`,
      })))
      const { ok, fixes } = await fixAndVerify(work)
      entry.ciFixes = fixes
      if (args.autoPush !== true) {
        log('autoPush not set: CI-lane fixes left uncommitted (dry run)')
        return { pass: false, cycles: cycle, history, dryRun: true }
      }
      if (!ok) {
        log(`cycle ${cycle}: CI-lane fixes left uncommitted for human review — not pushing unverified changes`)
        return { pass: false, cycles: cycle, history, reason: 'fix-verification-failed' }
      }
      const ciPush = await commitAndPush(cycle, 'ci')
      if (!ciPush.pass) {
        entry.ciPushFailed = ciPush
        log(`cycle ${cycle}: CI-lane push failed (${ciPush.detail}) — stopping`)
        return { pass: false, cycles: cycle, history, reason: 'push-failed' }
      }
      entry.ciPush = ciPush
      return null // pushed: fresh CI run next cycle
    }
    if (r.done && c.status === 'green') {
      if (pendingReplies > 0) {
        log(`cycle ${cycle}: PR green but ${pendingReplies} reply/resolve unfinished — re-arming to retry`)
        return null
      }
      log(`cycle ${cycle}: PR is green with no unresolved valid findings`)
      return { pass: true, cycles: cycle, history }
    }
    if (r.done && rigSide.length > 0 && fixable.length === 0 && c.infraRerun.length === 0 && c.status !== 'running') {
      log(`cycle ${cycle}: CI red only from rig-side failures — human/rig attention needed, nothing to fix in the PR`)
      return { pass: false, cycles: cycle, history, reason: 'ci-red-rig-side' }
    }
    if (c.status === 'running' || c.infraRerun.length > 0) {
      log(`cycle ${cycle}: CI still settling (${c.infraRerun.length} infra re-run(s)) — re-arming`)
      return null
    }
    if (!r.done) {
      // A bot has not reported for this head SHA yet. With CI already green there is
      // nothing else to wait on, so back off before re-arming or the cycle budget
      // burns on back-to-back re-harvests of the same unchanged PR.
      if (cycle < maxCycles) {
        log(`cycle ${cycle}: auto-review still pending — re-arming after a wait`)
        napMs = 60000 * cycle // taken at the top of the next cycle, after this one's summary
      } else {
        log(`cycle ${cycle}: auto-review still pending — cycle budget exhausted`)
      }
      return null
    }
    log(`cycle ${cycle}: nothing actionable`)
    return { pass: false, cycles: cycle, history, reason: 'unactionable' }
  } finally {
    if (ciPromise) entry.ci = await ciPromise
  }
}

for (let cycle = 1; cycle <= maxCycles; cycle++) {
  if (napMs > 0) { await nap(napMs); napMs = 0 }
  const entry = { cycle }
  history.push(entry)
  // A rejection from any worker not individually guarded (replies/resolve/scope/
  // fixer/push) must not skip the scoreboard — that is exactly the cycle worth
  // reporting. Converting it to a failure verdict instead of rethrowing keeps
  // `history`, as the dead-validator path does.
  let verdict
  try {
    verdict = await runCycle(cycle, entry)
  } catch (e) {
    entry.error = `cycle threw: ${e && e.message}`
    verdict = { pass: false, cycles: cycle, history, reason: 'cycle-threw' }
  } finally {
    entry.summary = cycleSummary(entry)
    log(entry.summary)
  }
  if (verdict) return verdict
}
return { pass: false, cycles: maxCycles, history, reason: 'maxCycles reached' }
