# Scheduler

The Scheduler decides what should run next.

It must not execute engines directly.

## Flow

Find idle workers -> find available work -> reserve range -> create job -> create execution request -> send request to Dispatcher -> react to completion or failure.

## Future Loop

Detect offline workers, recover abandoned work, assign jobs to idle workers, sleep, repeat.
