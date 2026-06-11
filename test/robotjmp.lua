--[[
robotjmp.lua
Tiny robot platformer Mode X demo for the new gfx API

Details:
  - sci-fi / alien planet visual theme
  - static world drawn once
  - PAGE2/background cache via gfx.set_background()
  - each frame restores only the old/current player rectangle
  - autopilot movement, no keyboard input required
  - initial loading can be very slow (1-2 minutes on Amstrad 1640)

Optimizations:
  - Separate pre-flipped frames are provided for each visual state:
	   frame 0 = walking right
	   frame 1 = walking left
	   frame 2 = flying right
	   frame 3 = flying left
	  
	   The left-facing frames could be produced at draw time with flip_x=true,
	   but runtime flipping forces the backend to use the slower generic blitter.
	   By generating the flipped frames once at startup and always drawing with
	   flip_x=false, the Mode X backend can use the fast compiled sprite path.
  - Left-facing frames are generated once at startup from the right-facing
    frames, so the source code stays smaller while runtime drawing still uses
    separate pre-flipped frames.
  - Explicit two-page background synchronization to prevent uninitialized VRAM flicker (not strictly needed).
--]]

local WIDTH = 320
local HEIGHT = 240
local TILE_SIZE = 16
local MAP_W = 64
local MAP_H = 15
local PLAYER_W = 16
local PLAYER_H = 20

local TILESET_ID = 1
local PLAYER_ID = 1

-- 0 = run forever
local MAX_FRAMES = 0

-- For maximum speed, keep this at 0.
-- If animation is too fast on a fast PC, try 1, 5, or 10.
local SLEEP_MS = 0

local level = {}

local player_x = 32
local player_y = 48
local player_vy = 0
local player_dir = 1
local player_grounded = false
local v_delay = 2
local ground_wait = 0

local drawn_frames = 0
local game_running = true

-- One previous player position per flip page.
-- gfx.open() Mode X starts with visible PAGE0 and draw PAGE1,
-- so the first Lua frame draws into logical page index 1.
local draw_page_index = 1
local page_has_player = { false, false }
local page_player_x = { 0, 0 }
local page_player_y = { 0, 0 }

local function ifloor(x) return math.floor(x) end
local function imin(a, b) if a < b then return a end return b end
local function imax(a, b) if a > b then return a end return b end
local function byte(c) return string.char(c) end

local function color_from_char(c)
  if c == "." then return 0 end  -- transparent
  if c == "D" then return 8 end  -- dark metal / yellow-dark
  if c == "M" then return 7 end  -- mid metal
  if c == "L" then return 15 end -- light highlight
  if c == "C" then return 10 end -- light/cyan panel/boots
  if c == "B" then return 9 end  -- dark brown / shadow
  if c == "Y" then return 14 end -- warm yellow / exhaust
  if c == "R" then return 4 end  -- red eyes/light
  return 0
end

local function sprite_from_ascii(src)
  local out = {}
  local n = 1
  local y

  for y = 1, PLAYER_H do
    local row = src[y]
    local x

    for x = 1, PLAYER_W do
      local ch = string.sub(row, x, x)
      out[n] = byte(color_from_char(ch))
      n = n + 1
    end
  end

  return table.concat(out)
end

local function flip_sprite_string(src)
  local out = {}
  local n = 1
  local y

  for y = 0, PLAYER_H - 1 do
    local x

    for x = 0, PLAYER_W - 1 do
      local src_x = PLAYER_W - 1 - x
      local idx = y * PLAYER_W + src_x + 1
      out[n] = string.sub(src, idx, idx)
      n = n + 1
    end
  end

  return table.concat(out)
end

