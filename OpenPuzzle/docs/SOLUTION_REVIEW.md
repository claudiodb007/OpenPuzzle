# Solution Report Review

Potential solutions are reported with metadata only. The server receives an
assignment UUID and an anonymous client UUID. It never receives the private
key, `found.txt`, a local filesystem path or raw engine output.

Reports begin in the `pending` state. A pending report does not:

- mark a puzzle as solved;
- stop range allocation;
- change scheduler priority;
- complete or cancel an assignment;
- prove that a private key is valid.

## Security boundary

There is currently no authenticated administrative web interface. Review
operations must therefore be performed through phpMyAdmin. Never place review
scripts or database credentials inside `public_html`.

Never paste a private key into phpMyAdmin, the website, a report, a support
ticket, a chat, an issue or a log.

## List pending reports

Use the private server operation named
`list_pending_solution_reports.sql`. Server database operations are
intentionally maintained outside the public OpenPuzzle client repository.

Copy its reviewed SQL into phpMyAdmin and execute it. The result contains
identifiers and public range metadata only.

## Verify a report

Independent verification must happen before approval, using trusted offline
tooling and public blockchain information.

Use the private server operation named
`verify_solution_report.sql`. Replace only `REPLACE_WITH_REPORT_UUID` with the
report UUID and execute the complete reviewed transaction in phpMyAdmin.

A successful update returns:

```text
reports_verified = 1
```

Verification changes only the report from `pending` to `verified`. It does not
mark the puzzle as solved. That remains a separate administrative action.

## Reject a report

Use the private server operation named
`reject_solution_report.sql`. Replace only `REPLACE_WITH_REPORT_UUID` and
execute the complete reviewed transaction.

A successful update returns:

```text
reports_rejected = 1
```

Rejection does not modify the puzzle, range coverage or scheduler.

## Unexpected results

A result of zero updated rows means the report UUID was not found or the
report had already been reviewed. Inspect the final `SELECT` before taking
further action.

Do not force an update by removing the `status = 'pending'` condition.

## Mark a verified puzzle as solved

This is a separate, final administrative operation. Perform it only after the
report is `verified` and a public Bitcoin transaction independently confirms
the solution.

Use the private server operation named
`mark_verified_puzzle_solved.sql`. Replace only:

- `REPLACE_WITH_VERIFIED_REPORT_UUID`;
- `REPLACE_WITH_64_CHARACTER_BITCOIN_TXID`.

Then execute the complete transaction in phpMyAdmin.

The operation proceeds only when the report exists, is `verified`, the puzzle
is not already solved and the TXID contains exactly 64 hexadecimal
characters.

A successful result contains:

```text
puzzles_marked_solved = 1
```

The operation sets `solved`, `solved_at` and `solved_txid`. It leaves `active`
unchanged so the solved puzzle remains available for public historical
display.

Assigned ranges for that puzzle are changed to `cancelled` and their leases
are cleared. This makes active clients stop safely when their next progress
request is rejected. Completed ranges and historical coverage are preserved.

If `puzzles_marked_solved` is zero, do not remove any safety condition. Check
the report UUID, report status and TXID before attempting the operation again.
