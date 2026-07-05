# Contributing to OpenPuzzle

Thank you for your interest in contributing to OpenPuzzle.

OpenPuzzle is an open-source orchestration platform for cryptographic keyspace exploration. The objective is to build a modular, maintainable and well-tested project.

---

## Development Principles

When contributing, please follow these principles:

- Keep the core independent from any specific search engine.
- OpenPuzzle coordinates work.
- External engines execute work.
- Prefer reusable components over duplicated code.
- Keep the Scheduler independent from engine-specific logic.
- Keep SQLite as the single source of truth.
- Write clean, readable and maintainable C++20 code.
- Keep documentation updated.
- Add tests whenever core behaviour changes.

---

## Project Philosophy

OpenPuzzle should remain engine-agnostic.

The Scheduler, Dispatcher and Worker Management layers should never depend on BitCrack-specific behaviour.

Support for search engines should be added through dedicated adapters rather than by modifying the core architecture.

---

## Code Style

- Use C++20.
- Prefer RAII over manual resource management.
- Prefer standard library facilities whenever possible.
- Keep functions small and focused.
- Avoid duplicated code.
- Follow the existing project structure.

---

## Building

Compile the project:

```bash
./scripts/build.sh
```

---

## Running Tests

Run the complete test suite:

```bash
./scripts/test.sh
```

Always ensure:

- The project builds successfully.
- All tests pass.
- New features include tests whenever possible.
- Existing functionality is not broken.

---

## Commit Messages

Use clear commit messages.

Good examples:

- Add worker management CLI
- Add execution queue
- Improve GPU profile manager
- Document project architecture

Avoid messages such as:

- update
- fix
- changes

---

## Pull Requests

A pull request should clearly explain:

- What changed
- Why it changed
- How it was tested

Small pull requests are preferred over very large ones.

---

## Documentation

When introducing new modules or major architectural changes, update the relevant documentation.

Current documentation includes:

- README.md
- ROADMAP.md
- docs/architecture/

---

## Reporting Issues

When reporting a bug, please include:

- Operating system
- Compiler version
- GPU model (if applicable)
- Command executed
- Complete error message
- Steps required to reproduce the problem

---

Thank you for helping improve OpenPuzzle.

---

## Code Style

Please follow these general coding guidelines:

- Use modern C++20.
- Prefer RAII over manual resource management.
- Prefer the C++ Standard Library whenever possible.
- Keep functions small and focused.
- Avoid duplicated code.
- Keep header and implementation files synchronized.
- Follow the existing project structure.

---

## Project Philosophy

OpenPuzzle is an orchestration platform.

The core should remain independent from any specific search engine.

The Scheduler, Dispatcher and Worker Management layers should never contain engine-specific behaviour.

Support for BitCrack, KeyHunt, Kangaroo or future engines should be implemented through dedicated adapters.

---

## Testing Requirements

Before submitting changes, always verify that:

- The project builds successfully.
- All unit tests pass.
- New functionality includes tests whenever practical.
- Existing functionality is not broken.

---

## Reporting Issues

When reporting a bug, please include:

- Operating system
- Compiler version
- GPU model (if applicable)
- Command executed
- Complete error message
- Steps required to reproduce the issue

This information helps reproduce and fix problems more quickly.

