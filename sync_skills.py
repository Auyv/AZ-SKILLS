#!/usr/bin/env python3
"""
AZ-SKILLS 同步脚本
用于将 AZ-SKILLS 中的 skill 同步到各 AI 工具的 skills 目录

同步逻辑：
- agent_targets 列表定义每个 AI 工具目标及其 skills 列表
- agent_targets.enabled 开关决定目标是否启用
- 双模式：显式 skills 字段为白名单模式；省略 skills 改用 exclude 字段则为
  黑名单模式（同步 全部源技能 - exclude），新增技能自动下发到黑名单目标

用法:
    python sync_skills.py                    # 无参数: 进入交互式 Shell
    python sync_skills.py --list            # 仅列出状态
    python sync_skills.py --to cline         # 只同步到 cline
    python sync_skills.py --to codex         # 只同步到 codex
    python sync_skills.py --dry-run          # 模拟运行，不实际复制
    python sync_skills.py --skill az-skill-creator  # 只同步指定 skill
    python sync_skills.py --skill az-skill-creator --to codex  # 将指定 skill 同步到一个目标

无参数时进入交互式 Shell（命令风格，支持 Tab 补全），可用命令：
    help [cmd]             显示帮助（可指定命令名查看详情）
    status, ls             查看 manifest 状态
    sync, s [opts] [targets...]
                           同步 skills（--all / --dry-run / --skills s1,s2）
    enable <target>...     启用一个或多个平台
    disable <target>...    禁用一个或多个平台
    target <name> ...      管理平台 skills (list/add/remove/clear/convert/policy/exclude)
    skills <name>          查看平台已配置的 skills
    quit, q, exit          退出
"""

import os
import re
import sys
import shlex
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

try:
    import readline  # Unix 标准库
    READLINE_AVAILABLE = True
except ImportError:
    try:
        import pyreadline3 as readline  # Windows 推荐
        READLINE_AVAILABLE = True
    except ImportError:
        try:
            import pyreadline as readline  # 旧版 Windows
            READLINE_AVAILABLE = True
        except ImportError:
            READLINE_AVAILABLE = False
            readline = None  # type: ignore

# ========== 配置 ==========

SCRIPT_DIR = Path(__file__).parent.resolve()
MANIFEST_PATH = SCRIPT_DIR / "manifest.yaml"
SKILLS_SOURCE_SUBDIR = "skills"


def _simple_yaml_load_manifest(text: str) -> dict:
    """Parse this repository's manifest.yaml when PyYAML is unavailable."""
    data = {"agent_targets": [], "skills": [], "sync_targets": []}
    section = None
    current = None
    current_list_key = None

    for raw_line in text.splitlines():
        line = raw_line.rstrip()
        stripped = line.strip()

        if not stripped or stripped.startswith("#"):
            continue

        if not line.startswith(" ") and stripped.endswith(":"):
            section = stripped[:-1]
            current = None
            current_list_key = None
            continue

        if stripped.startswith("- "):
            item = stripped[2:]
            if ":" in item:
                key, value = item.split(":", 1)
                key = key.strip()
                value = value.strip()
                list_keys = ("targets", "skills", "exclude")
                current = {key: [] if key in list_keys and not value else _simple_yaml_value(value)}
                current_list_key = key if key in list_keys and not value else None
                if section in ("skills", "sync_targets", "agent_targets"):
                    data[section].append(current)
            else:
                if current is not None:
                    list_key = current_list_key or "targets"
                    current.setdefault(list_key, []).append(_simple_yaml_value(item))
            continue

        if not line.startswith(" ") and ":" in stripped:
            key, value = stripped.split(":", 1)
            data[key] = value.strip().strip("'\"")
            continue

        if current is not None and ":" in stripped:
            key, value = stripped.split(":", 1)
            key = key.strip()
            value = value.strip()
            list_keys = ("targets", "skills", "exclude")
            current[key] = [] if key in list_keys and not value else _simple_yaml_value(value)
            current_list_key = key if key in list_keys and not value else None

    return data


def _simple_yaml_value(value: str):
    value = value.strip().strip("'\"")
    if value.lower() == "true":
        return True
    if value.lower() == "false":
        return False
    return value


def _simple_yaml_dump_manifest(manifest: dict) -> str:
    lines = [f"last_sync: '{manifest.get('last_sync', '')}'", "agent_targets:"]
    for target in manifest.get("agent_targets", manifest.get("sync_targets", [])):
        enabled = "true" if target.get("enabled", True) else "false"
        if "aggregate_subdir" in target:
            lines.append(f"- aggregate_subdir: '{target.get('aggregate_subdir', '')}'")
            lines.append(f"  enabled: {enabled}")
        else:
            lines.append(f"- enabled: {enabled}")
        lines.append(f"  name: {target['name']}")
        lines.append(f"  path: {target['path']}")
        if "skills" in target:
            lines.append("  skills:")
            for skill in target.get("skills") or []:
                lines.append(f"  - {skill}")
        else:
            lines.append("  exclude:")
            for skill in target.get("exclude") or []:
                lines.append(f"  - {skill}")

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
    output = f"{color}{text}{Colors.ENDC}" if sys.stdout.isatty() and color else text
    try:
        print(output)
    except UnicodeEncodeError:
        encoding = sys.stdout.encoding or "utf-8"
        safe_output = output.encode(encoding, errors="replace").decode(encoding, errors="replace")
        print(safe_output)

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
    """Directory copy fallback allows Windows sync to run without UAC."""
    return False

# ========== 核心逻辑 ==========

