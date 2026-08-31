#!/usr/bin/env python3
"""Sync README.rst sponsor sections and sponsor/priority labels from GitHub Sponsors.

Reads live sponsorship data (including private sponsors) via `gh api graphql`,
rewrites the four marker-delimited blocks in README.rst, and applies triage
labels to OPEN issues / PRs / discussions authored by entitled logins.

Every run plans first and prints what it would change, then asks before
touching anything. --yes skips the prompt (needed when stdin is not a tty),
--dry-run stops after the preview.

State lives in state.json next to this file (gitignored): the highest ticket
number already scanned, so later runs skip old tickets.
"""

import argparse
import difflib
import hashlib
import json
import re
import subprocess
import tempfile
import sys
from datetime import date
from pathlib import Path

HERE = Path(__file__).resolve().parent
REPO = HERE.parents[2]
README = REPO / "README.rst"
CONFIG = HERE / "config.json"
IGNORE_WORDS = REPO / "tools" / "codespell" / "ignore-words.txt"
STATE = HERE / "state.json"

OWNER, NAME = "hathach", "tinyusb"

L_SPONSOR = "Sponsor \U0001f496"
L_PRIO = "Prio \U0001f4cc"
L_PRIO_TOP = "Prio Top \U0001f6a8"
L_ADAFRUIT = "Adafruit \U0001f338"

# Label rules mirror .github/workflows/labeler.yml, which applies the same set when
# a ticket is opened. Keep the two in step or a ticket's labels start depending on
# which mechanism happened to touch it.
TIER_LABELS = {
    "QWORD": {L_SPONSOR, L_PRIO_TOP},
    "DWORD": {L_SPONSOR, L_PRIO_TOP},
    "WORD": {L_SPONSOR, L_PRIO},
    "BYTE": {L_SPONSOR},
    "BIT": {L_SPONSOR},
}
ADAFRUIT_LABELS = {L_ADAFRUIT, L_SPONSOR, L_PRIO_TOP}

# tier key -> (min $/month, README marker, placeholder when empty, avatar px)
TIERS = [
    ("QWORD", 512, "QWORD-SPONSORS", "*No QWORD sponsors yet — be the first!*", 120),
    ("DWORD", 128, "DWORD-BACKERS", "*No backers yet — be the first!*", 80),
    ("WORD", 32, "WORD-SUPPORTERS", "*No supporters yet — be the first!*", 40),
    ("BYTE", 8, "BYTE-THANKS", "*No names listed yet — be the first!*", 0),
    ("BIT", 2, None, None, 0),  # no README listing
]


def gh(*args, **kw):
  out = subprocess.run(["gh", *args], capture_output=True, text=True, **kw)
  if out.returncode:
    sys.exit(f"gh {' '.join(args[:2])} failed:\n{out.stderr.strip()}")
  return out.stdout


def graphql(query, **variables):
  args = ["api", "graphql", "-f", f"query={query}"]
  for k, v in variables.items():
    args += ["-F", f"{k}={'null' if v is None else v}"]
  data = json.loads(gh(*args))
  if "errors" in data:
    sys.exit("GraphQL errors:\n" + json.dumps(data["errors"], indent=2))
  return data["data"]


# ---------------------------------------------------------------- sponsors

SPONSOR_Q = """
query($cursor:String){ viewer{ login sponsorshipsAsMaintainer(first:100, includePrivate:true, activeOnly:true, after:$cursor){
  pageInfo{hasNextPage endCursor}
  nodes{ privacyLevel createdAt tier{monthlyPriceInDollars}
         sponsorEntity{ __typename ... on User{login name} ... on Organization{login name} } } } } }
"""


def tier_of(dollars):
  for key, floor, *_ in TIERS:
    if dollars >= floor:
      return key
  return "BIT"  # below the lowest published tier, but still a sponsor


