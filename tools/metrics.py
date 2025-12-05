#!/usr/bin/env python3
"""Calculate average size from multiple linker map files."""

import argparse
import glob
import json
import sys
import os

# Add linkermap module to path
sys.path.insert(0, os.path.join(os.path.dirname(__file__), 'linkermap'))
import linkermap


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


def combine_maps(map_files, filters=None):
    """Combine multiple map files into a list of json_data.

    Args:
        map_files: List of paths to linker map files or JSON files
        filters: List of path substrings to filter object files (default: [])

    Returns:
        all_json_data: Dictionary with mapfiles list and data from each map file
    """
    filters = filters or []
    all_json_data = {"mapfiles": [], "data": []}

    for map_file in map_files:
        if not os.path.exists(map_file):
            print(f"Warning: {map_file} not found, skipping", file=sys.stderr)
            continue

        try:
            if map_file.endswith('.json'):
                with open(map_file, 'r', encoding='utf-8') as f:
                    json_data = json.load(f)
                # Apply path filters to JSON data
                if filters:
                    filtered_files = [
                        f for f in json_data.get("files", [])
                        if f.get("path") and any(filt in f["path"] for filt in filters)
                    ]
                    json_data["files"] = filtered_files
            else:
                json_data = linkermap.analyze_map(map_file, filters=filters)
            all_json_data["mapfiles"].append(map_file)
            all_json_data["data"].append(json_data)
        except Exception as e:
            print(f"Warning: Failed to analyze {map_file}: {e}", file=sys.stderr)
            continue

    return all_json_data


def compute_avg(all_json_data):
    """Compute average sizes from combined json_data.

    Args:
        all_json_data: Dictionary with mapfiles and data from combine_maps()

    Returns:
        json_average: Dictionary with averaged size data
    """
    if not all_json_data["data"]:
        return None

    # Collect all sections preserving order
    all_sections = []
    for json_data in all_json_data["data"]:
        for s in json_data["sections"]:
            if s not in all_sections:
                all_sections.append(s)

    # Merge files with the same 'file' value and compute averages
    file_accumulator = {}  # key: file name, value: {"sections": {section: [sizes]}, "totals": [totals]}

    for json_data in all_json_data["data"]:
        for f in json_data["files"]:
            fname = f["file"]
            if fname not in file_accumulator:
                file_accumulator[fname] = {"sections": {}, "totals": [], "path": f.get("path")}
            file_accumulator[fname]["totals"].append(f["total"])
            for section, size in f["sections"].items():
                if section in file_accumulator[fname]["sections"]:
                    file_accumulator[fname]["sections"][section].append(size)
                else:
                    file_accumulator[fname]["sections"][section] = [size]

    # Build json_average with averaged values
    files_average = []
    for fname, data in file_accumulator.items():
        avg_total = round(sum(data["totals"]) / len(data["totals"]))
        avg_sections = {}
        for section, sizes in data["sections"].items():
            avg_sections[section] = round(sum(sizes) / len(sizes))
        files_average.append({
            "file": fname,
            "path": data["path"],
            "sections": avg_sections,
            "total": avg_total
        })

    json_average = {
        "mapfiles": all_json_data["mapfiles"],
        "sections": all_sections,
        "files": files_average
    }

    return json_average


def compare_maps(base_file, new_file, filters=None):
    """Compare two map/json files and generate difference report.

    Args:
        base_file: Path to base map/json file
        new_file: Path to new map/json file
        filters: List of path substrings to filter object files

    Returns:
        Dictionary with comparison data
    """
    filters = filters or []

    # Load both files
    base_data = combine_maps([base_file], filters)
    new_data = combine_maps([new_file], filters)

    if not base_data["data"] or not new_data["data"]:
        return None

    base_avg = compute_avg(base_data)
    new_avg = compute_avg(new_data)

    if not base_avg or not new_avg:
        return None

    # Collect all sections from both
    all_sections = list(base_avg["sections"])
    for s in new_avg["sections"]:
        if s not in all_sections:
            all_sections.append(s)

    # Build file lookup
    base_files = {f["file"]: f for f in base_avg["files"]}
    new_files = {f["file"]: f for f in new_avg["files"]}

    # Get all file names
    all_file_names = set(base_files.keys()) | set(new_files.keys())

    # Build comparison data
    comparison = []
    for fname in sorted(all_file_names):
        base_f = base_files.get(fname)
        new_f = new_files.get(fname)

        row = {"file": fname, "sections": {}, "total": {}}

        for section in all_sections:
            base_val = base_f["sections"].get(section, 0) if base_f else 0
            new_val = new_f["sections"].get(section, 0) if new_f else 0
            row["sections"][section] = {"base": base_val, "new": new_val, "diff": new_val - base_val}

        base_total = base_f["total"] if base_f else 0
        new_total = new_f["total"] if new_f else 0
        row["total"] = {"base": base_total, "new": new_total, "diff": new_total - base_total}

        comparison.append(row)

    return {
        "base_file": base_file,
        "new_file": new_file,
        "sections": all_sections,
        "files": comparison
    }


