--[[
Very simple clock with Hour, Minute, and Second hands for LuaGFX

What to add:
 * Circle around the clock (whenever something like gfx.circle gets added)
 * Ticks around the edge
 * Numbers around the edge
 * White circle as clock background
 * Toggling seconds hand (is skippish on slow hardware)

TODO:
 * When pressing CTRL+C, it stops the program but doesn't restore text mode.
 
Written by vmunix (GitHub username vmunix486) on ELKS 0.9.1, with edits.
]]--

local WIDTH = 320
local HEIGHT = 240

local center_x = WIDTH / 2
local center_y = HEIGHT / 2

local function get_time()
	local time = os.date("*t")

	return time.hour, time.min, time.sec
end

local function calc_hand_pos(degrees, length)
	local radians = math.rad(degrees - 90)
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

	gfx.open(WIDTH, HEIGHT)
	gfx.clear(0)
	gfx.present()
	gfx.clear(0)
	gfx.present()

	while true do
		draw_clock()
		gfx.sleep(1000) -- 1000 ms = 1 second
		gfx.clear(0)
	end
end

local ok, err = pcall(main)

gfx.close()

if not ok then
	print("clock: ", err)
end