def fetch_sponsors():
  """Active sponsorships, oldest first (chronological README order)."""
  sponsors, cursor = [], None
  while True:
    viewer = graphql(SPONSOR_Q, cursor=cursor)["viewer"]
    if not viewer:
      sys.exit("gh is authenticated with a token that has no user identity - "
               "it cannot see sponsorships")
    if viewer["login"].lower() != OWNER.lower():
      sys.exit(f"gh is authenticated as {viewer['login']}, not {OWNER} - "
               f"its sponsors are not the ones this README lists")
    page = viewer["sponsorshipsAsMaintainer"]
    for n in page["nodes"]:
      entity = n["sponsorEntity"]
      if not entity:  # private sponsor we somehow cannot resolve
        continue
      tier = tier_of((n["tier"] or {}).get("monthlyPriceInDollars") or 0)
      sponsors.append({
          "login": entity["login"],
          "name": (entity["name"] or "").strip() or entity["login"],
          "is_org": entity["__typename"] == "Organization",
          "private": n["privacyLevel"] == "PRIVATE",
          "since": n["createdAt"],
          "tier": tier,
      })
    if not page["pageInfo"]["hasNextPage"]:
      break
    cursor = page["pageInfo"]["endCursor"]
  sponsors.sort(key=lambda s: s["since"])
  return sponsors


# ------------------------------------------------------------------ README

def mask(login):
  """Private sponsor display name: first 3 chars, rest hidden behind a fixed
  4 stars so the real length does not leak. A login of 3 chars or fewer has no
  `rest` to hide, so it is withheld entirely."""
  return login[:3] + "****" if len(login) > 3 else "a private supporter"


def rst_escape(text):
  """A GitHub display name is free-form: backticks/angle brackets would break out
  of the inline-link markup and could point the link anywhere."""
  return re.sub(r"([*`<>|_\\])", r"\\\1", text)


def render(sponsors, size, use_company_name, seen):
  """One line of comma-separated entries, plus any avatar substitution defs."""
  entries, defs = [], []
  for s in sponsors:
    if s["login"].lower() in seen:  # duplicate |av-x| defs are an RST error
      continue
    seen.add(s["login"].lower())
    if s["private"]:
      entries.append(rst_escape(mask(s["login"])))  # no avatar, no link: both would out them
      continue
    # DWORD/QWORD perks promise a company name; Byte/Word promise a username.
    label = rst_escape(s["name"]) if use_company_name else "@" + s["login"]
    link = f"`{label} <https://github.com/{s['login']}>`__"
    if size:
      entries.append(f"|av-{s['login']}| {link}")
      defs += [f".. |av-{s['login']}| image:: https://github.com/{s['login']}.png?size={size}",
               f"   :target: https://github.com/{s['login']}",
               f"   :alt: {s['login']}", ""]
    else:
      entries.append(link)
  body = ", ".join(entries)
  return body + ("\n\n" + "\n".join(defs).rstrip() if defs else "")


def render_readme(sponsors, original):
  """Return README.rst with every marker block regenerated, or None if unchanged."""
  text, seen = original, set()
  for key, _floor, marker, placeholder, size in TIERS:
    if marker is None:
      continue
    members = [s for s in sponsors if s["tier"] == key]
    block = render(members, size, key in ("DWORD", "QWORD"), seen) if members else placeholder
    pattern = re.compile(rf"(^\.\. {re.escape(marker)}-START$\n)(.*?)(^\.\. {re.escape(marker)}-END$)",
                         re.M | re.S)
    if not pattern.search(text):
      sys.exit(f"README.rst: marker {marker}-START/-END not found")
    text = pattern.sub(lambda m: m.group(1) + "\n" + block + "\n\n" + m.group(3), text)
  return None if text == original else text