local function make_tiles()
  local out = {}
  local n = 1
  local t

  for t = 0, 3 do
    local y

    for y = 0, TILE_SIZE - 1 do
      local x

      for x = 0, TILE_SIZE - 1 do
        local c = 0

        if t == 0 then
          -- empty / space
          c = 0
        elseif t == 1 then
          -- warm alien rock ground
          c = 3
          if y == 0 then
            c = 15
          elseif y == 1 then
            c = 8
          elseif y == 2 then
            c = 14
          else
            if ((x * 3 + y * 5) % 11) == 0 then
              c = 4
            elseif ((x + y) % 7) == 0 then
              c = 14
            elseif ((x * 2 + y) % 9) == 0 then
              c = 9
            else
              c = 3
            end
          end

          -- dark cracks
          if (x == 3 and y > 5) or (x == 11 and y > 7) then
            c = 9
          end
        elseif t == 2 then
          -- sci-fi metal/copper panel, not brick-like
          c = 12

          -- dark outer frame
          if x == 0 or x == 15 or y == 0 or y == 15 then
            c = 11
          end

          -- warm inner frame
          if x == 1 or x == 14 or y == 1 or y == 14 then
            c = 9
          end

          -- top highlight
          if y == 2 and x > 1 and x < 14 then
            c = 15
          end

          -- recessed center panel
          if x >= 3 and x <= 12 and y >= 4 and y <= 11 then
            c = 12
          end

          -- center seam
          if y == 8 and x >= 3 and x <= 12 then
            c = 9
          end

          -- warm energy stripe
          if y == 5 and x >= 5 and x <= 10 then
            c = 8
          end

          -- small lights
          if y == 6 and (x == 6 or x == 9) then
            c = 15
          end

          -- rivets
          if (x == 3 and y == 3) or (x == 12 and y == 3) or
             (x == 3 and y == 12) or (x == 12 and y == 12) then
            c = 10
          end
        else
          -- warm energy platform / alien tech bridge
          c = 9
          if y == 0 then
            c = 15
          elseif y == 1 then
            c = 8
          elseif y == 2 then
            c = 14
          elseif y == 3 then
            c = 4
          else
            if ((x + y) % 6) == 0 then
              c = 14
            elseif x == 1 or x == 14 then
              c = 11
            else
              c = 9
            end
          end

          -- repeated tech supports
          if (x == 4 or x == 11) and y > 5 then
            c = 12
          end
        end

        out[n] = byte(c)
        n = n + 1
      end
    end
  end

  gfx.tileset(TILESET_ID, TILE_SIZE, TILE_SIZE, 4,
              gfx.NO_TRANSPARENT or 255, table.concat(out))
end

local function make_sprites()
  -- Tiny robot: 16x20, transparent background.
  -- Frame 0 = Facing Right
  -- Frame 1 = Facing Left (Generated procedurally below via flip_sprite_string)
  -- Frame 2 = Flying Right (Rocket frame with exhaust)
  -- Frame 3 = Flying Left (Generated procedurally below via flip_sprite_string)
  local right_pat = {
    ".......Y........",
    "......DD........",
    ".....DDDDD......",
    "....DMMMMMD.....",
    "....DMMRRMD.....",
    "....DMMMMMD.....",
    "....DMLLMD......",
    ".....DDDD.......",
    "....CCCCCC......",
    "...CMMMMMMCC....",
    "..C.MMMMMM.CC...",
    "....MMRRMM...C..",
    "....MMMMMM......",
    "....M....MM.....",
    "...MM.....MM....",
    "...BB.....CC....",
    "..BBB......CC...",
    "..CC........CC..",
    ".CCC........CC..",
    "................"
  }

  local jump_right_pat = {
    ".......Y........",
    "......DDD.......",
    ".....DDDDD......",
    "....DMMMMMD.....",
    "....DMMRRMD.....",
    "....DMMMMMD.....",
    "...CDMLLMD.C....",
    "..CCDDDDDDDCC...",
    "..CMMMMMMMMMC...",
    "..CMMMRRMMMC....",
    "...CMMMMMMMC....",
    "....MMMMMM......",
    "....M....M......",
    "...MM....MM.....",
    "...DD....DD.....",
    "...RR....RR.....",
    "....YY..YY......",
    ".....YYYY.......",
    "......YY........",
    "................"
  }

  -- NOTE:
  -- We store separate frames for right, left, flying-right and flying-left.
  -- The left frames could be drawn with runtime flip_x, but that would force
  -- the Mode X backend to use the slower generic sprite path. By prebuilding
  -- these frames once at startup and always drawing with flip_x=false, the C
  -- backend can use the fast compiled sprite blitter for every player frame.
  local right = sprite_from_ascii(right_pat)
  local left = flip_sprite_string(right)
  local jump_right = sprite_from_ascii(jump_right_pat)
  local jump_left = flip_sprite_string(jump_right)

  -- Compiled as a single 4-frame asset block.
  gfx.sprite(PLAYER_ID, PLAYER_W, PLAYER_H, 4, 0,
             right .. left .. jump_right .. jump_left)
end

local function level_index(col, row)
  return row * MAP_W + col + 1
end

local function set_tile(col, row, t)
  level[level_index(col, row)] = t
end

local function get_tile(col, row)
  return level[level_index(col, row)] or 0
end

