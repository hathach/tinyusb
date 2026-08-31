// Executable checks for hil-validate.js's result handling.
//
// The join that used to live here -- matching variant row names to boards, parsing
// `board locked` out of a prose detail, folding rows, keeping a wedged flag alive -- produced
// a defect in each of four review rounds, including a test that asserted an invariant using
// the one input shape that could not break it. That logic now lives in
// test/hil/helper/hil_report.py, where the roster is, and arrives here as fields. What is
// left is a lookup and a verdict, and this pins both.
//
// Run: node .claude/workflows/test-hil-validate.mjs
import { readFileSync } from 'node:fs'

const src = readFileSync(new URL('./hil-validate.js', import.meta.url), 'utf8')
// slice by marker, but never silently: a renamed marker must fail with its name, not with a
// confusing ReferenceError from a garbage slice
const cut = (start, end) => {
  const a = src.indexOf(start), b = src.indexOf(end)
  if (a < 0 || b < 0 || b <= a) {
    console.error(`FAIL: extraction marker moved — cannot find ${a < 0 ? `'${start}'` : `'${end}'`} in hil-validate.js`)
    process.exit(1)
  }
  return src.slice(a, b)
}
const body = cut('const byBoard =', 'const first = await runBoards')
  + cut('const summarize =', 'const { pass, wedged, locked } =')
// more than one schema declares `required:`; pick the HIL one by its contents
const HIL_REQUIRED = (src.match(/required: \[[^\]]*\]/g) || [])
  .map((m) => m.replace('required: ', '').replace(/'/g, '"'))
  .map((m) => JSON.parse(m))
  .find((a) => a.includes('wedged')) || []
const { byBoard, summarize, wedgedFor } = new Function(`${body}; return { byBoard, summarize, wedgedFor }`)()

let failed = 0
const check = (name, got, want) => {
  const g = JSON.stringify(got), w = JSON.stringify(want)
  if (g === w) return console.log(`  ok   ${name}`)
  failed++
  console.log(`  FAIL ${name}\n       got  ${g}\n       want ${w}`)
}
const R = (board, pass, locked = false, wedged = false, detail = '') =>
  ({ board, ran: true, pass, locked, detail, wedged })

console.log('byBoard — indexing the operator payload')
check('indexes by board', [...byBoard({ results: [R('a', true)] }).keys()], ['a'])
check('null payload', [...byBoard(null).keys()], [])
check('missing results', [...byBoard({}).keys()], [])
check('rows not an array', [...byBoard({ results: null }).keys()], [])
for (const bad of [[null], [undefined], [{ pass: true }], [{ board: 42 }]]) {
  try { check(`malformed row ${JSON.stringify(bad)}`, [...byBoard({ results: bad }).keys()], []) }
  catch (e) { failed++; console.log(`  FAIL malformed row threw ${e}`) }
}

console.log('wedgedFor — operator-authored names, variant spellings tolerated')
check('board name matches', wedgedFor(['nano'], 'nano'), true)
check('variant spelling matches', wedgedFor(['nano-fsdev'], 'nano'), true)
check('another board does not', wedgedFor(['other'], 'nano'), false)
check('prefix without dash does not', wedgedFor(['nanoch32'], 'nano'), false)
check('null list', wedgedFor(null, 'nano'), false)
check('non-string entry does not throw', wedgedFor([42, 'nano'], 'nano'), true)

console.log('summarize — the ship/no-ship verdict')
check('all pass', summarize([R('a', true)], false).pass, true)
check('one fail sinks it', summarize([R('a', true), R('b', false)], false).pass, false)
check('a locked board is not a pass', summarize([R('a', false, true)], false).pass, false)
check('locked is a field, not a prefix', summarize([R('a', false, true)], false).locked, ['a'])
check('a real failure is not locked', summarize([R('a', false, false)], false).locked, [])
check('a PASSING board is never locked', summarize([R('a', true, true)], false).locked, [])
check('force zeroes locked', summarize([R('a', false, true)], true).locked, [])
check('wedged surfaces', summarize([R('a', false, false, true)], false).wedged, ['a'])
check('a wedged board that passed still surfaces',
  summarize([R('a', true, false, true)], false).wedged, ['a'])

// A run-level caveat outranks per-row agreement: on the abandon and no-boards paths every
// row can legitimately pass while hil_test.py exits non-zero. Row agreement alone published
// those runs green.
check('all rows pass and no caveat is a pass',
  summarize([R('a', true), R('b', true)], false, '').pass, true)
check('an abandoned run is not a pass',
  summarize([R('a', true)], false,
    '**HIL run abandoned: the worker pool would not shut down.** x').pass, false)
check('an aborted run is not a pass',
  summarize([R('a', true)], false, '**HIL run aborted: a worker raised RuntimeError**').pass,
  false)
check('a no-boards run is not a pass',
  summarize([R('a', true)], false, '**HIL run selected no boards.** filters emptied').pass,
  false)
check('a rig-health note is NOT a caveat and does not fail the run',
  summarize([R('a', true)], false, '> **Rig note.** 2 process(es) in D state').pass, true)
check('a retry that abandoned sinks the run even with all rows passing',
  summarize([R('a', true)], false,
    '**HIL run abandoned: the worker pool would not shut down.** retry').pass, false)
check('an omitted caveat cannot silently disable the gate (schema requires it)',
  HIL_REQUIRED.includes('caveat'), true)

console.log(failed ? `\n${failed} FAILED` : '\nall checks passed')
process.exit(failed ? 1 : 0)
