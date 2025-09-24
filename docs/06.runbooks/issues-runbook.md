# Issues Runbook (Seeding from JSON)

Use the helper script to create issues from the tracking JSON files in `docs/05.tracking/`.

## Prereqs
- Install and authenticate GitHub CLI: `gh auth login`

## Dry run (prints commands)
```
python3 scripts/seed_issues.py --file docs/05.tracking/phase-4-issues.json --dry-run
python3 scripts/seed_issues.py --file docs/05.tracking/phase-5-issues.json --dry-run
```

## Create issues
```
python3 scripts/seed_issues.py --file docs/05.tracking/phase-4-issues.json --run
python3 scripts/seed_issues.py --file docs/05.tracking/phase-5-issues.json --run
```

Each issue includes paths and acceptance checks in the body and applies labels/milestones from the JSON.

