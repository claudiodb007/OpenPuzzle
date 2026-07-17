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

Open this repository file:

```text
server/database/operations/list_pending_solution_reports.sql
```

Copy its SQL into phpMyAdmin and execute it. The result contains identifiers
and public range metadata only.

## Verify a report

Independent verification must happen before approval, using trusted offline
tooling and public blockchain information.

Open:

```text
server/database/operations/verify_solution_report.sql
```

Replace only `REPLACE_WITH_REPORT_UUID` with the report UUID and execute the
complete transaction in phpMyAdmin.

A successful update returns:

```text
reports_verified = 1
```

Verification changes only the report from `pending` to `verified`. It does not
mark the puzzle as solved. That remains a separate administrative action.

## Reject a report

Open:

```text
server/database/operations/reject_solution_report.sql
```

Replace only `REPLACE_WITH_REPORT_UUID` and execute the complete transaction.

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
