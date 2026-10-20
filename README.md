# VS Hook Extension

Extensão nativa do VS Hook para REAPER 7.65 ou superior.

## Artefatos

- Windows x64: `reaper_VSHookExt.dll`.
- macOS universal: `reaper_VSHookExt.dylib`.

Os binários não ficam versionados. Os workflows em `.github/workflows` geram
os artefatos ao executar manualmente ou publicar uma tag `v*`.

A Hook Center instala o binário e a pasta auxiliar
`VSHookTelepromptSettings` dentro de `UserPlugins`. A pasta auxiliar é
necessária para abrir Configurações do Teleprompt e Recados pelo menu da
extensão, mas não faz parte dos artefatos gerados neste repositório.

## Ativação e uso offline

Após a ativação inicial, a extensão funciona com a Hook Center fechada e sem
internet, inclusive depois de longos períodos desconectada. Não há mais
contagem de dias, avisos de reconexão nem bloqueio pelo prazo de renovação do
token. O token local continua sendo conferido pela assinatura, produto e
identificação do computador; uma instalação sem ativação ou um token alterado
continua recusado. Os antigos arquivos auxiliares de relógio e avisos não são
mais necessários para carregar a extensão.

A Hook Center continua instalando/ativando o produto e transportando os
recursos que a utilizam explicitamente, como Project Sync entre computadores.
Isso não condiciona a abertura e o uso local da extensão.

Teste em macOS ou Developer Command Prompt do Visual Studio no Windows:

```text
node tests/run-license-runtime-policy-test.js
```

O teste compila o verificador real com uma chave RSA temporária de teste e
verifica tokens recentes e antigos, assinatura adulterada e vínculo com o
dispositivo, sem ler nem alterar a ativação instalada. A chave de produção e
as políticas de conta do servidor não são modificadas.

## Build nativo

Requisitos: CMake 3.20 ou superior e compilador com suporte a C++17.

Windows:

```powershell
cmake -S . -B build -A x64
cmake --build build --config Release
```

