import pathlib

def skip_example(example, board):
    ex_dir = pathlib.Path('examples/') / example
    bsp = pathlib.Path("hw/bsp")

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

    # Find the mcu
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
