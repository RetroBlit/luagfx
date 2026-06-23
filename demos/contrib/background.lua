--[[
This is a simple LuaGFX program that cycles through all 256 VGA colors
and uses each one as the background color.

Written by vmunix (Github username vmunix486) on ELKS 0.9.1 w/ edit.
]]--

local WIDTH = 320
local HEIGHT = 240
local COLOR = 0

local function bg()

	gfx.clear(COLOR)
	gfx.present()
	
	COLOR = COLOR + 1 
	if COLOR > 255 then 
		COLOR = 0 
	end
	
end

local function main()

	gfx.open(WIDTH, HEIGHT)
	
	-- Just clears the screen prudently. Clear both VGA pages.
	gfx.clear(0) 
	gfx.present()
	gfx.clear(0)
	gfx.present()

	while true do
		bg()
		gfx.sleep(1000)
	end
end

local success, err = pcall(main)

gfx.close()

if not success then
	print("Interrupted!")
	print(err)
end