def codespell_collisions(original, new_text):
  """Logins/names the repo's auto-fixing codespell hook would rewrite in place."""
  added = [l for l in difflib.unified_diff(original.splitlines(),
                                           new_text.splitlines(), n=0) if l.startswith("+")]
  if not added:
    return []
  with tempfile.NamedTemporaryFile("w", suffix=".txt", delete=True) as probe:
    # outside the repo and not dot-prefixed: codespell skips hidden files unless
    # .codespellrc is picked up from cwd, which would make this guard fail open
    probe.write("\n".join(added) + "\n")
    probe.flush()
    try:
      run = subprocess.run(["codespell", "--ignore-words", str(IGNORE_WORDS), probe.name],
                           capture_output=True, text=True)
    except FileNotFoundError:
      print("  note: codespell not on PATH - generated block is UNCHECKED")
      return []
  if run.returncode not in (0, 65):  # 65 = typos found; anything else is a tool error
    print(f"  note: codespell failed (rc={run.returncode}) - generated block is UNCHECKED")
    return []
  out = run.stdout
  # v2.2.4 (the pinned hook) lowercases dictionary keys before testing ignore-words,
  # so a cased entry would never match and -w would rewrite the login anyway.
  return sorted({l.split(":", 2)[2].split("==>")[0].strip().lower()
                 for l in out.splitlines() if l.count(":") >= 2})


def readme_diff(original, new_text):
  return "".join(difflib.unified_diff(
      original.splitlines(keepends=True), new_text.splitlines(keepends=True),
      fromfile="README.rst", tofile="README.rst (new)", n=2))


# ------------------------------------------------------------------ labels

def as_logins(value, where):
  """config.json is hand-edited: a bare string here would iterate as characters and
  label the single-letter accounts it spells."""
  if not isinstance(value, list) or not all(isinstance(x, str) for x in value):
    sys.exit(f"config.json: {where} must be a list of logins, got {value!r}")
  return [x for x in value if x]


def entitlements(sponsors, config):
  """login -> sorted labels, first matching rule only (Adafruit, then sponsor tier)."""
  ent = {}

  # GitHub logins are case-insensitive and config.json is hand-edited, so
  # normalise everywhere. labeler.yml compares with .toLowerCase() for this reason.
  skip = {x.lower() for x in as_logins(config.get("exclude", []), "exclude")}

  def grant(login, labels):
    login = login.lower()
    if login and login not in skip:  # the maintainer does not triage their own tickets
      ent.setdefault(login, set(labels))  # setdefault: first rule wins, never a union

  # Adafruit is evaluated FIRST, matching labeler.yml's branch order.
  # public_members ONLY: /members returns concealed members to an org admin, and
  # labelling one "Reported by an Adafruit member" publishes what they hid.
  members = set(gh("api", "orgs/adafruit/public_members", "--paginate", "-q", ".[].login").split())
  members |= set(as_logins(config.get("adafruit_members_extra", []), "adafruit_members_extra"))
  for m in members:
    grant(m, ADAFRUIT_LABELS)

  # Rules mirror .github/workflows/labeler.yml, which applies these same labels
  # when a ticket is opened. This pass backfills what the workflow cannot reach:
  # tickets older than it, and private sponsors its GITHUB_TOKEN cannot see.
  for s in sponsors:
    # A private sponsor gets NO label. Every candidate set was measured against the
    # live repo and each one identifies them: withholding `Sponsor 💖` leaves a bare
    # `Prio Top 🚨`, which nothing else in the repo emits; and a bare `Prio 📌`
    # appears on 1 of 204 open issues and 0 of 873 discussions. A label applied only
    # to private sponsors IS the disclosure, whichever label it is. The triage perk
    # cannot ride on a public label - honour it off-ticket.
    if s["private"]:
      continue
    labels = set(TIER_LABELS[s["tier"]])
    if not labels:
      continue
    if s["is_org"]:
      members = as_logins({k.lower(): v for k, v in config.get("org_members", {}).items()}
                          .get(s["login"].lower(), []), f"org_members[{s['login']}]")
      if not members:
        who = mask(s["login"]) if s["private"] else s["login"]
        print(f"note: org sponsor {who} ({s['tier']}) has no members in config.json - "
              f"nothing to label")
      for m in members:
        grant(m, labels)
    else:
      grant(s["login"], labels)

  return {k: sorted(v) for k, v in sorted(ent.items())}


