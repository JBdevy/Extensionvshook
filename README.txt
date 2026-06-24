VS Hook Loader - Hook Developer

Arquivos principais:
- extension/src/vshook_extension.cpp
- VS Hook Anotacoes.lua

Funcoes da extensao:
- Abre VS Hook Pro pelo menu Extensions.
- Abre VS Hook Basic pelo menu Extensions.
- Abre VS Hook Anotacoes pelo menu Extensions.
- Permite iniciar Pro ou Basic junto com o REAPER.
- Permite iniciar Pro ou Basic junto com o projeto atual. Essa opcao fica salva no RPP via ProjExtState. Depois de marcar, salve o projeto.
- Clipboard nativo sem SWS:
  reaper.VS_Hook_SetClipboard(texto)
  reaper.VS_Hook_GetClipboard("", 65536)
- API para alimentar a janela de anotações:
  reaper.VS_Hook_SetNotesState(musicaAtual, musicaNaFila)
  reaper.VS_Hook_ClearNotesState()

Instalacao esperada dos scripts:
- Windows: C:/Users/Public/VS Hook APP/VS Hook Pro.lua
- Windows: C:/Users/Public/VS Hook APP/VS Hook Basic.lua
- Windows: C:/Users/Public/VS Hook APP/VS Hook Anotacoes.lua
- macOS: REAPER Resource Path/Scripts/VS Hook APP/...

A janela VS Hook Anotacoes mostra somente:
- musica em reproducao
- musica na fila de espera
