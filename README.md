# VS Hook Extension

Extensão nativa do VS Hook para REAPER 7.65 ou superior.

## Artefatos

- Windows x64: pacote com `reaper_VSHookExt.dll` e
  `VSHookTelepromptSettings`.
- macOS universal: pacote com `reaper_VSHookExt.dylib` e
  `VSHookTelepromptSettings/VS Hook Teleprompt Settings.app`.

Os binários não ficam versionados. Os workflows em `.github/workflows` geram
os artefatos ao executar manualmente ou publicar uma tag `v*`.

Extraia o pacote e copie tanto o binário quanto a pasta
`VSHookTelepromptSettings` para `UserPlugins`. A pasta auxiliar é necessária
para abrir Configurações do Teleprompt e Recados pelo menu da extensão.

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

## Configurações do teleprompt

A interface Electron auxiliar fica em `teleprompt-settings`.

```powershell
cd teleprompt-settings
npm ci
npm run build:win
```

No macOS, use `npm run build:mac`.

## Estrutura

- `src`: código da extensão e dos decodificadores nativos.
- `sdk`: cabeçalhos do SDK do REAPER.
- `WDL`: componentes WDL/SWELL necessários no macOS.
- `teleprompt-settings`: interface de configurações e recados.
- `tests`: testes nativos.
- `.github/workflows`: builds oficiais de Windows e macOS.

Consulte `THIRD_PARTY_NOTICES.md` para os componentes de terceiros.
