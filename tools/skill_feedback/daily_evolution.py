from __future__ import annotations

import argparse
import os
import re
import shutil
from datetime import datetime
from pathlib import Path


REPO_ROOT = Path(__file__).resolve().parents[2]
SKILLS_DIR = REPO_ROOT / "skills"
FEEDBACK_ROOT = REPO_ROOT / "skill-feedback"
INBOX_DIR = FEEDBACK_ROOT / "inbox"
ARCHIVE_DIR = FEEDBACK_ROOT / "archive"
BACKUP_DIR = FEEDBACK_ROOT / "backups"
MAX_BACKUPS_PER_SKILL = 12


def now_stamp() -> str:
    return datetime.now().strftime("%Y%m%d_%H%M%S")


def today() -> str:
    return datetime.now().strftime("%Y-%m-%d")


def safe_name(value: str) -> str:
    value = value.strip().replace("\\", "-").replace("/", "-")
    return re.sub(r"[^A-Za-z0-9._\-\u4e00-\u9fff]+", "-", value).strip("-") or "unknown"


def ensure_dirs() -> None:
    for path in (INBOX_DIR, ARCHIVE_DIR, BACKUP_DIR):
        path.mkdir(parents=True, exist_ok=True)


def discover_skills() -> list[str]:
    if not SKILLS_DIR.exists():
        return []
    names: list[str] = []
    for skill_file in SKILLS_DIR.rglob("SKILL.md"):
        if "template" in {part.lower() for part in skill_file.parts}:
            continue
        names.append(str(skill_file.parent.relative_to(SKILLS_DIR)).replace("\\", "/"))
    return sorted(names)


def find_skill_dir(skill_name: str) -> Path:
    normalized = skill_name.strip().replace("\\", "/").strip("/")
    candidates = [
        SKILLS_DIR / normalized,
        SKILLS_DIR / safe_name(normalized),
    ]
    for skill_file in SKILLS_DIR.rglob("SKILL.md"):
        rel = str(skill_file.parent.relative_to(SKILLS_DIR)).replace("\\", "/")
        if rel == normalized or skill_file.parent.name == normalized:
            candidates.append(skill_file.parent)
    for candidate in candidates:
        if (candidate / "SKILL.md").exists():
            return candidate
    raise FileNotFoundError(f"Skill not found: {skill_name}")


def read_front_matter_field(text: str, field: str) -> str:
    match = re.search(rf"^{re.escape(field)}:\s*(.+)$", text, re.MULTILINE)
    return match.group(1).strip().strip('"') if match else ""


def feedback_summary(path: Path) -> dict[str, str]:
    text = path.read_text(encoding="utf-8")
    return {
        "file": path.name,
        "skill": read_front_matter_field(text, "skill") or "未填写",
        "created_at": read_front_matter_field(text, "created_at") or "未知",
        "pain": extract_section(text, "用户痛点描述"),
        "scenario": extract_section(text, "痛点场景举例"),
        "expected": extract_section(text, "期望效果"),
    }


def extract_section(text: str, title: str) -> str:
    pattern = rf"## {re.escape(title)}\n\n(.*?)(?=\n## |\Z)"
    match = re.search(pattern, text, flags=re.S)
    return match.group(1).strip() if match else ""


def prepare_daily_archive(day: str | None = None) -> Path | None:
    ensure_dirs()
    day = day or today()
    pending = sorted(INBOX_DIR.glob("feedback_*.md"))
    if not pending:
        return None

    day_dir = ARCHIVE_DIR / day
    day_dir.mkdir(parents=True, exist_ok=True)

    archived: list[Path] = []
    for source in pending:
        target = day_dir / source.name
        counter = 2
        while target.exists():
            target = day_dir / f"{source.stem}_{counter}{source.suffix}"
            counter += 1
        shutil.move(str(source), str(target))
        archived.append(target)

    write_workorder(day_dir, archived)
    report = day_dir / "evolution-report.md"
    if not report.exists():
        report.write_text(report_template(day, archived), encoding="utf-8")
    return day_dir


