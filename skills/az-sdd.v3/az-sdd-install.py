#!/usr/bin/env python3
"""Install the AZ-SDD skill set into a target skills directory."""

import argparse
import shutil
from pathlib import Path

SRC_ROOT = Path(__file__).resolve().parent

SKILLS = [
    "az-sdd-next",
    "az-sdd-rules",
    "az-sdd-init",
    "az-sdd-spec",
    "az-sdd-arch",
    "az-sdd-coding",
    "az-sdd-test",
    "az-sdd-track",
    "az-sdd-feedback",
]


def default_target(args: argparse.Namespace) -> Path:
    home = Path.home()
    if args.cline:
        return home / ".agents" / "skills"
    if args.claude:
        return home / ".claude" / "skills"
    if args.dest:
        return Path(args.dest).expanduser()
    return SRC_ROOT


def main() -> int:
    parser = argparse.ArgumentParser(description="Install the full AZ-SDD skill set")
    target = parser.add_mutually_exclusive_group()
    target.add_argument("--cline", action="store_true", help="Install to ~/.agents/skills")
    target.add_argument("--claude", action="store_true", help="Install to ~/.claude/skills")
    target.add_argument("--dest", help="Install to a custom skills directory")
    parser.add_argument("--dry-run", action="store_true", help="Show planned changes without copying")
    parser.add_argument("--force", action="store_true", help="Overwrite existing skill directories")
    args = parser.parse_args()

    dest_root = default_target(args)

    print("=" * 50)
    print("AZ-SDD v3 skill installation")
    print("=" * 50)
    print(f"Source: {SRC_ROOT}")
    print(f"Target: {dest_root}")
    print(f"Mode: {'dry-run' if args.dry_run else 'copy'}")
    print()

    if not args.dry_run:
        dest_root.mkdir(parents=True, exist_ok=True)

    for skill in SKILLS:
        src = SRC_ROOT / skill
        dst = dest_root / skill

        if not src.exists():
            print(f"[skip] {skill}: source directory not found")
            continue

        if dst.exists() and not args.force:
            print(f"[skip] {skill}: already exists (use --force to overwrite)")
            continue

        if args.dry_run:
            action = "overwrite" if dst.exists() else "copy"
            print(f"[dry-run] {action} {skill} -> {dst}")
            continue

        if dst.exists():
            shutil.rmtree(dst)

        shutil.copytree(src, dst)
        print(f"[ok] {skill}")

    print()
    print("=" * 50)
    print("Done")
    print("=" * 50)
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
