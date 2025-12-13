#!/usr/bin/env python3
"""Calculate average sizes from bloaty CSV or TinyUSB metrics JSON outputs."""

import argparse
import csv
import glob
import io
import json
import os
import sys
from collections import defaultdict


def expand_files(file_patterns):
    """Expand file patterns (globs) to list of files.

    Args:
        file_patterns: List of file paths or glob patterns

    Returns:
        List of expanded file paths
    """
    expanded = []
    for pattern in file_patterns:
        if '*' in pattern or '?' in pattern:
            expanded.extend(glob.glob(pattern))
        else:
            expanded.append(pattern)
    return expanded


def parse_bloaty_csv(csv_text, filters=None):
    """Parse bloaty CSV text and return normalized JSON data structure."""

    filters = filters or []
    reader = csv.DictReader(io.StringIO(csv_text))
    size_by_unit = defaultdict(int)
    symbols_by_unit: dict[str, defaultdict[str, int]] = defaultdict(lambda: defaultdict(int))
    sections_by_unit: dict[str, defaultdict[str, int]] = defaultdict(lambda: defaultdict(int))

    for row in reader:
        compile_unit = row.get("compileunits") or row.get("compileunit") or row.get("path")
        if compile_unit is None:
            continue

        if str(compile_unit).upper() == "TOTAL":
            continue

        if filters and not any(filt in compile_unit for filt in filters):
            continue

        try:
            vmsize = int(row.get("vmsize", 0))
        except ValueError:
            continue

        size_by_unit[compile_unit] += vmsize
        symbol_name = row.get("symbols", "")
        if symbol_name:
            symbols_by_unit[compile_unit][symbol_name] += vmsize
        section_name = row.get("sections") or row.get("section")
        if section_name and vmsize:
            sections_by_unit[compile_unit][section_name] += vmsize

    files = []
    for unit_path, total_size in size_by_unit.items():
        symbols = [
            {"name": sym, "size": sz}
            for sym, sz in sorted(symbols_by_unit[unit_path].items(), key=lambda x: x[1], reverse=True)
        ]
        sections = {sec: sz for sec, sz in sections_by_unit[unit_path].items() if sz}
        files.append(
            {
                "file": os.path.basename(unit_path) or unit_path,
                "path": unit_path,
                "size": total_size,
                "total": total_size,
                "symbols": symbols,
                "sections": sections,
            }
        )

    total_all = sum(size_by_unit.values())
    return {"files": files, "TOTAL": total_all}


def combine_files(input_files, filters=None):
    """Combine multiple metrics inputs (bloaty CSV or metrics JSON) into a single data set."""

    filters = filters or []
    all_json_data = {"file_list": [], "data": []}

    for fin in input_files:
        if not os.path.exists(fin):
            print(f"Warning: {fin} not found, skipping", file=sys.stderr)
            continue

        try:
            if fin.endswith(".json"):
                with open(fin, "r", encoding="utf-8") as f:
                    json_data = json.load(f)
                if filters:
                    json_data["files"] = [
                        f
                        for f in json_data.get("files", [])
                        if f.get("path") and any(filt in f["path"] for filt in filters)
                    ]
            elif fin.endswith(".csv"):
                with open(fin, "r", encoding="utf-8") as f:
                    csv_text = f.read()
                json_data = parse_bloaty_csv(csv_text, filters)
            else:
                if fin.endswith(".elf"):
                    print(f"Warning: {fin} is an ELF; please run bloaty with --csv output first. Skipping.",
                          file=sys.stderr)
                else:
                    print(f"Warning: {fin} is not a supported CSV or JSON metrics input. Skipping.",
                          file=sys.stderr)
                continue

            # Drop any fake TOTAL entries that slipped in as files
            json_data["files"] = [
                f for f in json_data.get("files", [])
                if str(f.get("file", "")).upper() != "TOTAL"
            ]

            all_json_data["file_list"].append(fin)
            all_json_data["data"].append(json_data)
        except Exception as e:  # pragma: no cover - defensive
            print(f"Warning: Failed to analyze {fin}: {e}", file=sys.stderr)
            continue

    return all_json_data


