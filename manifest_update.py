#!/usr/bin/env python3
"""
manifest.yaml 自动更新脚本
扫描 skills/ 目录下的 SKILL.md，将缺失的 skill 信息自动添加到 manifest.yaml

用法:
    python manifest_update.py                    # 交互模式（逐个确认）
    python manifest_update.py --auto             # 自动模式（不确认直接添加）
    python manifest_update.py --list-missing     # 仅列出缺失项，不修改
    python manifest_update.py --dry-run          # 模拟运行，不实际修改
"""

import os
import sys
import re
import argparse
from pathlib import Path
from typing import List, Optional
import yaml


SCRIPT_DIR = Path(__file__).parent.resolve()
MANIFEST_PATH = SCRIPT_DIR / "manifest.yaml"
SKILLS_DIR = SCRIPT_DIR / "skills"

DEFAULT_PLATFORMS = ["hermes", "cline", "claude_code", "opencode", "openclaw"]

SKIP_DIRS = {"template", "templates", "__pycache__", ".git"}
SKIP_NAMES = {"skill-name"}


def parse_front_matter(filepath: Path) -> Optional[dict]:
    """解析 SKILL.md 的 YAML front matter"""
    content = filepath.read_text(encoding="utf-8")
    match = re.match(r"^---\s*\n(.*?)\n---", content, re.DOTALL)
    if not match:
        return None
    try:
        return yaml.safe_load(match.group(1))
    except yaml.YAMLError:
        return None


def extract_skills_from_disk() -> List[dict]:
    """扫描 skills/ 目录，提取所有 skill 信息"""
    found = []

    for root, dirs, files in os.walk(SKILLS_DIR):
        dirs[:] = [d for d in dirs if d not in SKIP_DIRS]
        if "SKILL.md" not in files:
            continue

        skill_dir = Path(root)
        rel_path = skill_dir.relative_to(SKILLS_DIR)

        fm = parse_front_matter(skill_dir / "SKILL.md")
        if not fm:
            continue

        name = fm.get("name", "")
        if not name or name in SKIP_NAMES:
            continue

        description = fm.get("description", "").strip()
        description = re.sub(r"\s+", " ", description).strip()
        version = fm.get("version", "1.0")

        found.append({
            "name": name,
            "description": description,
            "version": str(version),
            "rel_path": str(rel_path),
        })

    return found


def load_manifest() -> dict:
    """加载 manifest.yaml"""
    if not MANIFEST_PATH.exists():
        print(f"[ERROR] manifest.yaml 不存在: {MANIFEST_PATH}")
        sys.exit(1)
    with open(MANIFEST_PATH, "r", encoding="utf-8") as f:
        return yaml.safe_load(f)


def save_manifest(data: dict):
    """保存 manifest.yaml"""
    with open(MANIFEST_PATH, "w", encoding="utf-8") as f:
        yaml.dump(data, f, allow_unicode=True, default_flow_style=False, sort_keys=False)


def existing_skill_names(manifest: dict) -> set:
    """获取 manifest 中已有的 skill name 集合"""
    names = set()
    for s in manifest.get("skills", []):
        names.add(s["name"])
        for sub in s.get("sub_skills", []):
            names.add(sub)
    return names


def build_new_entry(skill: dict) -> dict:
    """构建新的 manifest skill 条目"""
    entry = {
        "name": skill["name"],
        "description": skill["description"],
        "version": skill["version"],
        "platforms": {p: skill["name"] for p in DEFAULT_PLATFORMS},
    }
    return entry


def main():
    parser = argparse.ArgumentParser(
        description="manifest.yaml 自动更新脚本 - 将缺失的 skills 自动添加到清单",
    )
    parser.add_argument("--auto", action="store_true", help="自动模式（不逐个确认直接添加）")
    parser.add_argument("--list-missing", action="store_true", help="仅列出缺失项，不修改")
    parser.add_argument("--dry-run", action="store_true", help="模拟运行，不实际修改")
    args = parser.parse_args()

    manifest = load_manifest()
    existing = existing_skill_names(manifest)

    disk_skills = extract_skills_from_disk()

    missing = [s for s in disk_skills if s["name"] not in existing]

    if not missing:
        print("所有 skills 已在 manifest.yaml 中，无需更新。")
        return

    print(f"\n发现 {len(missing)} 个缺失的 skill:\n")
    for s in missing:
        print(f"  [{s['name']}]")
        print(f"    描述: {s['description'][:60]}{'...' if len(s['description']) > 60 else ''}")
        print(f"    版本: {s['version']}")
        print(f"    路径: {s['rel_path']}")
        print()

    if args.list_missing:
        return

    if not args.auto:
        reply = input("是否将这些 skill 添加到 manifest.yaml？(y/N): ").strip().lower()
        if reply not in ("y", "yes"):
            print("已取消。")
            return

    if args.dry_run:
        print("[DRY-RUN] 模拟运行，以下 skill 将被添加:")
        for s in missing:
            print(f"  + {s['name']} (v{s['version']})")
        return

    for s in missing:
        entry = build_new_entry(s)
        manifest["skills"].append(entry)
        print(f"  + 已添加: {s['name']}")

    save_manifest(manifest)
    print(f"\n完成！已添加 {len(missing)} 个 skill 到 manifest.yaml。")


if __name__ == "__main__":
    main()
