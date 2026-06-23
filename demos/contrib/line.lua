--[[
This is a very simple program written in lua for LuaGFX by toncho11.

What this program does is that it draws a line to the screen. That's it.

It's basically just a showcase of the gfx.line function, for people wanting to
learn.

Written by vmunix (Github username vmunix486) on ELKS 0.9.1 w/ edit.
]]--

-- Default resolution 32 x 240
local WIDTH = 320 -- This is how wide the resolution will be.
local HEIGHT = 240 -- This is how tall the resolution will be.

local COLOR = 1 -- This is the color of the line.

local function drawline() -- This is a function that draws the line itself.
	gfx.line( -- gfx.line is used to draw a line.
		10, -- This is the X coordinate for the first point on the line.
		50, -- This is the Y coordinate for the first point on the line.
		300, -- This is the X coordinate for the second point on the line.
		50, -- This is the Y coordinate for the second point on the line.
		COLOR) -- This is setting the color from the variable from earlier.
	
	 -- This shows the current drawing page, then switches to the other page.
	gfx.present()
end -- Ends the function. Similar to } in C/C++

local function main() -- The main function, VERY similar to C/C++
	gfx.open(WIDTH, HEIGHT)
	
	-- Clear the screen prudently 
	gfx.clear(0) 
	gfx.present()
	gfx.clear(0)
	gfx.present()

    -- LuaGFX / Mode X uses page flipping.
    -- Draw the same static image to both pages.
    drawline()
    drawline()

    -- Keep the program alive. There is currently no "press any key to close". 
	-- This is an infinite loop. It prevents the program from exiting and clearing the screen except for CTRL+C
    while true do
        gfx.sleep(1000)
    end

end -- Stop the main function.

local success, err = pcall(main) -- Execute main safely and check if it ended with an error or interruption.

gfx.close() -- Closes the graphics after completing the main and returns to normal text mode terminal

if not success then -- If error or interruption (such as CTRL+C for example)
	print("Interrupted!")
	print(err)
end -- ends the if not function.
