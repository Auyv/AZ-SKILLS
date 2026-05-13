#!/usr/bin/env python3
"""
AZ-SKILLS 同步脚本
用于将 AZ-SKILLS 中的 skill 同步到各 AI 工具的 skills 目录

用法:
    python sync_skills.py                    # 交互模式
    python sync_skills.py --full             # 全量同步
    python sync_skills.py --list            # 仅列出状态
    python sync_skills.py --to cline         # 只同步到 cline
    python sync_skills.py --dry-run          # 模拟运行，不实际复制
"""

import os
import sys
import shutil
import argparse
from pathlib import Path
from datetime import datetime
from typing import List, Dict, Optional
import yaml

# ========== 配置 ==========

SCRIPT_DIR = Path(__file__).parent.resolve()
MANIFEST_PATH = SCRIPT_DIR / "manifest.yaml"

# 源目录（相对于 manifest.yaml 位置）
# 实际路径 = manifest.yaml所在目录 / source_dir
SKILLS_SOURCE_SUBDIR = "skills"

# 默认同步目标（Windows 路径）
DEFAULT_TARGETS = {
    "hermes": "C:/Users/jpz/.hermes/skills",
    "cline": "C:/Users/jpz/.agents/skills",
    "claude_code": "C:/Users/jpz/.claude/skills",
    "opencode": "C:/Users/jpz/.config/opencode/skills",
    "openclaw": "C:/Users/jpz/.openclaw/skills",
}

# ========== 颜色输出 ==========

class Colors:
    HEADER = '\033[95m'
    OKBLUE = '\033[94m'
    OKCYAN = '\033[96m'
    OKGREEN = '\033[92m'
    WARNING = '\033[93m'
    FAIL = '\033[91m'
    ENDC = '\033[0m'
    BOLD = '\033[1m'

def colorprint(text: str, color: str = ""):
    if sys.stdout.isatty() and color:
        print(f"{color}{text}{Colors.ENDC}")
    else:
        print(text)

def info(msg): colorprint(msg, Colors.OKCYAN)
def success(msg): colorprint(msg, Colors.OKGREEN)
def warning(msg): colorprint(msg, Colors.WARNING)
def error(msg): colorprint(msg, Colors.FAIL)
def header(msg): colorprint(msg, Colors.HEADER + Colors.BOLD)

# ========== 核心逻辑 ==========

