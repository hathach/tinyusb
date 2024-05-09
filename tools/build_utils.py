import subprocess
import pathlib
import time

build_format = '| {:29} | {:30} | {:18} | {:7} | {:6} | {:6} |'

SUCCEEDED = "\033[32msucceeded\033[0m"
FAILED = "\033[31mfailed\033[0m"
SKIPPED = "\033[33mskipped\033[0m"


def skip_example(example, board):
    ex_dir = pathlib.Path('examples/') / example
    bsp = pathlib.Path("hw/bsp")

    if (bsp / board / "board.mk").exists():
        # board without family
        board_dir = bsp / board
        family = ""
        mk_contents = ""
    else:
        # board within family
        board_dir = list(bsp.glob("*/boards/" + board))
        if not board_dir:
            # Skip unknown boards
            return True

        board_dir = list(board_dir)[0]

        family_dir = board_dir.parent.parent
        family = family_dir.name

        # family.mk
        family_mk = family_dir / "family.mk"
        mk_contents = family_mk.read_text()

    # Find the mcu, first in family mk then board mk
    if "CFG_TUSB_MCU=OPT_MCU_" not in mk_contents:
        board_mk = board_dir / "board.cmake"
        if not board_mk.exists():
            board_mk = board_dir / "board.mk"

        mk_contents = board_mk.read_text()

    mcu = "NONE"
    for token in mk_contents.split():
        if "CFG_TUSB_MCU=OPT_MCU_" in token:
            # Strip " because cmake files has them.
            token = token.strip("\"")
            _, opt_mcu = token.split("=")
            mcu = opt_mcu[len("OPT_MCU_"):]
            break
        if "esp32s2" in token:
            mcu = "ESP32S2"
            break
        if "esp32s3" in token:
            mcu = "ESP32S3"
            break

    # Skip all OPT_MCU_NONE these are WIP port
    if mcu == "NONE":
        return True

    skip_file = ex_dir / "skip.txt"
    only_file = ex_dir / "only.txt"

    if skip_file.exists():
        skips = skip_file.read_text().split()
        if ("mcu:" + mcu in skips or
            "board:" + board in skips or
            "family:" + family in skips):
            return True

    if only_file.exists():
        onlys = only_file.read_text().split()
        if not ("mcu:" + mcu in onlys or
                "board:" + board in onlys or
                "family:" + family in onlys):
            return True

    return False


def build_example(example, board, make_option):
    start_time = time.monotonic()
    flash_size = "-"
    sram_size = "-"

    # succeeded, failed, skipped
    ret = [0, 0, 0]

    make_cmd = "make -j -C examples/{} BOARD={} {}".format(example, board, make_option)

    # Check if board is skipped
    if skip_example(example, board):
        status = SKIPPED
        ret[2] = 1
        print(build_format.format(example, board, status, '-', flash_size, sram_size))
    else:
        #subprocess.run(make_cmd + " clean", shell=True, stdout=subprocess.PIPE, stderr=subprocess.STDOUT)
        build_result = subprocess.run(make_cmd + " all", shell=True, stdout=subprocess.PIPE, stderr=subprocess.STDOUT)

        if build_result.returncode == 0:
            status = SUCCEEDED
            ret[0] = 1
            (flash_size, sram_size) = build_size(make_cmd)
            #subprocess.run(make_cmd + " copy-artifact", shell=True, stdout=subprocess.PIPE, stderr=subprocess.STDOUT)
        else:
            status = FAILED
            ret[1] = 1

        build_duration = time.monotonic() - start_time
        print(build_format.format(example, board, status, "{:.2f}s".format(build_duration), flash_size, sram_size))

        if build_result.returncode != 0:
            print(build_result.stdout.decode("utf-8"))

    return ret


def build_size(make_cmd):
    size_output = subprocess.run(make_cmd + ' size', shell=True, stdout=subprocess.PIPE).stdout.decode("utf-8").splitlines()
    for i, l in enumerate(size_output):
        text_title = 'text	   data	    bss	    dec'
        if text_title in l:
            size_list = size_output[i+1].split('\t')
            flash_size = int(size_list[0])
            sram_size = int(size_list[1]) + int(size_list[2])
            return (flash_size, sram_size)

    return (0, 0)
