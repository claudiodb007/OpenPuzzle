<img src="https://raw.githubusercontent.com/claudiodb007/OpenPuzzle/v1.0.59/OpenPuzzle/resources/branding/openpuzzle-logo-header.png" alt="OpenPuzzle logo" width="280">

# OpenPuzzle

OpenPuzzle is an open-source desktop and command-line client for the public Bitcoin Puzzle project. It asks the coordinator for a range, runs it with a search engine such as BitCrack, KeyHunt, Kangaroo or Vanity, and reports progress. You can monitor active work and, where supported, recover interrupted searches.

[Download for Linux](https://github.com/claudiodb007/OpenPuzzle/releases/latest) · [Public Windows preview](https://github.com/claudiodb007/OpenPuzzle/releases/tag/windows-test-20261021-1) · [Live dashboard](https://claudiodb.com/) · [Português](#guia-rápido-em-português)

## Install on Linux

Use the Debian package on **Ubuntu 24.04 or newer, x86-64**. For GPU use, install the driver and runtime required by your chosen CUDA or OpenCL engine.

1. Open [Releases](https://github.com/claudiodb007/OpenPuzzle/releases/latest) and expand **Assets**. Download the file named `OpenPuzzle-<version>-Linux-x86_64.deb`.
2. In the directory containing the download, install it with APT. For version 1.0.59:

   ```bash
   sudo apt install ./OpenPuzzle-1.0.59-Linux-x86_64.deb
   ```

3. Open **OpenPuzzle** from your applications menu, or run `openpuzzle-ui`.

## Start a search

In the desktop app, choose an execution mode and a device. Leave the puzzle at **0** to select the first unsolved puzzle automatically. Run **Benchmark** if the GPU setup needs a measured profile, then click **Start**. The status panel shows the range, speed and progress. **Safe Stop** finishes the current work before exiting.

On Linux you can also use the terminal:

```bash
openpuzzle run          # First unsolved puzzle
openpuzzle run 71       # Choose a puzzle explicitly
openpuzzle status       # Show local execution status
openpuzzle safestop     # Finish active work and stop taking new ranges
```

## Try the Windows preview

The [public Windows test release](https://github.com/claudiodb007/OpenPuzzle/releases/tag/windows-test-20261021-1) has a Windows x64 installer under **Assets**. Open **OpenPuzzle Test - Auto Benchmark** after installing, choose a backend and device under **Tools**, then run **Benchmark** if you have no measured profile for that setup.

## Guia rápido em português

O OpenPuzzle coordena a pesquisa distribuída do Bitcoin Puzzle. Pede intervalos ao servidor, acompanha o progresso e usa motores externos para fazer a pesquisa.

1. Em **Linux**, descarrega o ficheiro `.deb` em [Releases → Assets](https://github.com/claudiodb007/OpenPuzzle/releases/latest). Na pasta da transferência, instala com `sudo apt install ./OpenPuzzle-1.0.59-Linux-x86_64.deb`, ajustando o nome à versão descarregada.
2. Abre **OpenPuzzle**, escolhe o modo e o dispositivo. O puzzle **0** seleciona automaticamente o primeiro por resolver.
3. Se a GPU precisar de um perfil, corre o **Benchmark** e carrega em **Iniciar**. **Safe Stop** acaba o trabalho em curso antes de sair.

Para **Windows**, existe um [instalador x64 de teste](https://github.com/claudiodb007/OpenPuzzle/releases/tag/windows-test-20261021-1). Podes acompanhar o progresso no [painel do projeto](https://claudiodb.com/).

## Documentation and source

- [Source for the Linux 1.0.59 release](https://github.com/claudiodb007/OpenPuzzle/tree/v1.0.59)
- [Continuous execution and recovery](https://github.com/claudiodb007/OpenPuzzle/blob/v1.0.59/OpenPuzzle/docs/CLIENT_RUNTIME.md)
- [GPU controls](https://github.com/claudiodb007/OpenPuzzle/blob/v1.0.59/OpenPuzzle/docs/GPU_CONTROLS.md)
- [Report a problem or suggest an improvement](https://github.com/claudiodb007/OpenPuzzle/issues)
- [MIT license](LICENSE)
