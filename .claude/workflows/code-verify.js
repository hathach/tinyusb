export const meta = {
  name: 'code-verify',
  description: 'Run code-verifier or finding-verifier with Codex (default), Claude, or all independently',
  whenToUse: 'Nested by TinyUSB workflows that need structured code verification',
  phases: [{ title: 'Verify' }],
}

// args: { prompt: string, schema: object, provider?: 'codex'|'claude'|'all',
//         role?: 'code-verifier'|'finding-verifier', label?: string }
if (typeof args === 'string') { try { args = JSON.parse(args) } catch { args = null } }
if (!args || typeof args.prompt !== 'string' || !args.prompt.trim() ||
    !args.schema || typeof args.schema !== 'object' || Array.isArray(args.schema)) {
  throw new Error('args must be { prompt: string, schema: object, provider?, role?, label? }')
}
const provider = args.provider ?? 'codex'
if (!['codex', 'claude', 'all'].includes(provider)) {
  throw new Error('provider must be codex, claude, or all')
}
const role = args.role === undefined ? 'code-verifier' : args.role
if (!['code-verifier', 'finding-verifier'].includes(role)) {
  throw new Error('role must be code-verifier or finding-verifier')
}
const label = args.label || 'code-verifier'
// The launcher wraps Codex's answer with where it ran and how it ended:
// status 'ok' carries the result, 'timeout' means Codex ran and never
// finished. A reply without a job dir did not come from Codex, whatever its
// shape. Same contract as validate.js's CODEX_ENVELOPE.
const CODEX_ENVELOPE = {
  type: 'object', additionalProperties: false,
  required: ['job', 'thread', 'status', 'result'],
  properties: {
    job: { type: 'string' }, thread: { type: ['string', 'null'] },
    status: { enum: ['ok', 'timeout'] },
    result: { anyOf: [args.schema, { type: 'null' }] }, error: { type: ['string', 'null'] },
  },
}
const outcome = r => !r || typeof r.job !== 'string' || !r.job ? 'unavailable'
  : r.status === 'timeout' ? 'timeout'
  : r.status === 'ok' && r.thread && r.result ? 'ok' : 'unavailable'
const runCodex = () => agent(
  JSON.stringify({ prompt: args.prompt, schema: args.schema, role }), {
    label: `${label}:codex`, phase: 'Verify', agentType: 'codex-agent', schema: CODEX_ENVELOPE,
  }).then(r => {
  switch (outcome(r)) {
    case 'ok': return r.result
    case 'timeout': throw Object.assign(new Error(`codex verifier timed out: ${r.error}`), { timedOut: true })
    default: throw new Error('codex verifier failed')
  }
}).catch(e => {
  if (e && (e.timedOut || e.message === 'codex verifier failed')) throw e
  throw new Error(`codex verifier failed: ${e && e.message ? e.message : e}`)
})

const runClaude = (suffix = 'claude') => agent(args.prompt, {
  label: `${label}:${suffix}`, phase: 'Verify', agentType: role, schema: args.schema,
}).then(r => {
  if (!r) throw new Error('claude verifier failed')
  return r
}).catch(e => {
  if (e && e.message === 'claude verifier failed') throw e
  throw new Error(`claude verifier failed: ${e && e.message ? e.message : e}`)
})

// Codex that cannot run (no binary, no login, quota, a crash before any result)
// hands the job to Claude. A timeout is Codex having run: no second review.
const runCodexOrClaude = () => runCodex().catch(e => {
  if (e.timedOut) throw e
  log(`${label}: ${e.message} — reviewing with claude instead`)
  return runClaude('claude-fallback')
})

if (provider === 'codex') return runCodexOrClaude()
if (provider === 'claude') return runClaude()
const [codex, claude] = await parallel([runCodex, runClaude])
if (!codex) throw new Error('codex verifier failed')
if (!claude) throw new Error('claude verifier failed')
return { codex, claude }
