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
   - cria as entradas Extensions > VS Hook Pro e Extensions > VS Hook Basic
   - procura e registra estes scripts:
     Windows:
       C:/Users/Public/VS Hook APP/VS Hook Pro.lua
       C:/Users/Public/VS Hook APP/VS Hook Basic.lua
     macOS:
       Scripts/VS Hook APP/VS Hook Pro.lua
       Scripts/VS Hook APP/VS Hook Basic.lua
       Scripts/VS Hook Pro.lua
       Scripts/VS Hook Basic.lua
       VS Hook APP/VS Hook Pro.lua
       VS Hook APP/VS Hook Basic.lua
   - nao registra nem carrega Hook Lyrics
   - adiciona as opcoes:
       Iniciar VS Hook Pro Junto com o REAPER
       Iniciar VS Hook Basic Junto com o REAPER
   - as duas opcoes de auto-inicio sao exclusivas: ao marcar uma, a outra desmarca.

Alteracao desta versao:
- O menu Extensions nao mostra mais "VS Hook" sozinho.
- Agora mostra "VS Hook Pro" e "VS Hook Basic".
- O auto-inicio agora salva o modo escolhido: pro, basic ou vazio.
- Se o usuario antigo ja tinha "Abrir VS Hook junto com o REAPER" ativo, a nova extensao assume VS Hook Pro por compatibilidade.
- Windows continua compilando com runtime C/C++ estatico, igual ao padrao da SWS, para evitar DLL nao carregar em maquinas sem VC++ Redistributable.
