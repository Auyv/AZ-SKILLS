#!/usr/bin/env python3
"""
AZ-SKILLS 同步脚本
用于将 AZ-SKILLS 中的 skill 同步到各 AI 工具的 skills 目录

同步逻辑：
- 每个 skill 的 targets 列表决定同步到哪些目标
- sync_targets.enabled 全局开关决定目标是否启用

用法:
    python sync_skills.py                    # 同步所有 skill
    python sync_skills.py --list            # 仅列出状态
    python sync_skills.py --to cline         # 只同步到 cline
    python sync_skills.py --to codex         # 只同步到 codex
    python sync_skills.py --dry-run          # 模拟运行，不实际复制
    python sync_skills.py --skill az-sdd.v3  # 只同步指定 skill
"""

import os
import sys
import shutil
import argparse
import subprocess
from pathlib import Path
from datetime import datetime
from typing import List, Dict, Optional
try:
    import yaml
except ModuleNotFoundError:
    yaml = None

# ========== 配置 ==========

SCRIPT_DIR = Path(__file__).parent.resolve()
MANIFEST_PATH = SCRIPT_DIR / "manifest.yaml"
SKILLS_SOURCE_SUBDIR = "skills"


def _simple_yaml_load_manifest(text: str) -> dict:
    """Parse this repository's manifest.yaml when PyYAML is unavailable."""
    data = {"skills": [], "sync_targets": []}
    section = None
    current = None

    for raw_line in text.splitlines():
        line = raw_line.rstrip()
        stripped = line.strip()

        if not stripped or stripped.startswith("#"):
            continue

        if not line.startswith(" ") and stripped.endswith(":"):
            section = stripped[:-1]
            continue

        if stripped.startswith("- "):
            item = stripped[2:]
            if ":" in item:
                key, value = item.split(":", 1)
                key = key.strip()
                value = value.strip()
                current = {key: [] if key == "targets" and not value else _simple_yaml_value(value)}
                if section in ("skills", "sync_targets"):
                    data[section].append(current)
            else:
                if current is not None:
                    current.setdefault("targets", []).append(_simple_yaml_value(item))
            continue

        if not line.startswith(" ") and ":" in stripped:
            key, value = stripped.split(":", 1)
            data[key] = value.strip().strip("'\"")
            continue

        if current is not None and ":" in stripped:
            key, value = stripped.split(":", 1)
            key = key.strip()
            value = value.strip()
            current[key] = [] if key == "targets" and not value else _simple_yaml_value(value)

    return data


def _simple_yaml_value(value: str):
    value = value.strip().strip("'\"")
    if value.lower() == "true":
        return True
    if value.lower() == "false":
        return False
    return value


def _simple_yaml_dump_manifest(manifest: dict) -> str:
    lines = [f"last_sync: '{manifest.get('last_sync', '')}'", "skills:"]
    for skill in manifest.get("skills", []):
        lines.append(f"- name: {skill['name']}")
        lines.append("  targets:")
        for target in skill.get("targets", []):
            lines.append(f"  - {target}")

    lines.append("sync_targets:")
    for target in manifest.get("sync_targets", []):
        enabled = "true" if target.get("enabled", True) else "false"
        lines.append(f"- enabled: {enabled}")
        lines.append(f"  name: {target['name']}")
        lines.append(f"  path: {target['path']}")

    return "\n".join(lines) + "\n"

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


# ========== Windows 权限处理 ==========

def _is_windows_admin() -> bool:
    """Return whether the current process is elevated on Windows."""
    if os.name != "nt":
        return True

    try:
        import ctypes
        return bool(ctypes.windll.shell32.IsUserAnAdmin())
    except Exception:
        return False