class SkillSyncer:
    def __init__(self, manifest_path: Path):
        self.manifest_path = manifest_path
        self.manifest_dir = manifest_path.parent
        self.source_dir = self.manifest_dir / SKILLS_SOURCE_SUBDIR
        self.manifest = self._load_manifest()
        self.dry_run = False

    def _load_manifest(self) -> dict:
        """加载 manifest.yaml"""
        if not self.manifest_path.exists():
            error(f"Manifest 文件不存在: {self.manifest_path}")
            sys.exit(1)

        with open(self.manifest_path, 'r', encoding='utf-8') as f:
            return yaml.safe_load(f)

    def _save_manifest(self):
        """保存 manifest.yaml"""
        with open(self.manifest_path, 'w', encoding='utf-8') as f:
            yaml.dump(self.manifest, f, allow_unicode=True, default_flow_style=False)

    def _get_target_path(self, target_name: str) -> Optional[Path]:
        """获取目标路径"""
        # 优先使用 manifest 中定义的路径
        targets = self.manifest.get('sync_targets', [])
        for t in targets:
            if t.get('name') == target_name and t.get('enabled', True):
                return Path(t['path'])

        # 回退到默认路径
        if target_name in DEFAULT_TARGETS:
            return Path(DEFAULT_TARGETS[target_name])

        return None

    def _get_source_skills(self) -> List[str]:
        """获取源目录中的所有 skill"""
        if not self.source_dir.exists():
            warning(f"源目录不存在: {self.source_dir}")
            return []

        skills = []
        for item in self.source_dir.iterdir():
            if item.is_dir():
                skills.append(item.name)
        return sorted(skills)

    def _sync_skill_to_target(self, skill_name: str, target_path: Path, target_label: str) -> bool:
        """同步单个 skill 到目标目录"""
        src = self.source_dir / skill_name
        dest = target_path / skill_name

        if not src.exists():
            warning(f"  源 skill 不存在: {src}")
            return False

        # 检查是否需要同步（通过比较修改时间）
        need_sync = True
        if dest.exists():
            src_mtime = max(f.stat().st_mtime for f in src.rglob('*') if f.is_file()) if any(src.rglob('*')) else 0
            dest_mtime = max(f.stat().st_mtime for f in dest.rglob('*') if f.is_file()) if any(dest.rglob('*')) else 0
            need_sync = src_mtime > dest_mtime

        if not need_sync:
            info(f"  = {skill_name} (已是最新)")
            return True

        if self.dry_run:
            info(f"  → {skill_name} (模拟复制)")
            return True

        # 执行同步
        try:
            # 删除目标（如果是增量模式但目标存在）
            if dest.exists():
                shutil.rmtree(dest)

            # 复制目录
            shutil.copytree(src, dest)
            success(f"  ✓ {skill_name}")
            return True
        except Exception as e:
            error(f"  ✗ {skill_name}: {e}")
            return False

    def sync_to(self, target_name: str, full: bool = False) -> Dict[str, int]:
        """同步到指定目标"""
        target_path = self._get_target_path(target_name)
        if not target_path:
            error(f"未找到目标配置: {target_name}")
            return {"success": 0, "skipped": 0, "failed": 0}

        if not target_path.exists():
            warning(f"目标目录不存在，将创建: {target_path}")
            if not self.dry_run:
                target_path.mkdir(parents=True, exist_ok=True)

        info(f"\n{'='*50}")
        info(f"同步目标: {target_name}")
        info(f"源目录: {self.source_dir}")
        info(f"目标目录: {target_path}")
        info(f"模式: {'全量' if full else '增量'}")
        info(f"{'='*50}")

        skills = self._get_source_skills()
        if not skills:
            warning("没有找到需要同步的 skill")
            return {"success": 0, "skipped": 0, "failed": 0}

        success_count = 0
        skipped_count = 0
        failed_count = 0

        manifest_names = {s['name'] for s in self.manifest.get('skills', [])}

        for skill in skills:
            # 增量模式：只同步 manifest 中注册的 skill
            # 全量模式：所有目录都同步
            if not full and skill not in manifest_names:
                info(f"  - {skill} (未在 manifest 中注册，跳过)")
                skipped_count += 1
                continue

            if self._sync_skill_to_target(skill, target_path, target_name):
                success_count += 1
            else:
                failed_count += 1

        return {
            "success": success_count,
            "skipped": skipped_count,
            "failed": failed_count
        }

    def sync_all(self, full: bool = False) -> Dict[str, int]:
        """同步到所有已配置的目标"""
        header("\n🚀 AZ-SKILLS 同步开始")

        total = {"success": 0, "skipped": 0, "failed": 0}

        targets = self.manifest.get('sync_targets', [])
        for target in targets:
            if not target.get('enabled', True):
                continue

            result = self.sync_to(target['name'], full=full)
            total["success"] += result["success"]
            total["skipped"] += result["skipped"]
            total["failed"] += result["failed"]

        # 更新同步时间
        if not self.dry_run:
            self.manifest['last_sync'] = datetime.now().isoformat()
            self._save_manifest()

        header("\n" + "="*50)
        success(f"同步完成! 成功: {total['success']}, 跳过: {total['skipped']}, 失败: {total['failed']}")
        if self.dry_run:
            warning(" (模拟模式，未实际复制)")
        header("="*50)

        return total

    def list_status(self):
        """列出当前状态"""
        header("\n📋 AZ-SKILLS 状态概览")
        info(f"\nManifest: {self.manifest_path}")
        info(f"源目录: {self.source_dir}")

        # 列出源 skills
        skills = self._get_source_skills()
        info(f"\n源 skill 列表 ({len(skills)} 个):")
        for skill in skills:
            skill_info = next((s for s in self.manifest.get('skills', []) if s['name'] == skill), None)
            desc = skill_info.get('description', '-') if skill_info else '(未在 manifest 中)'
            info(f"  • {skill}: {desc}")

        # 列出目标状态
        info("\n同步目标状态:")
        targets = self.manifest.get('sync_targets', [])
        for target in targets:
            name = target['name']
            path = Path(target['path'])
            enabled = "✓" if target.get('enabled', True) else "✗"
            exists = "存在" if path.exists() else "不存在"

            # 统计已同步的 skill 数量
            if path.exists():
                synced = len([d for d in path.iterdir() if d.is_dir()])
                info(f"  [{enabled}] {name}: {exists} ({synced}/{len(skills)}) 个 skill")
            else:
                info(f"  [{enabled}] {name}: {exists}")

        # 最后同步时间
        last_sync = self.manifest.get('last_sync')
        if last_sync:
            info(f"\n最后同步: {last_sync}")
        else:
            info("\n最后同步: 从未同步过")

# ========== 主入口 ==========

def main():
    parser = argparse.ArgumentParser(
        description="AZ-SKILLS 同步脚本 - 跨 AI 工具的 skill 同步工具",
        formatter_class=argparse.RawDescriptionHelpFormatter,
        epilog="""
示例:
  python sync_skills.py              # 交互模式，同步到所有目标
  python sync_skills.py --full       # 全量同步
  python sync_skills.py --to cline   # 只同步到 cline
  python sync_skills.py --list       # 仅列出状态
  python sync_skills.py --dry-run    # 模拟运行，不实际复制
        """
    )
    parser.add_argument('--full', action='store_true', help='全量同步（包含所有 skills）')
    parser.add_argument('--to', choices=['hermes', 'cline', 'claude_code', 'opencode', 'openclaw'], help='指定同步目标')
    parser.add_argument('--list', action='store_true', help='仅列出当前状态')
    parser.add_argument('--dry-run', action='store_true', help='模拟运行，不实际复制')
    parser.add_argument('--manifest', default=str(MANIFEST_PATH), help=f'Manifest 文件路径 (默认: {MANIFEST_PATH})')

    args = parser.parse_args()

    # 转换为 Path
    manifest_path = Path(args.manifest)

    if not manifest_path.exists():
        error(f"Manifest 文件不存在: {manifest_path}")
        info("请先创建 manifest.yaml 文件")
        sys.exit(1)

    syncer = SkillSyncer(manifest_path)
    syncer.dry_run = args.dry_run

    if args.list:
        syncer.list_status()
        return

    if args.to:
        # 同步到指定目标
        target_labels = {"hermes": "Hermes", "cline": "Cline", "claude_code": "Claude Code", "opencode": "OpenCode", "openclaw": "OpenClaw"}
        result = syncer.sync_to(args.to, full=args.full)
        info(f"\n结果: 成功 {result['success']}, 跳过 {result['skipped']}, 失败 {result['failed']}")
    else:
        # 同步到所有目标
        syncer.sync_all(full=args.full)

if __name__ == "__main__":
    main()
