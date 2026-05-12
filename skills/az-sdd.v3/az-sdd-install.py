#!/usr/bin/env python3
"""az-sdd 全套技能安装脚本"""

import argparse
import shutil
from pathlib import Path

SRC_ROOT = Path(__file__).parent

parser = argparse.ArgumentParser(description="安装 az-sdd 全套技能")
parser.add_argument("--cline", action="store_true", help="安装到 Cline 全局技能目录")
parser.add_argument("--claude", action="store_true", help="安装到 Claude 全局技能目录")
args = parser.parse_args()

if args.cline:
    DEST_ROOT = Path(r"C:\Users\jpz\.agents\skills")
elif args.claude:
    DEST_ROOT = Path(r"C:\Users\jpz\.claude\skills")
else:
    DEST_ROOT = Path(__file__).parent

SKILLS = [
    "az-sdd-rules",
    "az-sdd-coding",
    "az-sdd-test",
    "az-sdd-spec",
    "az-sdd-arch",
    "az-sdd-track",
    "az-sdd-feedback",
    "az-sdd-init",
    "az-sdd-next"
]

print("=" * 50)
print("az-sdd 全套技能安装")
print("=" * 50)
print(f"源目录: {SRC_ROOT}")
print(f"目标目录: {DEST_ROOT}")
print()

for skill in SKILLS:
    src = SRC_ROOT / skill
    dst = DEST_ROOT / skill

    if not src.exists():
        print(f"[跳过] {skill} (源目录不存在)")
        continue

    if dst.exists():
        shutil.rmtree(dst)

    shutil.copytree(src, dst)
    print(f"[成功] {skill}")

print()
print("=" * 50)
print("安装完成")
print("=" * 50)