local function make_level()
  local x
  local y

  for y = 0, MAP_H - 1 do
    for x = 0, MAP_W - 1 do
      set_tile(x, y, 0)
    end
  end

  -- Alien ground
  for x = 0, MAP_W - 1 do
    set_tile(x, 13, 1)
    set_tile(x, 14, 1)
  end

  -- Floating alien-tech platforms
  for x = 7, 11 do
    set_tile(x, 10, 3)
  end

  -- Metal/copper panel platform
  for x = 18, 23 do
    set_tile(x, 9, 2)
  end

  -- Long warm energy bridge on the right
  for x = 30, 36 do
    set_tile(x, 11, 3)
  end

  -- Small sci-fi blocks near the ground
  set_tile(3, 12, 2)
  set_tile(4, 12, 2)
  set_tile(42, 12, 2)
  set_tile(50, 12, 3)
end

local function is_solid_at(px, py)
  local tx
  local ty

  if px < 0 or px >= MAP_W * TILE_SIZE then
    return true
  end
  if py < 0 then
    return false
  end
  if py >= MAP_H * TILE_SIZE then
    return true
  end

  tx = ifloor(px / TILE_SIZE)
  ty = ifloor(py / TILE_SIZE)
  return get_tile(tx, ty) ~= 0
end

local function draw_static_decor_full()
  -- Warm stars
  gfx.fill(18, 18, 2, 2, 15)
  gfx.fill(44, 42, 1, 1, 8)
  gfx.fill(78, 20, 1, 1, 15)
  gfx.fill(112, 58, 2, 2, 14)
  gfx.fill(150, 28, 1, 1, 8)
  gfx.fill(188, 50, 2, 2, 15)
  gfx.fill(224, 22, 1, 1, 14)
  gfx.fill(292, 36, 2, 2, 8)
  gfx.fill(270, 74, 1, 1, 15)
  gfx.fill(36, 92, 1, 1, 14)
  gfx.fill(132, 88, 2, 2, 8)
  gfx.fill(244, 104, 1, 1, 15)

  -- Large warm distant planet
  gfx.fill(232, 18, 46, 6, 9)
  gfx.fill(224, 24, 62, 8, 4)
  gfx.fill(218, 32, 74, 12, 6)
  gfx.fill(220, 44, 70, 10, 14)
  gfx.fill(226, 54, 56, 8, 3)
  gfx.fill(238, 62, 34, 4, 9)

  -- Planet warm bands / highlights
  gfx.fill(226, 28, 52, 2, 15)
  gfx.fill(222, 38, 66, 2, 8)
  gfx.fill(228, 48, 54, 2, 14)
  gfx.fill(240, 58, 28, 2, 15)

  -- Small amber moon
  gfx.fill(46, 28, 18, 4, 14)
  gfx.fill(42, 32, 26, 8, 3)
  gfx.fill(46, 40, 18, 4, 9)
  gfx.fill(50, 34, 4, 2, 15)
  gfx.fill(60, 36, 3, 2, 4)

  -- Warm alien mountains / horizon
  gfx.fill(18, 184, 62, 24, 9)
  gfx.fill(34, 168, 34, 40, 4)
  gfx.fill(46, 160, 12, 48, 14)
  gfx.fill(52, 164, 6, 16, 15)
  gfx.fill(148, 188, 82, 20, 9)
  gfx.fill(168, 172, 42, 36, 4)
  gfx.fill(184, 162, 10, 46, 14)
  gfx.fill(190, 166, 4, 14, 15)

  -- Warm alien crystals / energy vents
  gfx.fill(92, 190, 6, 18, 8)
  gfx.fill(94, 184, 2, 6, 15)
  gfx.fill(102, 194, 4, 14, 14)
  gfx.fill(276, 188, 6, 20, 8)
  gfx.fill(278, 180, 2, 8, 15)
  gfx.fill(286, 196, 4, 12, 14)

  -- Small antenna/base
  gfx.fill(14, 176, 4, 32, 12)
  gfx.fill(8, 172, 16, 4, 15)
  gfx.fill(10, 168, 4, 4, 8)
  gfx.fill(20, 168, 4, 4, 8)
end

local function draw_level_full()
  local row

  for row = 0, MAP_H - 1 do
    local col

    for col = 0, MAP_W - 1 do
      local t = get_tile(col, row)
      if t ~= 0 then
        gfx.draw_tile(TILESET_ID, t, col * TILE_SIZE, row * TILE_SIZE)
      end
    end
  end
end

local function draw_static_full()
  -- black/warm space background
  gfx.clear(0)
  draw_static_decor_full()
  draw_level_full()
end

