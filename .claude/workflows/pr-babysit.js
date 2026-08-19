export const meta = {
  name: 'pr-babysit',
  description: 'Drive a PR to green: pr-monitor triage (CI + bot reviews), port-dev fixes for validated findings, driver-reviewer verification, one commit+push per cycle',
  whenToUse: 'After opening a PR, from a checkout of the PR branch. Default is a dry run (fixes left uncommitted, nothing posted); passing autoPush: true is the explicit authorization for pushes and PR comments.',
  phases: [{ title: 'Triage' }, { title: 'Fix' }, { title: 'Verify' }, { title: 'Push' }],
}

// args: { pr: number, maxCycles?: number, autoPush?: boolean (default false = dry run) }
if (typeof args === 'string') { try { args = JSON.parse(args) } catch { /* not JSON: shape check below reports it */ } }
if (!args || !args.pr) {
  throw new Error('args must be { pr: number, maxCycles?, autoPush? }; run from a checkout of the PR branch')
}
args.pr = Number(args.pr)
if (!Number.isInteger(args.pr) || args.pr <= 0) {
  throw new Error('args.pr must be a positive integer PR number')
}
const maxCycles = args.maxCycles ?? 3
if (!Number.isInteger(maxCycles) || maxCycles < 1) {
  throw new Error('maxCycles must be an integer >= 1')
}

