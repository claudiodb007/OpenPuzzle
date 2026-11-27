# OpenPuzzle 1.0.63 para Windows

Para Windows x64. Instala primeiro o driver AMD ou NVIDIA da tua placa.

## Instalar ou abrir

- **Instalador EXE:** executa o instalador e abre **OpenPuzzle 1.0.63** no menu
  Iniciar. A instalação é feita para o utilizador atual.
- **ZIP portátil:** extrai todos os ficheiros e abre `Start.cmd` ou
  `openpuzzle-ui.exe` na pasta extraída.

Se estiveres a pesquisar com outra versão, usa **Safe Stop** e espera que o
trabalho em curso termine antes de abrir a 1.0.63. As duas opções usam os
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

**Safe Stop** acaba o range atual e encerra antes de pedir outro.

Em **Mensagens**, usa **Copiar diagnóstico** para copiar a versão, os modelos
das GPUs, os drivers, o estado da pesquisa e os códigos de erro. O relatório
omite os registos completos, os caminhos locais e as chaves privadas.
O `Diagnostico-GPU.cmd` também está disponível na pasta instalada ou extraída.

## Pesquisar com Vanity numa NVIDIA

1. Seleciona **Vanity / CUDA**, **Puzzle 71** e a GPU NVIDIA.
2. Se necessário, corre **Benchmark** em CUDA para criar o perfil dessa GPU.
3. Carrega em **Iniciar**. O Vanity verifica o range atribuído e pede outro
   automaticamente depois de concluir o atual.

Para interromper já, usa **Parar agora**. **Recuperar** no cartão do range
retoma a partir do último progresso verificado enquanto a atribuição estiver
válida. Se o servidor confirmar que a atribuição expirou ou já não existe,
o cliente arquiva o checkpoint e pede um novo range. **Mensagens** mostra
onde ficou guardado o arquivo.

## Licenças

As bibliotecas dinâmicas Qt 6.8.3 são do SDK oficial. As licenças, avisos,
SBOM e informação sobre o código correspondente estão em `licenses/qt`.
O aviso do SDK AMD ADLX está em `licenses/amd-adlx`. A aplicação carrega o
ADLX e o NVML a partir dos drivers instalados.
O motor Vanity é distribuído separadamente sob GPL-3.0. A licença e a
referência para o código correspondente estão em `licenses/vanity`.
