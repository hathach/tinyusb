#!/usr/bin/env python3
"""Calculate average size from multiple linker map files."""

import argparse
import glob
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
    import json

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
                        f for f in json_data["files"]
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


def main(argv=None):
    parser = argparse.ArgumentParser(description='Calculate average size from linker map files')
    parser.add_argument('files', nargs='+', help='Path to map file(s) or glob pattern(s)')
    parser.add_argument('-f', '--filter', dest='filters', action='append', default=[],
                        help='Only include object files whose path contains this substring (can be repeated)')
    parser.add_argument('-o', '--out', dest='out', default='metrics',
                        help='Output path basename for JSON and Markdown files (default: metrics)')
    parser.add_argument('-j', '--json', dest='json_out', action='store_true',
                        help='Write JSON output file')
    parser.add_argument('-m', '--markdown', dest='markdown_out', action='store_true',
                        help='Write Markdown output file')
    parser.add_argument('-q', '--quiet', dest='quiet', action='store_true',
                        help='Suppress summary output')
    args = parser.parse_args(argv)

    # Expand glob patterns
    map_files = expand_files(args.files)

    all_json_data = combine_maps(map_files, args.filters)
    json_average = compute_avg(all_json_data)

    if json_average is None:
        print("No valid map files found", file=sys.stderr)
        sys.exit(1)

    if not args.quiet:
        linkermap.print_summary(json_average, False)
    if args.json_out:
        linkermap.write_json(json_average, args.out + '.json')
    if args.markdown_out:
        linkermap.write_markdown(json_average, args.out + '.md')


if __name__ == '__main__':
    main()
