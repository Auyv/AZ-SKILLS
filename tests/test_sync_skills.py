import os
import subprocess
import tempfile
import unittest
from pathlib import Path

from sync_skills import SkillSyncer


def create_directory_link(link: Path, target: Path) -> None:
    if os.name == "nt":
        result = subprocess.run(
            ["cmd", "/c", "mklink", "/J", str(link), str(target)],
            capture_output=True,
            text=True,
            encoding="gbk",
            errors="replace",
        )
        if result.returncode != 0:
            raise OSError(result.stderr.strip())
    else:
        link.symlink_to(target, target_is_directory=True)


class PurgeManagedLinksTests(unittest.TestCase):
    def test_discovers_workflow_skill_from_separate_source_directory(self) -> None:
        with tempfile.TemporaryDirectory() as temp_dir:
            root = Path(temp_dir)
            source_dir = root / "skills"
            workflow_dir = root / "workflow"
            normal_skill = source_dir / "normal-skill"
            workflow_skill = workflow_dir / "release-workflow"
            normal_skill.mkdir(parents=True)
            workflow_skill.mkdir(parents=True)
            (normal_skill / "SKILL.md").write_text("# normal", encoding="utf-8")
            (workflow_skill / "SKILL.md").write_text("# workflow", encoding="utf-8")

            syncer = SkillSyncer.__new__(SkillSyncer)
            syncer.source_dir = source_dir
            syncer.workflow_dir = workflow_dir

            self.assertEqual(["normal-skill", "release-workflow"], syncer._get_source_skills())
            self.assertEqual(workflow_skill, syncer._get_skill_source("release-workflow"))

    def test_replaces_dangling_link_after_source_container_rename(self) -> None:
        with tempfile.TemporaryDirectory() as temp_dir:
            root = Path(temp_dir)
            source_dir = root / "skills"
            target_dir = root / "target"
            old_child = source_dir / "az-sdd.v3" / "az-sdd-next"
            new_child = source_dir / "az-sdd" / "az-sdd-next"
            target_dir.mkdir()
            old_child.mkdir(parents=True)
            (old_child / "SKILL.md").write_text("# old", encoding="utf-8")

            installed = target_dir / "az-sdd-next"
            create_directory_link(installed, old_child)
            old_child.parent.rename(new_child.parent)
            (new_child / "SKILL.md").write_text("# new", encoding="utf-8")

            syncer = SkillSyncer.__new__(SkillSyncer)
            syncer.dry_run = False

            self.assertTrue(syncer._link_one("az-sdd-next", new_child, target_dir))
            self.assertEqual("# new", (installed / "SKILL.md").read_text(encoding="utf-8"))

    def test_removes_stale_repository_link_but_preserves_user_content(self) -> None:
        with tempfile.TemporaryDirectory() as temp_dir:
            root = Path(temp_dir)
            source_dir = root / "skills"
            target_dir = root / "target"
            external_dir = root / "external-skill"
            source_dir.mkdir()
            target_dir.mkdir()
            external_dir.mkdir()

            current_source = source_dir / "current-skill"
            stale_source = source_dir / "stale-skill"
            current_source.mkdir()
            stale_source.mkdir()

            current_link = target_dir / "current-skill"
            stale_link = target_dir / "stale-skill"
            external_link = target_dir / "external-link"
            user_directory = target_dir / "user-skill"
            create_directory_link(current_link, current_source)
            create_directory_link(stale_link, stale_source)
            create_directory_link(external_link, external_dir)
            user_directory.mkdir()

            syncer = SkillSyncer.__new__(SkillSyncer)
            syncer.source_dir = source_dir

            # 本次清单只剩 current-skill；stale-skill 仍应作为历史受管链接清理。
            syncer._purge_managed_links(target_dir, ["current-skill"])

            self.assertFalse(current_link.exists())
            self.assertFalse(stale_link.exists())
            self.assertTrue(external_link.exists())
            self.assertTrue(user_directory.exists())

    def test_target_sync_prunes_removed_skill_and_recreates_current_skill(self) -> None:
        with tempfile.TemporaryDirectory() as temp_dir:
            root = Path(temp_dir)
            source_dir = root / "skills"
            target_dir = root / "target"
            source_dir.mkdir()
            target_dir.mkdir()

            current_source = source_dir / "current-skill"
            stale_source = source_dir / "stale-skill"
            current_source.mkdir()
            stale_source.mkdir()
            (current_source / "SKILL.md").write_text("# current", encoding="utf-8")
            (stale_source / "SKILL.md").write_text("# stale", encoding="utf-8")

            create_directory_link(target_dir / "current-skill", current_source)
            create_directory_link(target_dir / "stale-skill", stale_source)
            (target_dir / "user-skill").mkdir()

            syncer = SkillSyncer.__new__(SkillSyncer)
            syncer.source_dir = source_dir
            syncer.dry_run = False
            syncer.manifest = {
                "agent_targets": [
                    {
                        "name": "test-target",
                        "path": str(target_dir),
                        "aggregate_subdir": "",
                        "enabled": True,
                        "skills": ["current-skill"],
                    }
                ]
            }

            result = syncer.sync_to_target("test-target")

            self.assertEqual({"success": 1, "skipped": 0, "failed": 0}, result)
            self.assertTrue((target_dir / "current-skill").exists())
            self.assertFalse((target_dir / "stale-skill").exists())
            self.assertTrue((target_dir / "user-skill").exists())

    def test_aggregate_skill_publishes_shared_runtime(self) -> None:
        with tempfile.TemporaryDirectory() as temp_dir:
            root = Path(temp_dir)
            source_dir = root / "skills"
            target_dir = root / "target"
            aggregate = source_dir / "az-sdd"
            child = aggregate / "az-sdd-next"
            shared_script = aggregate / "shared" / "scripts" / "az_sdd.py"
            child.mkdir(parents=True)
            target_dir.mkdir()
            (child / "SKILL.md").write_text("# next", encoding="utf-8")
            shared_script.parent.mkdir(parents=True)
            shared_script.write_text("print('ok')", encoding="utf-8")

            syncer = SkillSyncer.__new__(SkillSyncer)
            syncer.source_dir = source_dir
            syncer.dry_run = False
            syncer.manifest = {
                "agent_targets": [
                    {
                        "name": "test-target",
                        "path": str(target_dir),
                        "aggregate_subdir": "",
                        "enabled": True,
                        "skills": ["az-sdd"],
                    }
                ]
            }

            result = syncer.sync_to_target("test-target")

            self.assertEqual({"success": 1, "skipped": 0, "failed": 0}, result)
            self.assertTrue((target_dir / "az-sdd-next" / "SKILL.md").exists())
            self.assertTrue((target_dir / "az-sdd-shared" / "scripts" / "az_sdd.py").exists())
            self.assertEqual(
                ["az-sdd-next", "az-sdd-shared"],
                syncer._collect_target_labels("test-target"),
            )


if __name__ == "__main__":
    unittest.main()
