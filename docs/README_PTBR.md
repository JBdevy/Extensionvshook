# VS Hook APP Loader Extension

Este pacote muda a estratégia:

- a extensão NÃO tenta recriar a UI em C++
- a extensão apenas registra e executa o teu script compilado
- o menu em `Extensions` aparece como `VS Hook APP`
- a ação no `Action List` também aparece como `VS Hook APP`

## Como funciona

A extensão usa:
- `custom_action` + `hookcommand2` para criar a ação própria
- `hookcustommenu` + `AddExtensionsMainMenu()` para criar o item em `Extensions`
- `AddRemoveReaScript()` para registrar o script no Action List
- `Main_OnCommand()` para executar o script registrado

## Nome do script que ela procura

A DLL procura este arquivo:

`%APPDATA%\REAPER\Scripts\VS Hook APP\VS Hook.lua`

No macOS, a pasta equivalente fica dentro do resource path do REAPER:
`~/Library/Application Support/REAPER/Scripts/VS Hook APP/VS Hook.lua`

## Importante

O arquivo que ela vai executar pode ser:
- o teu `.luac` renomeado para `.lua`

## Build

1. copie `reaper_plugin.h` e `reaper_plugin_functions.h` para `extension/sdk`
2. no x64 Native Tools:
   - `cmake -S . -B build -A x64`
   - `cmake --build build --config Release`
3. copie `build\Release\reaper_vshook.dll` para:
   - `%APPDATA%\REAPER\UserPlugins`

## Instalação do script

Crie a pasta:
`%APPDATA%\REAPER\Scripts\VS Hook APP`

e coloque lá o arquivo compilado com o nome:
`VS Hook.lua`

## Resultado

Depois de abrir o REAPER:
- em `Extensions` vai aparecer `VS Hook APP`
- no `Action List` vai aparecer `VS Hook APP`
- ao clicar, ele executa o teu script compilado
