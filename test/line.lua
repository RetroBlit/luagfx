--[[
This is a very simple program written in lua for luagfx by RetroBlit.

What this program does is that it draws a line to the screen. That's it.

It's basically just a showcase of the gfx.line function, for people wanting to
learn.

Written by vmunix (Github username vmunix486) on ELKS 0.9.1 w/ edit.
]]--

local WIDTH = 320 -- This is how wide the resolution will be.
local HEIGHT = 200 -- This is how tall the resolution will be.

local COLOR = 1 -- This is the color of the line.

gfx.open(WIDTH, HEIGHT) -- Open the display with the width and height defined above

local function drawline() -- This is a function that draws the line itself.
	gfx.line( -- gfx.line is used to draw a line.
		10, -- This is the X coordinate for the first point on th. lines
		50, -- This is the Y coordinate for the first point on the line.
		300, -- This is the X coordinate for the second point on the line.
		50, -- This is the Y coordinate for the second point on the line.
		COLOR) -- This is setting the color from the variable from earlier.
	gfx.present() -- This actually shows all the work we did to the screen
end -- Ends the function. Similar to } in C/C++

local function main() -- The main function, VERY similar to C/C++
	gfx.clear(0) 
	gfx.present()
	gfx.clear(0)
	gfx.present()

	-- This is the while loop. The "true" has to be there in order to work
	-- as far as I am concerned.
	while true do
		drawline() -- This is calling the function that draws the line
	end -- You should treat while loops like functions since they also need an end as well.
end -- Stop the main function.

local success, err = pcall(main) -- Not sure what this does, but looks important.

gfx.close() -- Closes the graphics and returns to normal text mode terminal

if not success then -- If "true" was not reached, then print this message.
	print("Interrupted!")
end -- ends the if not function.

-- EOF --