def format_diff(base, new, diff):
    """Format a diff value with percentage."""
    if diff == 0:
        return f"{new}"
    if base == 0 or new == 0:
        return f"{base} ➙ {new}"
    pct = (diff / base) * 100
    sign = "+" if diff > 0 else ""
    return f"{base} ➙ {new} ({sign}{diff}, {sign}{pct:.1f}%)"


def get_sort_key(sort_order):
    """Get sort key function based on sort order.

    Args:
        sort_order: One of 'size-', 'size+', 'name-', 'name+'

    Returns:
        Tuple of (key_func, reverse)
    """
    if sort_order == 'size-':
        return lambda x: x.get('total', 0) if isinstance(x.get('total'), int) else x['total']['new'], True
    elif sort_order == 'size+':
        return lambda x: x.get('total', 0) if isinstance(x.get('total'), int) else x['total']['new'], False
    elif sort_order == 'name-':
        return lambda x: x.get('file', ''), True
    else:  # name+
        return lambda x: x.get('file', ''), False


def write_compare_markdown(comparison, path, sort_order='size'):
    """Write comparison data to markdown file."""
    sections = comparison["sections"]

    md_lines = [
        "# Size Difference Report",
        "",
        "Because TinyUSB code size varies by port and configuration, the metrics below represent the averaged totals across all example builds."
        "",
        "Note: If there is no change, only one value is shown.",
        "",
    ]

    # Build header
    header = "| File |"
    separator = "|:-----|"
    for s in sections:
        header += f" {s} |"
        separator += "-----:|"
    header += " Total |"
    separator += "------:|"

    def is_significant(file_row):
        for s in sections:
            sd = file_row["sections"][s]
            diff = abs(sd["diff"])
            base = sd["base"]
            if base == 0:
                if diff != 0:
                    return True
            else:
                if (diff / base) * 100 > 1.0:
                    return True
        return False

    # Sort files based on sort_order
    if sort_order == 'size-':
        key_func = lambda x: abs(x["total"]["diff"])
        reverse = True
    elif sort_order in ('size', 'size+'):
        key_func = lambda x: abs(x["total"]["diff"])
        reverse = False
    elif sort_order == 'name-':
        key_func = lambda x: x['file']
        reverse = True
    else:  # name or name+
        key_func = lambda x: x['file']
        reverse = False
    sorted_files = sorted(comparison["files"], key=key_func, reverse=reverse)

    significant = []
    minor = []
    for f in sorted_files:
        # Skip files with no changes
        if f["total"]["diff"] == 0 and all(f["sections"][s]["diff"] == 0 for s in sections):
            continue
        (significant if is_significant(f) else minor).append(f)

    def render_table(title, rows):
        md_lines.append(f"## {title}")
        if not rows:
            md_lines.append("No entries.")
            md_lines.append("")
            return

        md_lines.append(header)
        md_lines.append(separator)

        sum_base = {s: 0 for s in sections}
        sum_base["total"] = 0
        sum_new = {s: 0 for s in sections}
        sum_new["total"] = 0

        for f in rows:
            row = f"| {f['file']} |"
            for s in sections:
                sd = f["sections"][s]
                sum_base[s] += sd["base"]
                sum_new[s] += sd["new"]
                row += f" {format_diff(sd['base'], sd['new'], sd['diff'])} |"

            td = f["total"]
            sum_base["total"] += td["base"]
            sum_new["total"] += td["new"]
            row += f" {format_diff(td['base'], td['new'], td['diff'])} |"

            md_lines.append(row)

        # Add sum row
        sum_row = "| **SUM** |"
        for s in sections:
            diff = sum_new[s] - sum_base[s]
            sum_row += f" {format_diff(sum_base[s], sum_new[s], diff)} |"
        total_diff = sum_new["total"] - sum_base["total"]
        sum_row += f" {format_diff(sum_base['total'], sum_new['total'], total_diff)} |"
        md_lines.append(sum_row)
        md_lines.append("")

    render_table("Changes >1% in any section", significant)
    render_table("Changes <1% in all sections", minor)

    with open(path, "w", encoding="utf-8") as f:
        f.write("\n".join(md_lines))


