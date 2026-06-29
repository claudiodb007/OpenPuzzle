# Changelog

## 0.10-dev — Foundation/Core

- Added persistent execution model.
- Added `executions` table.
- Added `statistics` table.
- Added richer `ranges` lifecycle.
- Added `list-ranges`.
- Added `complete-job` for manually completing job/range.
- Added `stats` command.

## 0.10.1-dev

- Fixed Boost.Multiprecision expression-template compile error in RangeAllocator.


## 0.11-dev — Execution Engine foundation

- Added `execution_progress` table.
- Added `audit_log` table.
- Added `dashboard` command.
- Added simulated progress checkpoint support for dry-run tests.
- Added audit events for dry-runs and executions.
- Added documentation for the Execution Engine.

## 0.11.1-dev

- Fixed command dispatcher: `dashboard` and `audit` are now registered.
- Updated CLI banner to 0.11.1-dev.

## 0.11.2-dev

- Fixed linker error by adding concrete dashboard/audit implementations.
