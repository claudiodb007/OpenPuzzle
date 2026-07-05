# Contributing to OpenPuzzle

OpenPuzzle is being developed as a modular orchestration platform.

## Development Rules

- Keep the core independent from specific engines.
- Do not add engine-specific logic to the Scheduler.
- Prefer services over large CLI implementations.
- Keep SQLite as the source of truth.
- Add tests when changing core behavior.
- Keep the build green before committing.

## Build

./scripts/build.sh

## Test

./scripts/test.sh

## Architecture

See:

docs/architecture/

## Commit Style

Examples:

- Add worker manager CLI
- Add execution queue CLI
- Document OpenPuzzle architecture# Contributing to OpenPuzzle

OpenPuzzle is being developed as a modular orchestration platform.

## Development Rules

- Keep the core independent from specific engines.
- Do not add engine-specific logic to the Scheduler.
- Prefer services over large CLI implementations.
- Keep SQLite as the source of truth.
- Add tests when changing core behavior.
- Keep the build green before committing.

## Build

./scripts/build.sh

## Test

./scripts/test.sh

## Architecture

See:

docs/architecture/

## Commit Style

Examples:

- Add worker manager CLI
- Add execution queue CLI
- Document OpenPuzzle architecture
