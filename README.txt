PASSO A PASSO DO ZERO

1) No projeto, substitua:
   - extension/sdk/reaper_plugin.h
   - extension/sdk/reaper_plugin_functions.h
   - extension/WDL  (de preferencia a pasta WDL inteira, usando a copia limpa)

2) Use estes arquivos:
   - extension/src/vshook_extension.cpp
   - extension/CMakeLists.txt
   - .github/workflows/build-vshook-loader.yml

3) Estrutura esperada:
   extension/
     CMakeLists.txt
     sdk/
       reaper_plugin.h
       reaper_plugin_functions.h
     WDL/
       swell/
         swell.h
         ...
     src/
       vshook_extension.cpp

4) No Mac, depois do artifact pronto:
   - abra o REAPER
   - descubra a pasta UserPlugins que voce quer testar
   - copie reaper_vshook.dylib
   - remova quarantine se precisar:
     xattr -dr com.apple.quarantine /caminho/para/reaper_vshook.dylib

5) Esse cpp:
   - cria o menu VS Hook em Extensions
   - registra/carrega apenas o script VS Hook.lua
   - no Windows procura em:
     C:/Users/Public/VS Hook APP/VS Hook.lua
     %PUBLIC%/VS Hook APP/VS Hook.lua
   - no macOS procura em:
     Scripts/VS Hook APP/VS Hook.lua
     Scripts/VS Hook.lua
     VS Hook APP/VS Hook.lua

Alteracao desta versao:
- Removido o carregamento/registro do Hook Lyrics pelo loader.
- O loader agora registra/carrega somente VS Hook.lua.
- Mantida a opcao no menu Extensions > Abrir junto com o REAPER.
- A opcao fica marcada/desmarcada e salva no ExtState persistente do REAPER.
- Quando ativa, o loader espera alguns ciclos de inicializacao e abre somente o VS Hook automaticamente.