const TRIAGE = {
  type: 'object', additionalProperties: false,
  required: ['ci', 'findings', 'replies', 'done'],
  properties: {
    ci: {
      type: 'object', additionalProperties: false,
      required: ['status', 'infraRerun', 'realFailures'],
      properties: {
        status: { type: 'string', enum: ['green', 'red', 'running'] },
        infraRerun: { type: 'array', items: { type: 'string' } },
        realFailures: {
          type: 'array',
          items: {
            type: 'object', additionalProperties: false,
            required: ['check', 'firstError', 'files'],
            properties: {
              check: { type: 'string' }, firstError: { type: 'string' },
              files: { type: 'array', items: { type: 'string' } },
            },
          },
        },
      },
    },
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
const OP = {
  type: 'object', additionalProperties: false,
  required: ['pass', 'detail'],
  properties: { pass: { type: 'boolean' }, detail: { type: 'string' } },
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
for (let cycle = 1; cycle <= maxCycles; cycle++) {
  const t = await agent(
    `Triage PR #${args.pr}. If checks are still running, wait for them first (gh pr checks ${args.pr} --watch as a BACKGROUND Bash task; the foreground timeout is capped at 10 min). ` +
    'Then follow your triage procedure: classify CI failures, re-run infra ones, harvest and adversarially validate bot review findings, draft replies for invalid/stale ones.',
    { label: `triage#${cycle}`, phase: 'Triage', agentType: 'pr-monitor', schema: TRIAGE },
  )
  if (!t) {
    history.push({ cycle, error: 'pr-monitor died' })
    return { pass: false, cycles: cycle, history, reason: 'pr-monitor-died' }
  }
  const entry = { cycle, triage: t }
  history.push(entry)

  // Post drafted replies to REFUTED findings as soon as triage produces them —
  // decoupled from fixing/pushing so done/unactionable cycles still post.
  // Reply AND resolve the thread. Outward-facing, so gated on autoPush.
  const freshReplies = t.replies.filter(r => !repliedIds.has(r.commentId))
  if (freshReplies.length > 0 && args.autoPush === true) {
    const posted = await agent(
      `Reply to and resolve these refuted review comments on PR #${args.pr}. For each: ${postReplyRecipe('reply')}` +
      `Replies: ${JSON.stringify(freshReplies)}. pass=true only if every reply was posted and every inline thread resolved; detail = what went where.`,
      { label: `replies#${cycle}`, phase: 'Push', model: 'sonnet', schema: OP },
    )
    // attempted counts as replied: better to drop a failed reply than spam duplicates
    freshReplies.forEach(r => repliedIds.add(r.commentId))
    if (!posted || !posted.pass) log(`cycle ${cycle}: refuted reply/resolve incomplete — ${posted ? posted.detail : 'agent died'}`)
  }

  if (t.done) {
    log(`cycle ${cycle}: PR is green with no unresolved valid findings`)
    return { pass: true, cycles: cycle, history }
  }

  // Group actionable work by top-level scope (plain JS — no model tokens).
  const groups = new Map()
  const groupOf = (key) => {
    if (!groups.has(key)) groups.set(key, { key, files: new Set(), notes: [] })
    return groups.get(key)
  }
  for (const f of t.findings.filter(x => x.verdict === 'valid')) {
    const g = groupOf(f.file.split('/').slice(0, 3).join('/'))
    g.files.add(f.file)
    g.notes.push(`${f.file}:${f.line} [${f.source}] ${f.claim} — hint: ${f.fixHint}`)
  }
  for (const rf of t.ci.realFailures) {
    const g = groupOf((rf.files[0] || rf.check).split('/').slice(0, 3).join('/'))
    rf.files.forEach(x => g.files.add(x))
    g.notes.push(`CI ${rf.check}: ${rf.firstError}`)
  }
  const work = [...groups.values()]

  if (work.length === 0) {
    if (t.ci.status === 'running' || t.ci.infraRerun.length > 0) {
      log(`cycle ${cycle}: only infra re-runs in flight — next cycle waits on them`)
      continue
    }
    log(`cycle ${cycle}: nothing actionable`)
    return { pass: false, cycles: cycle, history, reason: 'unactionable' }
  }

  const fixes = await pipeline(
    work,
    w => agent(
      `Fix the following issues on the current PR branch (the working tree IS the PR checkout).\n` +
      `Scope: ${[...w.files].join(', ')}\nIssues:\n- ${w.notes.join('\n- ')}`,
      { label: `fix:${w.key}`, phase: 'Fix', agentType: 'port-dev', effort: 'xhigh', schema: DEV },
    ),
    (fix, w) => fix && agent(
      `Verify the uncommitted changes for ${[...w.files].join(', ')} (use git diff -- <files>, and read any newly created untracked files directly) address these issues:\n- ${w.notes.join('\n- ')}\n` +
      'Return {"addresses": bool, "reason": string}.',
      { label: `check:${w.key}`, phase: 'Verify', agentType: 'driver-reviewer', effort: 'xhigh', schema: CHECK },
    ).then(v => ({ ...fix, addresses: !!(v && v.addresses), checkReason: v ? v.reason : 'verifier died' })),
  )
  const aliveFixes = fixes.filter(Boolean)
  if (aliveFixes.length < work.length) log(`${work.length - aliveFixes.length} fix group(s) lost to dead workers`)
  entry.fixes = aliveFixes

  if (args.autoPush !== true) {
    log('autoPush not set: fixes left uncommitted in the working tree (dry run)')
    return { pass: false, cycles: cycle, history, dryRun: true }
  }

  // Verification gates the push: never push a cycle containing an unverified
  // fix or the partial edits of a dead worker.
  const unverified = aliveFixes.filter(f => f.addresses !== true)
  if (aliveFixes.length < work.length || unverified.length > 0) {
    for (const f of unverified) log(`fix for ${f.item}: failed verification — ${f.checkReason}`)
    log(`cycle ${cycle}: fixes left uncommitted for human review — not pushing unverified changes`)
    return { pass: false, cycles: cycle, history, reason: 'fix-verification-failed' }
  }

  const push = await agent(
    `On the current PR branch: commit ALL working-tree changes as ONE commit (imperative message summarizing the cycle-${cycle} fixes for PR #${args.pr}, repo commit conventions), ` +
    "then push to the PR's remote branch. pass=true only if commit AND push succeeded; detail = pushed SHA.",
    { label: `push#${cycle}`, phase: 'Push', model: 'sonnet', schema: OP },
  )
  if (!push || !push.pass) {
    log(`cycle ${cycle}: push failed — stopping`)
    return { pass: false, cycles: cycle, history, reason: 'push-failed' }
  }

  // The valid bot findings were fixed and pushed — answer each inline comment
  // with what changed and resolve its thread. CI-failure work has no comment.
  const fixed = t.findings.filter(x => x.verdict === 'valid')
  if (fixed.length > 0) {
    const resolved = await agent(
      `The fixes for PR #${args.pr}'s valid review findings were just committed and pushed (${push.detail}). ` +
      `For each finding below: ${postReplyRecipe('fix note')}` +
      'Each reply states the finding is fixed in the pushed commit, with one line on the change. ' +
      `Findings: ${JSON.stringify(fixed.map(f => ({ commentId: f.commentId, file: f.file, line: f.line, claim: f.claim, fixHint: f.fixHint })))}. ` +
      'pass=true only if every reply was posted and every thread resolved; detail = what went where.',
      { label: `resolve#${cycle}`, phase: 'Push', model: 'sonnet', schema: OP },
    )
    if (!resolved || !resolved.pass) log(`cycle ${cycle}: fixed reply/resolve incomplete — ${resolved ? resolved.detail : 'agent died'}`)
  }
}
return { pass: false, cycles: maxCycles, history, reason: 'maxCycles reached' }
