# Execution Engine

The Execution Engine tracks the lifecycle of running a tool such as BitCrack.

Current development model:

```text
Range RESERVED
  -> Job RESERVED
  -> Execution RUNNING
  -> Progress checkpoints
  -> Execution FINISHED
  -> Job COMPLETED
  -> Range COMPLETED
```

## Planned parser

Future builds will parse BitCrack output in real time and extract:

- current speed in MKey/s
- current key/checkpoint
- elapsed time
- estimated progress
- found-key status

## Safety rule

A range should only be marked `COMPLETED` when the execution exits normally and the command keyspace matches the reserved range.
