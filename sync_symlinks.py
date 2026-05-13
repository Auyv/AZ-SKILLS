#!/usr/bin/env python3
"""
快速同步脚本：将 AZ-SKILLS/skills 下的所有 skill 创建软连接到全局 skills 目录
仅添加新的软连接，已存在或已为软连接的 skill 会被跳过

用法:
    python sync_symlinks.py                # 同步到 Hermes (~/.hermes/skills)
    python sync_symlinks.py --all           # 同步到所有平台
    python sync_symlinks.py --list          # 仅列出状态
    python sync_symlinks.py --to cline      # 指定平台
"""

import os
import sys
import argparse
from pathlib import Path

# ========== 配置 ==========
SCRIPT_DIR = Path(__file__).parent.resolve()
SKILLS_SOURCE = SCRIPT_DIR / "skills"

# 目标路径配置（相对于用户 home 目录）
TARGETS = {
    "hermes": ".hermes/skills",
    "cline": ".agents/skills",
    "claude_code": ".claude/skills",
    "opencode": ".config/opencode/skills",
    "openclaw": ".openclaw/skills",
}

# ========== 颜色 ==========
OK = "\033[92m"
WARN = "\033[93m"
FAIL = "\033[91m"
CYAN = "\033[96m"
BOLD = "\033[1m"
END = "\033[0m"

def cprint(text, color=""):
    if sys.stdout.isatty() and color:
        print(f"{color}{text}{END}")
    else:
        print(text)

def info(msg): cprint(msg, CYAN)
def success(msg): cprint(msg, OK)
def warning(msg): cprint(msg, WARN)
def error(msg): cprint(msg, FAIL)

# ========== 核心逻辑 ==========

def get_home():
    return Path.home()

def ensure_skill_link(skill_name: str, target_dir: Path) -> str:
    """为单个 skill 创建软连接，返回状态描述"""
    source = SKILLS_SOURCE / skill_name
    target = target_dir / skill_name

    if not source.exists():
        return f"源目录不存在: {source}"

    if target.is_symlink():
        return "已是软连接，跳过"
    elif target.exists():
        return f"目标已存在(非软连接)，跳过"
    else:
        try:
            target.symlink_to(source)
            return "创建成功"
        except OSError as e:
            return f"创建失败: {e}"

def sync_to_platform(platform: str, dry_run: bool = False, list_only: bool = False):
    """同步到指定平台"""
    home = get_home()
    target_path = home / TARGETS[platform]

    if not target_path.exists():
        error(f"目标目录不存在: {target_path}")
        error(f"请先确保 {platform} 已安装")
        return

    info(f"\n{'[模拟] ' if dry_run else ''}同步到 {platform}: {target_path}")

    # 获取所有 skill 源目录
    if not SKILLS_SOURCE.exists():
        error(f"源目录不存在: {SKILLS_SOURCE}")
        return

    skills = sorted([d.name for d in SKILLS_SOURCE.iterdir() if d.is_dir()])

    if not skills:
        warning("源目录为空")
        return

    results = {"created": [], "skipped": [], "failed": []}

    for skill in skills:
        status = ensure_skill_link(skill, target_path)

        if "已是软连接" in status or "已存在" in status:
            results["skipped"].append((skill, status))
            cprint(f"  . {skill}: {status}", WARN)
        elif "成功" in status:
            results["created"].append(skill)
            cprint(f"  + {skill}: {status}", OK)
        else:
            results["failed"].append((skill, status))
            cprint(f"  X {skill}: {status}", FAIL)

    # 摘要
    print()
    success(f"完成: 新增 {len(results['created'])} | 跳过 {len(results['skipped'])} | 失败 {len(results['failed'])}")
    return results

def list_all_status():
    """列出所有 skill 在各平台的同步状态"""
    home = get_home()
    info("AZ-SKILLS/skills 软连接状态\n")

    if not SKILLS_SOURCE.exists():
        error(f"源目录不存在: {SKILLS_SOURCE}")
        return

    skills = sorted([d.name for d in SKILLS_SOURCE.iterdir() if d.is_dir()])

    for skill in skills:
        print(f"  {skill}")
        for platform, rel_path in TARGETS.items():
            target = home / rel_path / skill
            if target.is_symlink():
                status = OK + "软连接" + END
            elif target.exists():
                status = WARN + "目录(非软连接)" + END
            else:
                status = FAIL + "未同步" + END
            print(f"    {platform:<15}: {status}")

# ========== 主入口 ==========

def main():
    parser = argparse.ArgumentParser(description="同步 AZ-SKILLS 到各 AI 平台")
    parser.add_argument("--to", choices=list(TARGETS.keys()), help="指定目标平台")
    parser.add_argument("--all", action="store_true", help="同步到所有平台")
    parser.add_argument("--list", action="store_true", help="仅列出状态")
    parser.add_argument("--dry-run", action="store_true", help="模拟运行")
    args = parser.parse_args()

    if args.list:
        list_all_status()
        return

    platforms = []
    if args.to:
        platforms = [args.to]
    elif args.all:
        platforms = list(TARGETS.keys())
    else:
        # 默认同步到 hermes
        platforms = ["hermes"]

    for platform in platforms:
        sync_to_platform(platform, dry_run=args.dry_run)

if __name__ == "__main__":
    main()
