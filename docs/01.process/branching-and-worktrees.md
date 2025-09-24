# Branching & Worktrees

## Why worktrees
- Parallelize streams without context switching and uncommitted file juggling.

## Quick start
```
git fetch --all --prune
git worktree add ../K1-09-docs-pipelines   -b docs/pipelines-ssot
git worktree add ../K1-09-docs-scheduling  -b docs/scheduling-ssot
git worktree add ../K1-09-docs-magic       -b docs/magic-numbers
git worktree add ../K1-09-docs-tooling     -b infra/docs-tooling
```

## Conventions
- One stream per worktree. Keep branches short-lived and focused.
- Use labels to connect PRs to streams and gates.
- Clean up: `git worktree remove ../path && git branch -D <branch>` once merged.
