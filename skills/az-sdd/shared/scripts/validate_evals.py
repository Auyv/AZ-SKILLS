#!/usr/bin/env python3
"""Validate AZ-SDD eval suite structure with the Python standard library."""

from __future__ import annotations

import json
import sys
from pathlib import Path


ROOT = Path(__file__).resolve().parents[2]


def validate_suite(path: Path, expected_name: str | None = None) -> list[str]:
    errors: list[str] = []
    try:
        data = json.loads(path.read_text(encoding="utf-8"))
    except (OSError, json.JSONDecodeError) as exc:
        return [f"{path}: {exc}"]
    name = data.get("skill_name")
    if expected_name and name != expected_name:
        errors.append(f"{path}: skill_name={name!r}, expected={expected_name!r}")
    if data.get("version") != "4.1.0":
        errors.append(f"{path}: version 必须是 4.1.0")
    evals = data.get("evals")
    if not isinstance(evals, list) or len(evals) < 5:
        errors.append(f"{path}: 至少需要 5 个 eval")
        return errors
    ids: set[object] = set()
    error_count = 0
    declared_errors: set[str] = set()
    skill_file = path.parents[1] / "SKILL.md"
    if skill_file.exists():
        for line in skill_file.read_text(encoding="utf-8").splitlines():
            stripped = line.strip()
            if stripped.startswith("- code:"):
                declared_errors.add(stripped.split(":", 1)[1].strip())
    for index, case in enumerate(evals, 1):
        prefix = f"{path}: eval[{index}]"
        if not isinstance(case, dict):
            errors.append(f"{prefix} 必须是对象")
            continue
        case_id = case.get("id")
        if case_id in ids:
            errors.append(f"{prefix} id 重复: {case_id}")
        ids.add(case_id)
        category = case.get("category")
        if category not in {"normal", "error"}:
            errors.append(f"{prefix} category 无效")
        if not isinstance(case.get("prompt"), str) or not case["prompt"].strip():
            errors.append(f"{prefix} prompt 不能为空")
        criteria = case.get("criteria")
        if not isinstance(criteria, list) or not criteria:
            errors.append(f"{prefix} criteria 不能为空")
        elif case.get("pass_score") != len(criteria):
            errors.append(f"{prefix} pass_score 应等于 criteria 数量")
        if category == "normal" and not case.get("expected_output"):
            errors.append(f"{prefix} normal 用例缺少 expected_output")
        if category == "error":
            error_count += 1
            code = case.get("expected_error_code")
            if case.get("expected_behavior") != "error" or not code:
                errors.append(f"{prefix} error 用例字段不完整")
            if declared_errors and code not in declared_errors:
                errors.append(f"{prefix} 错误码 {code} 未在 SKILL.md 声明")
    if error_count < 2:
        errors.append(f"{path}: 至少需要 2 个异常用例")
    return errors


def main() -> int:
    errors: list[str] = []
    count = 0
    for skill_dir in sorted(ROOT.glob("az-sdd-*")):
        if not skill_dir.is_dir():
            continue
        path = skill_dir / "evals" / "evals.json"
        count += 1
        if not path.exists():
            errors.append(f"{path}: 文件不存在")
            continue
        errors.extend(validate_suite(path, skill_dir.name))
    cross = ROOT / "shared" / "evals" / "cross-skill-evals.json"
    errors.extend(validate_suite(cross, "az-sdd"))
    if errors:
        print("\n".join(errors))
        return 1
    print(f"eval validation: OK ({count} skills + cross-skill suite)")
    return 0


if __name__ == "__main__":
    sys.exit(main())
