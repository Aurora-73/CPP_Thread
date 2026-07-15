#!/usr/bin/env python3
"""
Collapse nested directory chains where each directory contains exactly one
subdirectory and no files, keeping the directory with the longest name.

Example:
    aaa/bbbb/cc/  (aaa has only bbbb, bbbb has only cc, no files in between)
    → bbbb/       (longest name among the chain)

Usage:
    python collapse_dirs.py [path]
    Defaults to current directory if no path given.
"""

import sys
import shutil
from pathlib import Path


def collapse_dir(dir_path: Path, root: Path) -> bool:
    """Recursively collapse single-child directory chains.

    Returns True if this directory was removed (collapsed into parent).
    """
    for child in list(dir_path.iterdir()):
        if child.is_dir():
            collapse_dir(child, root)

    entries = list(dir_path.iterdir())
    subdirs = [e for e in entries if e.is_dir()]
    files  = [e for e in entries if e.is_file()]

    if len(subdirs) != 1 or len(files) != 0:
        return False
    if dir_path == root:
        return False

    child  = subdirs[0]
    parent = dir_path.parent

    if len(dir_path.name) >= len(child.name):
        # Keep parent name, absorb child contents
        for item in list(child.iterdir()):
            dest = dir_path / item.name
            if dest.exists():
                print(f"  SKIP {item} → {dest} (target exists)")
                continue
            shutil.move(str(item), str(dest))
        child.rmdir()
        print(f"  MERGE {child} → {dir_path}")
        return False
    else:
        # Keep child name, move it up
        target = parent / child.name
        if target.exists():
            print(f"  SKIP {dir_path}/{child.name} → {target} (target exists)")
            return False
        child.rename(target)
        dir_path.rmdir()
        print(f"  RAISE {dir_path}/{child.name} → {target}")
        return True


def main():
    root = Path(sys.argv[1]).resolve() if len(sys.argv) > 1 else Path.cwd()
    print(f"Collapsing single-child directories under: {root}")
    collapse_dir(root, root)
    print("Done.")


if __name__ == "__main__":
    main()
