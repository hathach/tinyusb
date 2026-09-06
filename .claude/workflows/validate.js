export const meta = {
  name: 'validate',
  description: 'Pre-PR software validation loop: unit tests + per-board build sweeps + code-size compare + PVS + diff reviews (claude + codex) in parallel; a red verdict dispatches a fix agent for the confirmed findings, then the affected stages re-run — up to maxCycles (default 5) validation passes; a fix that edits a workflow file stops with restartRequired so the caller re-invokes it',
  whenToUse: 'Before opening or updating a PR, after any non-trivial change',
  phases: [
    { title: 'Validate', detail: 'unit + builds + size + pvs + reviews in parallel' },
    { title: 'Fix', detail: 'one fix agent per red cycle; commits, then affected stages re-run' },
  ],
}

// args: { boards: string[], examples?: string, base?: string,
//         skip?: ('unit'|'size'|'pvs'|'review'|'codex')[], maxCycles?: number }
if (typeof args === 'string') { try { args = JSON.parse(args) } catch { /* not JSON: shape check below reports it */ } }
if (!args || !Array.isArray(args.boards) || args.boards.length === 0) {
  throw new Error('args must be { boards: string[], examples?, base?, skip?, maxCycles? }')
}
if (args.maxCycles !== undefined && (!Number.isInteger(args.maxCycles) || args.maxCycles < 1)) {
  throw new Error('maxCycles must be an integer >= 1')
}
const maxCycles = args.maxCycles ?? 5
const skip = args.skip || []
for (const s of skip) log(`stage skipped by request: ${s}`)
const base = args.base || 'master'
// Every stage agent re-resolves `base` in every cycle, and the fixer commits
// between cycles: a moving expression (HEAD~1, @{u}, a ^/~ walk) would advance
// with each fix commit, so cycle 2 would review only the fix and silently drop
// the original branch changes. Accept stationary refs only.
if (/(^|[^\w/-])HEAD/.test(base) || /[~^]/.test(base) || base.includes('@{')) {
  throw new Error(`base must be a fixed ref (sha or branch name), not the moving expression "${base}" — resolve it with git rev-parse first`)
}
const clip = (s, n = 800) =>
  s.length > n ? s.slice(0, n) + ` …[truncated ${s.length - n} chars]` : s

