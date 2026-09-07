#!/usr/bin/env python3
import json
import re
import pandas as pd
from tabulate import tabulate
from pathlib import Path
from get_deps import deps_all

# TOP is tinyusb root dir
TOP = Path(__file__).parent.parent.resolve()


# -----------------------------------------
# Dependencies
# -----------------------------------------
def gen_deps_doc():
    deps_rst = Path(TOP) / "docs/reference/dependencies.rst"
    df = pd.DataFrame.from_dict(deps_all, orient='index', columns=['Repo', 'Commit', 'Required by'])
    df = df[['Repo', 'Commit', 'Required by']].sort_index()
    df = df.rename_axis("Local Path")

    outstr = f"""\
************
Dependencies
************

MCU low-level peripheral drivers and external libraries for building TinyUSB examples

{tabulate(df, headers="keys", tablefmt='rst')}
"""

    with deps_rst.open('w') as f:
        f.write(outstr)


# -----------------------------------------
# Dependencies
# -----------------------------------------
def extract_metadata(file_path):
    metadata = {}
    try:
        with open(file_path, 'r') as file:
            content = file.read()
            # Match metadata block
            match = re.search(r'/\*\s*metadata:(.*?)\*/', content, re.DOTALL)
            if match:
                block = match.group(1)
                # Extract key-value pairs
                for line in block.splitlines():
                    key_value = re.match(r'\s*(\w+):\s*(.+)', line)
                    if key_value:
                        key, value = key_value.groups()
                        metadata[key] = value.strip()
    except FileNotFoundError:
        pass
    return metadata


def gen_boards_doc():
    # 'Manufacturer' : { 'Board' }
    vendor_data = {}
    # 'Board' : [ 'Name', 'Family', 'url', 'note' ]
    all_boards = {}
    #  extract metadata from family.c
    for family_dir in sorted((Path(TOP) / "hw/bsp").iterdir()):
        if family_dir.is_dir():
            family_c = family_dir / "family.c"
            if not family_c.exists():
                family_c = family_dir / "boards/family.c"
            f_meta = extract_metadata(family_c)
            if not f_meta:
                continue
            manuf = f_meta.get('manufacturer', '')
            if manuf not in vendor_data:
                vendor_data[manuf] = {}
            # extract metadata from board.h
            for board_dir in sorted((family_dir / "boards").iterdir()):
                if board_dir.is_dir():
                    b_meta = extract_metadata(board_dir / "board.h")
                    if not b_meta:
                        continue
                    b_entry = [
                        b_meta.get('name', ''),
                        family_dir.name,
                        b_meta.get('url', ''),
                        b_meta.get('note', '')
                    ]
                    vendor_data[manuf][board_dir.name] = b_entry
    boards_rst = Path(TOP) / "docs/reference/boards.rst"
    with boards_rst.open('w') as f:
        title = f"""\
****************
Supported Boards
****************

The board support code is only used for self-contained examples and testing. It is not used when TinyUSB is part of a larger project.
It is responsible for getting the MCU started and the USB peripheral clocked with minimal of on-board devices

-  One LED : for status
-  One Button : to get input from user
-  One UART : needed for logging with LOGGER=uart, maybe required for host/dual examples

Following boards are supported"""
        f.write(title)
        for manuf, boards in sorted(vendor_data.items()):
            f.write(f"\n\n{manuf}\n")
            f.write(f"{'-' * len(manuf)}\n\n")
            df = pd.DataFrame.from_dict(boards, orient='index', columns=['Name', 'Family', 'URL', 'Note'])
            df = df.rename_axis("Board")
            f.write(tabulate(df, headers="keys", tablefmt='rst'))


# -----------------------------------------
# HIL rig rosters
# -----------------------------------------
def hil_cell(text):
    """A '|' in free-form roster text would silently split the markdown row."""
    return ' '.join((text or '').split()).replace('|', '\\|')


def hil_rows(boards):
    rows = []
    for b in boards:
        tests = b.get('tests', {})
        if 'only' in tests:
            roles = sorted({t.split('/')[0] for t in tests['only']})
        else:
            roles = [r for r in ('device', 'host', 'dual') if tests.get(r)]
        rows.append([
            b['name'],
            ', '.join(roles),
            b.get('flasher', {}).get('name', ''),
            hil_cell(', '.join(v['name'] for v in b.get('variant') or [])),
            hil_cell(b.get('comment') or tests.get('comment')),
        ])
    return rows


def gen_hil_boards_doc():
    tinyusb = json.loads((Path(TOP) / "test/hil/tinyusb.json").read_text())
    hfp = json.loads((Path(TOP) / "test/hil/hfp.json").read_text())
    sections = [
        ("ci rig", "test/hil/tinyusb.json", tinyusb.get('boards', [])),
        ("hfp rig", "test/hil/hfp.json", hfp.get('boards', [])),
    ]
    headers = ['Board', 'Roles', 'Flasher', 'Variants', 'Note']

    out = ["<!-- Generated by tools/gen_doc.py - do not edit. -->", ""]
    for title, src, boards in sections:
        if not boards:
            continue
        out.append(f"### {title}")
        out.append("")
        out.append(f"{len(boards)} boards, from `{src}`.")
        out.append("")
        out.append(tabulate(hil_rows(boards), headers=headers, tablefmt='github'))
        out.append("")

    hil_md = Path(TOP) / "docs/reference/hil_boards.md"
    hil_md.write_text('\n'.join(out))


# -----------------------------------------
# Main
# -----------------------------------------
if __name__ == "__main__":
    gen_deps_doc()
    gen_boards_doc()
    gen_hil_boards_doc()
