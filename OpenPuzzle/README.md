# OpenPuzzle

<p align="center">

## Open-source orchestration platform for cryptographic keyspace exploration

</p>

> ⚠️ **OpenPuzzle is under active development.**
>
> APIs, database schema and CLI commands may change between releases.

---

## Why OpenPuzzle?

Existing search engines are excellent at performing cryptographic searches.

OpenPuzzle focuses on everything around the search itself:

- Managing workers
- Coordinating jobs
- Allocating ranges
- Tracking executions
- Benchmarking
- Recovering interrupted work
- Preparing distributed execution

This separation keeps the core modular and allows different search engines to be integrated without redesigning the platform.

---

## What is OpenPuzzle?

OpenPuzzle is an open-source project designed to coordinate cryptographic keyspace exploration.

Unlike tools such as BitCrack or KeyHunt, OpenPuzzle does **not** perform the cryptographic search itself.

Its purpose is to organize, schedule, monitor and recover search jobs while external engines perform the computation.

Current development is focused on the Bitcoin Puzzle project, but the architecture is designed to support additional search engines in the future.

---

## Current Features

- SQLite database
- Bitcoin Puzzle database (1–160)
- Wallet database
- Hash160 database
- Range database
- Puzzle management
- Worker management
- Execution queue
- Automatic range allocation
- GPU profile database
- Benchmark support
- Recovery support
- BitCrack output parser


## Project Structure

```text
OpenPuzzle/
├── docs/
├── include/
├── src/
├── tests/
├── scripts/
├── build/
├── data/
└── CMakeLists.txt
```

---

## Core Commands

Synchronize puzzle data:

```bash
./build/OpenPuzzle sync-data --dir data
```

List available puzzles:

```bash
./build/OpenPuzzle puzzle list
```

Show information about a puzzle:

```bash
./build/OpenPuzzle puzzle show 71
```

List registered workers:

```bash
./build/OpenPuzzle worker list
```

Create a new execution queue entry:

```bash
./build/OpenPuzzle queue add --puzzle 71 --block-bits 40
```

List queued jobs:

```bash
./build/OpenPuzzle queue list
```


## Build

Build the project:

./scripts/build.sh

---

## Running Tests

Execute the complete test suite:

./scripts/test.sh

---

## Project Status

OpenPuzzle is currently under active development.

Implemented components include:

- Puzzle database management
- Worker management
- Execution queue
- Automatic range allocation
- GPU profile management
- Benchmark framework
- Recovery framework
- BitCrack output parser
- SQLite persistence

The project is evolving towards a modular orchestration platform capable of coordinating cryptographic search engines across multiple GPUs and, in the future, multiple computers.


## Roadmap

The project roadmap is available in:

**ROADMAP.md**

## Support OpenPuzzle

OpenPuzzle is developed in free time and every contribution helps improve the project.

If you would like to support future development, you can make a Bitcoin donation.

bc1qs946xs860jpqd5jv8fcv3jzkpwwydmz44w25h6

Thank you for supporting OpenPuzzle!

---

## Contributing

Contributions, bug reports and feature requests are welcome.

Please read CONTRIBUTING.md before submitting pull requests.

---

## License

This project is released under the MIT License.

---

## Design Philosophy

OpenPuzzle is designed around one simple principle:

> **OpenPuzzle coordinates work. Search engines execute work.**

The OpenPuzzle core should remain independent from any specific search engine.

Search engines such as BitCrack, KeyHunt, Kangaroo or future implementations should be integrated through adapters rather than tightly coupled to the core platform.

This design keeps the project modular, maintainable and extensible.

---

## Current Development Status

OpenPuzzle is currently under active development.

The current implementation already provides:

- Persistent SQLite database
- Bitcoin Puzzle metadata
- Worker management
- Execution queue
- Automatic range allocation
- GPU profile management
- Benchmark foundation
- Recovery framework
- BitCrack output parser

The next development milestone is the implementation of the Service Layer, followed by the Dispatcher and the OpenPuzzleAgent.


---

## Requirements

The current development environment is:

- Linux
- GCC with C++20 support
- CMake 3.22 or newer
- SQLite3
- Git

Future releases are expected to support additional operating systems and toolchains.

---

## Project Goals

The long-term objective of OpenPuzzle is to provide a unified platform capable of managing large-scale cryptographic keyspace exploration.

Planned capabilities include:

- Multiple GPUs
- Multiple computers
- Multiple search engines
- Automatic scheduling
- Checkpoint recovery
- Performance benchmarking
- Distributed execution
- Modular engine plugins

OpenPuzzle is being designed as an orchestration platform rather than another search engine.


---

## Supported Engines

The OpenPuzzle core is designed to support multiple search engines.

Current integration:

- BitCrack (in development)

Planned integrations:

- KeyHunt
- Kangaroo
- VanitySearch
- Additional engines through a plugin architecture.


---

## Project Vision

The long-term vision of OpenPuzzle is to become a complete orchestration platform for cryptographic keyspace exploration.

Instead of focusing on a single search engine, OpenPuzzle aims to provide a unified environment capable of managing:

- Workers
- GPUs
- Search engines
- Jobs
- Queues
- Benchmarks
- Statistics
- Distributed execution

The goal is to allow researchers and enthusiasts to coordinate large search workloads from a single platform while keeping the architecture modular and extensible.

