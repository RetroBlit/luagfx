--[[
This is a simple DVD-like bouncing program for LuaGFX.

Alternatively, this program could be written using sprites instead of
gfx.fill, but for a small filled rectangle there is little or no speed
advantage.
 
Written by vmunix (Github username vmunix486) on ELKS 0.9.1 w/ edit.
]]--

local WIDTH = 320
local HEIGHT = 240
local SIZE = 5

local x = 10
local y = x + 10
local dx = 2
local dy = 2

local bc = 0 -- 1st Background Color
local cc = 0 -- 1st color of cube

cc = math.random(1, 255)

local function change_background_color()
	bc = bc + 1
	if bc > 255 then
		bc = 0
	end

	if bc == cc then
		cc = math.random(1, 255)
	end
	-- If the color of the cube is the same as the color of the background,
	-- then generate a new random color for the cube.
end

local function square()
	gfx.fill(x, y, SIZE, SIZE, cc) -- Make filled square of 5x5 pixels

	x = x + dx -- change position of x by dx
	y = y + dy -- change position of y by dy

	-- if statements
	if x > WIDTH - SIZE then
		x = WIDTH - SIZE
		dx = -2 -- hitting right side
		change_background_color() -- Change color when hitting right side
	end

	if y > HEIGHT - SIZE then
		y = HEIGHT - SIZE
		dy = -2 -- hitting bottom
		change_background_color() -- Change color when hitting bottom
	end

	if x < 0 then
		x = 0
		dx = 2 -- Hitting left side
		change_background_color() -- Change color when hitting left side
	end

	if y < 0 then
		y = 0
		dy = 2 -- Hitting top
		change_background_color() -- Change color when hitting top
	end
end

local function main()
	gfx.open(WIDTH, HEIGHT)

	gfx.clear(0)
	gfx.present()
	gfx.clear(0)
	gfx.present()

	while true do
		square()
		gfx.present()
		gfx.clear(bc) -- change the background with the new color
		gfx.sleep(20)
	end
end

local ok, err = pcall(main)

gfx.close()

if not ok then
	print("error")
	print(err)
end