def compute_avg(all_json_data):
    """Compute average sizes from combined json_data.

    Args:
        all_json_data: Dictionary with file_list and data from combine_files()

    Returns:
        json_average: Dictionary with averaged size data
    """
    if not all_json_data["data"]:
        return None

    # Merge files with the same 'file' value and compute averages
    file_accumulator = {}  # key: file name, value: {"sizes": [sizes], "totals": [totals], "symbols": {name: [sizes]}, "sections": {name: [sizes]}}

    for json_data in all_json_data["data"]:
        for f in json_data.get("files", []):
            fname = f["file"]
            if fname not in file_accumulator:
                file_accumulator[fname] = {
                    "sizes": [],
                    "totals": [],
                    "path": f.get("path"),
                    "symbols": defaultdict(list),
                    "sections": defaultdict(list),
                }
            size_val = f.get("size", f.get("total", 0))
            file_accumulator[fname]["sizes"].append(size_val)
            file_accumulator[fname]["totals"].append(f.get("total", size_val))
            for sym in f.get("symbols", []):
                name = sym.get("name")
                if name is None:
                    continue
                file_accumulator[fname]["symbols"][name].append(sym.get("size", 0))
            sections_map = f.get("sections") or {}
            for sname, ssize in sections_map.items():
                file_accumulator[fname]["sections"][sname].append(ssize)

    # Build json_average with averaged values
    files_average = []
    for fname, data in file_accumulator.items():
        avg_size = round(sum(data["sizes"]) / len(data["sizes"])) if data["sizes"] else 0
        symbols_avg = []
        for sym_name, sizes in data["symbols"].items():
            if not sizes:
                continue
            symbols_avg.append({"name": sym_name, "size": round(sum(sizes) / len(sizes))})
        symbols_avg.sort(key=lambda x: x["size"], reverse=True)
        sections_avg = {
            sec_name: round(sum(sizes) / len(sizes))
            for sec_name, sizes in data["sections"].items()
            if sizes
        }
        files_average.append(
            {
                "file": fname,
                "path": data["path"],
                "size": avg_size,
                "symbols": symbols_avg,
                "sections": sections_avg,
            }
        )

    totals_list = [d.get("TOTAL") for d in all_json_data["data"] if isinstance(d.get("TOTAL"), (int, float))]
    file_total = sum(f["size"] for f in files_average) if files_average else None
    section_total = sum(sum((f.get("sections") or {}).values()) for f in files_average)

    if section_total:
        total_size = section_total
    elif file_total is not None:
        total_size = file_total if file_total != 0 else 1
    elif totals_list:
        total_size = round(sum(totals_list) / len(totals_list))
    else:
        total_size = 1

    for f in files_average:
        f["percent"] = (f["size"] / total_size) * 100 if total_size else 0
        for sym in f["symbols"]:
            sym["percent"] = (sym["size"] / f["size"]) * 100 if f["size"] else 0

    json_average = {
        "file_list": all_json_data["file_list"],
        "TOTAL": total_size,
        "files": files_average,
    }

    return json_average