SCAN_Q = """
query($cursor:String){ repository(owner:"%s",name:"%s"){ %s(first:100, %sorderBy:{field:CREATED_AT,direction:DESC}, after:$cursor){
  pageInfo{hasNextPage endCursor}
  nodes{ number id %s author{login} labels(first:40){nodes{name}} } } } }
"""


def scan(kind, since):
  """Open tickets with number > since, newest first; stops at the watermark."""
  states = "" if kind == "discussions" else "states:OPEN, "
  closed = "closed" if kind == "discussions" else ""
  query = SCAN_Q % (OWNER, NAME, kind, states, closed)
  cursor, found = None, []
  while True:
    page = graphql(query, cursor=cursor)["repository"][kind]
    for n in page["nodes"]:
      if n["number"] <= since:
        return found
      if n.get("closed"):
        continue
      found.append({"number": n["number"], "id": n["id"],
                    "author": (n["author"] or {}).get("login"),
                    "labels": {x["name"] for x in n["labels"]["nodes"]}})
    if not page["pageInfo"]["hasNextPage"]:
      return found
    cursor = page["pageInfo"]["endCursor"]


def label_ids():
  q = '{repository(owner:"%s",name:"%s"){labels(first:100){nodes{name id}}}}' % (OWNER, NAME)
  return {n["name"]: n["id"] for n in graphql(q)["repository"]["labels"]["nodes"]}


def highest_number():
  q = ('{repository(owner:"%s",name:"%s"){'
       'issues(first:1,orderBy:{field:CREATED_AT,direction:DESC}){nodes{number}}'
       'pullRequests(first:1,orderBy:{field:CREATED_AT,direction:DESC}){nodes{number}}'
       'discussions(first:1,orderBy:{field:CREATED_AT,direction:DESC}){nodes{number}}}}') % (OWNER, NAME)
  r = graphql(q)["repository"]
  return max((v["nodes"][0]["number"] for v in r.values() if v["nodes"]), default=0)


def plan_labels(ent, since, private):
  """Open tickets above the watermark whose author is owed labels they lack."""
  actions = []
  # path segment differs per type, and doubles as the "which kind is this?" hint
  for kind, path in (("issues", "issues"), ("pullRequests", "pull"), ("discussions", "discussions")):
    for t in scan(kind, since):
      want = sorted(set(ent.get((t["author"] or "").lower(), [])) - t["labels"])
      if want:
        actions.append({"number": t["number"], "id": t["id"], "add": want,
                        "author": mask(t["author"]) if (t["author"] or "").lower() in private else t["author"],
                        "url": f"https://github.com/{OWNER}/{NAME}/{path}/{t['number']}"})
  return sorted(actions, key=lambda a: a["number"])


def print_label_table(actions):
  rows = [(a["url"], a["author"], ", ".join(a["add"])) for a in actions]
  head = ("Ticket", "Author", "Labels to add")
  # emoji render double-width, so pad by display width, not len()
  width = lambda t: len(t) + sum(c > "\u2100" for c in t)
  w = [max(width(r[i]) for r in rows + [head]) for i in range(3)]
  pad = lambda t, i: t + " " * (w[i] - width(t))
  print("  " + "  ".join(pad(head[i], i) for i in range(3)))
  print("  " + "  ".join("-" * w[i] for i in range(3)))
  for r in rows:
    print("  " + "  ".join(pad(r[i], i) for i in range(3)))


def apply_label_actions(actions, ids):
  for i, a in enumerate(actions, 1):
    # IDs inlined: `gh api graphql -F` cannot pass a list variable.
    graphql("mutation{addLabelsToLabelable(input:{labelableId:%s,labelIds:%s})"
            "{clientMutationId}}" % (json.dumps(a["id"]),
                                     json.dumps([ids[w] for w in a["add"]])))
    print(f"  [{i}/{len(actions)}] {a['url']}")  # per ticket: a mid-run failure must be legible
  print(f"labels: {len(actions)} ticket(s) updated")


