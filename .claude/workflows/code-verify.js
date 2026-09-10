export const meta = {
  name: 'code-verify',
  description: 'Run the canonical code-verifier with Codex (default), Claude, or all independently',
  whenToUse: 'Nested by TinyUSB workflows that need structured code verification',
  phases: [{ title: 'Verify' }],
}

// args: { prompt: string, schema: object, provider?: 'codex'|'claude'|'all', label?: string }
if (typeof args === 'string') { try { args = JSON.parse(args) } catch { args = null } }
if (!args || typeof args.prompt !== 'string' || !args.prompt.trim() ||
    !args.schema || typeof args.schema !== 'object' || Array.isArray(args.schema)) {
  throw new Error('args must be { prompt: string, schema: object, provider?, label? }')
}
const provider = args.provider ?? 'codex'
if (!['codex', 'claude', 'all'].includes(provider)) {
  throw new Error('provider must be codex, claude, or all')
}
const label = args.label || 'code-verifier'
// The launcher wraps Codex's answer with where it ran: a reply without a job
// dir and thread id did not come from Codex, whatever its shape.
// A timed-out run is an envelope with result: null and an error line.
const envelope = schema => ({
  type: 'object', additionalProperties: false,
  required: ['job', 'thread', 'result'],
  properties: {
    job: { type: 'string' }, thread: { type: ['string', 'null'] },
    result: { anyOf: [schema, { type: 'null' }] }, error: { type: 'string' },
  },
})
// A timeout before Codex announced its thread has thread: null; the job dir
// and the error line still say Codex ran.
const fromCodex = r => r && typeof r.job === 'string' && r.job
const runCodex = () => agent(
  JSON.stringify({ prompt: args.prompt, schema: args.schema }), {
    label: `${label}:codex`, phase: 'Verify', agentType: 'codex-agent', schema: envelope(args.schema),
  }).then(r => {
  if (fromCodex(r) && r.result === null && r.error) throw new Error(`codex verifier timed out: ${r.error}`)
  if (!fromCodex(r) || !r.thread || !r.result) throw new Error('codex verifier failed')
  return r.result
}).catch(e => {
  if (e && /^codex verifier (failed|timed out)/.test(e.message)) throw e
  throw new Error(`codex verifier failed: ${e && e.message ? e.message : e}`)
})

const runClaude = (suffix = 'claude') => agent(args.prompt, {
  label: `${label}:${suffix}`, phase: 'Verify', agentType: 'code-verifier', schema: args.schema,
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
  if (/timed out/.test(e.message)) throw e
  log(`${label}: ${e.message} — reviewing with claude instead`)
  return runClaude('claude-fallback')
})

if (provider === 'codex') return runCodexOrClaude()
if (provider === 'claude') return runClaude()
const [codex, claude] = await parallel([runCodex, runClaude])
if (!codex) throw new Error('codex verifier failed')
if (!claude) throw new Error('claude verifier failed')
return { codex, claude }