class SkillSyncer:
    def __init__(self, manifest_path: Path):
        self.manifest_path = manifest_path
        self.manifest_dir = manifest_path.parent
        self.source_dir = self.manifest_dir / SKILLS_SOURCE_SUBDIR
        self.manifest = self._load_manifest()
        self.dry_run = False

    def _get_source_roots(self) -> List[Path]:
        """返回技能源码目录；工作流型 Skill 也统一位于 skills/。"""
        return [self.source_dir]

    def _get_skill_source(self, skill_name: str) -> Optional[Path]:
        """按名称定位唯一的技能源码目录，避免跨目录重名。"""
        matches = [root / skill_name for root in self._get_source_roots() if (root / skill_name).exists()]
        if len(matches) == 1:
            return matches[0]
        if len(matches) > 1:
            error(f"技能名称重复，无法确定源码: {skill_name} ({', '.join(str(item) for item in matches)})")
        return None

    def _source_roots_text(self) -> str:
        return ', '.join(str(root) for root in self._get_source_roots())

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
                yaml.dump(self.manifest, f, allow_unicode=True, default_flow_style=False, sort_keys=False)
            else:
                f.write(_simple_yaml_dump_manifest(self.manifest))

    def _get_agent_targets(self) -> List[Dict]:
        """获取 target-centric 配置；兼容旧版 sync_targets。"""
        return self.manifest.get('agent_targets') or self.manifest.get('sync_targets', [])

    def _get_target_config(self, target_name: str) -> Optional[Dict]:
        """获取目标配置"""
        targets = self._get_agent_targets()
        for t in targets:
            if t.get('name') == target_name:
                return t
        return None

    @staticmethod
    def _target_mode(config: Optional[Dict]) -> str:
        """返回 target 同步模式：'include'（白名单）或 'exclude'（黑名单）。

        判定规则：显式存在 ``skills`` 字段即为白名单；否则为黑名单，
        使用 ``exclude`` 列表（缺省视为空）。
        """
        if config and 'skills' in config:
            return 'include'
        return 'exclude'

    def _get_target_exclude(self, target_name: str) -> List[str]:
        """获取黑名单模式下 target 的 exclude 列表。"""
        config = self._get_target_config(target_name)
        if not config:
            return []
        return list(config.get('exclude') or [])

    def _get_target_skill_names(self, target_name: str) -> List[str]:
        """获取指定 target 应同步的 skills。

        - 白名单模式（显式 ``skills`` 字段）：直接使用该列表
        - 黑名单模式：全部源技能 − exclude
        - 兼容旧版 manifest 顶层 skills[].targets
        """
        config = self._get_target_config(target_name)
        if config:
            if 'skills' in config:
                return list(config.get('skills') or [])
            excluded = set(config.get('exclude') or [])
            return [s for s in self._get_source_skills() if s not in excluded]

        skill_names = []
        for skill_info in self.manifest.get('skills', []):
            if target_name in skill_info.get('targets', []):
                skill_names.append(skill_info['name'])
        return skill_names

    def _get_skill_targets(self, skill_name: str) -> List[str]:
        """获取指定 skill 要同步到的 target 列表（含黑名单模式目标）。"""
        if self.manifest.get('agent_targets'):
            result = []
            for target in self._get_agent_targets():
                name = target.get('name')
                if not name:
                    continue
                if 'skills' in target:
                    if skill_name in (target.get('skills') or []):
                        result.append(name)
                else:
                    if skill_name not in set(target.get('exclude') or []):
                        result.append(name)
            return result

        for skill_info in self.manifest.get('skills', []):
            if skill_info.get('name') == skill_name:
                return list(skill_info.get('targets', []))
        return []

    def _get_manifest_skill_names(self) -> List[str]:
        """获取 manifest 管理的 skill 名称。

        存在任一黑名单模式 target 时，全部源技能均视为受管。
        """
        if self.manifest.get('agent_targets'):
            names = []
            seen = set()
            has_blacklist = False
            for target in self._get_agent_targets():
                if 'skills' not in target:
                    has_blacklist = True
                    continue
                for skill_name in target.get('skills') or []:
                    if skill_name not in seen:
                        seen.add(skill_name)
                        names.append(skill_name)
            if has_blacklist:
                for skill_name in self._get_source_skills():
                    if skill_name not in seen:
                        seen.add(skill_name)
                        names.append(skill_name)
            return names
        return [s['name'] for s in self.manifest.get('skills', [])]

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
        """获取 skills/ 目录中的所有 skill。"""
        skills = set()
        for source_root in self._get_source_roots():
            if not source_root.exists():
                continue
            for item in source_root.iterdir():
                if item.is_dir():
                    skills.add(item.name)
        if not skills:
            warning(f"源目录不存在或没有技能: {self._source_roots_text()}")
        return sorted(skills)

    def _sync_skill_to_target(self, skill_name: str, target_path: Path, target_name: str) -> bool:
        """同步单个 skill 到目标目录。

        - 如果 src 根下有 SKILL.md，按 skill_name 平铺到 target_path
        - 如果 src 是聚合容器（根下无 SKILL.md），扫描直接子目录，
          把每个含 SKILL.md 的子目录平铺到 target_path（用 child.name 命名）
        - 聚合容器若含 shared/，额外发布为独立的 <容器名>-shared 运行时目录
        """
        src = self._get_skill_source(skill_name)

        if src is None:
            warning(f"  源 skill 不存在或名称重复: {skill_name}")
            return False

        # 聚合容器：根下无 SKILL.md，递归展开子目录
        if not (src / 'SKILL.md').exists():
            sub_skills = sorted([
                c for c in src.iterdir()
                if c.is_dir() and (c / 'SKILL.md').exists()
            ])
            if not sub_skills:
                warning(f"  {skill_name} 根下无 SKILL.md 且无含 SKILL.md 的子目录，跳过")
                return False
            info(f"  {skill_name} 是聚合容器，展开 {len(sub_skills)} 个子 skill:")
            all_ok = True
            for sub in sub_skills:
                if not self._link_one(sub.name, sub, target_path):
                    all_ok = False
            shared_dir = src / 'shared'
            if shared_dir.is_dir():
                shared_label = self._aggregate_shared_label(skill_name)
                info(f"  发布共享运行时: {shared_label}")
                if not self._link_one(shared_label, shared_dir, target_path):
                    all_ok = False
            return all_ok

        return self._link_one(skill_name, src, target_path)

    @staticmethod
    def _aggregate_shared_label(skill_name: str) -> str:
        """为聚合技能的 shared/ 生成稳定且不带版本号的目标名称。"""
        base_name = re.sub(r'\.v\d+(?:\.\d+)*$', '', skill_name)
        return f"{base_name}-shared"

    def _link_one(self, label: str, src: Path, target_path: Path) -> bool:
        """创建单个同步项：优先 symlink，Windows 失败时尝试 junction，再回退为目录复制。"""
        dest = target_path / label

        if self.dry_run:
            info(f"  → {label} (模拟同步)")
            return True

        try:
            target_path.mkdir(parents=True, exist_ok=True)

            # 删除已存在的目标。Windows junction 的源目录被移动后，
            # dest.exists() 会返回 False，因此必须独立识别悬空 junction。
            is_junction = getattr(dest, "is_junction", lambda: False)()
            if dest.exists() or dest.is_symlink() or is_junction:
                if dest.is_symlink() or is_junction:
                    dest.unlink()
                else:
                    shutil.rmtree(dest)

            link_errors = []

            # 优先 symlink；Windows 用户模式无特权时 fallback 到 junction
            try:
                dest.symlink_to(src, target_is_directory=src.is_dir())
            except (OSError, NotImplementedError) as sym_err:
                link_errors.append(f"symlink 失败: {sym_err}")
                if os.name == 'nt':
                    r = subprocess.run(
                        ['cmd', '/c', 'mklink', '/J', str(dest), str(src)],
                        capture_output=True, text=True,
                        encoding='gbk', errors='replace',
                    )
                    if r.returncode != 0:
                        link_errors.append(f"junction 失败: {r.stderr.strip()}")
                    else:
                        success(f"  ✓ {label}")
                        return True
                else:
                    link_errors.append("当前平台不支持 junction fallback")

                shutil.copytree(src, dest)
                warning(f"  ! {label}: 已回退为目录复制 ({'; '.join(link_errors)})")
                return True

            success(f"  ✓ {label}")
            return True
        except Exception as e:
            error(f"  ✗ {label}: {e}")
            return False

    def _collect_target_labels(self, target_name: str, skill_names: Optional[List[str]] = None) -> List[str]:
        """收集 manifest 中 target_name 应同步的所有 skill 标签（含聚合容器展开）。

        返回的 label 集合是 manifest **管理**的 skill 名，用于后续定向清理。
        用户在目标目录里自建的 skill 不会被纳入此集合。

        skill_names: 可选，自定义 skills 列表；为 None 时使用 manifest 配置。
        """
        if skill_names is None:
            skill_names = self._get_target_skill_names(target_name)

        labels: List[str] = []
        for sn in skill_names:
            src = self._get_skill_source(sn)
            if src is None:
                continue
            if (src / 'SKILL.md').exists():
                labels.append(sn)
            else:
                # 聚合容器：展开子目录
                for c in sorted(src.iterdir()):
                    if c.is_dir() and (c / 'SKILL.md').exists():
                        labels.append(c.name)
                if (src / 'shared').is_dir():
                    labels.append(self._aggregate_shared_label(sn))
        return labels

    def _purge_managed_links(
        self, az_skills_path: Path, current_labels: Optional[List[str]] = None
    ) -> None:
        """删除目标目录中由本仓库管理的旧 link，保留用户自建内容。

        受管项按链接的真实目标判断：只有指向本仓库 ``skills/`` 源目录的
        symlink/junction 才会删除。不能只按当前 manifest 标签过滤，否则已经
        从目标 skills 清单移除的历史链接永远不会进入清理范围。

        ``current_labels`` 为兼容既有内部调用保留；受管判断不依赖当前清单。
        普通目录、外部链接和任何其它内容均不动。
        """
        if not az_skills_path.exists():
            return

        source_roots = [source_root.resolve() for source_root in self._get_source_roots()]
        for entry in az_skills_path.iterdir():
            if not (entry.is_symlink() or entry.is_junction()):
                continue

            try:
                resolved = entry.resolve(strict=False)
            except OSError:
                continue
            if not any(self._is_within_source_root(resolved, source_root) for source_root in source_roots):
                continue

            try:
                entry.unlink()
            except Exception as e:
                warning(f"  清理旧 link 失败 {entry.name}: {e}")

    @staticmethod
    def _is_within_source_root(path: Path, source_root: Path) -> bool:
        try:
            path.relative_to(source_root)
            return True
        except ValueError:
            return False

    def _get_az_skills_path(self, target_name: str, target_path: Path) -> Path:
        """获取 skill 同步根目录。

        大多数 target 默认聚合在 <target_path>/az-skills/ 子目录。
        claude_code 等不支持聚合的 target 可在 sync_targets 里设
        aggregate_subdir: '' 来直接平铺到 <target_path> 根。
        """
        config = self._get_target_config(target_name)
        sub = config.get('aggregate_subdir', 'az-skills') if config else 'az-skills'
        if sub:
            return target_path / sub
        return target_path

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
        for target in self._get_agent_targets():
            target_name = target.get('name')
            if target_name and target.get('enabled', True):
                targets_to_sync.add(target_name)

        # 仅清掉 manifest 管理的旧 link，保留用户在目标目录里自建的 skill
        for target_name in targets_to_sync:
            target_path = self._get_target_path(target_name)
            if target_path:
                az_skills_path = self._get_az_skills_path(target_name, target_path)
                if not self.dry_run:
                    try:
                        az_skills_path.mkdir(parents=True, exist_ok=True)
                        self._purge_managed_links(
                            az_skills_path, self._collect_target_labels(target_name)
                        )
                    except Exception as e:
                        warning(f"  目标目录准备失败 {az_skills_path}: {e}")

        # 按目标分组统计
        target_stats = {}

        for target in self._get_agent_targets():
            target_name = target.get('name')
            if not target_name or not target.get('enabled', True):
                continue

            target_path = self._get_target_path(target_name)
            if not target_path:
                continue

            az_skills_path = self._get_az_skills_path(target_name, target_path)
            if target_name not in target_stats:
                target_stats[target_name] = {"success": 0, "failed": 0}

            for skill_name in self._get_target_skill_names(target_name):
                if skill_name not in skills:
                    info(f"  - {skill_name} (源目录中不存在，跳过)")
                    total["skipped"] += 1
                    continue

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

    def sync_to_target(self, target_name: str, skill_names: Optional[List[str]] = None) -> Dict[str, int]:
        """同步到指定目标

        skill_names: 可选，自定义本次要同步的 skills 子集；为 None 时使用 manifest 配置。
        """
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

        try:
            target_exists = target_path.exists()
        except Exception as e:
            target_exists = True
            warning(f"检查目标目录失败 {target_path}: {e}")

        if not target_exists:
            warning(f"目标目录不存在，将创建: {target_path}")

        # 在目标目录下创建 az-skills 子文件夹（不强制清空整个目录以避免与运行中的进程冲突）
        az_skills_path = self._get_az_skills_path(target_name, target_path)

        # 确定本次要同步的 skills（skill_names 为 None 时使用 manifest 配置）
        target_skills = skill_names if skill_names is not None else self._get_target_skill_names(target_name)

        # 仅清掉 manifest 管理的旧 link，保留用户在目标目录里自建的 skill
        if not self.dry_run:
            try:
                az_skills_path.mkdir(parents=True, exist_ok=True)
                self._purge_managed_links(
                    az_skills_path, self._collect_target_labels(target_name, skill_names=target_skills)
                )
            except Exception as e:
                warning(f"  目标目录准备失败 {az_skills_path}: {e}")

        info(f"\n{'='*50}")
        info(f"同步目标: {target_name}")
        info(f"源目录: {self._source_roots_text()}")
        info(f"目标目录: {az_skills_path}")
        if skill_names is not None:
            info(f"本次 skills (自定义): {len(target_skills)} 个")
        info(f"{'='*50}")

        success_count = 0
        failed_count = 0

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

    def sync_skill(self, skill_name: str, target_name: Optional[str] = None) -> Dict[str, int]:
        """同步指定 skill"""
        info(f"\n同步 skill: {skill_name}")

        # 检查源是否存在
        src = self._get_skill_source(skill_name)
        if src is None:
            error(f"源 skill 不存在或名称重复: {skill_name}")
            return {"success": 0, "skipped": 0, "failed": 0}

        # 获取该 skill 的目标列表
        targets = self._get_skill_targets(skill_name)
        if target_name is not None:
            if target_name not in targets:
                error(f"skill 未配置到目标 {target_name}: {skill_name}")
                return {"success": 0, "skipped": 0, "failed": 0}
            targets = [target_name]
        if not targets:
            error(f"未在 manifest 中找到 skill: {skill_name}")
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

            # 使用 az-skills 子文件夹
            az_skills_path = self._get_az_skills_path(target_name, target_path)

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
        info(f"源目录: {self._source_roots_text()}")

        # 列出源 skills
        skills = self._get_source_skills()
        info(f"\n源 skill 列表 ({len(skills)} 个):")

        manifest_skill_names = set(self._get_manifest_skill_names())
        for skill in skills:
            in_manifest = skill in manifest_skill_names
            desc = '-' if in_manifest else '(未在 manifest 中)'
            targets = self._get_skill_targets(skill) if in_manifest else []
            info(f"  • {skill}: {desc}")
            if targets:
                info(f"    同步目标: {', '.join(targets)}")

        # 列出目标状态
        info("\n同步目标状态:")
        targets = self._get_agent_targets()
        for target in targets:
            name = target['name']
            raw_path = target.get('path')
            path = Path(raw_path).expanduser() if raw_path else None
            enabled = "✓" if target.get('enabled', True) else "✗"

            if self._target_mode(target) == 'include':
                mode_desc = "白名单"
            else:
                excluded = len(target.get('exclude') or [])
                mode_desc = f"黑名单, 排除{excluded}"

            if not path:
                info(f"  [{enabled}] {name} ({mode_desc}): (未配置路径)")
                continue

            # 使用配置的聚合子文件夹（claude_code 等为平铺）
            az_skills_path = self._get_az_skills_path(name, path)

            if az_skills_path.exists():
                labels = self._collect_target_labels(name)
                synced = len([label for label in labels if (az_skills_path / label).exists()])
                info(f"  [{enabled}] {name} ({mode_desc}): {az_skills_path} ({synced}/{len(labels)}) 个 skill")
            else:
                info(f"  [{enabled}] {name} ({mode_desc}): {path} (目标目录不存在)")

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
  python sync_skills.py              # 进入交互式 Shell
  python sync_skills.py --to cline   # 只同步到 cline
  python sync_skills.py --to codex   # 只同步到 codex
  python sync_skills.py --skill az-skill-creator  # 只同步指定 skill
  python sync_skills.py --list       # 仅列出状态
  python sync_skills.py --dry-run    # 模拟运行，不实际复制
  python sync_skills.py --convert pi # 将 pi 从白名单转为黑名单模式
        """
    )
    parser.add_argument('--to', choices=['hermes', 'cline', 'claude_code', 'opencode', 'openclaw', 'codex', 'pi', 'dsh'], help='指定同步目标')
    parser.add_argument('--convert', metavar='TARGET', help='将指定目标从白名单转为黑名单模式 (可见集合不变)')
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
        info(f"请先复制 {MANIFEST_PATH.name}.demo 为 {MANIFEST_PATH.name}，再按本机平台修改")
        sys.exit(1)

    syncer = SkillSyncer(manifest_path)
    syncer.dry_run = args.dry_run

    if args.list:
        syncer.list_status()
        return

    if args.convert:
        config = syncer._get_target_config(args.convert)
        if not config:
            error(f"未找到目标配置: {args.convert}")
            sys.exit(1)
        shell = InteractiveShell(syncer)
        shell._target_convert(config)
        return

    if args.skill:
        # 同步指定 skill
        result = syncer.sync_skill(args.skill, target_name=args.to)
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

    # 无参数时，进入交互式 Shell
    _interactive_menu(syncer)


# ========== 交互式 Shell ==========

class CommandCompleter:
    """Tab 补全器 - 根据当前输入位置返回候选列表"""

    COMMANDS = ['help', 'status', 'sync', 'enable', 'disable',
                'target', 'skills', 'quit']
    SYNC_FLAGS = ['--all', '--dry-run', '-n', '--skills']
    TARGET_SUBCMDS = ['list', 'add', 'remove', 'clear',
                      'convert', 'policy', 'exclude']
    POLICY_VALUES = ['include', 'exclude']
    EXCLUDE_SUBCMDS = ['list', 'add', 'remove', 'clear']
    ALIASES = {
        'h': 'help', '?': 'help',
        'ls': 'status', 'list': 'status',
        's': 'sync',
        'q': 'quit', 'exit': 'quit',
    }

    def __init__(self, syncer: 'SkillSyncer'):
        self.syncer = syncer
        self._targets_cache: Optional[List[str]] = None
        self._skills_cache: Optional[List[str]] = None

    @property
    def targets(self) -> List[str]:
        if self._targets_cache is None:
            self._targets_cache = [t['name'] for t in self.syncer._get_agent_targets()]
        return self._targets_cache

    @property
    def skills(self) -> List[str]:
        if self._skills_cache is None:
            self._skills_cache = self.syncer._get_source_skills()
        return self._skills_cache

    def target_skills(self, target_name: str) -> List[str]:
        config = self.syncer._get_target_config(target_name)
        return list(config.get('skills') or []) if config else []

    def complete(self, text: str, state: int):
        if not READLINE_AVAILABLE:
            return None
        try:
            line = readline.get_line_buffer()
            begin = readline.get_begidx()

            before = line[:begin]
            words = before.split()
            after_space = before.endswith(' ')

            candidates = self._candidates(words, after_space)
            matches = [c for c in candidates if c.startswith(text)]

            if state < len(matches):
                return matches[state]
            return None
        except Exception:
            return None

    def _candidates(self, words: List[str], after_space: bool) -> List[str]:
        if not words:
            return list(self.COMMANDS) + ['h', 's', 'q', '?', 'ls', 'exit']

        cmd_raw = words[0].lower()
        cmd = self.ALIASES.get(cmd_raw, cmd_raw)

        if cmd == 'help':
            return list(self.COMMANDS) + ['h', 's', 'q', '?', 'ls', 'exit']

        if cmd == 'status':
            return []

        if cmd == 'sync':
            return self._sync_candidates(words, after_space)

        if cmd in ('enable', 'disable'):
            return self.targets

        if cmd == 'target':
            return self._target_candidates(words, after_space)

        if cmd == 'skills':
            return self.targets

        if cmd == 'quit':
            return []

        return []

    def _sync_candidates(self, words: List[str], after_space: bool) -> List[str]:
        # sync [--all] [--dry-run] [--skills s1,s2,...] [targets...]
        n = len(words)

        if n == 1:
            return self.SYNC_FLAGS + self.targets

        # 2+ words: 取“上一个已完成的 token”做上下文判断
        # after_space=True 表示开始输入新 token，前一个就是 words[-1]
        # after_space=False 表示正在补全当前 token，前一个是 words[-2]
        prev = words[-1] if after_space else (words[-2] if n >= 2 else '')
        if prev in ('--skills', '-s'):
            return self.skills

        return self.SYNC_FLAGS + self.targets

    def _target_candidates(self, words: List[str], after_space: bool) -> List[str]:
        # target <name> <subcmd> [skill...]
        n = len(words)

        if n == 1:
            return self.targets

        if n == 2 and not after_space:
            return self.targets

        target_name = words[1]
        if target_name not in self.targets:
            return []

        # 有 target 名，开始补 subcmd
        if n == 2 or (n == 3 and not after_space):
            return self.TARGET_SUBCMDS

        subcmd = words[2]
        if subcmd == 'add':
            return self.skills
        if subcmd == 'remove':
            return self.target_skills(target_name)
        if subcmd == 'policy':
            if n == 3 or (n == 4 and not after_space):
                return self.POLICY_VALUES
            return []
        if subcmd == 'exclude':
            if n == 3 or (n == 4 and not after_space):
                return self.EXCLUDE_SUBCMDS
            excl_sub = words[3]
            if excl_sub == 'add':
                return self.skills
            if excl_sub == 'remove':
                return self.syncer._get_target_exclude(target_name)
            return []
        return []


class InteractiveShell:
    """交互式 Shell - 命令风格界面（支持 Tab 补全 + 历史）"""

    PROMPT = "az-skills> "
    HISTORY_FILE = Path.home() / ".az_skills_history"

    def __init__(self, syncer: 'SkillSyncer'):
        self.syncer = syncer
        self.running = True
        self._setup_readline()

    # ---- readline 集成 ----

    def _setup_readline(self):
        if not READLINE_AVAILABLE:
            return
        try:
            completer = CommandCompleter(self.syncer)
            readline.set_completer(completer.complete)
            readline.parse_and_bind('tab: complete')
            try:
                readline.set_completer_delims(' \t\n')
            except Exception:
                pass

            # 加载历史
            try:
                if self.HISTORY_FILE.exists():
                    readline.read_history_file(str(self.HISTORY_FILE))
            except Exception:
                pass

            # 退出时保存历史
            import atexit
            def _save():
                try:
                    readline.write_history_file(str(self.HISTORY_FILE))
                except Exception:
                    pass
            atexit.register(_save)
        except Exception as e:
            warning(f"readline 初始化失败: {e}")

    # ---- 命令实现 ----

    def cmd_help(self, args):
        """显示帮助（可指定命令名查看详情）"""
        if not args:
            self._print_main_help()
            return
        cmd_raw = args[0].lower()
        cmd = CommandCompleter.ALIASES.get(cmd_raw, cmd_raw)
        method = getattr(self, f'cmd_{cmd}', None)
        if method and method.__doc__:
            info(f"  {cmd}: {method.__doc__.strip()}")
        else:
            warning(f"未知命令: {cmd_raw}")

    def _print_main_help(self):
        header("\n可用命令:")
        info("  help [cmd]             显示帮助（可指定命令名查看详情）")
        info("  status, ls, list       查看 manifest 状态")
        info("  sync, s [opts] [targets...]   同步 skills（help sync 查看详情）")
        info("  enable <target>...     启用一个或多个平台")
        info("  disable <target>...    禁用一个或多个平台")
        info("  target <name> ...      管理平台 skills（help target 查看详情）")
        info("  skills <name>          查看平台已配置的 skills")
        info("  quit, q, exit          退出")
        info("")
        info("提示: Tab 补全 / ↑↓ 历史 / Ctrl+C 取消当前输入 / quit 退出")
        header("")

    def cmd_status(self, args):
        """查看 manifest 状态概览"""
        self.syncer.list_status()

    def cmd_sync(self, args):
        """sync [--all] [--dry-run|-n] [--skills s1,s2,...] [target1 target2 ...]

        示例:
          sync --all                          同步所有启用平台
          sync hermes cline                   同步指定平台
          sync hermes --dry-run               模拟同步
          sync hermes --skills foo,bar        用自定义 skills 子集同步
          sync --all --dry-run                全平台模拟
        """
        if not args:
            warning("用法: sync --all | sync <target>... [--dry-run] [--skills s1,s2,...]")
            return

        sync_all = False
        dry_run = False
        target_names: List[str] = []
        skills_override: Optional[List[str]] = None

        i = 0
        while i < len(args):
            arg = args[i]
            if arg in ('--all', '-a'):
                sync_all = True
            elif arg in ('--dry-run', '-n'):
                dry_run = True
            elif arg in ('--skills', '-s'):
                i += 1
                if i < len(args):
                    skills_override = [s.strip() for s in args[i].split(',') if s.strip()]
            elif arg.startswith('-'):
                warning(f"未知参数: {arg}")
                return
            else:
                target_names.append(arg)
            i += 1

        if sync_all:
            selected = [t for t in self.syncer._get_agent_targets() if t.get('enabled', True)]
        elif target_names:
            selected = []
            for name in target_names:
                config = self.syncer._get_target_config(name)
                if not config:
                    warning(f"未找到目标: {name}")
                    continue
                if not config.get('enabled', True):
                    warning(f"目标未启用（已跳过）: {name}")
                    continue
                selected.append(config)
        else:
            warning("请指定同步目标（--all 或平台名）")
            return

        if not selected:
            warning("无有效目标")
            return

        # 确认
        info(f"\n即将同步到 {len(selected)} 个平台: {', '.join(t['name'] for t in selected)}")
        if skills_override is not None:
            info(f"自定义 skills: {', '.join(skills_override) if skills_override else '(空)'}")
        else:
            info("使用 manifest 默认 skills 配置")
        if dry_run:
            info("模式: 模拟运行")

        try:
            confirm = input("确认执行? [Y/n] > ").strip().lower()
        except (EOFError, KeyboardInterrupt):
            return
        if confirm in ('n', 'no'):
            info("已取消")
            return

        # 执行
        original_dry = self.syncer.dry_run
        self.syncer.dry_run = dry_run
        total = {"success": 0, "skipped": 0, "failed": 0}
        try:
            for target in selected:
                name = target['name']
                if skills_override is None:
                    result = self.syncer.sync_to_target(name)
                else:
                    result = self.syncer.sync_to_target(name, skill_names=skills_override)
                total["success"] += result["success"]
                total["skipped"] += result["skipped"]
                total["failed"] += result["failed"]
        finally:
            self.syncer.dry_run = original_dry

        if not dry_run:
            self.syncer.manifest['last_sync'] = datetime.now().isoformat()
            self.syncer._save_manifest()

        header("\n" + "=" * 60)
        success(f"同步完成! 成功: {total['success']}, 跳过: {total['skipped']}, 失败: {total['failed']}")
        if dry_run:
            warning(" (模拟模式，未实际复制)")
        header("=" * 60)

    def cmd_enable(self, args):
        """启用平台: enable <target>..."""
        if not args:
            warning("用法: enable <target>...")
            return
        changed = False
        for name in args:
            config = self.syncer._get_target_config(name)
            if not config:
                warning(f"未找到目标: {name}")
                continue
            if config.get('enabled', True):
                info(f"  {name} 已是启用状态")
                continue
            config['enabled'] = True
            changed = True
            success(f"已启用: {name}")
        if changed:
            self.syncer._save_manifest()

    def cmd_disable(self, args):
        """禁用平台: disable <target>..."""
        if not args:
            warning("用法: disable <target>...")
            return
        changed = False
        for name in args:
            config = self.syncer._get_target_config(name)
            if not config:
                warning(f"未找到目标: {name}")
                continue
            if not config.get('enabled', True):
                info(f"  {name} 已是禁用状态")
                continue
            config['enabled'] = False
            changed = True
            warning(f"已禁用: {name}")
        if changed:
            self.syncer._save_manifest()

    def cmd_target(self, args):
        """管理平台 skills: target <name> [list|add|remove|clear|convert|policy|exclude] [args...]

        示例:
          target hermes                列出 hermes 的同步配置
          target hermes list           同上
          target hermes add foo bar    白名单模式: 添加 skills
          target hermes remove foo     白名单模式: 移除 skill
          target hermes clear          清空白名单
          target hermes convert        转为黑名单模式 (可见集合不变)
          target hermes policy         查看当前模式
          target hermes policy exclude 切换到黑名单模式
          target hermes exclude add foo       黑名单加入 foo
          target hermes exclude remove foo    黑名单移除 foo
          target hermes exclude list          查看黑名单
        """
        if not args:
            warning("用法: target <name> [list|add|remove|clear|convert|policy|exclude] [args...]")
            return
        target_name = args[0]
        config = self.syncer._get_target_config(target_name)
        if not config:
            warning(f"未找到目标: {target_name}")
            return
        if len(args) == 1:
            self._show_target_config(config)
            return
        subcmd = args[1].lower()
        rest = args[2:]
        if subcmd in ('list', 'ls', 'show'):
            self._show_target_config(config)
        elif subcmd == 'add':
            self._target_add(config, rest)
        elif subcmd == 'remove':
            self._target_remove(config, rest)
        elif subcmd == 'clear':
            self._target_clear(config)
        elif subcmd == 'convert':
            self._target_convert(config)
        elif subcmd == 'policy':
            self._target_policy(config, rest)
        elif subcmd == 'exclude':
            self._target_exclude(config, rest)
        else:
            warning(f"未知子命令: {subcmd}")
            info("可用: list, add, remove, clear, convert, policy, exclude")

    def _show_target_skills(self, config: Dict):
        skills = list(config.get('skills') or [])
        info(f"  {config['name']} (已配置 {len(skills)} 个):")
        if skills:
            for sk in skills:
                info(f"    - {sk}")
        else:
            info("    (空)")

    def _show_target_excludes(self, config: Dict):
        excludes = list(config.get('exclude') or [])
        info(f"  {config['name']} 黑名单 (排除 {len(excludes)} 个):")
        if excludes:
            for sk in excludes:
                info(f"    - {sk}")
        else:
            info("    (空)")

    def _show_target_config(self, config: Dict):
        mode = self.syncer._target_mode(config)
        mode_desc = "白名单 (skills)" if mode == 'include' else "黑名单 (exclude)"
        info(f"  {config['name']} — 模式: {mode_desc}, 启用: {'是' if config.get('enabled', True) else '否'}")
        if 'skills' in config:
            self._show_target_skills(config)
        else:
            self._show_target_excludes(config)

    def _target_convert(self, config: Dict):
        """白名单 → 黑名单：exclude = 全部源技能 − 当前 skills，可见集合不变。"""
        if 'skills' not in config:
            info(f"  {config['name']} 已是黑名单模式，无需转换")
            return
        current = set(config.get('skills') or [])
        all_source = self.syncer._get_source_skills()
        excluded = [s for s in all_source if s not in current]
        missing = current - set(all_source)
        if missing:
            warning(f"  以下白名单项在源目录中不存在，转换后将不再被任何方式引用: "
                    f"{', '.join(sorted(missing))}")
        del config['skills']
        config['exclude'] = excluded
        self.syncer._save_manifest()
        success(f"  已转换 {config['name']}: 黑名单排除 {len(excluded)} 个"
                f"，同步 {len(all_source) - len(excluded)} 个 (与转换前一致)")
        if not all_source:
            warning("  源目录为空，请检查后重新执行同步")

    def _target_policy(self, config: Dict, args: List[str]):
        """查看或切换 target 同步模式。"""
        if not args:
            mode = self.syncer._target_mode(config)
            mode_desc = "include (白名单)" if mode == 'include' else "exclude (黑名单)"
            info(f"  {config['name']} 当前模式: {mode_desc}")
            info("  切换用法: policy include | policy exclude")
            return
        action = args[0].lower()
        if action == 'include':
            if 'skills' in config:
                info(f"  {config['name']} 已是白名单模式")
                return
            excluded = set(config.get('exclude') or [])
            all_source = self.syncer._get_source_skills()
            config['skills'] = [s for s in all_source if s not in excluded]
            if 'exclude' in config:
                del config['exclude']
            self.syncer._save_manifest()
            success(f"  已切换 {config['name']} 为白名单模式 ({len(config['skills'])} 个)")
        elif action == 'exclude':
            if 'skills' not in config:
                info(f"  {config['name']} 已是黑名单模式")
                return
            self._target_convert(config)
        else:
            warning(f"  未知模式: {action}（可用: include / exclude）")

    def _target_exclude(self, config: Dict, args: List[str]):
        """维护黑名单: exclude [list|add|remove|clear] [skill...]"""
        if 'skills' in config:
            warning(f"  {config['name']} 是白名单模式，请先执行: "
                    f"target {config['name']} convert")
            return
        if not args or args[0].lower() in ('list', 'ls', 'show'):
            self._show_target_excludes(config)
            return
        subcmd = args[0].lower()
        skills = args[1:]
        if subcmd == 'add':
            self._target_exclude_add(config, skills)
        elif subcmd == 'remove':
            self._target_exclude_remove(config, skills)
        elif subcmd == 'clear':
            self._target_exclude_clear(config)
        else:
            warning(f"  未知子命令: {subcmd}（可用: list, add, remove, clear）")

    def _target_exclude_add(self, config: Dict, skills: List[str]):
        if not skills:
            warning("请指定要排除的 skill")
            return
        current = list(config.get('exclude') or [])
        current_set = set(current)
        changed = False
        for sk in skills:
            if sk in current_set:
                info(f"  已在黑名单中: {sk}")
                continue
            current.append(sk)
            current_set.add(sk)
            success(f"  已排除: {sk}")
            changed = True
        if changed:
            config['exclude'] = current
            self.syncer._save_manifest()

    def _target_exclude_remove(self, config: Dict, skills: List[str]):
        if not skills:
            warning("请指定要取消排除的 skill")
            return
        current = list(config.get('exclude') or [])
        current_set = set(current)
        changed = False
        for sk in skills:
            if sk not in current_set:
                warning(f"  不在黑名单中: {sk}")
                continue
            current.remove(sk)
            current_set.discard(sk)
            info(f"  已取消排除: {sk}")
            changed = True
        if changed:
            config['exclude'] = current
            self.syncer._save_manifest()

    def _target_exclude_clear(self, config: Dict):
        if not config.get('exclude'):
            info(f"  {config['name']} 黑名单已为空")
            return
        config['exclude'] = []
        self.syncer._save_manifest()
        warning(f"  已清空 {config['name']} 的黑名单")

    def _target_add(self, config: Dict, skills: List[str]):
        if 'skills' not in config:
            warning(f"  {config['name']} 是黑名单模式，添加请用: "
                    f"target {config['name']} exclude remove <skill>")
            return
        if not skills:
            warning("请指定要添加的 skill")
            return
        current = list(config.get('skills') or [])
        current_set = set(current)
        changed = False
        for sk in skills:
            if sk in current_set:
                info(f"  已存在: {sk}")
                continue
            current.append(sk)
            current_set.add(sk)
            success(f"  已添加: {sk}")
            changed = True
        if changed:
            config['skills'] = current
            self.syncer._save_manifest()

    def _target_remove(self, config: Dict, skills: List[str]):
        if 'skills' not in config:
            warning(f"  {config['name']} 是黑名单模式，移除请用: "
                    f"target {config['name']} exclude add <skill>")
            return
        if not skills:
            warning("请指定要移除的 skill")
            return
        current = list(config.get('skills') or [])
        current_set = set(current)
        changed = False
        for sk in skills:
            if sk not in current_set:
                warning(f"  未配置: {sk}")
                continue
            current.remove(sk)
            current_set.discard(sk)
            info(f"  已移除: {sk}")
            changed = True
        if changed:
            config['skills'] = current
            self.syncer._save_manifest()

    def _target_clear(self, config: Dict):
        if 'skills' not in config:
            warning(f"  {config['name']} 是黑名单模式，清空请用: "
                    f"target {config['name']} exclude clear")
            return
        if not config.get('skills'):
            info(f"  {config['name']} 已为空")
            return
        config['skills'] = []
        self.syncer._save_manifest()
        warning(f"  已清空 {config['name']} 的 skills")

    def cmd_skills(self, args):
        """查看平台 skills: skills <name>"""
        if not args:
            warning("用法: skills <target>")
            return
        config = self.syncer._get_target_config(args[0])
        if not config:
            warning(f"未找到目标: {args[0]}")
            return
        self._show_target_config(config)

    def cmd_quit(self, args):
        """退出交互式 Shell"""
        self.running = False
        info("Bye!")

    # ---- 主循环 ----

    def dispatch(self, line: str) -> bool:
        line = line.strip()
        if not line or line.startswith('#'):
            return True

        try:
            parts = shlex.split(line)
        except ValueError as e:
            warning(f"解析错误: {e}")
            return True

        cmd_raw = parts[0].lower()
        args = parts[1:]
        cmd = CommandCompleter.ALIASES.get(cmd_raw, cmd_raw)

        method = getattr(self, f'cmd_{cmd}', None)
        if not method:
            warning(f"未知命令: {cmd_raw}（输入 help 查看帮助）")
            return True

        try:
            method(args)
        except (KeyboardInterrupt, EOFError):
            print()
        except Exception as e:
            error(f"命令执行出错: {e}")
        return self.running

    def run(self):
        header("\n" + "=" * 60)
        header("       AZ-SKILLS Shell")
        header("=" * 60)
        if READLINE_AVAILABLE:
            info("输入 help 查看可用命令，Tab 补全，↑↓ 历史，Ctrl+C 取消，quit 退出")
        else:
            warning("当前环境无 readline，Tab 补全不可用")
            info("可安装 pyreadline3 启用: pip install pyreadline3")
            info("输入 help 查看命令，quit 退出")
        header("=" * 60)

        while True:
            try:
                line = input(f"\n{self.PROMPT}")
            except (EOFError, KeyboardInterrupt):
                print()
                break
            if not self.dispatch(line):
                break


def _interactive_menu(syncer: SkillSyncer):
    """交互式 Shell - 命令风格界面（支持 Tab 补全）"""
    shell = InteractiveShell(syncer)
    shell.run()


if __name__ == "__main__":
    main()
