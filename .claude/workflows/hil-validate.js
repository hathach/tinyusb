export const meta = {
  name: 'hil-validate',
  description: 'Hardware-in-the-loop run: one hil-operator flashes and tests every board in a single hil_test.py run; per-board flock locks arbitrate with concurrent CI (the actions-runner keeps running)',
  whenToUse: 'After validate passes, to exercise built firmware on the physical rig. Requires the boards to be built (examples/cmake-build-<board>, plus a dir per declared variant). If the result has non-empty `locked`, ask the user: force (re-invoke with force: true), continue waiting (re-invoke later), or accept the partial result. Pass force: true ONLY with explicit user authorization.',
  phases: [{ title: 'HIL', detail: 'one hil-operator, every board in one hil_test.py run' }],
}

// args: { boards: string[], force?: boolean }
if (typeof args === 'string') { try { args = JSON.parse(args) } catch { /* not JSON: shape check below reports it */ } }
if (!args || !Array.isArray(args.boards) || args.boards.length === 0) {
  throw new Error('args must be { boards: string[], force? } with the boards already built')
}

// The operator returns hil_report.py's JSON verbatim plus its own observations. It does NOT
// retype the report table: rows are named per variant, a variant need not start with the board
// name, and lock contention is a cell rather than a phrase — rebuilding board identity from
// prose produced a defect in each of four review rounds. hil_report.py does that join against
// the roster, so `locked` and `ran` arrive as fields and nothing here parses a detail string.
const BOARD = {
  type: 'object', additionalProperties: false,
  required: ['board', 'ran', 'pass', 'locked', 'detail'],
  properties: {
    board: { type: 'string' }, ran: { type: 'boolean' }, pass: { type: 'boolean' },
    locked: { type: 'boolean' }, detail: { type: 'string' },
  },
}
const HIL = {
  type: 'object', additionalProperties: false,
  required: ['results', 'wedged', 'caveat'],
  properties: {
    results: { type: 'array', items: BOARD },
    // the operator's own observation — not derivable from the report
    wedged: { type: 'array', items: { type: 'string' } },
    banner: { type: 'string' },
    // the run-level caveat: abandoned / aborted / selected-no-boards. `banner` carries rig
    // HEALTH across an --accumulate retry; `caveat` carries how THIS run ended, and every
    // row can still say pass while it failed — so it gates `pass` in summarize() below.
    caveat: { type: 'string' },
  },
}

// ONE operator for the whole set, and one hil_test.py inside it. hil_test.py already runs the
// boards concurrently: it round-robins them across host controllers and holds per-controller
// permits (hil_lock.py FLASH_PARALLEL/USBTEST_PARALLEL) that bound simultaneous flashes and
// usbtest batteries. Those permits live in one process, so a second hil_test.py does not share
// them - N parallel single-board runs multiply the budget by N onto the same xHCI cards, for no
// wall-clock gain over one run that already parallelizes them.
const runBoards = (boards, isRetry = false) => agent(
  `Run the HIL tests for these boards per .claude/skills/hil/SKILL.md: ${boards.join(', ')}. ` +
  `Pass them ALL to ONE hil_test.py invocation as repeated -b flags (${boards.map((b) => `-b ${b}`).join(' ')}) — it schedules them across host controllers and budgets concurrent flashes and usbtest batteries itself. Never start a second hil_test.py alongside it. ` +
  (isRetry
    ? 'This is a RE-RUN of boards an earlier run could not take: pass --accumulate as well, or hil_test.py unlinks the report and the whole-fleet table collapses to just these boards. '
    : '') +
  'Do NOT touch the actions-runner service and do NOT pre-hold the board locks — hil_test.py self-locks each board for its flash+test. ' +
  (args.force
    ? 'THE USER HAS EXPLICITLY AUTHORIZED FORCING: run hil_test.py with HIL_NO_BOARD_LOCK=1 in the environment (bypasses the board lock check; do NOT release or kill the existing holder). '
    : 'A board whose lock is held (a dev session or concurrent CI job) fails fast inside the run without blocking the others — never force the lock. ') +
  'If hil_test.py refuses the run with "board(s) not in <config>", re-run it WITHOUT the unknown names but keep the FULL board list on the hil_report call below — it emits a ran:false entry for every board you name, so the unknown ones surface as "no report row" instead of costing the whole batch. ' +
  'Use the config for this host (hostname first). Run hil_test.py as a BACKGROUND Bash task and wait for it (a stuck fleet runs to its pool guard, 60 min by default — beyond any foreground timeout); never cancel it early. ' +
  'On non-lock failures retry ONCE from the re-run spec hil_test.py just wrote — `<config>.failed`, which already begins with --accumulate — adding -v. A usbtest battery that produced per-case verdicts is NOT auto-retried, so its result already stands. ' +
  'THEN, from the directory the run wrote its report to, produce the results with:\n' +
  `  python3 test/hil/helper/hil_report.py <the config you used> ${boards.map((b) => `-b ${b}`).join(' ')}\n` +
  'Return its `results` array, `banner` and `caveat` EXACTLY as printed — do not retype, reword, re-order or "correct" them, and never transcribe the markdown table instead. ' +
  'Add `wedged`: the board names whose board or fixture your run left unresponsive (usually none). That is your own observation and the one field you author; put `dmesg | tail -50` in your reply text for any board you list.',
  {
    label: boards.length === 1 ? `hil:${boards[0]}` : `hil:${boards.length} boards`,
    phase: 'HIL', agentType: 'hil-operator', schema: HIL,
  },
)

