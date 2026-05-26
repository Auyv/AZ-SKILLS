# Skill Feedback Tool

This tool collects local feedback for skills and prepares a daily AI work order.

## Submit Feedback

Run:

```powershell
python tools/skill_feedback/open_feedback_form.py
```

Or double-click:

```text
scripts/OpenSkillFeedback.bat
```

Feedback is first saved to:

```text
skill-feedback/inbox/
```

## Prepare Daily Evolution

Run:

```powershell
python tools/skill_feedback/daily_evolution.py prepare
```

The command moves pending feedback into:

```text
skill-feedback/archive/YYYY-MM-DD/
```

It also creates:

- `evolution-workorder.md`
- `evolution-report.md`

## Backup Before Editing a Skill

Run:

```powershell
python tools/skill_feedback/daily_evolution.py backup --skill auto-skill-creator
```

Backups are stored in:

```text
skill-feedback/backups/<skill-name>/
```

Only the latest 12 backups are retained for each skill.
