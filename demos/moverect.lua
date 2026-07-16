-- move_rect.lua
--
-- Move a filled rectangle sprite:
--   I = up
--   J = left
--   K = down
--   M = right
--   Escape or Q = quit

local SCREEN_W = 320
local SCREEN_H = 240

local SPRITE_ID = 0
local RECT_W = 20
local RECT_H = 14

local BACKGROUND_COLOR = 0
local RECT_COLOR = 12
local MOVE_STEP = 3
local FRAME_DELAY_MS = 16

local x = math.floor((SCREEN_W - RECT_W) / 2)
local y = math.floor((SCREEN_H - RECT_H) / 2)

local running = true

-- Create one frame containing a solid-colored rectangle.
local rectangle_pixels =
    string.rep(string.char(RECT_COLOR), RECT_W * RECT_H)

gfx.open(SCREEN_W, SCREEN_H)

gfx.sprite(
    SPRITE_ID,
    RECT_W,
    RECT_H,
    1,                    -- one animation frame
    gfx.NO_TRANSPARENT,
    rectangle_pixels
)

while running do
    -- Process every pending keyboard event.
    while true do
        local key = gfx.keypressed()

        if key == nil then
            break
        end

        if key == "i" then
            y = y - MOVE_STEP

        elseif key == "j" then
            x = x - MOVE_STEP

        elseif key == "k" then
            y = y + MOVE_STEP

        elseif key == "m" then
            x = x + MOVE_STEP

        elseif key == "escape" or key == "q" then
            running = false
        end
    end

    -- Keep the entire sprite inside the screen.
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

    -- Clear the current back/draw page so the sprite position from
    -- two frames earlier does not remain visible after page flipping.
    gfx.clear(BACKGROUND_COLOR)

    gfx.draw_sprite(
        SPRITE_ID,
        x,
        y,
        0,       -- frame
        false    -- horizontal flip
    )

    gfx.present()
    gfx.sleep(FRAME_DELAY_MS)
end

gfx.close()
