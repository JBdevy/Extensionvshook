VS Hook Loader | Hook Developer

Esta extensao adiciona no menu Extensions:

- VS Hook Pro
- VS Hook Basic
- Iniciar VS Hook Pro junto com o REAPER
- Iniciar VS Hook Basic junto com o REAPER
- Iniciar VS Hook Pro junto com ESTE projeto
- Iniciar VS Hook Basic junto com ESTE projeto

Regras:

- Pro e Basic sao exclusivos: ao abrir um, a extensao fecha o outro se ele estiver aberto.
- As opcoes de iniciar junto com o REAPER sao exclusivas: quando Pro esta ativo, Basic fica desativado; quando Basic esta ativo, Pro fica desativado.
- As opcoes de iniciar junto com ESTE projeto tambem sao exclusivas.
- A opcao ativa usa a marcacao nativa do menu do REAPER, sem letra V no texto.
- A marcacao do menu e atualizada quando o menu abre, sem precisar reiniciar o REAPER.
- A configuracao global usa ExtState persistente.
- A configuracao por projeto usa ProjExtState dentro do proprio .RPP, igual a SWS. Depois de marcar, salve o projeto para a configuracao ficar permanente.

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