const STAGE = {
  type: 'object', additionalProperties: false,
  required: ['pass', 'detail'],
  properties: { pass: { type: 'boolean' }, detail: { type: 'string' } },
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
const PVS = {
  type: 'object', additionalProperties: false,
  required: ['pass', 'ga1', 'ga2', 'changedFindings', 'detail'],
  properties: {
    pass: { type: 'boolean' }, ga1: { type: 'integer' }, ga2: { type: 'integer' },
    changedFindings: {
      type: 'array',
      items: {
        type: 'object', additionalProperties: false,
        required: ['file', 'line', 'rule', 'level', 'message'],
        properties: {
          file: { type: 'string' }, line: { type: 'integer' }, rule: { type: 'string' },
          level: { type: 'integer' }, message: { type: 'string' },
        },
      },
    },
    detail: { type: 'string' },
  },
}

const REVIEW = {
  type: 'object', additionalProperties: false,
  required: ['pass', 'findings', 'detail'],
  properties: {
    pass: { type: 'boolean' },
    findings: {
      type: 'array',
      items: {
        type: 'object', additionalProperties: false,
        required: ['file', 'line', 'severity', 'summary'],
        properties: {
          file: { type: 'string' }, line: { type: 'integer' },
          severity: { type: 'string' }, summary: { type: 'string' },
        },
      },
    },
    detail: { type: 'string' },
  },
}

const FIX = {
  type: 'object', additionalProperties: false,
  required: ['changed', 'commit', 'files', 'summary'],
  properties: {
    changed: { type: 'boolean' }, commit: { type: 'string' },
    files: { type: 'array', items: { type: 'string' } },
    summary: { type: 'string' },
  },
}

// read back out of git what the fix commit actually touched
const PATHS = {
  type: 'object', additionalProperties: false,
  required: ['paths', 'isHead'],
  properties: {
    paths: { type: 'array', items: { type: 'string' } },
    isHead: { type: 'boolean' },
  },
}

// gate helpers — enforced here, never trusted from the agents
const reviewBlocking = f =>
  /^confirmed/i.test(f.severity) && /\bP[01]\b/i.test(f.severity) &&
  /\b(correctness|safety|security)\b/i.test(f.severity)

// ---------------------------------------------------------------------------
// Stage builders, parameterized so later cycles can re-run a subset. Stage
// names: 'unit', 'build:<board>', 'size', 'pvs', and the dual-provider
// 'reviews' scheduler, which emits the public 'review' and 'codex' rows.
// ---------------------------------------------------------------------------
const reviewStageNames = []
if (!skip.includes('review')) reviewStageNames.push('review')
if (!skip.includes('codex')) reviewStageNames.push('codex')
const reviewProvider = reviewStageNames.length === 2 ? 'both'
  : reviewStageNames[0] === 'review' ? 'claude'
    : reviewStageNames[0] === 'codex' ? 'codex' : null
const reviewPrompt =
  `Code-review this branch's diff vs ${base} (git diff ${base}...HEAD), coverage-first: walk every hunk, no spot checks. ` +
  'Find pass — candidate defects across all dimensions: correctness/logic, ISR & concurrency safety, ' +
  'memory/resource handling (bounds, leaks, no dynamic alloc), API contract & spec conformance, ' +
  'security of untrusted input parsing, behavior regressions; plus quality/simplification notes. ' +
  'Verify pass — adversarially check each candidate against the surrounding code: verdict CONFIRMED ' +
  '(failing scenario constructed) or PLAUSIBLE (could not refute); report both, drop only refuted ones. ' +
  'Read-only: never apply fixes. severity must be "CONFIRMED P0 correctness", "CONFIRMED P1 safety", ' +
  '"PLAUSIBLE P2 quality", or the same format with P0-P3 and one of correctness, safety, security, ' +
  'quality, simplification, or style. pass=false only for a CONFIRMED P0/P1 correctness, safety, or ' +
  'security bug; detail = one-line review summary.'
const stageNames = []
if (!skip.includes('unit')) stageNames.push('unit')
for (const b of args.boards) stageNames.push(`build:${b}`)
if (!skip.includes('size')) stageNames.push('size')
if (!skip.includes('pvs')) stageNames.push('pvs')
if (reviewProvider) stageNames.push('reviews')
const scheduleName = name => name === 'review' || name === 'codex' ? 'reviews' : name
const displayNames = names => names.flatMap(name => name === 'reviews' ? reviewStageNames : [name])

function runReviewProvider(provider, label) {
  if (provider === 'codex') return agent(
    JSON.stringify({ prompt: reviewPrompt, schema: REVIEW }),
    { label: `${label}:codex`, phase: 'Validate', agentType: 'codex-code-verifier', schema: REVIEW },
  )
  return agent(reviewPrompt, {
    label: `${label}:claude`, phase: 'Validate', agentType: 'code-verifier', schema: REVIEW,
  })
}

function stageThunk(name, cycle) {
  const label = (cycle > 1 ? `c${cycle}:` : '') + name
  // findings: [] so a dead review/codex stage flows through fixerEvidence()
  // instead of throwing on f.findings inside the fix dispatch's catch
  const died = { stage: name, pass: false, findings: [], detail: 'stage agent died' }

  if (name === 'unit') return () => agent(
    'Run the TinyUSB unit tests: cd test/unit-test && ceedling test:all. ' +
    'pass=true only if every test passes. detail = the ceedling summary line, or the first failing test output.',
    { label, phase: 'Validate', model: 'haiku', schema: STAGE },
  ).then(r => r ? { stage: name, ...r } : died).catch(() => died)

  if (name.startsWith('build:')) {
    const b = name.slice('build:'.length)
    return () => agent(
      `Build TinyUSB examples for board ${b}` + (args.examples ? ` (only: ${args.examples})` : ' (full example set)') + '.',
      { label, phase: 'Validate', agentType: 'builder', schema: BUILD },
    ).then(r => r ? {
      stage: name, pass: r.pass,
      detail: r.pass ? `${r.builtCount} examples built` : clip(JSON.stringify(r.failures)),
    } : died).catch(() => died)
  }

  if (name === 'size') return () => agent(
    `Compare TinyUSB code size against ${base}: python3 tools/metrics_compare_base.py --base-branch ${base} -b ${args.boards[0]} -e device/cdc_msc (exactly this command — no extra positional args). ` +
    'The report lands in cmake-metrics/<board>/metrics_compare.md. pass=false only if the tool itself errors; ' +
    'detail = the flash/RAM delta summary from the report (mention any example that grew).',
    { label, phase: 'Validate', model: 'haiku', schema: STAGE },
  ).then(r => r ? { stage: name, ...r } : died).catch(() => died)

  if (name === 'pvs') return () => agent(
    `Run PVS-Studio static analysis for board ${args.boards[0]}, gating on files changed vs ${base}. ` +
    'Parallel build agents are running — use your dedicated build dir, never cmake-build-<board>.',
    { label, phase: 'Validate', agentType: 'static-analyzer', effort: 'low', schema: PVS },
  ).then(r => r ? {
    stage: name, pass: r.pass,
    detail: r.pass ? r.detail : clip(`${r.detail} ${JSON.stringify(r.changedFindings)}`),
  } : died).catch(() => died)

  if (name === 'reviews') return () => {
    const pending = reviewProvider === 'both'
      ? parallel(['codex', 'claude'].map(provider => () => runReviewProvider(provider, label)))
        .then(([codex, claude]) => ({ codex, claude }))
      : runReviewProvider(reviewProvider, label).then(r => ({ [reviewProvider]: r }))
    return pending.then(byProvider => {
      const rows = []
      if (reviewStageNames.includes('review')) rows.push(byProvider.claude ? {
        stage: 'review', pass: byProvider.claude.pass &&
          !byProvider.claude.findings.some(reviewBlocking),
        findings: byProvider.claude.findings, detail: byProvider.claude.detail,
      } : { stage: 'review', pass: false, findings: [], detail: 'stage agent died' })
      if (reviewStageNames.includes('codex')) rows.push(byProvider.codex ? {
        stage: 'codex', pass: byProvider.codex.pass &&
          !byProvider.codex.findings.some(reviewBlocking),
        findings: byProvider.codex.findings, detail: byProvider.codex.detail,
      } : { stage: 'codex', pass: false, findings: [], detail: 'stage agent died' })
      return rows
    }).catch(() => reviewStageNames.map(stage => ({
      stage, pass: false, findings: [], detail: 'stage agent died',
    })))
  }

  throw new Error(`unknown stage ${name}`)
}

// Only the material that FAILED the gate reaches the fixer: confirmed review
// P0/P1 correctness/safety/security findings and failed unit/build/size/pvs
// stage evidence.
// PLAUSIBLE and quality findings stay report-only — fixing them here would
// churn style on an otherwise green branch. Bounded at the leaves (per-stage
// finding cap, clipped summaries/details) so the serialized JSON stays valid
// and every failed stage is represented — a document-level clip could cut
// mid-JSON and silently drop trailing stages.
function fixerEvidence(failures) {
  return failures.map(f => {
    const findings = (f.findings || [])
      .filter(reviewBlocking)
      .slice(0, 10)
      .map(x => ({ ...x, summary: clip(x.summary, 300) }))
    if (f.stage === 'review' || f.stage === 'codex')
      return { stage: f.stage, detail: clip(f.detail, 300), findings }
    return { stage: f.stage, detail: clip(f.detail) }
  })
}

function fixThunkPrompt(cycle, failures) {
  return 'You are the fix agent of the validate loop, cycle ' + cycle + ', in this TinyUSB repo (work from the repo root). ' +
    'Failed stages: ' + failures.map(f => f.stage).join(', ') + '. ' +
    'The JSON below carries their evidence (review findings are pre-verified CONFIRMED, codex ones are P0/P1):\n' +
    JSON.stringify(fixerEvidence(failures), null, 1) + '\n\n' +
    'For each item: verify it against the actual code first; fix the real ones with the smallest correct change, matching surrounding style. ' +
    'Skip anything that is an infrastructure failure rather than a code defect (missing CLI, tool crash, dead stage agent) and anything you can refute with evidence — say which and why in summary. ' +
    'Run the tests/suites covering what you changed. ' +
    'BEFORE editing anything, run git status --porcelain and record every path already dirty in either column ' +
    '(staged or unstaged — those are someone else\'s in-flight edits, and git add <path> would sweep them into your commit). ' +
    'If any file you need to modify is in that set, edit nothing at all: return changed=false, naming the file in summary. ' +
    'Commit as ONE commit, staging ONLY the files you changed (git add <paths> — never git add -A or git commit -a). ' +
    'Message: imperative mood, subject like "validate: fix cycle ' + cycle + ' findings", ' +
    'NO trailers of any kind (no Co-Authored-By, no Claude-Session). Do NOT push. Never spawn subagents. ' +
    'Return: changed=true only if you committed; commit = the new sha (empty string if none); ' +
    'files = the commit\'s own paths, verbatim from git show --name-only --format= HEAD (empty if you did not commit); ' +
    'summary = one paragraph of what was fixed/skipped and why.'
}

// ---------------------------------------------------------------------------
// The loop: validate → (red) fix → re-run affected stages, up to maxCycles
// validation passes. Reviews always re-run after a fix (their input is the
// diff, which just changed); unit/builds/size/pvs re-run only when the fix
// touched code they consume, or when they failed themselves.
// ---------------------------------------------------------------------------
const latest = new Map()  // stage name -> most recent result
const history = []
let toRun = new Set(stageNames)

for (let cycle = 1; cycle <= maxCycles; cycle++) {
  const ran = displayNames([...toRun])
  log(`cycle ${cycle}/${maxCycles}: running ${ran.length}/${displayNames(stageNames).length} stage(s)`)
  const batches = await parallel([...toRun].map(n => stageThunk(n, cycle)))
  const results = batches.flatMap(r => Array.isArray(r) ? r : [r]).filter(Boolean)
  for (const r of results) latest.set(r.stage, r)
  const failures = [...latest.values()].filter(r => !r.pass)
  const entry = { cycle, ran, failed: failures.map(f => f.stage), fix: null }
  history.push(entry)

  if (failures.length === 0) {
    log(`cycle ${cycle}: all stages green`)
    return { pass: true, cycles: history, stages: [...latest.values()], failures: [] }
  }
  log(`cycle ${cycle}: ${failures.length} stage(s) failing: ${entry.failed.join(', ')}`)
  if (cycle === maxCycles) break

  const fix = await (async () => {
    try {
      return await agent(fixThunkPrompt(cycle, failures),
        { label: `c${cycle}:fix`, phase: 'Fix', model: 'sonnet', schema: FIX })
    } catch { return null }
  })()
  if (!fix) { entry.fix = 'fix agent died'; break }
  entry.fix = { changed: fix.changed, commit: fix.commit, files: fix.files, summary: clip(fix.summary) }
  if (!fix.changed) {
    // Nothing fixable in code. A dead stage agent is still worth retrying —
    // that failure is transient infrastructure, and a retry is the only
    // useful action for it. Everything else (refuted findings, missing CLI)
    // would just spin, so stop with the report.
    const deadStages = failures.filter(f => f.detail === 'stage agent died').map(f => f.stage)
    if (deadStages.length > 0) {
      log(`cycle ${cycle}: fix agent changed nothing — retrying dead stage(s): ${deadStages.join(', ')}`)
      toRun = new Set(deadStages.map(scheduleName))
      continue
    }
    log(`cycle ${cycle}: fix agent changed nothing — stopping`)
    break
  }

  // What re-runs is gated on what the commit actually contains, never on the
  // fixer's self-report (the FIX schema lets `files` be empty or wrong, and a
  // code fix reported as a doc path would keep stale-green results). Read the
  // paths back out of git; if that read fails, fall back to the self-report and
  // grant no exemption below.
  const verified = await (async () => {
    if (!fix.commit) return null
    try {
      return await agent(
        `In this repo run: git show --name-only --format= ${fix.commit} and git rev-parse HEAD. ` +
        'paths = the repo-relative paths that commit touched, verbatim, one per array entry; ' +
        `isHead = true only if git rev-parse HEAD is exactly ${fix.commit}. ` +
        'Read-only: edit nothing, commit nothing, never spawn subagents.',
        { label: `c${cycle}:fix-paths`, phase: 'Fix', model: 'haiku', schema: PATHS })
    } catch { return null }
  })()
  const trusted = !!(verified && verified.isHead && verified.paths.length > 0)
  const files = trusted ? verified.paths : (fix.files || [])
  entry.fix.files = files
  entry.fix.verified = trusted
  if (!trusted) log(`cycle ${cycle}: could not confirm the fix commit's paths — treating the fix as touching everything`)

  // The fixer rewrote this workflow, but the stage thunks, the gates and this
  // loop are the old file — already loaded in memory. Re-running here would
  // validate the corrected workflow with superseded orchestration and could
  // report green off it, so hand the restart back to the caller instead.
  const workflowFiles = files.filter(f => /^\.claude\/workflows\//.test(f))
  if (workflowFiles.length > 0) {
    log(`cycle ${cycle}: the fix commit edits ${workflowFiles.join(', ')} — stopping. ` +
      'Re-invoke validate so the committed workflow is loaded fresh; this run\'s verdict is not final.')
    return { pass: false, restartRequired: true, cycles: history, stages: [...latest.values()], failures }
  }

  // A fix can invalidate any stage: builds/unit consume src|hw|examples|test,
  // and size/pvs run tools the fixer may have edited. Only a pure-docs fix
  // is safe to exempt — everything else re-runs the full stage set. Agent and
  // skill instructions are Markdown but drive the stage agents themselves, so
  // they are operational, not documentation: editing them must re-run
  // everything, or the loop reports green on results the old instructions produced.
  const docsOnly = trusted && files.length > 0 && files.every(f =>
    !f.startsWith('.claude/') && f !== 'CLAUDE.md' && f !== 'AGENTS.md' &&
    (/^docs\//.test(f) || f.endsWith('.md') || f.endsWith('.rst')))
  toRun = new Set(failures.map(f => scheduleName(f.stage)))
  if (reviewProvider) toRun.add('reviews')
  if (!docsOnly) for (const n of stageNames) toRun.add(n)
}

const failures = [...latest.values()].filter(r => !r.pass)
return { pass: false, cycles: history, stages: [...latest.values()], failures }