def confirm(question):
  if not sys.stdin.isatty():
    sys.exit("stdin is not a terminal - re-run with --yes to apply, or --dry-run to plan only")
  return input(f"{question} [y/N] ").strip().lower() in ("y", "yes")


# -------------------------------------------------------------------- main

def print_rules():
  """The applied policy, read out of the constants above so it cannot drift."""
  print("Label rules mirror .github/workflows/labeler.yml (the source of truth for new "
        "tickets).\nThis script backfills what that workflow cannot reach: tickets older "
        "than it, and\nprivate sponsors, which its GITHUB_TOKEN cannot see at all.\n")
  row = "  {:<9} {:>5}  {:<17} {:<22} {}"
  print(row.format("Tier", "$/mo", "README section", "Listed as", "Labels"))
  for key, floor, marker, _placeholder, size in TIERS:
    company = key in ("DWORD", "QWORD")
    listed = ("not listed" if marker is None else
              (("logo + " if company else "avatar + ") if size else "")
              + ("company name" if company else "@username"))
    print(row.format(key, floor, marker or "-", listed, " ".join(sorted(TIER_LABELS[key]))))
  print(row.format("Adafruit", "-", "hand-written", "-", " ".join(sorted(ADAFRUIT_LABELS))))
  print("\nAdafruit membership comes from orgs/adafruit/public_members, never /members:"
        "\n  an org admin sees concealed members too, and the Adafruit label would publish"
        "\n  an affiliation those people deliberately hid."
        "\nA private sponsor is masked in the README (first 3 chars, no avatar, no link) and"
        "\n  gets NO label at all: any label applied only to private sponsors is itself the"
        "\n  disclosure. Measured live - a bare Prio Top 🚨 is emitted by nothing else in the"
        "\n  repo, and a bare Prio 📌 by 1 of 204 open issues. Honour their perk off-ticket."
        "\nRules are first-match-only (Adafruit, then tier), never a union: two priority"
        "\n  labels on one ticket double-count it in triage."
        "\nOnly OPEN tickets are labelled, labels are only ever added, and a ticket reopened"
        "\n  below the watermark needs --full-rescan.")


