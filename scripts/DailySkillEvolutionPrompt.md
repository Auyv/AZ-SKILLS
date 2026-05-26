# AZ-SKILLS Daily Skill Evolution

Run this process from the repository root.

## Step 1: Prepare Work Order

Run:

```powershell
python tools/skill_feedback/daily_evolution.py prepare
```

If the command prints `No pending feedback to process.`, stop and report that there was no pending feedback.

If it prints a directory path, open:

- `evolution-workorder.md`
- every `feedback_*.md` in that same directory
- `evolution-report.md`

## Step 2: Evaluate Feedback

For every feedback item, apply these rules:

1. Rationally decide whether the feedback is positive for the skill's original purpose.
2. Preserve each skill's single responsibility.
3. Prefer the smallest useful change.

Classify each item as:

- modify: change the skill
- no-change: user usage issue, scope mismatch, personal preference, insufficient evidence, or risky expansion
- defer: valid but needs more evidence or a broader design decision

## Step 3: Backup Before Edits

Before editing any skill, run:

```powershell
python tools/skill_feedback/daily_evolution.py backup --skill <skill-name>
```

Use the exact skill name or path under `skills/`.

## Step 4: Edit Skills Conservatively

Only edit files under the target skill directory. Prefer these small changes:

- clarify trigger scenarios
- clarify boundaries and non-goals
- add FAQ or troubleshooting notes
- improve workflow wording

Avoid adding unrelated responsibilities, broad new integrations, or large rewrites.

## Step 5: Complete Report

Update the same directory's `evolution-report.md`.

The report must include:

- modified items, each citing the corresponding feedback file
- non-modified items, each citing the corresponding feedback file and explaining the correct usage or rejection reason
- backup path for every modified skill
- residual risks or follow-up observations