def compare_files(base_file, new_file, filters=None):
    """Compare two CSV or JSON inputs and generate difference report."""
    filters = filters or []

    base_avg = compute_avg(combine_files([base_file], filters))
    new_avg = compute_avg(combine_files([new_file], filters))

    if not base_avg or not new_avg:
        return None

    base_files = {f["file"]: f for f in base_avg["files"]}
    new_files = {f["file"]: f for f in new_avg["files"]}
    all_file_names = set(base_files.keys()) | set(new_files.keys())

    comparison_files = []
    for fname in sorted(all_file_names):
        b = base_files.get(fname, {})
        n = new_files.get(fname, {})
        b_size = b.get("size", 0)
        n_size = n.get("size", 0)
        base_sections = b.get("sections") or {}
        new_sections = n.get("sections") or {}

        # Symbol diffs
        b_syms = {s["name"]: s for s in b.get("symbols", [])}
        n_syms = {s["name"]: s for s in n.get("symbols", [])}
        all_syms = set(b_syms.keys()) | set(n_syms.keys())
        symbols = []
        for sym in all_syms:
            sb = b_syms.get(sym, {}).get("size", 0)
            sn = n_syms.get(sym, {}).get("size", 0)
            symbols.append({"name": sym, "base": sb, "new": sn, "diff": sn - sb})
        symbols.sort(key=lambda x: abs(x["diff"]), reverse=True)

        comparison_files.append({
            "file": fname,
            "size": {"base": b_size, "new": n_size, "diff": n_size - b_size},
            "symbols": symbols,
            "sections": {
                name: {
                    "base": base_sections.get(name, 0),
                    "new": new_sections.get(name, 0),
                    "diff": new_sections.get(name, 0) - base_sections.get(name, 0),
                }
                for name in sorted(set(base_sections) | set(new_sections))
            },
        })

    total = {
        "base": base_avg.get("TOTAL", 0),
        "new": new_avg.get("TOTAL", 0),
        "diff": new_avg.get("TOTAL", 0) - base_avg.get("TOTAL", 0),
    }

    return {
        "base_file": base_file,
        "new_file": new_file,
        "total": total,
        "files": comparison_files,
    }


def get_sort_key(sort_order):
    """Get sort key function based on sort order.

    Args:
        sort_order: One of 'size-', 'size+', 'name-', 'name+'

    Returns:
        Tuple of (key_func, reverse)
    """

    def _size_val(entry):
        if isinstance(entry.get('total'), int):
            return entry.get('total', 0)
        if isinstance(entry.get('total'), dict):
            return entry['total'].get('new', 0)
        return entry.get('size', 0)

    if sort_order == 'size-':
        return _size_val, True
    elif sort_order == 'size+':
        return _size_val, False
    elif sort_order == 'name-':
        return lambda x: x.get('file', ''), True
    else:  # name+
        return lambda x: x.get('file', ''), False


def format_diff(base, new, diff):
    """Format a diff value with percentage."""
    if diff == 0:
        return f"{new}"
    if base == 0 or new == 0:
        return f"{base} ➙ {new}"
    pct = (diff / base) * 100
    sign = "+" if diff > 0 else ""
    return f"{base} ➙ {new} ({sign}{diff}, {sign}{pct:.1f}%)"


def write_json_output(json_data, path):
    """Write JSON output with indentation."""

    with open(path, "w", encoding="utf-8") as outf:
        json.dump(json_data, outf, indent=2)


def render_combine_table(json_data, sort_order='name+'):
    """Render averaged sizes as markdown table lines (no title)."""
    files = json_data.get("files", [])
    if not files:
        return ["No entries."]

    key_func, reverse = get_sort_key(sort_order)
    files_sorted = sorted(files, key=key_func, reverse=reverse)

    total_size = json_data.get("TOTAL") or sum(f.get("size", 0) for f in files_sorted)

    pct_strings = [
        f"{(f.get('percent') if f.get('percent') is not None else (f.get('size', 0) / total_size * 100 if total_size else 0)):.1f}%"
        for f in files_sorted]
    pct_width = 6
    size_width = max(len("size"), *(len(str(f.get("size", 0))) for f in files_sorted), len(str(total_size)))
    file_width = max(len("File"), *(len(f.get("file", "")) for f in files_sorted), len("TOTAL"))

    # Build section totals on the fly from file data
    sections_global = defaultdict(int)
    for f in files_sorted:
        for name, size in (f.get("sections") or {}).items():
            sections_global[name] += size
    # Display sections in reverse alphabetical order for stable column layout
    section_names = sorted(sections_global.keys(), reverse=True)
    section_widths = {}
    for name in section_names:
        max_val = max((f.get("sections", {}).get(name, 0) for f in files_sorted), default=0)
        section_widths[name] = max(len(name), len(str(max_val)), 1)

    if not section_names:
        header = f"| {'File':<{file_width}} | {'size':>{size_width}} | {'%':>{pct_width}} |"
        separator = f"| :{'-' * (file_width - 1)} | {'-' * (size_width - 1)}: | {'-' * (pct_width - 1)}: |"
    else:
        header_parts = [f"| {'File':<{file_width}} |"]
        sep_parts = [f"| :{'-' * (file_width - 1)} |"]
        for name in section_names:
            header_parts.append(f" {name:>{section_widths[name]}} |")
            sep_parts.append(f" {'-' * (section_widths[name] - 1)}: |")
        header_parts.append(f" {'size':>{size_width}} | {'%':>{pct_width}} |")
        sep_parts.append(f" {'-' * (size_width - 1)}: | {'-' * (pct_width - 1)}: |")
        header = "".join(header_parts)
        separator = "".join(sep_parts)

    lines = [header, separator]

    for f, pct_str in zip(files_sorted, pct_strings):
        size_val = f.get("size", 0)
        parts = [f"| {f.get('file', ''):<{file_width}} |"]
        if section_names:
            sections_map = f.get("sections") or {}
            for name in section_names:
                parts.append(f" {sections_map.get(name, 0):>{section_widths[name]}} |")
        parts.append(f" {size_val:>{size_width}} | {pct_str:>{pct_width}} |")
        lines.append("".join(parts))

    total_parts = [f"| {'TOTAL':<{file_width}} |"]
    if section_names:
        for name in section_names:
            total_parts.append(f" {sections_global.get(name, 0):>{section_widths[name]}} |")
    total_parts.append(f" {total_size:>{size_width}} | {'100.0%':>{pct_width}} |")
    lines.append("".join(total_parts))
    return lines


