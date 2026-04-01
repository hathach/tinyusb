# hil

Run Hardware-in-the-Loop (HIL) tests on physical boards.

## Arguments
- $ARGUMENTS: Optional flags (e.g. board name, extra args). If empty, runs all boards with default config.

## Instructions

1. Determine the HIL config file:
   ```bash
   HIL_CONFIG=$( (systemctl list-units --type=service --state=running 2>/dev/null; systemctl --user list-units --type=service --state=running 2>/dev/null) | grep -q 'actions\.runner' && echo tinyusb.json || echo local.json )
   ```
   Default is `local.json` for local development.

2. Parse $ARGUMENTS:
   - If $ARGUMENTS contains `-b BOARD_NAME`, run for that specific board only.
   - If $ARGUMENTS is empty or has no `-b`, run for all boards in the config.
   - Pass through any other flags (e.g. `-v` for verbose) directly to the command.

3. Run the HIL test from the repo root directory:
   - Specific board: `python test/hil/hil_test.py -b BOARD_NAME -B examples $HIL_CONFIG $EXTRA_ARGS`
   - All boards: `python test/hil/hil_test.py -B examples $HIL_CONFIG $EXTRA_ARGS`

4. Use a timeout of at least 20 minutes (600000ms). HIL tests take 2-5 minutes. NEVER cancel early.

5. After the test completes:
   - Show the test output to the user.
   - Summarize pass/fail results per board.
   - If there are failures, suggest re-running with `-v` flag for verbose output to help debug.
