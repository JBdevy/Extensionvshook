VS Hook Loader | Hook Developer

Esta extensao adiciona no menu Extensions:

- VS Hook Pro
- VS Hook Basic
- Iniciar VS Hook Pro junto com o REAPER
- Iniciar VS Hook Basic junto com o REAPER
- Iniciar VS Hook Pro junto com ESTE projeto
- Iniciar VS Hook Basic junto com ESTE projeto

Regras:

- As opcoes de iniciar junto com o REAPER sao exclusivas: quando Pro esta ativo, Basic fica desativado; quando Basic esta ativo, Pro fica desativado.
- As opcoes de iniciar junto com ESTE projeto tambem sao exclusivas.
- A opcao ativa aparece com V no menu.
- A configuracao global usa ExtState persistente.
- A configuracao do projeto usa ProjExtState e fica salva no .RPP quando o projeto e salvo.

Scripts esperados:

Windows:
- C:/Users/Public/VS Hook APP/VS Hook Pro.lua
- C:/Users/Public/VS Hook APP/VS Hook Basic.lua

macOS:
- REAPER/Scripts/VS Hook APP/VS Hook Pro.lua
- REAPER/Scripts/VS Hook APP/VS Hook Basic.lua

APIs nativas mantidas:

reaper.VS_Hook_SetClipboard(texto)
reaper.VS_Hook_GetClipboard("", 65536)