def _relaunch_as_windows_admin() -> bool:
    """Relaunch the current command through UAC and return whether it was started."""
    if os.name != "nt":
        return False

    try:
        import ctypes

        def ps_quote(value: str) -> str:
            return "'" + value.replace("'", "''") + "'"

        script_path = str(Path(__file__).resolve())
        python_args = " ".join(ps_quote(arg) for arg in [script_path, *sys.argv[1:]])
        command = (
            f"Set-Location -LiteralPath {ps_quote(str(SCRIPT_DIR))}; "
            f"& {ps_quote(sys.executable)} {python_args}; "
            "$exitCode = $LASTEXITCODE; "
            "Write-Host ''; "
            "Read-Host '执行结束，按 Enter 关闭窗口'; "
            "exit $exitCode"
        )
        params = subprocess.list2cmdline([
            "-NoProfile",
            "-ExecutionPolicy",
            "Bypass",
            "-Command",
            command,
        ])
        result = ctypes.windll.shell32.ShellExecuteW(
            None,
            "runas",
            "powershell.exe",
            params,
            str(SCRIPT_DIR),
            1,
        )
        return result > 32
    except Exception as exc:
        error(f"自动申请管理员权限失败: {exc}")
        return False


def _sync_requires_elevation(args: argparse.Namespace) -> bool:
    """Only real sync operations need elevation for Windows directory symlinks."""
    return os.name == "nt" and not args.list and not args.dry_run and not args.no_elevate

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
            if yaml:
                return yaml.safe_load(f)
            return _simple_yaml_load_manifest(f.read())

    def _save_manifest(self):
        """保存 manifest.yaml"""
        with open(self.manifest_path, 'w', encoding='utf-8') as f:
            if yaml:
                yaml.dump(self.manifest, f, allow_unicode=True, default_flow_style=False)
            else:
                f.write(_simple_yaml_dump_manifest(self.manifest))

    def _get_target_config(self, target_name: str) -> Optional[Dict]:
        """获取目标配置"""
        targets = self.manifest.get('sync_targets', [])
        for t in targets:
            if t.get('name') == target_name:
                return t
        return None

    def _get_target_path(self, target_name: str) -> Optional[Path]:
        """获取目标路径"""
        config = self._get_target_config(target_name)
        if not config:
            return None
        if not config.get('enabled', True):
            return None
        path_str = config.get('path', '')
        if path_str:
            return Path(path_str).expanduser()
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

    def _sync_skill_to_target(self, skill_name: str, target_path: Path, target_name: str) -> bool:
        """同步单个 skill 到目标目录（使用符号链接）"""
        src = self.source_dir / skill_name
        dest = target_path / skill_name

        if not src.exists():
            warning(f"  源 skill 不存在: {src}")
            return False

        if self.dry_run:
            info(f"  → {skill_name} (模拟创建符号链接)")
            return True

        # 执行同步：创建符号链接
        try:
            # 删除已存在的目标
            if dest.exists() or dest.is_symlink():
                if dest.is_symlink():
                    dest.unlink()
                else:
                    shutil.rmtree(dest)

            # 创建符号链接
            dest.symlink_to(src)
            success(f"  ✓ {skill_name}")
            return True
        except Exception as e:
            error(f"  ✗ {skill_name}: {e}")
            return False

    def _get_az_skills_path(self, target_path: Path) -> Path:
        """获取 az-skills 子文件夹路径"""
        return target_path / "az-skills"

    def sync_all(self) -> Dict[str, int]:
        """同步所有 skill 到各自的目标"""
        header("\n🚀 AZ-SKILLS 同步开始")

        total = {"success": 0, "skipped": 0, "failed": 0}
        skills = self._get_source_skills()

        if not skills:
            warning("没有找到需要同步的 skill")
            return total

        # 收集所有需要同步的目标
        targets_to_sync = set()
        for skill_info in self.manifest.get('skills', []):
            skill_name = skill_info['name']
            if skill_name not in skills:
                continue
            for target_name in skill_info.get('targets', []):
                config = self._get_target_config(target_name)
                if config and config.get('enabled', True):
                    targets_to_sync.add(target_name)

        # 先清空所有目标下的 az-skills 子文件夹
        for target_name in targets_to_sync:
            target_path = self._get_target_path(target_name)
            if target_path:
                az_skills_path = self._get_az_skills_path(target_path)
                if not self.dry_run:
                    if az_skills_path.exists():
                        shutil.rmtree(az_skills_path)
                    az_skills_path.mkdir(parents=True, exist_ok=True)

        # 按目标分组统计
        target_stats = {}

        for skill_info in self.manifest.get('skills', []):
            skill_name = skill_info['name']
            if skill_name not in skills:
                info(f"  - {skill_name} (源目录中不存在，跳过)")
                total["skipped"] += 1
                continue

            targets = skill_info.get('targets', [])

            for target_name in targets:
                # 检查目标是否启用
                config = self._get_target_config(target_name)
                if not config or not config.get('enabled', True):
                    continue

                target_path = self._get_target_path(target_name)
                if not target_path:
                    continue

                # 使用 az-skills 子文件夹
                az_skills_path = self._get_az_skills_path(target_path)

                # 初始化目标统计
                if target_name not in target_stats:
                    target_stats[target_name] = {"success": 0, "failed": 0}

                # 执行同步到 az-skills 子文件夹
                if self._sync_skill_to_target(skill_name, az_skills_path, target_name):
                    target_stats[target_name]["success"] += 1
                    total["success"] += 1
                else:
                    target_stats[target_name]["failed"] += 1
                    total["failed"] += 1

        # 显示每个目标的统计
        for target_name, stats in sorted(target_stats.items()):
            info(f"  {target_name}: 成功 {stats['success']}, 失败 {stats['failed']}")

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

    def sync_to_target(self, target_name: str) -> Dict[str, int]:
        """同步到指定目标"""
        config = self._get_target_config(target_name)
        if not config:
            error(f"未找到目标配置: {target_name}")
            return {"success": 0, "skipped": 0, "failed": 0}

        if not config.get('enabled', True):
            warning(f"目标 {target_name} 未启用")
            return {"success": 0, "skipped": 0, "failed": 0}

        target_path = self._get_target_path(target_name)
        if not target_path:
            error(f"未找到目标路径: {target_name}")
            return {"success": 0, "skipped": 0, "failed": 0}

        if not target_path.exists():
            warning(f"目标目录不存在，将创建: {target_path}")
            if not self.dry_run:
                target_path.mkdir(parents=True, exist_ok=True)

        # 在目标目录下创建 az-skills 子文件夹
        az_skills_path = self._get_az_skills_path(target_path)

        # 先清空 az-skills 子文件夹
        if az_skills_path.exists():
            if not self.dry_run:
                shutil.rmtree(az_skills_path)
        if not self.dry_run:
            az_skills_path.mkdir(parents=True, exist_ok=True)

        info(f"\n{'='*50}")
        info(f"同步目标: {target_name}")
        info(f"源目录: {self.source_dir}")
        info(f"目标目录: {az_skills_path}")
        info(f"{'='*50}")

        success_count = 0
        failed_count = 0

        # 获取该目标需要同步的 skills
        target_skills = []
        for skill_info in self.manifest.get('skills', []):
            skill_name = skill_info['name']
            targets = skill_info.get('targets', [])
            if target_name in targets:
                target_skills.append(skill_name)

        for skill in target_skills:
            if self._sync_skill_to_target(skill, az_skills_path, target_name):
                success_count += 1
            else:
                failed_count += 1

        info(f"\n结果: 成功 {success_count}, 跳过 0, 失败 {failed_count}")
        return {
            "success": success_count,
            "skipped": 0,
            "failed": failed_count
        }

    def sync_skill(self, skill_name: str) -> Dict[str, int]:
        """同步指定 skill"""
        info(f"\n同步 skill: {skill_name}")

        # 检查源是否存在
        src = self.source_dir / skill_name
        if not src.exists():
            error(f"源 skill 不存在: {src}")
            return {"success": 0, "skipped": 0, "failed": 0}

        # 获取该 skill 的目标列表
        skill_info = None
        for s in self.manifest.get('skills', []):
            if s['name'] == skill_name:
                skill_info = s
                break

        if not skill_info:
            error(f"未在 manifest 中找到 skill: {skill_name}")
            return {"success": 0, "skipped": 0, "failed": 0}

        targets = skill_info.get('targets', [])
        if not targets:
            warning(f"skill {skill_name} 没有配置 targets")
            return {"success": 0, "skipped": 0, "failed": 0}

        success_count = 0
        failed_count = 0

        for target_name in targets:
            config = self._get_target_config(target_name)
            if not config or not config.get('enabled', True):
                continue

            target_path = self._get_target_path(target_name)
            if not target_path:
                continue

            # 确保目录存在
            if not target_path.exists():
                if not self.dry_run:
                    target_path.mkdir(parents=True, exist_ok=True)

            # 使用 az-skills 子文件夹
            az_skills_path = self._get_az_skills_path(target_path)
            if not az_skills_path.exists():
                if not self.dry_run:
                    az_skills_path.mkdir(parents=True, exist_ok=True)

            info(f"  → {target_name}: {az_skills_path}")
            if self._sync_skill_to_target(skill_name, az_skills_path, target_name):
                success_count += 1
            else:
                failed_count += 1

        return {"success": success_count, "skipped": 0, "failed": failed_count}

    def list_status(self):
        """列出当前状态"""
        header("\n📋 AZ-SKILLS 状态概览")
        info(f"\nManifest: {self.manifest_path}")
        info(f"源目录: {self.source_dir}")

        # 列出源 skills
        skills = self._get_source_skills()
        info(f"\n源 skill 列表 ({len(skills)} 个):")

        manifest_skills = {s['name']: s for s in self.manifest.get('skills', [])}
        for skill in skills:
            skill_info = manifest_skills.get(skill)
            desc = skill_info.get('description', '-') if skill_info else '(未在 manifest 中)'
            targets = skill_info.get('targets', []) if skill_info else []
            info(f"  • {skill}: {desc}")
            if targets:
                info(f"    同步目标: {', '.join(targets)}")

        # 列出目标状态
        info("\n同步目标状态:")
        targets = self.manifest.get('sync_targets', [])
        for target in targets:
            name = target['name']
            path = Path(target['path']).expanduser() if target.get('path') else None
            enabled = "✓" if target.get('enabled', True) else "✗"

            # 使用 az-skills 子文件夹
            az_skills_path = self._get_az_skills_path(path) if path else None
            exists = "存在" if az_skills_path and az_skills_path.exists() else "不存在"

            # 统计该目标应同步的 skill 数量
            target_skills = [s for s in self.manifest.get('skills', []) if name in s.get('targets', [])]

            if az_skills_path and az_skills_path.exists():
                synced = len([d for d in az_skills_path.iterdir() if d.is_symlink()])
                info(f"  [{enabled}] {name}: {az_skills_path} ({synced}/{len(target_skills)}) 个 skill")
            else:
                info(f"  [{enabled}] {name}: {path} (az-skills 子文件夹不存在)")

        # 最后同步时间
        last_sync = self.manifest.get('last_sync')
        if last_sync:
            info(f"\n最后同步: {last_sync}")
        else:
            info("\n最后同步: 从未同步过")

