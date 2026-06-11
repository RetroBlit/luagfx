--[[
3D rotating cube demo
Developed by: Anton Andreev
New gfx API version
Optimized:
  - no full-screen clear
  - uses Lua math.sin / math.cos
  - one old cube cache per flip page
--]]

local WIDTH  = 320
local HEIGHT = 200

gfx.open(WIDTH, HEIGHT)

local cube = {
  {-1, -1, -1}, { 1, -1, -1}, { 1,  1, -1}, {-1,  1, -1},
  {-1, -1,  1}, { 1, -1,  1}, { 1,  1,  1}, {-1,  1,  1}
}

local edges = {
  {1, 2}, {2, 3}, {3, 4}, {4, 1},
  {5, 6}, {6, 7}, {7, 8}, {8, 5},
  {1, 5}, {2, 6}, {3, 7}, {4, 8}
}

local angleX = 0
local angleY = 0

local size = 40
local fov = 3

local cx = WIDTH / 2
local cy = HEIGHT / 2

local transformed = {}

-- One previous cube position per flip page.
local old_for_page = {
  {},
  {}
}

local draw_page_index = 1

local function rotateX(x, y, z, angle)
  local cosA = math.cos(angle)
  local sinA = math.sin(angle)

  return x,
         y * cosA - z * sinA,
         y * sinA + z * cosA
end

local function rotateY(x, y, z, angle)
  local cosA = math.cos(angle)
  local sinA = math.sin(angle)

  return x * cosA + z * sinA,
         y,
        -x * sinA + z * cosA
end

local function project3D(x, y, z)
  local scale = fov / (fov + z)

  local screenX = x * scale * size + cx
  local screenY = y * scale * size + cy

  -- Avoid 0.5 literal for your ELKS Lua parser.
  return math.floor(screenX + 1 / 2),
         math.floor(screenY + 1 / 2)
end

local function compute_cube()
  for i, v in ipairs(cube) do
    local x = v[1]
    local y = v[2]
    local z = v[3]

    local rx, ry, rz = rotateX(x, y, z, angleX)
    local fx, fy, fz = rotateY(rx, ry, rz, angleY)

    local sx, sy = project3D(fx, fy, fz)

    if transformed[i] then
      transformed[i][1] = sx
      transformed[i][2] = sy
    else
      transformed[i] = {sx, sy}
    end
  end
end

local function draw_cube(points, color)
  for _, edge in ipairs(edges) do
    local p1 = points[edge[1]]
    local p2 = points[edge[2]]

    if p1 and p2 then
      gfx.line(p1[1], p1[2], p2[1], p2[2], color)
    end
  end
end

local function copy_points(dst, src)
  for i = 1, #src do
    if dst[i] then
      dst[i][1] = src[i][1]
      dst[i][2] = src[i][2]
    else
      dst[i] = {src[i][1], src[i][2]}
    end
  end
end

local function main()
  -- Clear both Mode X pages once at startup.
  gfx.clear(0)
  gfx.present()
  gfx.clear(0)
  gfx.present()

  while true do
    local old = old_for_page[draw_page_index]

    -- Erase only the cube previously drawn on this page.
    draw_cube(old, 0)

    -- Compute and draw new cube.
    compute_cube()
    draw_cube(transformed, 15)

    -- Remember what this page now contains.
    copy_points(old, transformed)

    -- Show completed page.
    gfx.present()

    if draw_page_index == 1 then
      draw_page_index = 2
    else
      draw_page_index = 1
    end

    angleX = angleX + 5 / 100
    angleY = angleY + 3 / 100

    if angleX > math.pi * 2 then
      angleX = angleX - math.pi * 2
    end

    if angleY > math.pi * 2 then
      angleY = angleY - math.pi * 2
    end

    --gfx.sleep(20)
  end
end

local success, err = pcall(main)

gfx.close()

if not success then
  print("Interrupted! Exiting gracefully.")
  print(err)
end