def write_workorder(day_dir: Path, feedback_files: list[Path]) -> None:
    summaries = [feedback_summary(path) for path in feedback_files]
    lines = [
        f"# Skill 每日进化工作单 - {day_dir.name}",
        "",
        "## 执行原则",
        "",
        "1. 理性判断反馈是否为正向反馈，防止 skill 因用户个人意志偏离最初目的。",
        "2. 坚持每个 skill 的单一职责原则，防止功能过度臃肿。",
        "3. 每次修改建议保持最简原则，防止过度修改产生其他影响。",
        "",
        "## 执行流程",
        "",
        "1. 阅读本工作单和同目录下的反馈单。",
        "2. 对每条反馈判断：是否正向、是否属于目标 skill 职责、是否需要修改。",
        "3. 需要修改 skill 前，先运行 `python tools/skill_feedback/daily_evolution.py backup --skill <skill>`。",
        "4. 修改后更新同目录 `evolution-report.md`，明确写入修改点和非修改点。",
        "",
        "## 待处理反馈",
        "",
    ]

    for item in summaries:
        lines.extend(
            [
                f"### {item['file']}",
                "",
                f"- Skill: `{item['skill']}`",
                f"- 创建时间: {item['created_at']}",
                f"- 反馈单: [{item['file']}](./{item['file']})",
                f"- 痛点: {item['pain'] or '未填写'}",
                f"- 场景: {item['scenario'] or '未填写'}",
                f"- 期望: {item['expected'] or '未填写'}",
                "",
                "处理判断:",
                "- 是否正向反馈: 待判断",
                "- 是否符合单一职责: 待判断",
                "- 建议动作: 待判断",
                "",
            ]
        )

    (day_dir / "evolution-workorder.md").write_text("\n".join(lines), encoding="utf-8")


def report_template(day: str, feedback_files: list[Path]) -> str:
    feedback_links = "\n".join(f"- [{path.name}](./{path.name})" for path in feedback_files)
    return f"""# Skill 每日进化汇报 - {day}

## 本次处理的反馈单

{feedback_links}

## 修改点

| Skill | 修改内容 | 引用反馈单 | 备份位置 |
|------|---------|------------|----------|
| - | - | - | - |

## 非修改点

| Skill | 未修改原因 | 引用反馈单 | 给用户的正确使用方式 |
|------|------------|------------|----------------------|
| - | - | - | - |

## 风险与后续观察

- 暂无。
"""


def backup_skill(skill_name: str) -> Path:
    ensure_dirs()
    source = find_skill_dir(skill_name)
    rel_name = str(source.relative_to(SKILLS_DIR)).replace("\\", "__").replace("/", "__")
    target_parent = BACKUP_DIR / safe_name(rel_name)
    target_parent.mkdir(parents=True, exist_ok=True)
    target = target_parent / now_stamp()
    shutil.copytree(source, target)
    prune_backups(target_parent)
    return target


def prune_backups(skill_backup_dir: Path) -> None:
    backups = sorted([p for p in skill_backup_dir.iterdir() if p.is_dir()], key=lambda p: p.name)
    excess = len(backups) - MAX_BACKUPS_PER_SKILL
    for old in backups[: max(0, excess)]:
        shutil.rmtree(old)


def list_pending() -> None:
    ensure_dirs()
    pending = sorted(INBOX_DIR.glob("feedback_*.md"))
    if not pending:
        print("No pending feedback.")
        return
    for path in pending:
        item = feedback_summary(path)
        print(f"{path.name}\t{item['skill']}\t{item['created_at']}")


def main() -> int:
    parser = argparse.ArgumentParser(description="Prepare daily skill feedback evolution work.")
    sub = parser.add_subparsers(dest="command", required=True)

    prepare = sub.add_parser("prepare", help="Archive pending feedback and create a work order.")
    prepare.add_argument("--date", help="Archive date, default: today.")

    backup = sub.add_parser("backup", help="Backup a skill before modifying it.")
    backup.add_argument("--skill", required=True, help="Skill name or path under skills/.")

    sub.add_parser("list", help="List pending feedback.")
    sub.add_parser("skills", help="List discovered skills.")

    args = parser.parse_args()

    if args.command == "prepare":
        day_dir = prepare_daily_archive(args.date)
        if day_dir is None:
            print("No pending feedback to process.")
        else:
            print(day_dir)
        return 0
    if args.command == "backup":
        print(backup_skill(args.skill))
        return 0
    if args.command == "list":
        list_pending()
        return 0
    if args.command == "skills":
        for name in discover_skills():
            print(name)
        return 0
    return 1


if __name__ == "__main__":
    raise SystemExit(main())
