#!/usr/bin/env python3
"""Write the web 'Pedia bundle as a BYTE-REPRODUCIBLE uncompressed tar.

The device downloads this artifact and checks it against the SHA-256 the manifest
published alongside it, so the hash has to describe the CONTENT and nothing else.
Plain `tar` does not manage that: it records each file's mtime and the building
user's uid/gid, so the same bundle built twice — on a laptop and on a CI runner,
or on one runner twice — produces two different archives and two different hashes.
A device that fetched the manifest before a rebuild and the artifact after one
would then fail its digest check and report Corrupt, which reads as a hostile
network when it is really just a second build of identical files.

Every field that could carry build-time noise is therefore pinned: entries are
emitted in sorted order, with a fixed mtime, uid/gid 0, no user/group names, and
normalised modes. Same files in, same bytes out, anywhere.

The shape the device requires (src/core/net/tar_reader.h):
  * UNCOMPRESSED — the reader is a fixed-offset header parse, nothing inflates.
  * USTAR, not PAX. Python defaults to PAX, which prefixes the archive with an
    extended header the reader can only treat as Skip — legal, but a block of
    payload that exists solely to describe metadata nothing here reads.
  * Files at the archive's TOP LEVEL, no leading `web/` or `./` component: the
    device supplies the extraction root, so the archive must not name one.
  * Names within 100 bytes, the USTAR field the reader parses into TarEntry::name.
    Python raises rather than truncating, which is the failure we want.

Usage: make_web_tar.py <source-dir> <out.tar>
"""

import io
import os
import sys
import tarfile

# A fixed timestamp for every entry. The value is arbitrary — nothing reads an
# mtime off this archive — but it sits above FAT32's 1980 floor so an extracted
# file can carry it on the device's SD card without being clamped.
FIXED_MTIME = 1577836800  # 2020-01-01T00:00:00Z

# What belongs in the bundle, relative to the source dir. Mirrors `make pedia-sd`
# so a card written by an update and one staged by hand hold the same files.
TOP_LEVEL_FILES = ("index.html", "style.css", "app.js", "VERSION")
SUBDIRS = ("assets", "data")

# Editor and Finder droppings that must never reach the device.
def _excluded(name):
    base = os.path.basename(name)
    return base.startswith("._") or base == ".DS_Store"


def _entry(name, mode, kind, size=0):
    info = tarfile.TarInfo(name)
    info.mode = mode
    info.type = kind
    info.size = size
    info.mtime = FIXED_MTIME
    info.uid = info.gid = 0
    info.uname = info.gname = ""
    return info


def build(src, out):
    files = []           # (archive name, source path)
    for name in TOP_LEVEL_FILES:
        path = os.path.join(src, name)
        if not os.path.isfile(path):
            sys.exit(f"error: {path} is missing — the bundle would be incomplete")
        files.append((name, path))

    dirs = []
    for sub in SUBDIRS:
        root = os.path.join(src, sub)
        if not os.path.isdir(root):
            sys.exit(f"error: {root} is missing — the bundle would be incomplete")
        dirs.append(sub)
        for walk_root, walk_dirs, walk_files in os.walk(root):
            walk_dirs.sort()
            rel_dir = os.path.relpath(walk_root, src)
            for d in walk_dirs:
                dirs.append(os.path.join(rel_dir, d))
            for f in sorted(walk_files):
                if _excluded(f):
                    continue
                files.append((os.path.join(rel_dir, f), os.path.join(walk_root, f)))

    # Sorting is what makes the ORDER reproducible; os.walk's is filesystem order,
    # which differs between machines even when the contents match.
    dirs.sort()
    files.sort()

    with tarfile.open(out, "w", format=tarfile.USTAR_FORMAT) as tar:
        for d in dirs:
            tar.addfile(_entry(d, 0o755, tarfile.DIRTYPE))
        for name, path in files:
            with open(path, "rb") as fh:
                data = fh.read()
            tar.addfile(_entry(name, 0o644, tarfile.REGTYPE, len(data)),
                        io.BytesIO(data))
    return len(files), len(dirs)


def main():
    if len(sys.argv) != 3:
        sys.exit(__doc__.strip().splitlines()[-1])
    src, out = sys.argv[1], sys.argv[2]
    os.makedirs(os.path.dirname(os.path.abspath(out)), exist_ok=True)
    n_files, n_dirs = build(src, out)
    print(f"built {out}  ({n_files} files, {n_dirs} dirs, "
          f"{os.path.getsize(out)} bytes)")


if __name__ == "__main__":
    main()
