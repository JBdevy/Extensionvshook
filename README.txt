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
   - quando clicar, procura primeiro:
     Scripts/VS Hook APP/VS Hook.lua
   - tambem tenta:
     Scripts/VS Hook/VS Hook.lua
     Scripts/VS Hook Server/VS Hook.lua
