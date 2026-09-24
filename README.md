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

## Estrutura

- `src`: código da extensão e dos decodificadores nativos.
- `sdk`: cabeçalhos do SDK do REAPER.
- `WDL`: componentes WDL/SWELL necessários no macOS.
- `tests`: testes nativos.
- `.github/workflows`: builds oficiais de Windows e macOS.

Consulte `THIRD_PARTY_NOTICES.md` para os componentes de terceiros.

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