# ========== 主入口 ==========

def main():
    parser = argparse.ArgumentParser(
        description="AZ-SKILLS 同步脚本 - 基于 targets 配置的跨工具同步",
        formatter_class=argparse.RawDescriptionHelpFormatter,
        epilog="""
示例:
  python sync_skills.py              # 同步所有 skill
  python sync_skills.py --to cline   # 只同步到 cline
  python sync_skills.py --to codex   # 只同步到 codex
  python sync_skills.py --skill az-sdd.v3  # 只同步指定 skill
  python sync_skills.py --list       # 仅列出状态
  python sync_skills.py --dry-run    # 模拟运行，不实际复制
        """
    )
    parser.add_argument('--to', choices=['hermes', 'cline', 'claude_code', 'opencode', 'openclaw', 'codex'], help='指定同步目标')
    parser.add_argument('--skill', help='指定同步的 skill 名称')
    parser.add_argument('--list', action='store_true', help='仅列出当前状态')
    parser.add_argument('--dry-run', action='store_true', help='模拟运行，不实际复制')
    parser.add_argument('--no-elevate', action='store_true', help='Windows 下不自动申请管理员权限')
    parser.add_argument('--manifest', default=str(MANIFEST_PATH), help=f'Manifest 文件路径 (默认: {MANIFEST_PATH})')

    args = parser.parse_args()

    if _sync_requires_elevation(args) and not _is_windows_admin():
        info("检测到 Windows 符号链接同步需要管理员权限，正在请求 UAC 授权...")
        if _relaunch_as_windows_admin():
            info("已启动管理员权限同步窗口，当前进程退出。")
            return
        error("未能获取管理员权限，请手动以管理员身份运行，或使用 --no-elevate 跳过自动提权。")
        sys.exit(1)

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

    if args.skill:
        # 同步指定 skill
        result = syncer.sync_skill(args.skill)
        info(f"\n结果: 成功 {result['success']}, 跳过 {result['skipped']}, 失败 {result['failed']}")
        if not syncer.dry_run:
            syncer.manifest['last_sync'] = datetime.now().isoformat()
            syncer._save_manifest()
        return

    if args.to:
        # 同步到指定目标
        result = syncer.sync_to_target(args.to)
        if not syncer.dry_run:
            syncer.manifest['last_sync'] = datetime.now().isoformat()
            syncer._save_manifest()
        return

    # 无参数时，进入交互式菜单
    _interactive_menu(syncer)

