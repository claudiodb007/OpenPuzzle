# OpenPuzzle

OpenPuzzle is an open-source orchestration platform for distributed cryptographic keyspace exploration.

It is not a search engine. OpenPuzzle coordinates work and delegates execution to external engines such as BitCrack, KeyHunt, Kangaroo, VanitySearch or future plugins.

## Current Features

- SQLite persistence
- Bitcoin puzzle metadata synchronization
- Puzzle CLI
- Worker management
- Worker maintenance states
- Range allocation
- Execution queue
- Job and execution tracking
- GPU profile management
- Benchmark foundation
- BitCrack output parsing
- Recovery support
- Architecture documentation

## Core Commands

```bash
OpenPuzzle sync-data --dir data
OpenPuzzle puzzle list
OpenPuzzle puzzle show 71

OpenPuzzle worker register --machine escritorio --gpu "RTX 4070 Super" --backend cuda --engine bitcrack
OpenPuzzle worker list
OpenPuzzle worker drain 1
OpenPuzzle worker enable 1

OpenPuzzle queue add --puzzle 71 --block-bits 40
OpenPuzzle queue list --puzzle 71
