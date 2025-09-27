# Commit Message Scaffold

Template
```
feat({area}): {concise outcome}
- what changed and why (bullet points; minimal, specific)
- problems solved (evidence)
- lessons learned (1–3)
- recommendations/next steps
```

Example
```
feat(audio/tempo): retune novelty + resolver for high-tempo tracking
- added HM history + dual ACF (mix/HM) to surface fast pulses
- rebalanced weights + per‑band medians so HM onsets survive
- resolver now blends phase (low/HM), adds sticky bias, and seeds from HM peak when close
- diagnostics expanded (toggles + lane scores)
Problems solved: stuck at slow harmonic (85–90 BPM), no visibility
Lessons: seeding and medians dominate outcomes
Next: sweep HM weight/medians; lock promotion thresholds with candidate logs
```

