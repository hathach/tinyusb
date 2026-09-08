export const meta = {
  name: 'fanout-dev',
  description: 'Implement one described change across many ports/file-sets: writers, one combined simplification pass, independent builder verification, optional review',
  whenToUse: 'Applying a fix or pattern across multiple TinyUSB ports (e.g. the same DCD bug in several drivers)',
  phases: [
    { title: 'Implement', detail: 'code-writer per item' },
    { title: 'Simplify', detail: 'one pass after all shared-checkout writers finish' },
    { title: 'Verify', detail: 'builder single-example check' },
    { title: 'Review', detail: 'optional code-verifier pass' },
  ],
}

// args: { task: string, items: string[], board?: string | Record<string,string>, review?: boolean, worktree?: boolean }
if (typeof args === 'string') { try { args = JSON.parse(args) } catch { /* not JSON: shape check below reports it */ } }
if (!args || !args.task || !Array.isArray(args.items) || args.items.length === 0) {
  throw new Error('args must be { task: string, items: string[], board?, review?, worktree? }')
}
const boardFor = (item) =>
  typeof args.board === 'string' ? args.board : (args.board && args.board[item]) || null
const short = (s) => s.replace(/\/+$/, '').split('/').slice(-2).join('/')
if (args.worktree) log('worktree mode: combined simplification, independent builder verification and review deferred until integration (workers verify inside their own worktrees)')

const DEV = {
  type: 'object', additionalProperties: false,
  required: ['item', 'diffstat', 'buildOk', 'board', 'notes'],
  properties: {
    item: { type: 'string' }, diffstat: { type: 'string' }, buildOk: { type: 'boolean' },
    board: { type: 'string' }, notes: { type: 'string' },
  },
}
const BUILD = {
  type: 'object', additionalProperties: false,
  required: ['board', 'pass', 'builtCount', 'failures'],
  properties: {
    board: { type: 'string' }, pass: { type: 'boolean' }, builtCount: { type: 'integer' },
    failures: {
      type: 'array',
      items: {
        type: 'object', additionalProperties: false,
        required: ['example', 'class', 'firstError'],
        properties: { example: { type: 'string' }, class: { type: 'string' }, firstError: { type: 'string' } },
      },
    },
  },
}
const FINDINGS = {
  type: 'object', additionalProperties: false,
  required: ['scope', 'dimension', 'findings'],
  properties: {
    scope: { type: 'string' }, dimension: { type: 'string' },
    findings: {
      type: 'array',
      items: {
        type: 'object', additionalProperties: false,
        required: ['file', 'line', 'snippet', 'why', 'severity', 'confidence'],
        properties: {
          file: { type: 'string' }, line: { type: 'integer' }, snippet: { type: 'string' },
          why: { type: 'string' }, severity: { type: 'string' }, confidence: { type: 'string' },
        },
      },
    },
  },
}

const SIMPLIFY = {
  type: 'object', additionalProperties: false,
  required: ['changed', 'files', 'summary'],
  properties: {
    changed: { type: 'boolean' }, files: { type: 'array', items: { type: 'string' } },
    summary: { type: 'string' },
  },
}

const devs = await pipeline(
  args.items,

  item => agent(
    `${args.task}\n\nAssigned scope: ${item} — touch nothing outside it.` +
    (boardFor(item)
      ? ` Verify with board ${boardFor(item)}.`
      : ' Pick a verification board from hw/bsp whose family uses this scope.'),
    {
      label: `dev:${short(item)}`, phase: 'Implement',
      agentType: 'code-writer', schema: DEV,
      ...(args.worktree ? { isolation: 'worktree' } : {}),
    },
  ),
)

const summarize = (rows, buildClean) => {
  const dropped = args.items.length - rows.length
  if (dropped > 0) log(`${dropped} item(s) dropped (worker died)`)
  log(`${rows.length}/${args.items.length} items completed; ${rows.filter(buildClean).length} build-clean`)
  return rows
}

// worktree mode: edits live in each worker's own worktree; neither an independent
// verifier nor one combined simplifier in the shared tree can see them — trust
// dev.buildOk and leave both to integration.
if (args.worktree) return summarize(devs.filter(Boolean), r => r.buildOk)

const live = args.items.filter((_, index) => devs[index])
if (live.length === 0) return summarize([], r => r.verifyBuild === true)

const simplification = await agent(
  `Simplify the completed changes for this task:\n${args.task}\n\n` +
  `Assigned scopes: ${JSON.stringify(live)}. Touch nothing outside them.\n` +
  `Writer notes: ${JSON.stringify(devs.filter(Boolean).map(dev => ({ item: dev.item, notes: dev.notes })))}\n` +
  'All writers have finished. Inspect staged and unstaged changes and task-owned untracked files. ' +
  'Make one behavior-preserving pass; no changes is success. Independent builds and optional review follow.',
  { label: 'simplify', phase: 'Simplify', agentType: 'code-simplifier', schema: SIMPLIFY },
)
if (!simplification) throw new Error('simplifier failed — inspect possible partial edits before retrying')
log(`simplify: ${simplification.changed ? simplification.files.join(', ') : 'no changes'} — ${simplification.summary}`)

const results = await pipeline(
  args.items,

  (item, _item, index) => {
    const dev = devs[index]
    if (!dev) return null
    return agent(
      `Build the single example device/cdc_msc for board ${dev.board}. Use a unique build dir (mktemp -d) to avoid collisions with parallel builds.`,
      { label: `verify:${short(item)}`, phase: 'Verify', agentType: 'builder', schema: BUILD },
    ).then(b => {
      // verifyBuild: true/false = real builder verdict; null = builder died
      if (!b) log(`verify:${short(item)}: builder agent died — independent verification unknown`)
      return { ...dev, verifyBuild: b ? b.pass : null }
    })
  },

  (r, item) => {
    if (!r || !args.review) return r
    return workflow('code-verify', {
      prompt: `Review the uncommitted change in ${item} (inspect staged and unstaged changes with git diff HEAD -- ${item}, and read task-owned untracked files) against this task:\n${args.task}\n` +
      'Dimension: does the diff correctly and completely implement the task with no unintended side effects? Coverage-first findings.',
      label: `review:${short(item)}`,
      schema: FINDINGS,
    }).catch(() => null).then(f => {
      // review: array = findings; null = reviewer died; absent = not requested
      if (!f) log(`review:${short(item)}: reviewer agent died`)
      return { ...r, review: f ? f.findings : null }
    })
  },
)

return summarize(results.filter(Boolean), r => r.verifyBuild === true)