def cmd_combine(args):
    """Handle combine subcommand."""
    map_files = expand_files(args.files)
    all_json_data = combine_maps(map_files, args.filters)
    json_average = compute_avg(all_json_data)

    if json_average is None:
        print("No valid map files found", file=sys.stderr)
        sys.exit(1)

    if not args.quiet:
        linkermap.print_summary(json_average, False, args.sort)
    if args.json_out:
        linkermap.write_json(json_average, args.out + '.json')
    if args.markdown_out:
        linkermap.write_markdown(json_average, args.out + '.md', sort_opt=args.sort,
                                 title="TinyUSB Average Code Size Metrics")


def cmd_compare(args):
    """Handle compare subcommand."""
    comparison = compare_maps(args.base, args.new, args.filters)

    if comparison is None:
        print("Failed to compare files", file=sys.stderr)
        sys.exit(1)

    write_compare_markdown(comparison, args.out + '.md', args.sort)
    print(f"Comparison written to {args.out}.md")


def main(argv=None):
    parser = argparse.ArgumentParser(description='Code size metrics tool')
    subparsers = parser.add_subparsers(dest='command', required=True, help='Available commands')

    # Combine subcommand
    combine_parser = subparsers.add_parser('combine', help='Combine and average multiple map files')
    combine_parser.add_argument('files', nargs='+', help='Path to map file(s) or glob pattern(s)')
    combine_parser.add_argument('-f', '--filter', dest='filters', action='append', default=[],
                                help='Only include object files whose path contains this substring (can be repeated)')
    combine_parser.add_argument('-o', '--out', dest='out', default='metrics',
                                help='Output path basename for JSON and Markdown files (default: metrics)')
    combine_parser.add_argument('-j', '--json', dest='json_out', action='store_true',
                                help='Write JSON output file')
    combine_parser.add_argument('-m', '--markdown', dest='markdown_out', action='store_true',
                                help='Write Markdown output file')
    combine_parser.add_argument('-q', '--quiet', dest='quiet', action='store_true',
                                help='Suppress summary output')
    combine_parser.add_argument('-S', '--sort', dest='sort', default='name+',
                                choices=['size', 'size-', 'size+', 'name', 'name-', 'name+'],
                                help='Sort order: size/size- (descending), size+ (ascending), name/name+ (ascending), name- (descending). Default: name+')

    # Compare subcommand
    compare_parser = subparsers.add_parser('compare', help='Compare two map files')
    compare_parser.add_argument('base', help='Base map/json file')
    compare_parser.add_argument('new', help='New map/json file')
    compare_parser.add_argument('-f', '--filter', dest='filters', action='append', default=[],
                                help='Only include object files whose path contains this substring (can be repeated)')
    compare_parser.add_argument('-o', '--out', dest='out', default='metrics_compare',
                                help='Output path basename for Markdown file (default: metrics_compare)')
    compare_parser.add_argument('-S', '--sort', dest='sort', default='name+',
                                choices=['size', 'size-', 'size+', 'name', 'name-', 'name+'],
                                help='Sort order: size/size- (descending), size+ (ascending), name/name+ (ascending), name- (descending). Default: name+')

    args = parser.parse_args(argv)

    if args.command == 'combine':
        cmd_combine(args)
    elif args.command == 'compare':
        cmd_compare(args)


if __name__ == '__main__':
    main()