def main():
  p = argparse.ArgumentParser(description=__doc__,
                              formatter_class=argparse.RawDescriptionHelpFormatter)
  p.add_argument("--rules", action="store_true",
                 help="print the tier/label policy and exit")
  p.add_argument("--dry-run", action="store_true",
                 help="preview and stop; writes nothing, not even state.json")
  p.add_argument("--yes", action="store_true",
                 help="skip the confirmation prompt (required when stdin is not a terminal)")
  p.add_argument("--full-rescan", action="store_true",
                 help="ignore the watermark and scan every open ticket")
  only = p.add_mutually_exclusive_group()
  only.add_argument("--readme-only", action="store_true", help="skip the label pass")
  only.add_argument("--labels-only", action="store_true", help="skip the README pass")
  args = p.parse_args()

  if args.rules:
    return print_rules()

  if REPO != Path.cwd().resolve() and REPO not in Path.cwd().resolve().parents:
    sys.exit(f"run from inside {REPO} - this script writes that checkout, not the cwd")

  config = json.loads(CONFIG.read_text(encoding="utf-8"))
  unknown = {k for k in config if not k.startswith("_")} - {"exclude", "org_members",
                                                            "adafruit_members_extra"}
  if unknown:  # a mistyped key reads as absent, and `exclude` failing open means
    sys.exit(f"config.json: unknown key(s) {sorted(unknown)}")  # labelling our own tickets
  state = json.loads(STATE.read_text(encoding="utf-8")) if STATE.exists() else {}
  original = README.read_text(encoding="utf-8")  # one snapshot, re-checked before the write

  sponsors = fetch_sponsors()
  print(f"{len(sponsors)} active sponsor(s):")
  for s in sponsors:
    who = mask(s["login"]) + " (private)" if s["private"] else s["login"]
    print(f"  {s['since'][:10]}  {s['tier']:<5}  {who}")

  # ---------------------------------------------------------------- plan
  if not args.labels_only and not sponsors:
    # Rewriting every section back to "be the first!" is indistinguishable from a
    # token that cannot see the sponsorships. Refuse rather than wipe.
    sys.exit("no active sponsorships returned - refusing to rewrite README.rst")
  new_readme = None if args.labels_only else render_readme(sponsors, original)

  actions, ids, watermark, fingerprint = [], {}, None, None
  if not args.readme_only:
    ent = entitlements(sponsors, config)
    fingerprint = hashlib.sha256(json.dumps(ent, sort_keys=True).encode()).hexdigest()[:16]
    changed = bool(state) and fingerprint != state.get("fingerprint")
    since = 0 if args.full_rescan or changed else state.get("last_ticket", 0)
    if changed:
      print("entitlements changed since last run - rescanning all open tickets")
    elif args.full_rescan:
      print("--full-rescan - ignoring the watermark")
    print(f"scanning open tickets above #{since}")
    ids = label_ids()
    watermark = highest_number()
    # checked before the prompt: an unknown name must not KeyError mid-apply
    unknown = {n for n in (L_SPONSOR, L_PRIO, L_PRIO_TOP, L_ADAFRUIT) if n not in ids}
    if unknown:
      sys.exit(f"labels missing from the repo: {sorted(unknown)}")
    private = {s["login"].lower() for s in sponsors if s["private"]}
    actions = plan_labels(ent, since, private)

  # ------------------------------------------------------------- preview
  collisions = codespell_collisions(original, new_readme) if new_readme else []
  if not args.labels_only:
    print("\nREADME.rst")
    print(readme_diff(original, new_readme) if new_readme else "  no change\n")
  if collisions:
    print(f"  note: codespell (-w) would rewrite {', '.join(collisions)} in the generated block;\n"
          f"  adding them to {IGNORE_WORDS.relative_to(REPO)} on apply\n")
  if not args.readme_only:
    print("Tickets")
    if actions:
      print_label_table(actions)
    else:
      print("  no change")
    print()

  if not new_readme and not actions:
    print("nothing to do")
    return
  if args.dry_run:
    print("dry run - nothing applied")
    return
  if not args.yes and not confirm("Apply these changes?"):
    sys.exit("aborted - nothing applied")

  # --------------------------------------------------------------- apply
  # Remote label writes go FIRST: they cannot be undone by git, so if they fail
  # partway the local README edit has not happened and `git status` stays honest.
  if actions:
    apply_label_actions(actions, ids)
  if new_readme:
    if README.read_text(encoding="utf-8") != original:  # edited during the prompt
      sys.exit("README.rst changed while this run was in progress - "
               "labels are applied, re-run for the README")
    if collisions:  # before the write, so the hook cannot mangle it first
      have = [w for w in IGNORE_WORDS.read_text(encoding="utf-8").splitlines() if w.strip()]
      IGNORE_WORDS.write_text("\n".join(sorted(set(have) | set(collisions))) + "\n", encoding="utf-8")
      print(f"ignore-words.txt: added {', '.join(collisions)}")
    README.write_text(new_readme, encoding="utf-8")
    print("README.rst: updated")
  # Written only here: a dry run or an aborted confirmation must leave the
  # watermark alone, or the next run would skip tickets it never labelled.
  if watermark is not None:
    STATE.write_text(json.dumps(
        {"last_ticket": watermark, "fingerprint": fingerprint, "updated": date.today().isoformat()},
        indent=2) + "\n", encoding="utf-8")
    print(f"state.json: last_ticket={watermark}")


if __name__ == "__main__":
  main()
