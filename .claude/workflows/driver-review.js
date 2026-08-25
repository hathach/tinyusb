export const meta = {
  name: 'driver-review',
  description: 'Review driver directories across dimensions with driver-reviewer scanners, then adversarially verify every finding; returns only confirmed findings',
  whenToUse: 'Auditing dcd/hcd drivers for a bug class (pass question) or a full-dimension review (default dimensions)',
  phases: [
    { title: 'Scan', detail: 'driver-reviewer per (dir x dimension)' },
    { title: 'Verify', detail: 'adversarial refutation per finding' },
  ],
}

// args: { dirs: string[], dimensions?: string[], question?: string }
if (typeof args === 'string') { try { args = JSON.parse(args) } catch { /* not JSON: shape check below reports it */ } }
if (!args || !Array.isArray(args.dirs) || args.dirs.length === 0) {
  throw new Error('args must be { dirs: string[], dimensions?, question? }')
}
const DIMS = args.question ? [args.question] : (args.dimensions || [
  'correctness: transfer state machines, endpoint bookkeeping, completion and error paths',
  'ISR safety: work deferred to task context, shared-state races, register access ordering',
  'register use vs datasheet and MCU errata: cross-check the reference manual AND errata sheets via the read-doc skill (python3 .claude/skills/read-doc/search.py <keywords>); a missing erratum workaround is a finding',
  'style: repo conventions (TU_ASSERT, no dynamic allocation, include order, naming)',
])
if (!DIMS.length) {
  // [] is truthy, so `dimensions: []` would silently review nothing and
  // return a verdict indistinguishable from a genuinely clean pass
  throw new Error('dimensions resolved to an empty list — pass a non-empty array or omit it for the defaults')
}
const short = (s) => s.replace(/\/+$/, '').split('/').slice(-2).join('/')

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
const VERDICT = {
  type: 'object', additionalProperties: false,
  required: ['real', 'reason'],
  properties: { real: { type: 'boolean' }, reason: { type: 'string' } },
}

const pairs = args.dirs.flatMap(dir => DIMS.map(dim => ({ dir, dim })))
log(`${pairs.length} scan units (${args.dirs.length} dirs x ${DIMS.length} dimensions)`)

const results = await pipeline(
  pairs,

  p => agent(
    `Review ${p.dir} for exactly one dimension: ${p.dim}. Read the sources yourself. Coverage-first — report everything, a verifier filters.`,
    { label: `scan:${short(p.dir)}`, phase: 'Scan', agentType: 'driver-reviewer', effort: 'xhigh', schema: FINDINGS },
  ),

  (scan, p) => {
    if (!scan) return null  // dead scanner — dropped, counted, and logged below
    if (scan.findings.length === 0) return { dir: p.dir, dim: p.dim, findings: [] }
    return parallel(scan.findings.map(f => () =>
      agent(
        `Adversarially verify ONE review finding about ${p.dir}.\nDimension: ${p.dim}\nFinding: ${JSON.stringify(f)}\n` +
        'Read the cited code plus enough context (callers, ISR paths, macros, and the datasheet if register-related) to judge. ' +
        'Try to REFUTE it; real=true only if it survives your best attempt. Return {"real": bool, "reason": string}.',
        { label: `verify:${short(p.dir)}:${f.line}`, phase: 'Verify', agentType: 'driver-reviewer', effort: 'xhigh', schema: VERDICT },
      ).then(v => v && { ...f, verdict: v })
    )).then(vs => {
      const alive = vs.filter(Boolean)
      if (alive.length < scan.findings.length) {
        log(`${short(p.dir)}: ${scan.findings.length - alive.length} finding(s) lost to dead verifiers — treat as unverified, re-run if needed`)
      }
      return { dir: p.dir, dim: p.dim, findings: alive.filter(x => x.verdict.real) }
    })
  },
)

const units = results.filter(Boolean)
if (units.length < pairs.length) log(`${pairs.length - units.length} scan unit(s) dropped (scanner died)`)
const confirmed = units.filter(r => r.findings.length > 0)
log(`${confirmed.length} scan units produced confirmed findings`)
return confirmed
