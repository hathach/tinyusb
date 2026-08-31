export const meta = {
  name: 'fanout-dev',
  description: 'Implement one described change across many ports/file-sets: one code-writer worker per item, independent builder verification, optional review',
  whenToUse: 'Applying a fix or pattern across multiple TinyUSB ports (e.g. the same DCD bug in several drivers)',
  phases: [
    { title: 'Implement', detail: 'code-writer per item (opus xhigh)' },
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
if (args.worktree) log('worktree mode: independent builder verification and review skipped (workers verify inside their own worktrees)')

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

const results = await pipeline(
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

  (dev, item) => {
    if (!dev) return null
    // worktree mode: edits live in the worker's own worktree; an independent
    // verifier in the shared tree cannot see them — trust dev.buildOk.
    if (args.worktree) return dev
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
    if (!r || !args.review || args.worktree) return r
    return agent(
      `Review the uncommitted change in ${item} (inspect with: git diff -- ${item}) against this task:\n${args.task}\n` +
      'Dimension: does the diff correctly and completely implement the task with no unintended side effects? Coverage-first findings.',
      { label: `review:${short(item)}`, phase: 'Review', agentType: 'code-verifier', schema: FINDINGS },
    ).then(f => {
      // review: array = findings; null = reviewer died; absent = not requested
      if (!f) log(`review:${short(item)}: reviewer agent died`)
      return { ...r, review: f ? f.findings : null }
    })
  },
)

const done = results.filter(Boolean)
const dropped = args.items.length - done.length
if (dropped > 0) log(`${dropped} item(s) dropped (worker died)`)
log(`${done.length}/${args.items.length} items completed; ${done.filter(r => r.buildOk && r.verifyBuild !== false).length} build-clean`)
return done
