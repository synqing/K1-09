# Dependency Risk Assessment (Template)

Use this template when introducing/upgrading dependencies (libraries, actions, tools).

## Summary
- Name and version
- Scope of use

## Risks
- Build size impact (flash/RAM)
- API/ABI changes
- Licensing/attribution requirements

## Mitigations
- Pin versions; record in `platformio.ini` or workflow
- Add size-budget guardrail
- Provide rollback path and test plan
