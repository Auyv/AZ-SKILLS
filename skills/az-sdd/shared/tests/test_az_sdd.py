from __future__ import annotations

import argparse
import json
import sys
import tempfile
import unittest
from pathlib import Path
from unittest import mock


SCRIPT_DIR = Path(__file__).resolve().parents[1] / "scripts"
sys.path.insert(0, str(SCRIPT_DIR))

import az_sdd  # noqa: E402


SPEC_TEXT = """# 功能规范：demo

| 字段 | 内容 |
|---|---|
| 状态 | 已批准 |

## 验收标准

- [ ] [001] 命令输出 demo。

### MT001 手工验证
"""


class AzSddTests(unittest.TestCase):
    def setUp(self) -> None:
        self.temp = tempfile.TemporaryDirectory()
        self.project = Path(self.temp.name)
        root = self.project / "az-sdd"
        for name in ("INIT.md", "STATUS.md", "TRACK.md", "SPEC.md", "ARCH.md", "FEEDBACK.md"):
            (root / name).parent.mkdir(parents=True, exist_ok=True)
            (root / name).write_text(f"# {name}\n", encoding="utf-8")
        for name in ("spec", "arch", "coding", "test", "reports", "feedback"):
            (root / name).mkdir(parents=True, exist_ok=True)
        self.spec = root / "spec" / "0010-demo.spec.md"
        self.spec.write_text(SPEC_TEXT, encoding="utf-8")
        self.rules = az_sdd.StateRules()

    def tearDown(self) -> None:
        self.temp.cleanup()

    def args(self, command: str, **kwargs: object) -> argparse.Namespace:
        values = {"command": command, "project": str(self.project), "dry_run": False}
        values.update(kwargs)
        return argparse.Namespace(**values)

    def enable_v4(self) -> None:
        result = az_sdd.run(self.args("migrate", apply=True, source_version="v3", target_version="v4"))
        self.assertEqual("success", result["status"])

    def create_ready_change(self, change_id: str = "CHG-0001", risk_factors: list[str] | None = None) -> Path:
        self.enable_v4()
        result = az_sdd.run(
            self.args(
                "change-init",
                change_id=change_id,
                title="调整 demo 行为",
                target="specs/0010-demo.spec.md",
                operation="MODIFIED",
                apply=True,
                risk_factor=risk_factors or [],
            )
        )
        self.assertEqual("success", result["status"])
        change = self.project / "az-sdd" / "changes" / change_id
        proposal = change / "proposal.md"
        proposal.write_text(proposal.read_text(encoding="utf-8").replace("TODO", "已明确"), encoding="utf-8")
        delta = change / "spec.delta.md"
        delta.write_text(delta.read_text(encoding="utf-8").replace("命令输出 demo。", "命令输出 v4 demo。"), encoding="utf-8")
        tasks = change / "tasks.md"
        tasks.write_text(tasks.read_text(encoding="utf-8").replace("- [ ]", "- [x]"), encoding="utf-8")
        (change / "evidence.md").write_text("# Evidence\n\n自动化测试 8/8 通过。\n", encoding="utf-8")
        (change / "acceptance.md").write_text("# Acceptance\n\n验收结果: 通过\n", encoding="utf-8")
        return change

    def test_rules_are_loaded_from_yaml(self) -> None:
        self.assertIn("approved", self.rules.states)
        self.assertIn("developing", self.rules.transitions["approved"])

    def test_validate_valid_project(self) -> None:
        result = az_sdd.run(self.args("validate"))
        self.assertEqual("success", result["status"])
        self.assertEqual([], result["data"]["issues"])

    def test_uninitialized_project(self) -> None:
        with tempfile.TemporaryDirectory() as empty:
            result = az_sdd.run(argparse.Namespace(command="validate", project=empty, dry_run=False))
        self.assertEqual("ERR_PROJECT_NOT_INITIALIZED", result["error"]["code"])

    def test_illegal_transition_does_not_write(self) -> None:
        before = self.spec.read_text(encoding="utf-8")
        result = az_sdd.run(
            self.args(
                "transition",
                feature_id="0010",
                source="approved",
                target="completed",
                reason="test",
            )
        )
        self.assertEqual("ERR_TRANSITION_NOT_ALLOWED", result["error"]["code"])
        self.assertEqual(before, self.spec.read_text(encoding="utf-8"))

    def test_legal_transition_dry_run_does_not_write(self) -> None:
        before = self.spec.read_text(encoding="utf-8")
        result = az_sdd.run(
            self.args(
                "transition",
                feature_id="0010",
                source="approved",
                target="developing",
                reason="test",
                dry_run=True,
            )
        )
        self.assertEqual("success", result["status"])
        self.assertEqual(before, self.spec.read_text(encoding="utf-8"))

    def test_legal_transition_writes_and_backs_up(self) -> None:
        result = az_sdd.run(
            self.args(
                "transition",
                feature_id="0010",
                source="approved",
                target="developing",
                reason="unit test",
            )
        )
        self.assertEqual("success", result["status"])
        self.assertIn("开发中", self.spec.read_text(encoding="utf-8"))
        backups = list((self.project / ".ai_tmp" / "bak" / "az-sdd").rglob("0010-demo.spec.md"))
        self.assertTrue(backups)
        self.assertIn("approved → developing", (self.project / "az-sdd" / "TRACK.md").read_text(encoding="utf-8"))

    def test_rebuild_dry_run_does_not_write(self) -> None:
        status = self.project / "az-sdd" / "STATUS.md"
        before = status.read_text(encoding="utf-8")
        data = az_sdd.rebuild_indexes(self.project, self.rules, dry_run=True)
        self.assertTrue(data["changed"])
        self.assertEqual(before, status.read_text(encoding="utf-8"))

    def test_result_schema_shape(self) -> None:
        result = az_sdd.run(self.args("status"))
        expected = {"status", "timestamp", "skill", "data", "warnings", "error", "next_action"}
        self.assertEqual(expected, set(result))
        json.dumps(result, ensure_ascii=False)

    def test_v4_migrate_dry_run_has_zero_writes(self) -> None:
        before = sorted(str(path.relative_to(self.project)) for path in self.project.rglob("*"))
        result = az_sdd.run(self.args("migrate", apply=False, source_version="v3", target_version="v4"))
        after = sorted(str(path.relative_to(self.project)) for path in self.project.rglob("*"))
        self.assertEqual("success", result["status"])
        self.assertTrue(result["data"]["dry_run"])
        self.assertEqual(before, after)
        self.assertFalse((self.project / "az-sdd" / ".az-sdd-v4.json").exists())

    def test_v4_init_new_project_requires_apply(self) -> None:
        with tempfile.TemporaryDirectory() as empty:
            project = Path(empty)
            dry = az_sdd.run(argparse.Namespace(command="init", project=empty, title="demo", apply=False, dry_run=True))
            self.assertEqual("success", dry["status"])
            self.assertFalse((project / "az-sdd").exists())
            applied = az_sdd.run(argparse.Namespace(command="init", project=empty, title="demo", apply=True, dry_run=False))
            self.assertEqual("success", applied["status"])
            self.assertTrue((project / "az-sdd" / "baseline" / "specs").is_dir())
            self.assertTrue((project / "az-sdd" / ".az-sdd-v4.json").is_file())
            duplicate = az_sdd.run(argparse.Namespace(command="init", project=empty, title="demo", apply=True, dry_run=False))
            self.assertEqual("ERR_ALREADY_INITIALIZED", duplicate["error"]["code"])

    def test_v4_migrate_preserves_v3_and_feature_ids(self) -> None:
        legacy_before = self.spec.read_text(encoding="utf-8")
        self.enable_v4()
        baseline = self.project / "az-sdd" / "baseline" / "specs" / self.spec.name
        self.assertTrue(self.spec.exists())
        self.assertEqual(legacy_before, self.spec.read_text(encoding="utf-8"))
        self.assertEqual(legacy_before, baseline.read_text(encoding="utf-8-sig"))
        status = az_sdd.run(self.args("status"))
        self.assertEqual("v4", status["data"]["change_model"]["format"])
        self.assertEqual(["0010"], status["data"]["change_model"]["baseline_feature_ids"])

    def test_v4_migrate_is_idempotent(self) -> None:
        self.enable_v4()
        metadata = self.project / "az-sdd" / ".az-sdd-v4.json"
        before = metadata.read_text(encoding="utf-8")
        second = az_sdd.run(self.args("migrate", apply=True, source_version="v3", target_version="v4"))
        self.assertEqual("success", second["status"])
        self.assertEqual(before, metadata.read_text(encoding="utf-8"))
        self.assertEqual("unchanged", second["data"]["copies"][0]["state"])
        self.assertTrue((self.project / "az-sdd" / "changes" / ".gitkeep").is_file())

    def test_v4_rejects_legacy_transition_and_rebuilds_only_status(self) -> None:
        self.enable_v4()
        baseline = self.project / "az-sdd" / "baseline" / "specs" / "0010-demo.spec.md"
        spec_index = self.project / "az-sdd" / "SPEC.md"
        baseline_before = baseline.read_text(encoding="utf-8-sig")
        index_before = spec_index.read_text(encoding="utf-8")
        transition = az_sdd.run(
            self.args(
                "transition",
                feature_id="0010",
                source="approved",
                target="developing",
                reason="must reject",
            )
        )
        self.assertEqual("ERR_TRANSITION_NOT_ALLOWED", transition["error"]["code"])
        self.assertEqual(baseline_before, baseline.read_text(encoding="utf-8-sig"))
        rebuilt = az_sdd.run(self.args("rebuild-indexes", dry_run=False))
        self.assertEqual("success", rebuilt["status"])
        self.assertEqual("v4", rebuilt["data"]["format"])
        self.assertEqual(index_before, spec_index.read_text(encoding="utf-8"))
        self.assertIn("AZ-SDD:AUTO:V4_STATUS", (self.project / "az-sdd" / "STATUS.md").read_text(encoding="utf-8"))

    def test_v4_change_init_dry_run_has_zero_writes(self) -> None:
        self.enable_v4()
        result = az_sdd.run(
            self.args(
                "change-init",
                change_id="CHG-0001",
                title="demo",
                target="specs/0010-demo.spec.md",
                operation="MODIFIED",
                apply=False,
            )
        )
        self.assertEqual("success", result["status"])
        self.assertEqual("fast", result["data"]["risk"]["path"])
        self.assertFalse((self.project / "az-sdd" / "changes" / "CHG-0001").exists())

    def test_v4_risk_expands_required_gates(self) -> None:
        change = self.create_ready_change(risk_factors=["security", "deployment"])
        blocked = az_sdd.run(self.args("change-validate", change_id="CHG-0001"))
        self.assertEqual("ERR_RISK_GATE_REQUIRED", blocked["error"]["code"])
        messages = "\n".join(item["message"] for item in blocked["data"]["issues"])
        self.assertIn("完整 design", messages)
        self.assertIn("完整测试范围", messages)
        self.assertIn("人工验收", messages)
        self.assertIn("回滚计划", messages)
        self.assertEqual("expanded", blocked["data"]["risk"]["path"])

        (change / "design.md").write_text("# Design\n\n设计已确认。\n", encoding="utf-8")
        (change / "evidence.md").write_text("# Evidence\n\n测试范围: 完整\n\n所有测试通过。\n", encoding="utf-8")
        (change / "acceptance.md").write_text("# Acceptance\n\n验收类型: 人工\n验收结果: 通过\n", encoding="utf-8")
        proposal = change / "proposal.md"
        proposal.write_text(proposal.read_text(encoding="utf-8") + "\n## 回滚\n\n恢复旧版本。\n", encoding="utf-8")
        ready = az_sdd.run(self.args("change-validate", change_id="CHG-0001"))
        self.assertEqual("success", ready["status"])

    def test_v4_rejects_unknown_risk_factor(self) -> None:
        self.enable_v4()
        result = az_sdd.run(
            self.args(
                "change-init",
                change_id="CHG-0001",
                title="demo",
                target="specs/0010-demo.spec.md",
                operation="MODIFIED",
                apply=False,
                risk_factor=["made_up"],
            )
        )
        self.assertEqual("ERR_INVALID_RISK_FACTOR", result["error"]["code"])

    def test_v4_archive_rejects_unfinished_change(self) -> None:
        self.enable_v4()
        az_sdd.run(
            self.args(
                "change-init",
                change_id="CHG-0001",
                title="demo",
                target="specs/0010-demo.spec.md",
                operation="MODIFIED",
                apply=True,
            )
        )
        result = az_sdd.run(self.args("archive", change_id="CHG-0001", apply=True))
        self.assertEqual("ERR_CHANGE_NOT_READY", result["error"]["code"])
        self.assertTrue((self.project / "az-sdd" / "changes" / "CHG-0001").exists())

    def test_v4_archive_merges_delta_and_creates_backup(self) -> None:
        self.create_ready_change()
        result = az_sdd.run(self.args("archive", change_id="CHG-0001", apply=True))
        self.assertEqual("success", result["status"])
        baseline = self.project / "az-sdd" / "baseline" / "specs" / "0010-demo.spec.md"
        self.assertIn("v4 demo", baseline.read_text(encoding="utf-8"))
        self.assertFalse((self.project / "az-sdd" / "changes" / "CHG-0001").exists())
        archives = list((self.project / "az-sdd" / "archive").glob("*-CHG-0001"))
        self.assertEqual(1, len(archives))
        self.assertTrue(Path(result["data"]["backup"]).exists())

    def test_v4_archive_detects_baseline_hash_conflict(self) -> None:
        self.create_ready_change()
        baseline = self.project / "az-sdd" / "baseline" / "specs" / "0010-demo.spec.md"
        baseline.write_text(baseline.read_text(encoding="utf-8") + "\n外部变化\n", encoding="utf-8")
        before = baseline.read_text(encoding="utf-8")
        result = az_sdd.run(self.args("archive", change_id="CHG-0001", apply=True))
        self.assertEqual("ERR_BASELINE_CONFLICT", result["error"]["code"])
        self.assertEqual(before, baseline.read_text(encoding="utf-8"))
        self.assertTrue((self.project / "az-sdd" / "changes" / "CHG-0001").exists())

    def test_v4_change_rejects_unsafe_target(self) -> None:
        self.enable_v4()
        result = az_sdd.run(
            self.args(
                "change-init",
                change_id="CHG-0001",
                title="demo",
                target="../outside.spec.md",
                operation="ADDED",
                apply=True,
            )
        )
        self.assertEqual("ERR_INVALID_INPUT", result["error"]["code"])

    def test_v4_archive_rolls_back_after_write_failure(self) -> None:
        change = self.create_ready_change()
        baseline = self.project / "az-sdd" / "baseline" / "specs" / "0010-demo.spec.md"
        before = baseline.read_text(encoding="utf-8")
        with mock.patch.object(az_sdd.v4_model, "write_text", side_effect=OSError("injected failure")):
            result = az_sdd.run(self.args("archive", change_id="CHG-0001", apply=True))
        self.assertEqual("ERR_ARCHIVE_FAILED", result["error"]["code"])
        self.assertEqual(before, baseline.read_text(encoding="utf-8"))
        self.assertTrue(change.exists())
        self.assertFalse(list((self.project / "az-sdd" / "archive").glob("*-CHG-0001")))

    def test_v4_archive_supports_added_and_removed_specs(self) -> None:
        self.enable_v4()
        add = az_sdd.run(
            self.args(
                "change-init",
                change_id="CHG-ADD-0001",
                title="新增规范",
                target="specs/0011-added.spec.md",
                operation="ADDED",
                apply=True,
            )
        )
        self.assertEqual("success", add["status"])
        add_dir = self.project / "az-sdd" / "changes" / "CHG-ADD-0001"
        delta = add_dir / "spec.delta.md"
        delta.write_text(delta.read_text(encoding="utf-8").replace("TODO: 描述新增的当前行为。", "新增行为已定义。"), encoding="utf-8")
        (add_dir / "proposal.md").write_text("# Proposal\n\n已明确。\n", encoding="utf-8")
        (add_dir / "tasks.md").write_text("# Tasks\n\n- [x] 完成。\n", encoding="utf-8")
        (add_dir / "evidence.md").write_text("# Evidence\n\n测试通过。\n", encoding="utf-8")
        (add_dir / "acceptance.md").write_text("# Acceptance\n\n验收结果: 通过\n", encoding="utf-8")
        self.assertEqual("success", az_sdd.run(self.args("archive", change_id="CHG-ADD-0001", apply=True))["status"])
        added = self.project / "az-sdd" / "baseline" / "specs" / "0011-added.spec.md"
        self.assertTrue(added.exists())

        remove = az_sdd.run(
            self.args(
                "change-init",
                change_id="CHG-REMOVE-0001",
                title="移除规范",
                target="specs/0011-added.spec.md",
                operation="REMOVED",
                apply=True,
            )
        )
        self.assertEqual("success", remove["status"])
        remove_dir = self.project / "az-sdd" / "changes" / "CHG-REMOVE-0001"
        (remove_dir / "proposal.md").write_text("# Proposal\n\n已明确。\n", encoding="utf-8")
        (remove_dir / "tasks.md").write_text("# Tasks\n\n- [x] 完成。\n", encoding="utf-8")
        (remove_dir / "evidence.md").write_text("# Evidence\n\n测试通过。\n", encoding="utf-8")
        (remove_dir / "acceptance.md").write_text("# Acceptance\n\n验收结果: 通过\n", encoding="utf-8")
        self.assertEqual("success", az_sdd.run(self.args("archive", change_id="CHG-REMOVE-0001", apply=True))["status"])
        self.assertFalse(added.exists())


if __name__ == "__main__":
    unittest.main()