macOS:

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --config Release
```

O workflow do macOS compila separadamente `x86_64` para macOS 10.13+ e
`arm64` para macOS 11+, depois combina os dois binários.

## Desenho das telas principais

O painel principal (incluindo suas abas), a Hook Controller e os Big Clocks
usam a camada `native_ui_renderer`. O layout, as áreas de clique, as cores
configuráveis e os cálculos das barras de progresso/regresso continuam
compartilhados com os controles existentes.

No Windows, Direct2D desenha em uma superfície persistente da janela e
DirectWrite desenha os textos. O dispositivo de hardware é preferido; quando
indisponível, Direct2D pode selecionar software. Operações GDI remanescentes
usam a mesma superfície por interoperabilidade, respeitando a ordem de desenho
e os recortes. Falhas na criação ou perda do dispositivo permitem redesenho
pelo caminho anterior, com nova tentativa posterior.

No macOS, Core Graphics e Core Text desenham no contexto do SWELL, e o AppKit
usa camadas de Core Animation para apresentação. O painel principal conserva
seu buffer e seus recortes incrementais. Isso não transfere toda rasterização
para a GPU. Ambos os sistemas mantêm caches limitados de texto.

Os testes de renderização independem do REAPER. No Windows, verificam a
mistura Direct2D/GDI, recortes, texto, recriação da superfície e redução de
progresso; no Mac, verificam Core Graphics/Core Text em escalas 1x e 2x.

```text
cmake -S . -B build -DVSHOOK_BUILD_TESTS=ON
cmake --build build --config Release
ctest --test-dir build -C Release --output-on-failure
```

Os testes de cada plataforma precisam ser executados no sistema correspondente.
O ganho de desempenho da interface completa deve ser medido no REAPER com
projetos representativos.

## Estrutura

- `src`: código da extensão e dos decodificadores nativos.
- `sdk`: cabeçalhos do SDK do REAPER.
- `WDL`: componentes WDL/SWELL necessários no macOS.
- `tests`: testes nativos.
- `.github/workflows`: builds oficiais de Windows e macOS.

Consulte `THIRD_PARTY_NOTICES.md` para os componentes de terceiros.

## Itens do Premix

O Premix inclui os itens cujo início está dentro da música, incluindo regiões
filhas: início da música inclusivo, fim exclusivo. Um áudio que começou antes
não pertence ao Premix dessa música. O grid do Mixer continua mostrando os
trechos sobrepostos, pois representa o que existe naquele intervalo.

Filhos de markers enviados pelo app como `m12` são resolvidos apenas para o
marker correspondente, sem confundir uma região com o mesmo número. O snapshot
confirma esse identificador em `selectedSongId` e informa o identificador nativo
em `selectedCanonicalSongId`, mantendo compatibilidade com os apps existentes.

Ao trocar de aba, o estado anterior é descartado antes de consumir o próximo
comando do app. A seleção de Premix enviada para o projeto novo não é apagada
no ciclo seguinte. Comandos com `premixProjectId`/`premixProjectPath` são
validados contra a aba ativa para descartar gestos atrasados do projeto anterior.

Teste de regressão:

```text
node tests/run-premix-selection-test.js
node tests/run-premix-item-scope-test.js
node tests/run-project-tab-premix-test.js
```

## Consumo em repouso

A seleção de pistas do fadeout é resolvida em uma passagem pelo projeto,
mantendo ordem, compatibilidade com o estado Lua e atualização após renomear,
remover pistas ou trocar de projeto.

Com Project Sync sem par, transporte parado e sem consumidores, o snapshot
completo é atualizado a cada segundo. Janelas, consultas da Hook Center/app
(inclusive sem heartbeat de Diretor), reprodução, cronômetro, fadeout e um
par conectado preservam o intervalo normal de 220 ms. Comandos forçados não
esperam esse intervalo. A descoberta e a recepção de comandos continuam ativas.

No macOS/Linux, a espera de conexões usa `poll`, permitindo descritores acima
de 1023 em projetos com muitos arquivos abertos. Erros fatais encerram a ponte
para reinicialização, sem um ciclo contínuo que consuma CPU.

O painel conserva áreas separadas de pintura no backbuffer. Animações RGB
invalidam os contornos; relógio, letreiro e botões piscantes mantêm suas próprias
atualizações. Mudanças de layout, modais e buffers novos seguem o redesenho
completo. Cores fixas de contorno não acionam o timer de animação RGB.
Com `VSHOOK_BUILD_TESTS=ON`, o alvo `vshook_ui_damage_mac_test` compara os pixels
da pintura parcial e completa em CoreGraphics, inclusive em escala Retina.
Durante a reprodução, os cartões de transporte invalidam suas barras e o
tempo restante; Parts invalida a regressão e o item engatilhado, sem repintar
as duas colunas inteiras. Mudanças de conteúdo, layout, seleção e modais
mantêm a composição completa. Preenchimentos arredondados usam o caminho
retangular somente quando toda a área alterada está no interior sólido;
contornos fora dessa área são ignorados. O teste de pixels também verifica
esse caso, cantos, áreas separadas e formas pequenas. A cadência do transporte
e o serviço do aplicativo remoto não dependem dessas otimizações de pintura.
O RGB possui cadência própria de 100 ms, também durante reprodução e arraste;
o timer de 30/60 Hz dos controles não acelera o redesenho decorativo. A cor
continua calculada pelo tempo real, conservando a duração de cada ciclo.

Teste de regressão em macOS/Linux com compilador C++17, ou no Developer
Command Prompt do Visual Studio no Windows:

```text
node tests/run-idle-work-test.js
node tests/run-json-string-cache-test.js
node tests/run-director-heartbeat-paint-test.js
node tests/run-passive-state-test.js
```

O teste executa as funções extraídas do código de produção, compara a seleção
com o comportamento anterior e verifica a retomada por clientes remotos com
as janelas fechadas. Reduções de CPU/bateria devem ser medidas novamente no
REAPER após carregar o novo binário.

O app Músico também recebe regiões, repertórios e status quando todas as
janelas do VS Hook estão fechadas e não há Diretor conectado. A consulta
`GET /state` mantém esse leitor ativo por 12 segundos, sem ativar fila,
AutoStop, Multiloops, Premix, afinador ou auto-início do cronômetro. A entrada
posterior do Diretor força um snapshot novo antes de retomar essas funções.
O poll de descoberta da Hook Center usa `GET /state?discovery=1`: recebe o
cache existente sem manter um leitor ativo. Sem app, a descoberta não passa a
reconstruir o estado completo. O teste passivo executa o guard real do timer
e os trechos de efeitos do snapshot, incluindo desconexão e reconexão.

O status do Project Sync reutiliza a serialização JSON do manifesto e do plano
de diferenças enquanto o conteúdo permanece idêntico. O cache guarda somente
o último valor de cada campo e usa o mesmo mutex do estado. Alterações no
documento, inclusive sem mudança de revisão, invalidam o valor imediatamente;
transporte e progresso continuam sendo publicados em cada consulta. O teste
de JSON verifica equivalência byte a byte e informa um benchmark isolado, sem
interpretá-lo como redução de CPU do processo inteiro.

Heartbeats repetidos do Diretor atualizam a presença sem invalidar toda a
janela. Mudanças na permissão e fechamento dos avisos ainda repintam de
imediato; a recepção remota continua ativa com o painel fechado.

## Clonar repertório

No seletor de repertórios, `Clone` fica à esquerda de `Multi`. Com um único
repertório da aba atual selecionado, cria `Nome original - clone 2`,
`Nome original - clone 3` etc., evitando nomes já existentes. A numeração é por
nome original; clonar uma cópia não acumula sufixos. Copia os itens e sua ordem, incluindo blocos e campos de
configuração, sem duplicar mídia nem alterar o repertório em reprodução.
A cópia fica selecionada no modal; use `Abrir` quando quiser ativá-la.
Para repertórios de outra aba, abra essa aba antes de clonar, assim como nas
demais operações de edição do seletor.

Teste de regressão (Developer Command Prompt do Visual Studio):

```text
node tests/run-playlist-clone-test.js
```
