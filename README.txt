VS Hook Loader | Hook Developer

Esta extensao adiciona no menu Extensions:

- VS Hook Pro 2.3
- VS Hook Basic 2.3
- Iniciar VS Hook Pro junto com o REAPER
- Iniciar VS Hook Basic junto com o REAPER
- Iniciar VS Hook Pro junto com ESTE projeto
- Iniciar VS Hook Basic junto com ESTE projeto

Regras:

- Pro e Basic sao exclusivos: ao abrir um, a extensao fecha o outro se ele estiver aberto.
- A extensao tambem guarda o ultimo modo aberto na sessao para fechar o anterior mesmo quando o REAPER nao reporta corretamente o toggle do defer script.
- As opcoes de iniciar junto com o REAPER sao exclusivas: quando Pro esta ativo, Basic fica desativado; quando Basic esta ativo, Pro fica desativado.
- As opcoes de iniciar junto com ESTE projeto tambem sao exclusivas.
- Iniciar junto com o REAPER e iniciar junto com ESTE projeto tambem sao exclusivos entre si: marcou um, o outro e desmarcado.
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

Build macOS corrigido:
- O artefato macOS continua sendo um unico reaper_vshook.dylib.
- x86_64 e compilado com deployment target 10.13 para clientes macOS 10.x Intel.
- arm64 e compilado com deployment target 11.0 para Apple Silicon.
- O universal final e criado com lipo, seguindo a mesma ideia da SWS: target por arquitetura antes de juntar.
