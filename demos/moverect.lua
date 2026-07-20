--[[
Moves a filled rectangle with the arrow keys.
Escape or Q exits.

Queued key-repeat events are combined into one movement per frame.

Nano-X uses sprite save-under to restore the previous rectangle area.
Mode X uses its background page. Because gfx.present() alternates between
two Mode X drawing pages, each movement is applied once before present()
and once afterward to keep both pages synchronized.
]]

local SCREEN_W = 320
local SCREEN_H = 240

local RECT_W = 20
local RECT_H = 14

local BACKGROUND_COLOR = 0
local RECT_COLOR = 12

local MOVE_STEP = 3
local FRAME_DELAY_MS = 4 -- it is recommended to use at least 4 ms of pause per frame with Nano-X backend

local SPRITE_ID = 0

local x = math.floor((SCREEN_W - RECT_W) / 2)
local y = math.floor((SCREEN_H - RECT_H) / 2)

local running = true

gfx.open(SCREEN_W, SCREEN_H)

-- Initialize the background.
--
-- Mode X copies this background to PAGE2 and both drawing pages.
-- Nano-X uses either its background cache or sprite save-under.
gfx.clear(BACKGROUND_COLOR)
gfx.set_background()

-- Define the filled rectangle as a one-frame sprite.
local rectangle_pixels =
    string.rep(string.char(RECT_COLOR), RECT_W * RECT_H)

gfx.sprite(
    SPRITE_ID,
    RECT_W,
    RECT_H,
    1,
    BACKGROUND_COLOR,
    rectangle_pixels
)

-- Optional optimization. Raw drawing remains available as a fallback.
gfx.compile_sprite(SPRITE_ID)

-- Draw the initial rectangle on the first Mode X page.
gfx.draw_sprite(SPRITE_ID, x, y, 0, false)
gfx.present()

-- Synchronize the other Mode X page.
--
-- On Nano-X this repeats the same final drawing safely.
gfx.draw_sprite(SPRITE_ID, x, y, 0, false)

local function clamp_position()
    if x < 0 then
        x = 0
    elseif x > SCREEN_W - RECT_W then
        x = SCREEN_W - RECT_W
    end

    if y < 0 then
        y = 0
    elseif y > SCREEN_H - RECT_H then
        y = SCREEN_H - RECT_H
    end
end

local function move_on_both_pages(old_x, old_y, new_x, new_y)
    -- Update the current Mode X draw page, or the Nano-X window.
    gfx.move_sprite(
        SPRITE_ID,
        old_x,
        old_y,
        new_x,
        new_y,
        0,
        false
    )

    -- Display the completed Mode X page, or flush Nano-X.
    gfx.present()

    -- Synchronize the other Mode X page.
    gfx.move_sprite(
        SPRITE_ID,
        old_x,
        old_y,
        new_x,
        new_y,
        0,
        false
    )
end

while running do
    local old_x = x
    local old_y = y
    local latest_direction = nil

    -- Drain queued events, retaining only the newest direction.
    while true do
        local key = gfx.keypressed()

        if key == nil then
            break
        end

        if key == "up" or
           key == "left" or
           key == "down" or
           key == "right" then
            latest_direction = key

        elseif key == "escape" or key == "q" then
            running = false
        end
    end

    if running then
        if latest_direction == "up" then
            y = y - MOVE_STEP
        elseif latest_direction == "left" then
            x = x - MOVE_STEP
        elseif latest_direction == "down" then
            y = y + MOVE_STEP
        elseif latest_direction == "right" then
            x = x + MOVE_STEP
        end

        clamp_position()

        if x ~= old_x or y ~= old_y then
            move_on_both_pages(old_x, old_y, x, y)
        end
    end

    gfx.sleep(FRAME_DELAY_MS)
end

gfx.close()