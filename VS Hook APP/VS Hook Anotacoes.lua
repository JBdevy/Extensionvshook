--[[
VS Hook Anotações | Hook Developer
Janela simples para mostrar a música em reprodução e a música na fila.
A extensão VS Hook atualiza os dados via ExtState/API:
  reaper.VS_Hook_SetNotesState(musicaAtual, musicaNaFila)
]]

local SECTION = "VS_HOOK_NOTES"
local CURRENT_KEY = "CURRENT_SONG"
local QUEUED_KEY = "QUEUED_SONG"
local TITLE = "VS Hook Anotações"

local function trim(value)
  return tostring(value or ""):gsub("^%s+", ""):gsub("%s+$", "")
end

local function get_state()
  local current = trim(reaper.GetExtState(SECTION, CURRENT_KEY) or "")
  local queued = trim(reaper.GetExtState(SECTION, QUEUED_KEY) or "")
  if current == "" then current = "Nenhuma música em reprodução" end
  if queued == "" then queued = "Nenhuma música na fila" end
  return current, queued
end

local function set_color(hex)
  local r = math.floor(hex / 0x10000) % 0x100
  local g = math.floor(hex / 0x100) % 0x100
  local b = hex % 0x100
  gfx.set(r / 255, g / 255, b / 255, 1)
end

local function draw_text_fit(text, x, y, w, max_size, min_size)
  text = tostring(text or "")
  local size = max_size
  repeat
    gfx.setfont(1, "Arial", size)
    local tw = gfx.measurestr(text)
    if tw <= w or size <= min_size then break end
    size = size - 1
  until false
  gfx.x = x
  gfx.y = y
  gfx.drawstr(text)
end

local function draw()
  local w, h = gfx.w, gfx.h
  set_color(0x111318)
  gfx.rect(0, 0, w, h, true)

  local current, queued = get_state()

  set_color(0x2A2F3A)
  gfx.rect(12, 12, w - 24, h - 24, false)

  set_color(0x9AA4B2)
  gfx.setfont(1, "Arial", 16)
  gfx.x = 24
  gfx.y = 26
  gfx.drawstr("EM REPRODUÇÃO")

  set_color(0xFFFFFF)
  draw_text_fit(current, 24, 52, w - 48, 28, 14)

  set_color(0x3A404D)
  gfx.rect(24, 94, w - 48, 1, true)

  set_color(0xF6C343)
  gfx.setfont(1, "Arial", 16)
  gfx.x = 24
  gfx.y = 112
  gfx.drawstr("FILA DE ESPERA")

  set_color(0xFFFFFF)
  draw_text_fit(queued, 24, 138, w - 48, 24, 14)

  set_color(0x707A89)
  gfx.setfont(1, "Arial", 12)
  gfx.x = 24
  gfx.y = h - 34
  gfx.drawstr("Atualização automática pelo VS Hook")
end

local function loop()
  draw()
  gfx.update()
  local ch = gfx.getchar()
  if ch == 27 or ch < 0 then return end
  reaper.defer(loop)
end

gfx.init(TITLE, 520, 230, 0)
loop()
