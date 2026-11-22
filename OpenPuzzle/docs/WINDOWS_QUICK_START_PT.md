# OpenPuzzle 1.0.60 para Windows

Para Windows x64. Instala primeiro o driver AMD ou NVIDIA da tua placa.

## Instalar ou abrir

- **Instalador EXE:** executa o instalador e abre **OpenPuzzle 1.0.60** no menu
  Iniciar. A instalação é feita para o utilizador atual.
- **ZIP portátil:** extrai todos os ficheiros e abre `Start.cmd` ou
  `openpuzzle-ui.exe` na pasta extraída.

Se estiveres a pesquisar com outra versão, usa **Safe Stop** e espera que o
trabalho em curso termine antes de abrir a 1.0.60. As duas opções usam os
perfis e o estado existentes em `%LOCALAPPDATA%\OpenPuzzle`.

## Fazer uma pesquisa

1. Em **Nova execução**, escolhe **BitCrack / CUDA** para NVIDIA ou **BitCrack /
   OpenCL** para uma GPU compatível. Também podes usar **KeyHunt / CPU**.
2. Seleciona o dispositivo e corre **Benchmark** para obter um perfil.
3. Usa o puzzle **0** para escolher automaticamente o primeiro por resolver,
   ou indica o número do puzzle. Carrega em **Iniciar**.

A aplicação mostra o progresso, a velocidade e a telemetria que o driver
disponibiliza. Em **Potência e ventoinhas das GPUs**, escolhe a placa física
e usa **Atualizar**. Os controlos disponíveis dependem da placa e do driver.
O limite de potência AMD é um ajuste percentual. O consumo medido aparece
em watts quando o driver o reporta. Aplicar alterações pode exigir elevação.

**Safe Stop** acaba o range atual e encerra antes de pedir outro. O
`Diagnostico-GPU.cmd` recolhe um relatório de leitura para diagnóstico. O
Vanity para Windows fica previsto para uma versão posterior aos testes.

## Licenças

As bibliotecas dinâmicas Qt 6.8.3 são do SDK oficial. As licenças, avisos,
SBOM e informação sobre o código correspondente estão em `licenses/qt`.
O aviso do SDK AMD ADLX está em `licenses/amd-adlx`. A aplicação carrega o
ADLX e o NVML a partir dos drivers instalados.