// A lookup, not a reconciliation: hil_report.py emits exactly one entry per requested board,
// so a missing entry means the operator dropped it rather than that the names disagree.
const byBoard = (out) => new Map((out?.results || [])
  .filter((r) => r && typeof r.board === 'string')
  .map((r) => [r.board, r]))

// `wedged` is the one field the operator authors, so it may echo a report row name
// ('nano-fsdev') where the prompt asked for a board name. Accept the variant spelling
// rather than dropping a wedge over it -- only `wedged` sends anyone to usb-kernel-recover.
const wedgedFor = (list, b) => (Array.isArray(list) ? list : [])
  .some((w) => typeof w === 'string' && (w === b || w.startsWith(`${b}-`)))

const first = await runBoards(args.boards)
const firstRows = byBoard(first)
const firstWedged = first?.wedged || []
const results = args.boards.map((b) => {
  const r = firstRows.get(b)
  if (!r) {
    return {
      board: b, pass: false, locked: false, ran: false, wedged: wedgedFor(firstWedged, b),
      detail: first ? 'hil-operator returned no entry for this board' : 'hil-operator agent died',
    }
  }
  return { ...r, wedged: wedgedFor(firstWedged, b) }
})
for (const r of results) log(`${r.board}: ${r.pass ? 'PASS' : r.locked ? 'LOCKED' : 'FAIL'}`)
if (first?.banner) log(`report banner: ${first.banner.trim().split('\n')[0]}`)
if (first?.caveat) log(`report caveat: ${first.caveat.trim().split('\n')[0]}`)

let runCaveat = first?.caveat || ''
// A concurrent CI job may have held some boards (its hil_test.py flock).
// CI finishes a board in minutes — retry locked boards once, at the end.
if (!args.force) {
  const relock = results.filter((r) => r.locked && !r.pass).map((r) => r.board)
  if (relock.length) {
    log(`was locked, retrying once: ${relock.join(', ')}`)
    const again = await runBoards(relock, true)
    const rows = byBoard(again)
    const againWedged = again?.wedged || []
    for (let i = 0; i < results.length; i++) {
      const b = results[i].board
      if (!relock.includes(b)) continue
      const r = rows.get(b)
      // No entry keeps the board `locked`, so it still reaches the user's force/wait/accept
      // decision instead of being published as a hardware failure.
      if (r) {
        // a passing retry does NOT clear a wedge the first run left behind: only `wedged`
        // sends anyone to usb-kernel-recover
        results[i] = { ...r, wedged: results[i].wedged || wedgedFor(againWedged, b) }
      } else {
        results[i].detail += again ? ' (retry returned no entry)' : ' (retry operator died)'
      }
      log(`${b}: retry ${results[i].pass ? 'PASS' : 'FAIL'}`)
    }
    // the retry's own run-level verdict, not the first attempt's: a retry that abandoned
    // or aborted must sink the run even though its rows may all say pass.
    if (again?.caveat) runCaveat = again.caveat
  }
}

// pass/wedged/locked in one place so it can be exercised without running an agent.
// `caveat` is a RUN-level verdict and must gate `pass`: on the abandon and no-boards
// paths every row can legitimately say pass while the run itself failed (hil_test.py
// os._exit(1) -- a red job), so per-row agreement alone published those runs as green.
const summarize = (rs, force, caveat) => ({
  pass: rs.every((r) => r.pass) && !/^\*\*HIL run (abandoned|aborted|selected no boards)/m
    .test(caveat || ''),
  wedged: rs.filter((r) => r.wedged).map((r) => r.board),
  locked: force ? [] : rs.filter((r) => !r.pass && r.locked).map((r) => r.board),
})

const { pass, wedged, locked } = summarize(results, args.force, runCaveat)
if (wedged.length) log(`WEDGED boards needing usb-kernel-recover: ${wedged.join(', ')}`)
// Workers cannot prompt the user — surface still-locked boards for the main
// session to ask: force (re-invoke with force: true), wait, or accept.
if (locked.length) log(`still locked after retry: ${locked.join(', ')} — ask the user: force / keep waiting / accept`)
return { pass, results, wedged, locked, caveat: runCaveat }
