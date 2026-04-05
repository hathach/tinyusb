# hil

Run Hardware-in-the-Loop (HIL) tests on physical boards.

## Arguments
- $ARGUMENTS: Optional flags (e.g. board name, extra args). If empty, runs all boards with default config.

## Instructions

1. Parse $ARGUMENTS:
   - If $ARGUMENTS contains `-b BOARD_NAME`, run for that specific board only.
   - If $ARGUMENTS is empty or has no `-b`, run for all boards in the config.
   - Pass through any other flags (e.g. `-v` for verbose, `-r N` for retry count) directly to the command.

2. Determine whether to run **locally** or **remotely via SSH**:
   - **Local**: boards are attached to this machine (default when `local.json` is used)
   - **Remote (`ssh ci.lan`)**: boards are attached to the CI machine (when `tinyusb.json` is used)

3. **Local execution** (boards attached to this machine):
   ```bash
   python test/hil/hil_test.py -b BOARD_NAME -B examples $HIL_CONFIG $EXTRA_ARGS
   ```

4. **Remote execution** (boards attached to `ci.lan`):
   Only copy the minimal files needed (firmware binaries + test script + config), then run remotely.

   ```bash
   REMOTE=ci.lan
   REMOTE_DIR=/tmp/tinyusb-hil

   # Create remote working directory
   ssh $REMOTE "rm -rf $REMOTE_DIR && mkdir -p $REMOTE_DIR/test/hil"

   # Copy HIL test script and its dependency
   scp test/hil/hil_test.py test/hil/pymtp.py test/hil/tinyusb.json $REMOTE:$REMOTE_DIR/test/hil/

   # Copy only the firmware binaries for the target board(s)
   # For a specific board:
   scp -r examples/cmake-build-$BOARD_NAME $REMOTE:$REMOTE_DIR/examples/

   # Or for all boards that have been built:
   # for dir in examples/cmake-build-*/; do scp -r "$dir" $REMOTE:$REMOTE_DIR/examples/; done

   # Run the test remotely
   ssh $REMOTE "cd $REMOTE_DIR && python3 test/hil/hil_test.py -b $BOARD_NAME -B examples tinyusb.json $EXTRA_ARGS"
   ```

   Note: The remote machine (`ci.lan`) must have:
   - Python 3 with `pyserial` installed (`pip install pyserial`)
   - Flasher tools: `JLinkExe`, `openocd`, etc. as needed by the board
   - USB access to the boards (udev rules configured)

5. Use a timeout of at least 20 minutes (600000ms). HIL tests take 2-5 minutes. NEVER cancel early.

6. After the test completes:
   - Show the test output to the user.
   - Summarize pass/fail results per board.
   - If there are failures, suggest re-running with `-v` flag for verbose output to help debug.
