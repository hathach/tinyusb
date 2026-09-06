export const meta = {
  name: 'code-verify',
  description: 'Run the canonical code-verifier with Codex (default), Claude, or both independently',
  whenToUse: 'Nested by TinyUSB workflows that need structured code verification',
  phases: [{ title: 'Verify' }],
}

// args: { prompt: string, schema: object, provider?: 'codex'|'claude'|'both', label?: string }
if (typeof args === 'string') { try { args = JSON.parse(args) } catch { args = null } }
if (!args || typeof args.prompt !== 'string' || !args.prompt.trim() ||
    !args.schema || typeof args.schema !== 'object' || Array.isArray(args.schema)) {
  throw new Error('args must be { prompt: string, schema: object, provider?, label? }')
}
const provider = args.provider || 'codex'
if (!['codex', 'claude', 'both'].includes(provider)) {
  throw new Error('provider must be codex, claude, or both')
}
const label = args.label || 'code-verifier'
const codexPrompt = `Act only as a process bridge; do not perform verification yourself.
From the repository root, read .codex/agents/code-verifier.toml with Python's
tomllib. Create one temporary directory and arrange to remove it on exit. Write
the JSON inside <schema> to a schema file. Write the adapter's
developer_instructions followed by the text inside <task> to a prompt file.
Run Codex with a 600-second timeout from the repository root:
timeout 600s codex exec -C <root> -m <adapter model>
  -c model_reasoning_effort=<adapter model_reasoning_effort>
  --sandbox read-only --output-schema <schema file>
  --output-last-message <result file> - < <prompt file>
If every command succeeds, return only the result file's JSON as your final
answer. If anything fails, do not perform the verification or fabricate JSON.
<schema>${JSON.stringify(args.schema)}</schema>
<task>${args.prompt}</task>`

const runCodex = () => agent(codexPrompt, {
  label: `${label}:codex`, phase: 'Verify', model: 'haiku', effort: 'low', schema: args.schema,
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
return { codex, claude }
