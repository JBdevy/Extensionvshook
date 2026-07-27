# VS Hook Extension

Extensão nativa do VS Hook para REAPER 7.65 ou superior.

## Artefatos

- Windows x64: `reaper_VSHookExt.dll`.
- macOS universal: `reaper_VSHookExt.dylib`.

Os binários não ficam versionados. Os workflows em `.github/workflows` geram
os artefatos ao executar manualmente ou publicar uma tag `v*`.

A Hook Center instala o binário e a pasta auxiliar
`VSHookTelepromptSettings` dentro de `UserPlugins`. A pasta auxiliar é
necessária somente para abrir o aplicativo de Recados pelo menu da extensão,
mas não faz parte dos artefatos gerados neste repositório. As Configurações do
Teleprompt são nativas e funcionam diretamente pela extensão.

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

A tela de Configurações do Teleprompt é nativa, incluindo seletor de cor por
RGB, código hexadecimal e paleta. As opções de TP1, TP2 e Recados são salvas
automaticamente no mesmo estado usado pelas janelas nativas.

## Aplicativo auxiliar de Recados

O aplicativo Electron usado para enviar Recados fica em
`teleprompt-settings`.

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
- `teleprompt-settings`: aplicativo auxiliar de envio de recados.
- `tests`: testes nativos.
- `.github/workflows`: builds oficiais de Windows e macOS.

Consulte `THIRD_PARTY_NOTICES.md` para os componentes de terceiros.
