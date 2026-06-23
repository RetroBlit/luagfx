--[[
This is a program that cycles through all the colors and sets it to the background color.

Written by vmunix (Github username vmunix486) on ELKS 0.9.1 w/ edit.
]]--

local WIDTH = 320
local HEIGHT = 240 -- If it doesn't work, change this to 200'

local COLOR = 1

local a = 0

gfx.open(WIDTH, HEIGHT)

local function bg()
	while( true )
	do
		gfx.clear(a)
		a = a+1
		gfx.present()
		-- os.execute("sleep 1")
		-- if this doesn't work, comment it out and use the method above
		gfx.sleep(1000)
	end
end

local function main()
	gfx.clear(0) 
	gfx.present()
	gfx.clear(0)
	gfx.present()

	while true do
		bg()
	end
end

local success, err = pcall(main)

gfx.close()

if not success then
	print("Interrupted!")
end

-- EOF --
