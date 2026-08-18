#!/usr/bin/env python3
"""Search the Calibre library by metadata and print matching document paths.

Usage: search.py KEYWORD [KEYWORD...]        all keywords must match (AND)
       search.py --any KEYWORD [KEYWORD...]  any keyword matches (OR)

Matches title, authors, tags, series, publisher, description and stored
filename, and prints the exact path to read, best match first.

Exit 0 matched, 1 nothing matched, 2 bad usage or no library.
"""
import glob
import os
import sqlite3
import sys
import unicodedata
import urllib.parse

LIB = os.path.realpath(os.path.expanduser(os.environ.get("CALIBRE_LIBRARY") or "~/Documents/calibre-library"))
DB = os.path.join(LIB, "metadata.db")
LIMIT = 40

QUERY = """
SELECT b.id, b.title, b.path,
       (SELECT group_concat(a.name, ', ') FROM authors a
          JOIN books_authors_link l ON l.author = a.id WHERE l.book = b.id),
       (SELECT group_concat(t.name, ', ') FROM tags t
          JOIN books_tags_link l ON l.tag = t.id WHERE l.book = b.id),
       (SELECT group_concat(s.name, ', ') FROM series s
          JOIN books_series_link l ON l.series = s.id WHERE l.book = b.id),
       (SELECT group_concat(p.name, ', ') FROM publishers p
          JOIN books_publishers_link l ON l.publisher = p.id WHERE l.book = b.id),
       (SELECT c.text FROM comments c WHERE c.book = b.id),
       (SELECT group_concat(d.format || '/' || d.name, char(10)) FROM data d WHERE d.book = b.id)
FROM books b
"""

_authors = None


def norm(s):
    # NFKC + casefold so MICRO SIGN/GREEK MU, curly quotes and dashes compare equal.
    return unicodedata.normalize("NFKC", s).casefold()


def resolve(bid, path, fmt, name):
    """Absolute path of one format row, or None if the file is not on disk.

    Calibre renames `<author>/<title> (<id>)` when metadata is edited and leaves
    the old directory behind, so on a miss retry by the stable book id.
    """
    ext = "." + fmt.lower()
    exact = os.path.join(LIB, path, name + ext)
    if os.path.exists(exact):
        return exact
    global _authors
    if _authors is None:
        _authors = {}
        for d in os.listdir(LIB):  # case-only duplicates exist on a case-sensitive mount
            _authors.setdefault(d.lower(), []).append(d)
    for author in _authors.get(path.split("/")[0].lower(), ()):
        for d in glob.glob(os.path.join(glob.escape(os.path.join(LIB, author)), "* (%d)" % bid)):
            for f in sorted(glob.glob(os.path.join(glob.escape(d), "*" + ext))):
                return f
    return None


def main(argv):
    match_any = "--any" in argv
    keywords = [norm(k) for k in argv if k != "--any"]
    if not keywords:
        print(__doc__, file=sys.stderr)
        return 2

    if not os.path.exists(DB):
        print(f"no Calibre database at {DB}", file=sys.stderr)
        return 2

    db = sqlite3.connect("file:" + urllib.parse.quote(DB) + "?mode=ro", uri=True)
    hits = []
    for bid, title, path, authors, tags, series, publisher, comments, files in db.execute(QUERY):
        entries = [e.split("/", 1) for e in (files or "").split("\n") if e]
        hay = norm(" ".join(x for x in (title, authors, tags, series, publisher, comments) if x)
                   + " " + " ".join(n for _, n in entries))
        found = sum(k in hay for k in keywords)
        if not found or (not match_any and found < len(keywords)):
            continue
        in_title = sum(k in norm(title) for k in keywords)
        hits.append((-found, -in_title, title, authors, tags, bid, path, entries))

    if not hits:
        print("no match")
        return 1

    hits.sort(key=lambda h: h[:3])  # authors/tags may be None and are not comparable
    print(f"{len(hits)} book(s)" + (f", showing the {LIMIT} best" if len(hits) > LIMIT else ""))
    for _, _, title, authors, tags, bid, path, entries in hits[:LIMIT]:
        print(f"\n{title}" + (f"  [{authors}]" if authors else "") + (f"  tags: {tags}" if tags else ""))
        if not entries:
            print("  (no file in this library)")
        for fmt, name in entries:
            p = resolve(bid, path, fmt, name)
            print(f"  {fmt} {p}" if p else f"  {fmt} MISSING (library mid-sync or file deleted)")
    return 0


if __name__ == "__main__":
    try:
        sys.exit(main(sys.argv[1:]))
    except BrokenPipeError:
        os.dup2(os.open(os.devnull, os.O_WRONLY), sys.stdout.fileno())
        sys.exit(0)
