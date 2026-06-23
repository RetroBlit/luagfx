-- Simple DVD-like bouncing program

local WIDTH = 320
local HEIGHT = 240

gfx.open(WIDTH,HEIGHT)

local x = 10
local y = x + 10
local old_x = x
local old_y = y
local dx = 2
local dy = 2
local frame = 0

local COLOR = 1
local bc = 0 -- 1st Background Color
local cc = 0 -- 1st color of cube
cc = math.random(0, 256)

local function square()
	gfx.fill(x, y, 5, 5, cc) -- Make filled square of 5x5 pixels
	x = x + dx -- change position of x by dx
	y = y + dy -- change position of y by dy

	-- if statements
	if x > 310 then dx = -2 end -- hitting bottom
	if y > 230 then dy = -2 end -- hitting right side
	if x < 5 then dx = 2 end -- Hitting top
	if y < 5 then dy = 2 end -- Hitting left side
	if x > 310 then bc = bc + 1 end -- Change color when hitting bottom
	if y > 230 then bc = bc + 1 end -- Chnage color when hitting right side
	if x < 5 then bc = bc + 1 end -- Change color when hitting top
	if y < 5 then bc = bc + 1 end -- Change color when hitting right side
	if bc == cc then cc = math.random(0, 256) end 
	-- If the color of the cube is the same as the color of the background,
	-- then generate a new random color for the cube.
end

local function main()
	gfx.clear(0)
	gfx.present()
	gfx.clear(0)
	gfx.present()

	while true do
		square()
		gfx.present()
		gfx.clear(bc) -- change the background with the new color
	end
end

local ok, err = pcall(main)
gfx.close()

if not ok then
	print("error")
	print(err)
end
-- EOF --
