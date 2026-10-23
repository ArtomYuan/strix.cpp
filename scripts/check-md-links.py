#!/usr/bin/env python3
"""Check relative links in this repository's markdown files.

Absolute URLs, anchors and mailto links are skipped. A target that does not
exist on disk is a failure unless it matches an entry in
`.github/md-link-allowlist.txt` (upstream leftovers that are kept verbatim so
that upstream syncs stay clean).

Usage: python3 scripts/check-md-links.py [--verbose]
Exit code: 0 when every non-allowlisted relative link resolves, 1 otherwise.
"""

import fnmatch
import os
import posixpath
import re
import sys

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
ALLOWLIST = os.path.join(ROOT, ".github", "md-link-allowlist.txt")
LINK_RE = re.compile(r"\[[^\]]*\]\(([^)\s]+)\)")

# Files rendered by GitHub outside their on-disk directory. A PR template is
# shown in the pull request form, where relative links resolve against the
# repository root, not against .github/.
ROOT_RENDERED = {".github/pull_request_template.md"}


def load_allowlist():
    entries = []
    if not os.path.exists(ALLOWLIST):
        return entries
    with open(ALLOWLIST, encoding="utf-8") as fh:
        for line in fh:
            line = line.strip()
            if not line or line.startswith("#"):
                continue
            parts = line.split("\t")
            if len(parts) != 2:
                print(f"allowlist: malformed line: {line!r}", file=sys.stderr)
                continue
            entries.append((parts[0].strip(), parts[1].strip()))
    return entries


def allowed(source, target, allowlist):
    return any(
        fnmatch.fnmatch(source, s_glob) and fnmatch.fnmatch(target, t_glob)
        for s_glob, t_glob in allowlist
    )


def exists(candidate):
    path = os.path.join(ROOT, candidate)
    if os.path.exists(path):
        return True
    for suffix in ("/README.md", "/index.md"):
        if os.path.exists(path.rstrip("/") + suffix):
            return True
    return False


def main():
    verbose = "--verbose" in sys.argv
    allowlist = load_allowlist()
    broken, checked, scanned = [], 0, 0

    for dirpath, dirnames, filenames in os.walk(ROOT):
        dirnames[:] = [d for d in dirnames if d not in (".git", "node_modules", "build")]
        for name in filenames:
            if not name.endswith(".md"):
                continue
            full = os.path.join(dirpath, name)
            rel = os.path.relpath(full, ROOT).replace(os.sep, "/")
            scanned += 1
            with open(full, encoding="utf-8", errors="ignore") as fh:
                text = fh.read()
            for match in LINK_RE.finditer(text):
                raw = match.group(1)
                if raw.startswith(("http://", "https://", "mailto:", "#", "<")):
                    continue
                target = raw.split("#")[0]
                if not target:
                    continue
                checked += 1
                if target.startswith("/"):
                    resolved = target.lstrip("/")
                elif rel in ROOT_RENDERED:
                    resolved = posixpath.normpath(target)
                else:
                    resolved = posixpath.normpath(
                        posixpath.join(posixpath.dirname(rel), target)
                    )
                if exists(resolved):
                    continue
                if allowed(rel, target, allowlist):
                    if verbose:
                        print(f"allowlisted: {rel} -> {target}")
                    continue
                broken.append((rel, target))

    print(f"scanned {scanned} markdown files, checked {checked} relative links")
    for source, target in broken:
        print(f"BROKEN {source} -> {target}")
    if broken:
        print(f"{len(broken)} broken relative link(s)")
        return 1
    print("all relative links resolve")
    return 0


if __name__ == "__main__":
    sys.exit(main())
