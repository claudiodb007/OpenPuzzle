# OpenPuzzle 1.0.11

## One-command onboarding

The supported first-run flow remains intentionally simple:

```bash
openpuzzle run
```

OpenPuzzle detects and validates the local GPU engine, creates a safe GPU
profile when one does not exist, and only then contacts the coordination
server. No separate setup command is required.

The first-use output now presents four explicit stages:

1. hardware and bundled engine readiness;
2. local GPU profile detection;
3. the safe automatic benchmark;
4. permission to contact the server and request work.

If the benchmark fails, the client confirms that no assignment was requested,
prints a stable error code and gives exact recovery commands. Existing valid
profiles continue without repeating onboarding on every assignment.

This phase does not change the installed client or a running execution.
