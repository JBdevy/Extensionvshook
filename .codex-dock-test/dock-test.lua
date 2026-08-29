gfx.init('Dock flag test', 420, 520, 0, 20, 20)
gfx.dock(769)

local function loop()
  gfx.set(0.08, 0.08, 0.08, 1)
  gfx.rect(0, 0, gfx.w, gfx.h, 1)
  gfx.set(1, 1, 1, 1)
  gfx.x, gfx.y = 20, 20
  gfx.drawstr('Docker 3 flag test')
  gfx.update()
  reaper.defer(loop)
end

loop()
