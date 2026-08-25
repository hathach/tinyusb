export const meta = {
  name: 'hil-validate',
  description: 'Serialized hardware-in-the-loop run: flash+test each board with hil-operator; per-board flock locks arbitrate with concurrent CI (the actions-runner keeps running)',
  whenToUse: 'After validate passes, to exercise built firmware on the physical rig. Requires examples/cmake-build-<board> for each board. If the result has non-empty `locked`, ask the user: force (re-invoke with force: true), continue waiting (re-invoke later), or accept the partial result. Pass force: true ONLY with explicit user authorization.',
  phases: [{ title: 'HIL', detail: 'strictly serial per-board hil-operator runs' }],
}

// args: { boards: string[], force?: boolean }
if (typeof args === 'string') { try { args = JSON.parse(args) } catch { /* not JSON: shape check below reports it */ } }
if (!args || !Array.isArray(args.boards) || args.boards.length === 0) {
  throw new Error('args must be { boards: string[], force? } with examples/cmake-build-<board> already built')
}

const HIL = {
  type: 'object', additionalProperties: false,
  required: ['board', 'pass', 'detail', 'wedged'],
  properties: {
    board: { type: 'string' }, pass: { type: 'boolean' },
    detail: { type: 'string' }, wedged: { type: 'boolean' },
  },
}

const runBoard = (b) => agent(
  `Run the HIL test for board ${b} per .claude/skills/hil/SKILL.md. Do NOT touch the actions-runner service and do NOT pre-hold the board lock — hil_test.py self-locks the board while testing. ` +
  (args.force
    ? 'THE USER HAS EXPLICITLY AUTHORIZED FORCING: run hil_test.py with HIL_NO_BOARD_LOCK=1 in the environment (bypasses the board lock check; do NOT release or kill the existing holder). '
    : 'If the run fails because the board lock is held (a dev session or concurrent CI job), report pass=false and set detail to start EXACTLY with "board locked:" followed by the holder JSON verbatim — never force the lock. ') +
  'Reserve the phrase "board locked" strictly for lock contention; describe a frozen or non-enumerating board as "unresponsive" instead. ' +
  `Firmware is in examples/cmake-build-${b}. Use the config for this host (hostname first), single-board flag -b ${b}. Run hil_test.py as a BACKGROUND Bash task and wait for it (a stuck fleet runs to its pool guard, 60 min by default — beyond any foreground timeout); never cancel it early. ` +
  'On non-lock failures retry once with -v -r 1 (one verbose attempt for diagnosis; note a usbtest battery that produced per-case verdicts is NOT auto-retried, so its result already stands). wedged=true if the board/fixture is unresponsive after the run (capture dmesg | tail -50 into detail).',
  { label: `hil:${b}`, phase: 'HIL', agentType: 'hil-operator', schema: HIL },
)

const results = []
for (const b of args.boards) {
  const r = await runBoard(b)
  results.push(r || { board: b, pass: false, detail: 'hil-operator agent died', wedged: false })
  log(`${b}: ${results[results.length - 1].pass ? 'PASS' : 'FAIL'}`)
}

// A concurrent CI job may have held some boards (its hil_test.py flock).
// CI finishes a board in minutes — retry locked boards once, at the end.
if (!args.force) {
  for (let i = 0; i < results.length; i++) {
    if (results[i].pass || !results[i].detail.startsWith('board locked')) continue
    log(`${results[i].board}: was locked — retrying once`)
    const r = await runBoard(results[i].board)
    if (r) results[i] = r
    else results[i].detail += ' (retry operator died)'
    log(`${results[i].board}: retry ${results[i].pass ? 'PASS' : 'FAIL'}`)
  }
}

const wedged = results.filter(r => r.wedged).map(r => r.board)
if (wedged.length) log(`WEDGED boards needing usb-kernel-recover: ${wedged.join(', ')}`)
// Workers cannot prompt the user — surface still-locked boards for the main
// session to ask: force (re-invoke with force: true), wait, or accept.
const locked = args.force ? [] : results.filter(r => !r.pass && r.detail.startsWith('board locked')).map(r => r.board)
if (locked.length) log(`still locked after retry: ${locked.join(', ')} — ask the user: force / keep waiting / accept`)
return { pass: results.every(r => r.pass), results, wedged, locked }
