# Codex Task: Mechanical Header Split (Safe)

**Goal:** Split one header into `.h/.cpp` with no behaviour changes.
**Rules:**
- No new functions/vars; no signature changes.
- Keep includes minimal (run IWYU suggestion).
- Do not touch aggregate initialisers beyond policy (CI will enforce).

**Procedure:**
1. List all declarations and definitions currently in <HEADER>.
2. Create `<BASENAME>.cpp`; move definitions there.
3. In `<HEADER>`, keep only declarations, include guards, minimal includes (prefer forward decls).
4. Build: `pio run`. Fix missing includes ONLY.
5. Run scanner: `python tools/aggregate_init_scanner.py --mode=strict --roots src include lib`
6. Commit with conventional message: `refactor(header): split <file> into .h/.cpp (mechanical)`
7. Open PR.

**Checklist:**
- [ ] No logic changed.
- [ ] IWYU applied locally (see script below).
- [ ] CI is green.
