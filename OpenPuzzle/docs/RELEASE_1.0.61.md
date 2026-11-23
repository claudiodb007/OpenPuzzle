# OpenPuzzle 1.0.61 — Vanity CUDA on Windows

The Windows x64 installer and portable ZIP include Vanity / CUDA for the
public Puzzle 71. Use a compatible NVIDIA GPU and select **Vanity / CUDA**.
Run a CUDA benchmark on the selected device if the desktop needs a profile.
The client sends heartbeats and progress to the coordinator and requests
another assignment after each verified completion.

**Safe Stop** finishes the current range and stops. **Stop now** preserves
the verified progress. **Recover** resumes the same range while its lease
is valid. An expired lease leaves the local state available for review.

Windows x64 packages:

- `OpenPuzzle-1.0.61-Windows-x86_64-Setup.exe`
- `OpenPuzzle-1.0.61-Windows-x86_64.zip`

Linux x86_64 packages:

- `OpenPuzzle-1.0.61-Linux-x86_64.deb`
- `OpenPuzzle-1.0.61-Linux-x86_64.tar.gz`
- `OpenPuzzle-1.0.61-portable-XXXXXXXX.deb` for the built-in updater.
  The suffix is the first eight characters of the Debian package SHA-256.

Use **Safe Stop** and wait for the active range to finish before opening the
new version. Open **OpenPuzzle 1.0.61** from the Windows Start menu after
installation, or extract the portable ZIP and open `Start.cmd`.

The Vanity engine is a separate GPL-3.0 program. See the corresponding source
in `OpenPuzzle-Vanity` and the notice in the Windows package.