def _interactive_menu(syncer: SkillSyncer):
    """交互式菜单选择同步目标"""
    targets = syncer.manifest.get('sync_targets', [])
    enabled_targets = [t for t in targets if t.get('enabled', True)]

    if not enabled_targets:
        error("没有已启用的同步目标")
        return

    header("\n🎯 选择同步目标")
    info("输入序号选择（多个用空格分隔），或输入 'a' 全选，'q' 退出\n")

    for i, t in enumerate(enabled_targets, 1):
        name = t['name']
        path = syncer._get_target_path(name)
        path_str = str(path) if path else "(未配置)"
        exists = "✓" if path and path.exists() else "✗"

        # 统计该目标应同步的 skill 数量
        target_skills = [s for s in syncer.manifest.get('skills', []) if name in s.get('targets', [])]
        info(f"  [{i}] {name}: {path_str} [{exists}] ({len(target_skills)} skills)")

    print()
    try:
        choice = input("请选择 > ").strip().lower()
    except (EOFError, KeyboardInterrupt):
        print()
        return

    if choice == 'q':
        return

    if choice == 'a':
        selected = enabled_targets
    else:
        try:
            indices = [int(x) for x in choice.split()]
            selected = [enabled_targets[i-1] for i in indices if 1 <= i <= len(enabled_targets)]
        except (ValueError, IndexError):
            error("无效输入")
            return

    if not selected:
        error("未选择任何目标")
        return

    header(f"\n开始同步到 {len(selected)} 个目标")
    total = {"success": 0, "skipped": 0, "failed": 0}

    for target in selected:
        result = syncer.sync_to_target(target['name'])
        total["success"] += result["success"]
        total["skipped"] += result["skipped"]
        total["failed"] += result["failed"]

    if not syncer.dry_run:
        syncer.manifest['last_sync'] = datetime.now().isoformat()
        syncer._save_manifest()

    header("\n" + "="*50)
    success(f"同步完成! 成功: {total['success']}, 跳过: {total['skipped']}, 失败: {total['failed']}")
    if syncer.dry_run:
        warning(" (模拟模式，未实际复制)")
    header("="*50)

if __name__ == "__main__":
    main()