def write_combine_markdown(json_data, path, sort_order='name+', title="TinyUSB Average Code Size Metrics"):
    """Write averaged size data to a markdown file."""

    md_lines = [f"# {title}", ""]
    md_lines.extend(render_combine_table(json_data, sort_order))
    md_lines.append("")

    if json_data.get("file_list"):
        md_lines.extend(["<details>", "<summary>Input files</summary>", ""])
        md_lines.extend([f"- {mf}" for mf in json_data["file_list"]])
        md_lines.extend(["", "</details>", ""])

    with open(path, "w", encoding="utf-8") as f:
        f.write("\n".join(md_lines))


def write_compare_markdown(comparison, path, sort_order='size'):
    """Write comparison data to markdown file."""
    md_lines = [
        "# Size Difference Report",
        "",
        "Because TinyUSB code size varies by port and configuration, the metrics below represent the averaged totals across all example builds.",
        "",
        "Note: If there is no change, only one value is shown.",
        "",
    ]

    significant, minor, unchanged = _split_by_significance(comparison["files"], sort_order)

    def render(title, rows, collapsed=False):
        if collapsed:
            md_lines.append(f"<details><summary>{title}</summary>")
            md_lines.append("")
        else:
            md_lines.append(f"## {title}")

        md_lines.extend(render_compare_table(_build_rows(rows, sort_order), include_sum=True))
        md_lines.append("")

        if collapsed:
            md_lines.append("</details>")
            md_lines.append("")

    render("Changes >1% in size", significant)
    render("Changes <1% in size", minor)
    render("No changes", unchanged, collapsed=True)

    with open(path, "w", encoding="utf-8") as f:
        f.write("\n".join(md_lines))


def print_compare_summary(comparison, sort_order='name+'):
    """Print diff report to stdout in table form."""

    files = comparison["files"]

    rows = _build_rows(files, sort_order)
    lines = render_compare_table(rows, include_sum=True)
    for line in lines:
        print(line)


def _build_rows(files, sort_order):
    """Sort files and prepare printable fields."""

    def sort_key(file_row):
        if sort_order == 'size-':
            return abs(file_row["size"]["diff"])
        if sort_order in ('size', 'size+'):
            return abs(file_row["size"]["diff"])
        if sort_order == 'name-':
            return file_row['file']
        return file_row['file']

    reverse = sort_order in ('size-', 'name-')
    files_sorted = sorted(files, key=sort_key, reverse=reverse)

    rows = []
    for f in files_sorted:
        sd = f["size"]
        diff_val = sd['new'] - sd['base']
        if sd['base'] == 0:
            pct_str = "n/a"
        else:
            pct_val = (diff_val / sd['base']) * 100
            pct_str = f"{pct_val:+.1f}%"
        rows.append({
            "file": f['file'],
            "base": sd['base'],
            "new": sd['new'],
            "diff": diff_val,
            "pct": pct_str,
            "sections": f.get("sections", {}),
        })
    return rows