local function draw_player()
  -- OPTIMIZATION: All flip_x arguments are strictly false.
  -- We specify pre-calculated frame offsets to utilize the fast compiled C blitter loop.
  if not player_grounded then
    if player_dir < 0 then
      gfx.draw_sprite(PLAYER_ID, player_x, player_y, 3, false) -- Frame 3: Flying Left
    else
      gfx.draw_sprite(PLAYER_ID, player_x, player_y, 2, false) -- Frame 2: Flying Right
    end
  elseif player_dir < 0 then
    gfx.draw_sprite(PLAYER_ID, player_x, player_y, 1, false) -- Frame 1: Pre-baked Left
  else
    gfx.draw_sprite(PLAYER_ID, player_x, player_y, 0, false) -- Frame 0: Pre-baked Right
  end
end

local function restore_old_player_on_page(page_index)
  local old_x
  local old_y
  local min_x
  local min_y
  local max_x
  local max_y

  if not page_has_player[page_index + 1] then
    return
  end

  old_x = page_player_x[page_index + 1]
  old_y = page_player_y[page_index + 1]

  -- Restore the union of old and new player rectangles.
  -- This is safer for fast horizontal movement.
  min_x = imin(old_x, player_x) - 8
  min_y = imin(old_y, player_y) - 4
  max_x = imax(old_x + PLAYER_W, player_x + PLAYER_W) + 8
  max_y = imax(old_y + PLAYER_H, player_y + PLAYER_H) + 4

  gfx.restore(min_x, min_y, max_x - min_x, max_y - min_y)
end

local function mark_player_on_page(page_index)
  page_has_player[page_index + 1] = true
  page_player_x[page_index + 1] = player_x
  page_player_y[page_index + 1] = player_y
end

local function move_vertical()
  local new_y

  v_delay = v_delay - 1
  if v_delay > 0 then
    return
  end
  v_delay = 2

  if player_grounded and player_vy == 0 then
    ground_wait = ground_wait + 1
    if ground_wait > 42 then
      player_vy = -9
      player_grounded = false
      ground_wait = 0
    end
  end

  player_vy = player_vy + 1
  if player_vy > 5 then
    player_vy = 5
  end

  new_y = player_y + player_vy

  if player_vy > 0 then
    local bottom = new_y + PLAYER_H - 1
    if is_solid_at(player_x + 2, bottom) or
       is_solid_at(player_x + PLAYER_W - 3, bottom) then
      new_y = ifloor(bottom / TILE_SIZE) * TILE_SIZE - PLAYER_H
      player_vy = 0
      player_grounded = true
    else
      player_grounded = false
    end
  elseif player_vy < 0 then
    local top = new_y
    if is_solid_at(player_x + 2, top) or
       is_solid_at(player_x + PLAYER_W - 3, top) then
      new_y = (ifloor(top / TILE_SIZE) + 1) * TILE_SIZE
      player_vy = 0
    end
    player_grounded = false
  end

  player_y = new_y
end

local function move_horizontal()
  player_x = player_x + player_dir * 4

  if player_x < 24 then
    player_x = 24
    player_dir = 1
  end

  if player_x > 280 then
    player_x = 280
    player_dir = -1
  end
end

local function update_player()
  move_horizontal()
  move_vertical()
end

local function game_init()
  gfx.open(WIDTH, HEIGHT)

  make_tiles()
  make_sprites()
  make_level()

  player_x = 32
  player_y = 48
  player_vy = 0
  player_dir = 1
  player_grounded = false
  v_delay = 2
  ground_wait = 0
  drawn_frames = 0
  game_running = true

  draw_page_index = 1
  page_has_player[1] = false
  page_has_player[2] = false

  -- Write static scene data over BOTH hardware blit pages
  -- explicitly before locking down the snapshot cache. Prevents old garbage data
  -- left over in VRAM from flashing for a single frame.
  draw_static_full()
  gfx.present() -- Present first static page and switch draw page

  draw_static_full()
  gfx.present() -- Present second static page and switch back

  -- Cache the synchronized scene as the background plane template.
  gfx.set_background()
end

local function game_frame()
  update_player()

  restore_old_player_on_page(draw_page_index)
  draw_player()
  mark_player_on_page(draw_page_index)

  gfx.present()

  if draw_page_index == 1 then
    draw_page_index = 0
  else
    draw_page_index = 1
  end

  drawn_frames = drawn_frames + 1
  if MAX_FRAMES > 0 and drawn_frames >= MAX_FRAMES then
    game_running = false
  end

  if SLEEP_MS > 0 then
    gfx.sleep(SLEEP_MS)
  end
end

local function main()
  game_init()
  while game_running do
    game_frame()
  end
end

local ok, err = pcall(main)
gfx.close()

if not ok then
  print("robotjmp.lua interrupted/error:")
  print(err)
end
