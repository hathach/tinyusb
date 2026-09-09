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
const runCodex = () => agent(
  JSON.stringify({ role: 'code-verifier', prompt: args.prompt, schema: args.schema }), {
    label: `${label}:codex`, phase: 'Verify', agentType: 'codex-agent', schema: args.schema,
  }).then(r => {
  if (!r) throw new Error('codex verifier failed')
  return r
}).catch(e => {
  if (e && e.message === 'codex verifier failed') throw e
  throw new Error(`codex verifier failed: ${e && e.message ? e.message : e}`)
})

const runClaude = () => agent(args.prompt, {
  label: `${label}:claude`, phase: 'Verify', agentType: 'code-verifier', schema: args.schema,
}).then(r => {
  if (!r) throw new Error('claude verifier failed')
  return r
}).catch(e => {
  if (e && e.message === 'claude verifier failed') throw e
  throw new Error(`claude verifier failed: ${e && e.message ? e.message : e}`)
})

if (provider === 'codex') return runCodex()
if (provider === 'claude') return runClaude()
const [codex, claude] = await parallel([runCodex, runClaude])
if (!codex) throw new Error('codex verifier failed')
if (!claude) throw new Error('claude verifier failed')
return { codex, claude }