def _split_by_significance(files, sort_order):
    """Split files into >1% changes, <1% changes, and no changes."""

    def is_significant(file_row):
        base = file_row["size"]["base"]
        diff = abs(file_row["size"]["diff"])
        if base == 0:
            return diff != 0
        return (diff / base) * 100 > 1.0

    rows_sorted = sorted(
        files,
        key=lambda f: abs(f["size"]["diff"]) if sort_order.startswith("size") else f["file"],
        reverse=sort_order in ('size-', 'name-'),
    )

    significant = []
    minor = []
    unchanged = []
    for f in rows_sorted:
        if f["size"]["diff"] == 0:
            unchanged.append(f)
        else:
            (significant if is_significant(f) else minor).append(f)

    return significant, minor, unchanged


def render_compare_table(rows, include_sum):
    """Return markdown table lines for given rows."""
    if not rows:
        return ["No entries.", ""]

    # collect section columns (reverse alpha)
    section_names = sorted(
        {name for r in rows for name in (r.get("sections") or {})},
        reverse=True,
    )

    def fmt_abs(val_old, val_new):
        diff = val_new - val_old
        if diff == 0:
            return f"{val_new}"
        sign = "+" if diff > 0 else ""
        return f"{val_old} ➙ {val_new} ({sign}{diff})"

    sum_base = sum(r["base"] for r in rows)
    sum_new = sum(r["new"] for r in rows)
    total_diff = sum_new - sum_base
    total_pct = "n/a" if sum_base == 0 else f"{(total_diff / sum_base) * 100:+.1f}%"

    file_width = max(len("file"), *(len(r["file"]) for r in rows), len("TOTAL"))
    size_width = max(
        len("size"),
        *(len(fmt_abs(r["base"], r["new"])) for r in rows),
        len(fmt_abs(sum_base, sum_new)),
    )
    pct_width = max(len("% diff"), *(len(r["pct"]) for r in rows), len(total_pct))
    section_widths = {}
    for name in section_names:
        max_val_len = 0
        for r in rows:
            sec_entry = (r.get("sections") or {}).get(name, {"base": 0, "new": 0})
            max_val_len = max(max_val_len, len(fmt_abs(sec_entry.get("base", 0), sec_entry.get("new", 0))))
        section_widths[name] = max(len(name), max_val_len, 1)

    header_parts = [f"| {'file':<{file_width}} |"]
    sep_parts = [f"| :{'-' * (file_width - 1)} |"]
    for name in section_names:
        header_parts.append(f" {name:>{section_widths[name]}} |")
        sep_parts.append(f" {'-' * (section_widths[name] - 1)}: |")
    header_parts.append(f" {'size':>{size_width}} | {'% diff':>{pct_width}} |")
    sep_parts.append(f" {'-' * (size_width - 1)}: | {'-' * (pct_width - 1)}: |")
    header = "".join(header_parts)
    separator = "".join(sep_parts)

    lines = [header, separator]

    for r in rows:
        parts = [f"| {r['file']:<{file_width}} |"]
        sections_map = r.get("sections") or {}
        for name in section_names:
            sec_entry = sections_map.get(name, {"base": 0, "new": 0})
            parts.append(f" {fmt_abs(sec_entry.get('base', 0), sec_entry.get('new', 0)):>{section_widths[name]}} |")
        parts.append(f" {fmt_abs(r['base'], r['new']):>{size_width}} | {r['pct']:>{pct_width}} |")
        lines.append("".join(parts))

    if include_sum:
        total_parts = [f"| {'TOTAL':<{file_width}} |"]
        for name in section_names:
            total_base = sum((r.get("sections") or {}).get(name, {}).get("base", 0) for r in rows)
            total_new = sum((r.get("sections") or {}).get(name, {}).get("new", 0) for r in rows)
            total_parts.append(f" {fmt_abs(total_base, total_new):>{section_widths[name]}} |")
        total_parts.append(f" {fmt_abs(sum_base, sum_new):>{size_width}} | {total_pct:>{pct_width}} |")
        lines.append("".join(total_parts))
    return lines


