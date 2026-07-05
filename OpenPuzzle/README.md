# OpenPuzzle

**OpenPuzzle** is an open-source orchestration platform for distributed cryptographic keyspace exploration.

OpenPuzzle is not a search engine. It coordinates workers, GPUs, ranges, jobs and external engines such as BitCrack, KeyHunt, Kangaroo and future plugins.

## Current Features

- SQLite persistence
- Bitcoin Puzzle database (1–160)
- Wallet, Range and Hash160 synchronization
- Worker management
- Worker maintenance states
- Execution queue
- Automatic range allocation
- GPU profile management
- Benchmark foundation
- Recovery support
- Architecture documentation

## Architecture

CLI
  -> Services
  -> Scheduler / Queue / Worker / Puzzle
  -> Dispatcher
  -> Engine Manager
  -> External Engines
  -> GPU / CPU

Architecture documentation is available in:

docs/architecture/

## Roadmap

- Dispatcher
- Worker Agent
- Heartbeats
- Autonomous Scheduler
- Console Dashboard
- REST API
- Multi-PC Cluster
- Web Dashboard
- Plugin Engine System

## Build

    ./scripts/build.sh

## Tests

    ./scripts/test.sh

## Support OpenPuzzle

If OpenPuzzle has been useful to you and would like to support its development, you can make a Bitcoin donation.

Bitcoin (Bech32):

bc1qs946xs860jpqd5jv8fcv3jzkpwwydmz44w25h6

Thank you for supporting OpenPuzzle.

## Contributing

See CONTRIBUTING.md

## License

MIT License
