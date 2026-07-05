# OpenPuzzle Architecture Overview

OpenPuzzle is not a search engine. It is an orchestration platform for search engines.

OpenPuzzle coordinates work. Engines execute work.

## Principle

The Scheduler must never call BitCrack, KeyHunt, Kangaroo or any concrete engine directly.

The Scheduler creates execution requests. The Dispatcher and Worker Agent execute them.

## Layers

CLI -> Services -> Scheduler/Queue/Worker/Puzzle -> Dispatcher -> Engine Manager -> External Engines -> GPU/CPU

## Source of Truth

SQLite is the single source of truth for puzzles, ranges, jobs, workers, executions, heartbeats, checkpoints and statistics.
