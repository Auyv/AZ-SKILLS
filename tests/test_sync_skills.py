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


if __name__ == "__main__":
    unittest.main()
