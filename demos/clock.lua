--[[
Very simple clock with Hour, Minute, and Second hands.

What to add:
 * Circle around the clock (whenever something like gfx.circle gets added
 * Ticks around the edge
 * Numbers around the edge
 * White circle as clock background
 * Toggling seconds hand (is skippish on slow hardware)

TODO:
 * When pressing CTRL+C, it stops the program but doesn't restore text mode.
]]--

local WIDTH = 320
local HEIGHT = 240

local COLOR = 1

local center_x = WIDTH/2
local center_y = HEIGHT/2

local hand_length = 80

local clock_running = true

gfx.open(WIDTH, HEIGHT)
gfx.clear(0)

local function get_time()
	local secs = tonumber(os.date("%S"))
	local mins = tonumber(os.date("%M"))
	local hrs = tonumber(os.date("%H"))

	return hrs, mins, secs
end

local function calc_hand_pos(degrees, length)
	local radians = math.rad(degrees)
	local x = center_x + length * math.cos(radians)
	local y = center_y + length * math.sin(radians)

	return x, y
end

local function draw_hand(degrees, length, color)
	local x, y = calc_hand_pos(degrees, length)
	gfx.line(
		center_x,
		center_y,
		x,
		y,
		color)
end

local function draw_clock()
	local hrs, mins, secs = get_time()

	local angle_secs = secs * 6
	local angle_mins = mins * 6 + secs * 0.1
	local angle_hrs = (hrs % 12) * 30 + mins * 0.5

	draw_hand(angle_hrs, 40, 1)
	draw_hand(angle_mins, 60, 10)
	draw_hand(angle_secs, 70, 6)

	gfx.present()
end

local function main()
	while clock_running do
		draw_clock()
		gfx.sleep(1000) -- 1000 ms = 1 second
		gfx.clear(0)
	end
end

local ok, err = pcall(main)
gfx.close()

if not ok then
	gfx.close()
	print("clock: ", err)
end
