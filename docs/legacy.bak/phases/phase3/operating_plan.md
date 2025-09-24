# Phase 3 Operating Plan — Dual‑Strip Aware, 100 FPS, Single‑Core

This document is the authoritative plan for Phase 3. It is the north‑star and the guardrail for implementation and review.

Goals
- 120 FPS target (8 ms frame budget) on a single core.
- Both LED strips are first‑class, never gated. Visual quality or cost adjustments must apply uniformly to both.
- Asynchronous LED transmission with double buffering on both strips; compute frame N while hardware transmits N‑1.
- Static allocations for all hot‑path buffers; zero dynamic allocation during runtime.
- Deterministic five‑phase loop with watchdog protection and bounded waits.
