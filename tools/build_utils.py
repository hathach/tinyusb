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

        # family CMake
        family_mk = family_dir / "family.cmake"

        # family.mk
        if not family_mk.exists():
            family_mk = family_dir / "family.mk"

        mk_contents = family_mk.read_text()

    # Find the mcu, first in family mk then board mk
    if "CFG_TUSB_MCU=OPT_MCU_" not in mk_contents:
        board_mk = board_dir / "board.cmake"
        if not board_mk.exists():
            board_mk = board_dir / "board.mk"

        mk_contents = board_mk.read_text()

    for token in mk_contents.split():
        if "CFG_TUSB_MCU=OPT_MCU_" in token:
            # Strip " because cmake files has them.
            token = token.strip("\"")
            _, opt_mcu = token.split("=")
            mcu = opt_mcu[len("OPT_MCU_"):]

    # Skip all OPT_MCU_NONE these are WIP port
    if mcu == "NONE":
        return True

    skip_file = ex_dir / "skip.txt"
    only_file = ex_dir / "only.txt"

    if skip_file.exists() and only_file.exists():
        raise RuntimeError("Only have a skip or only file. Not both.")
    elif skip_file.exists():
        skips = skip_file.read_text().split()
        return ("mcu:" + mcu in skips or
                "board:" + board in skips or
                "family:" + family in skips)
    elif only_file.exists():
        onlys = only_file.read_text().split()
        return not ("mcu:" + mcu in onlys or
                    "board:" + board in onlys or
                    "family:" + family in onlys)

    return False


def build_example(example, board, make_option):
    start_time = time.monotonic()
    flash_size = "-"
    sram_size = "-"

    # succeeded, failed, skipped
    ret = [0, 0, 0]

    # Check if board is skipped
    if skip_example(example, board):
        status = SKIPPED
        ret[2] = 1
        print(build_format.format(example, board, status, '-', flash_size, sram_size))
    else:
        build_result = subprocess.run("make -j -C examples/{} BOARD={} {} all".format(example, board, make_option), shell=True,
                                      stdout=subprocess.PIPE, stderr=subprocess.STDOUT)

        if build_result.returncode == 0:
            status = SUCCEEDED
            ret[0] = 1
            (flash_size, sram_size) = build_size(example, board)
            subprocess.run("make -j -C examples/{} BOARD={} {} copy-artifact".format(example, board, make_option), shell=True,
                           stdout=subprocess.PIPE, stderr=subprocess.STDOUT)
        else:
            status = FAILED
            ret[1] = 1

        build_duration = time.monotonic() - start_time
        print(build_format.format(example, board, status, "{:.2f}s".format(build_duration), flash_size, sram_size))

        if build_result.returncode != 0:
            print(build_result.stdout.decode("utf-8"))

    return ret


def build_size(example, board):
    elf_file = 'examples/{}/_build/{}/*.elf'.format(example, board)
    size_output = subprocess.run('size {}'.format(elf_file), shell=True, stdout=subprocess.PIPE).stdout.decode("utf-8")
    size_list = size_output.split('\n')[1].split('\t')
    flash_size = int(size_list[0])
    sram_size = int(size_list[1]) + int(size_list[2])
    return (flash_size, sram_size)
