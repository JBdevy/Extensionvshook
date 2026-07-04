--[[
APP ATIVO | Hook Developer
Tela leve exibida quando o App Diretor assume o controle.
Este script nao controla o VS Hook. Ele apenas mostra o bloqueio local e chama a extensao
para retomar o acesso no PC quando o usuario clicar no botao.
]]

local TITLE = "APP ATIVO | Hook Developer"
local WINDOW_SECTION = "JBKEYS_VSHOOK_APP_ACTIVE_WINDOW"
local DOCK_KEY = "DOCKSTATE_V1"
local X_KEY = "WINDOW_X_V1"
local Y_KEY = "WINDOW_Y_V1"
local W_KEY = "WINDOW_W_V1"
local H_KEY = "WINDOW_H_V1"

local DEFAULT_W = 460
local DEFAULT_H = 260
local DEFAULT_DOCK = 513

local mouse_last_down = false
local resume_requested = false
local resume_error_until = 0

local function now_time()
  return reaper and reaper.time_precise and reaper.time_precise() or os.clock()
end

local function read_num(key, default)
  if not reaper or not reaper.GetExtState then return default end
  local value = tonumber(reaper.GetExtState(WINDOW_SECTION, key) or "")
  return value or default
end

local function write_num(key, value, persist)
  if reaper and reaper.SetExtState then
    reaper.SetExtState(WINDOW_SECTION, key, tostring(math.floor(tonumber(value) or 0)), persist and true or false)
  end
end

local function save_window_state()
  if not gfx or not gfx.dock then return end
  local ok, dock, x, y, w, h = pcall(gfx.dock, -1, 0, 0, 0, 0)
  if not ok then return end
  dock = tonumber(dock) or 0
  x = tonumber(x) or 100
  y = tonumber(y) or 100
  w = tonumber(w) or gfx.w or DEFAULT_W
  h = tonumber(h) or gfx.h or DEFAULT_H
  write_num(DOCK_KEY, dock, true)
  write_num(X_KEY, x, true)
  write_num(Y_KEY, y, true)
  write_num(W_KEY, w, true)
  write_num(H_KEY, h, true)
end

local function init_window()
  local dock = read_num(DOCK_KEY, DEFAULT_DOCK)
  local x = read_num(X_KEY, 120)
  local y = read_num(Y_KEY, 120)
  local w = math.max(360, read_num(W_KEY, DEFAULT_W))
  local h = math.max(220, read_num(H_KEY, DEFAULT_H))
  gfx.init(TITLE, w, h, dock, x, y)
end

local function point_in_rect(mx, my, x, y, w, h)
  return mx >= x and mx <= x + w and my >= y and my <= y + h
end

local function draw_text_center(text, x, y, w, h, r, g, b, font_size)
  gfx.set(r, g, b, 1)
  gfx.setfont(1, "Arial", font_size or 18)
  local tw, th = gfx.measurestr(text)
  gfx.x = x + (w - tw) / 2
  gfx.y = y + (h - th) / 2
  gfx.drawstr(text)
end

local function draw_button(x, y, w, h, label, hot)
  if hot then
    gfx.set(0.95, 0.78, 0.18, 1)
  else
    gfx.set(0.86, 0.66, 0.08, 1)
  end
  gfx.rect(x, y, w, h, true)
  gfx.set(0.10, 0.10, 0.10, 1)
  gfx.rect(x, y, w, h, false)
  draw_text_center(label, x, y, w, h, 0.05, 0.05, 0.05, 17)
end

local function resume_pc_access()
  resume_requested = true

  -- Caminho direto pela API da extensao, quando ela estiver visivel no Lua.
  if reaper and reaper.VS_Hook_Native_ResumePcAccess then
    local ok, result = pcall(reaper.VS_Hook_Native_ResumePcAccess)
    if ok and result then
      return true
    end
  end

  -- Fallback leve: o script nao tenta controlar o REAPER. Ele apenas deixa
  -- um pedido para a extensao ler no timer dela, na thread principal.
  if reaper and reaper.SetExtState then
    local token = "pc_resume_" .. tostring(math.floor(now_time() * 1000))
    reaper.SetExtState("VS_HOOK_NATIVE_BRIDGE", "PC_RESUME_REQUEST_V1", token, false)
    return true
  end

  resume_requested = false
  resume_error_until = now_time() + 3.0
  return false
end

local function draw_ui()
  local w = gfx.w or DEFAULT_W
  local h = gfx.h or DEFAULT_H
  local mx = gfx.mouse_x or 0
  local my = gfx.mouse_y or 0
  local mouse_down = (gfx.mouse_cap & 1) == 1
  local click = mouse_down and not mouse_last_down

  gfx.set(0.07, 0.07, 0.075, 1)
  gfx.rect(0, 0, w, h, true)

  gfx.set(0.98, 0.78, 0.12, 1)
  gfx.rect(0, 0, w, 5, true)

  draw_text_center("APP DO DIRETOR ATIVO", 20, 30, w - 40, 34, 1.0, 0.82, 0.18, 22)

  gfx.set(0.86, 0.86, 0.86, 1)
  gfx.setfont(1, "Arial", 17)
  local msg1 = "Com o App Diretor ativo, nao e possivel controlar"
  local msg2 = "o VS Hook ao mesmo tempo em dois lugares."
  local tw1, th1 = gfx.measurestr(msg1)
  local tw2, th2 = gfx.measurestr(msg2)
  gfx.x = (w - tw1) / 2
  gfx.y = 90
  gfx.drawstr(msg1)
  gfx.x = (w - tw2) / 2
  gfx.y = 90 + th1 + 6
  gfx.drawstr(msg2)

  local bw = math.min(260, w - 60)
  local bh = 42
  local bx = (w - bw) / 2
  local by = h - 78
  local hot = point_in_rect(mx, my, bx, by, bw, bh)
  draw_button(bx, by, bw, bh, resume_requested and "Retomando..." or "Retomar Acesso no PC", hot)

  if click and hot and not resume_requested then
    resume_pc_access()
  end

  if resume_error_until > now_time() then
    draw_text_center("A extensao VS Hook nao respondeu.", 20, h - 30, w - 40, 20, 1.0, 0.35, 0.25, 14)
  end

  mouse_last_down = mouse_down
end

local function main()
  local char = gfx.getchar()
  if char < 0 then
    save_window_state()
    return
  end

  draw_ui()
  gfx.update()
  reaper.defer(main)
end

init_window()
if reaper and reaper.atexit then reaper.atexit(save_window_state) end
main()
