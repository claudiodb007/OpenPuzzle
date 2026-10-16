# Native Windows panel languages

The Windows panel and Linux Qt panel share `UiTranslations.hpp`. The existing
125 Linux translation entries and language codes remain unchanged. The Qt
`UiLanguage` functions are adapters; Windows needs no Qt runtime.

Both panels offer English (`en`), Portuguese (`pt`), French (`fr`) and Spanish
(`es`), with English as the initial fallback. Windows stores only an explicit
language selection in HKCU\Software\OpenPuzzle\WindowsPanel\Language.
Reading the preference does not create its key. Unknown, malformed or missing
preferences fall back to English. No server option is introduced.

Changing the language updates captions, status, confirmations and known display
messages. Original CLI output remains unchanged for status parsing, telemetry
selection and thermal policy parsing. IDs, file paths and unknown technical
payloads are retained. Changing the language does not reload a selected profile,
restart an engine, create identity or request an assignment.

The portable language suite checks all four languages, complete entries,
placeholder preservation, known messages and unmodified path/protocol data.
The native window fixture uses an isolated HKCU test key, verifies persistence
and malformed-data fallback, switches all four HWND captions and changes the
language while its owned fake runtime is active. It retains the existing
cooperative-close, fixed-server, theme, logo and scrolling checks.

The combined package expects 39 Windows suites, 19 portable suites and 171 full
Linux suites. Local cross-compilation establishes build compatibility only;
native MSVC execution and visual review require the Windows report.