def cmd_combine(args):
    """Handle combine subcommand."""
    input_files = expand_files(args.files)
    all_json_data = combine_files(input_files, args.filters)
    json_average = compute_avg(all_json_data)

    if json_average is None:
        print("No valid map files found", file=sys.stderr)
        sys.exit(1)

    if not args.quiet:
        for line in render_combine_table(json_average, sort_order=args.sort):
            print(line)
    if args.json_out:
        write_json_output(json_average, args.out + '.json')
    if args.markdown_out:
        write_combine_markdown(json_average, args.out + '.md', sort_order=args.sort,
                               title="TinyUSB Average Code Size Metrics")


def cmd_compare(args):
    """Handle compare subcommand."""
    comparison = compare_files(args.base, args.new, args.filters)

    if comparison is None:
        print("Failed to compare files", file=sys.stderr)
        sys.exit(1)

    if not args.quiet:
        print_compare_summary(comparison, args.sort)
    if args.markdown_out:
        write_compare_markdown(comparison, args.out + '.md', args.sort)
        if not args.quiet:
            print(f"Comparison written to {args.out}.md")


def main(argv=None):
    parser = argparse.ArgumentParser(description='Code size metrics tool')
    subparsers = parser.add_subparsers(dest='command', required=True, help='Available commands')

    # Combine subcommand
    combine_parser = subparsers.add_parser('combine', help='Combine and average bloaty CSV outputs or metrics JSON files')
    combine_parser.add_argument('files', nargs='+',
                                help='Path to bloaty CSV output or TinyUSB metrics JSON file(s) (including linkermap-generated) or glob pattern(s)')
    combine_parser.add_argument('-f', '--filter', dest='filters', action='append', default=[],
                                help='Only include compile units whose path contains this substring (can be repeated)')
    combine_parser.add_argument('-o', '--out', dest='out', default='metrics',
                                help='Output path basename for JSON and Markdown files (default: metrics)')
    combine_parser.add_argument('-j', '--json', dest='json_out', action='store_true',
                                help='Write JSON output file')
    combine_parser.add_argument('-m', '--markdown', dest='markdown_out', action='store_true',
                                help='Write Markdown output file')
    combine_parser.add_argument('-q', '--quiet', dest='quiet', action='store_true',
                                help='Suppress summary output')
    combine_parser.add_argument('-S', '--sort', dest='sort', default='size-',
                                choices=['size', 'size-', 'size+', 'name', 'name-', 'name+'],
                                help='Sort order: size/size- (descending), size+ (ascending), name/name+ (ascending), name- (descending). Default: size-')

    # Compare subcommand
    compare_parser = subparsers.add_parser('compare', help='Compare two metrics inputs (bloaty CSV or metrics JSON)')
    compare_parser.add_argument('base', help='Base CSV/metrics JSON file')
    compare_parser.add_argument('new', help='New CSV/metrics JSON file')
    compare_parser.add_argument('-f', '--filter', dest='filters', action='append', default=[],
                                help='Only include compile units whose path contains this substring (can be repeated)')
    compare_parser.add_argument('-o', '--out', dest='out', default='metrics_compare',
                                help='Output path basename for Markdown/JSON files (default: metrics_compare)')
    compare_parser.add_argument('-m', '--markdown', dest='markdown_out', action='store_true',
                                help='Write Markdown output file')
    compare_parser.add_argument('-S', '--sort', dest='sort', default='name+',
                                choices=['size', 'size-', 'size+', 'name', 'name-', 'name+'],
                                help='Sort order: size/size- (descending), size+ (ascending), name/name+ (ascending), name- (descending). Default: name+')
    compare_parser.add_argument('-q', '--quiet', dest='quiet', action='store_true',
                                help='Suppress stdout summary output')

    args = parser.parse_args(argv)

    if args.command == 'combine':
        cmd_combine(args)
    elif args.command == 'compare':
        cmd_compare(args)


if __name__ == '__main__':
    main()
