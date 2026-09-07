export const meta = {
  name: 'full-check',
  description: 'Composed pre-PR gate: validate (software) then, only if green, hil-validate (hardware)',
  whenToUse: 'One-shot pre-PR verdict; usually launched via the /pre-pr skill',
  phases: [{ title: 'Software' }, { title: 'Hardware' }],
}

// args: { boards: string[], hilBoards?: string[], examples?: string, base?: string,
//         skip?: string[], reviewProvider?: 'codex'|'claude'|'both' }
if (typeof args === 'string') { try { args = JSON.parse(args) } catch { /* not JSON: shape check below reports it */ } }
if (!args || !Array.isArray(args.boards) || args.boards.length === 0) {
  throw new Error('args must be { boards: string[], hilBoards?, examples?, base?, skip?, reviewProvider? }')
}

phase('Software')
const software = await workflow('validate', {
  boards: args.boards, examples: args.examples, base: args.base, skip: args.skip,
  reviewProvider: args.reviewProvider,
})
if (!software || !software.pass) {
  log('software validation failed — skipping HIL')
  return { pass: false, software, hardware: null }
}

const hilBoards = args.hilBoards || []
if (hilBoards.length === 0) {
  log('no HIL boards requested — software-only verdict')
  return { pass: true, software, hardware: null }
}

phase('Hardware')
const hardware = await workflow('hil-validate', { boards: hilBoards })
if (hardware && hardware.locked && hardware.locked.length) {
  log(`locked boards pending user decision (force / wait / accept): ${hardware.locked.join(', ')}`)
}
return { pass: !!(hardware && hardware.pass), software, hardware }
