--[[
This example moves a filled rectangle using the arrow keys, while Escape or Q
exits the program. The rectangle is displayed immediately when the program 
starts and can then be moved using the arrow keys.

Nano-X draws directly to the visible window, so clearing and redrawing the
complete screen caused visible flickering while a key was held. To reduce
this flickering, repeated keyboard events are combined into one movement per
frame, and only the narrow strips exposed by the rectangle's movement are
updated.

Mode X alternates between two drawing pages whenever gfx.present() is called.
The same strip update is therefore applied to both pages so that stale parts
of the rectangle do not remain on the alternate page. This allows the same
Lua program to work correctly with both Nano-X and Mode X.
]]

local SCREEN_W = 320
local SCREEN_H = 240

local RECT_W = 20
local RECT_H = 14

local BACKGROUND_COLOR = 0
local RECT_COLOR = 12

local MOVE_STEP = 3
local FRAME_DELAY_MS = 16

local x = math.floor((SCREEN_W - RECT_W) / 2)
local y = math.floor((SCREEN_H - RECT_H) / 2)

local running = true
local rectangle_visible = false

gfx.open(SCREEN_W, SCREEN_H)

-- Clear both Mode X pages.
-- In Nano-X, this simply clears the same visible window twice.
gfx.clear(BACKGROUND_COLOR)
gfx.present()

gfx.clear(BACKGROUND_COLOR)
gfx.present()

-- Apply one rectangle movement to the current drawing surface.
-- This function does not call gfx.present().
local function update_rectangle(old_x, old_y, new_x, new_y)
    local current_x = old_x
    local current_y = old_y

    -- Horizontal movement.
    if new_x ~= current_x then
        local dx = new_x - current_x
        local amount = math.abs(dx)

        if amount >= RECT_W then
            -- The old and new rectangles do not overlap.
            gfx.fill(
                new_x,
                current_y,
                RECT_W,
                RECT_H,
                RECT_COLOR
            )

            gfx.fill(
                current_x,
                current_y,
                RECT_W,
                RECT_H,
                BACKGROUND_COLOR
            )

        elseif dx > 0 then
            -- Add the new strip on the right.
            gfx.fill(
                current_x + RECT_W,
                current_y,
                amount,
                RECT_H,
                RECT_COLOR
            )

            -- Remove the abandoned strip on the left.
            gfx.fill(
                current_x,
                current_y,
                amount,
                RECT_H,
                BACKGROUND_COLOR
            )

        else
            -- Add the new strip on the left.
            gfx.fill(
                new_x,
                current_y,
                amount,
                RECT_H,
                RECT_COLOR
            )

            -- Remove the abandoned strip on the right.
            gfx.fill(
                new_x + RECT_W,
                current_y,
                amount,
                RECT_H,
                BACKGROUND_COLOR
            )
        end

        current_x = new_x
    end

    -- Vertical movement.
    if new_y ~= current_y then
        local dy = new_y - current_y
        local amount = math.abs(dy)

        if amount >= RECT_H then
            -- The old and new rectangles do not overlap.
            gfx.fill(
                current_x,
                new_y,
                RECT_W,
                RECT_H,
                RECT_COLOR
            )

            gfx.fill(
                current_x,
                current_y,
                RECT_W,
                RECT_H,
                BACKGROUND_COLOR
            )

        elseif dy > 0 then
            -- Add the new strip at the bottom.
            gfx.fill(
                current_x,
                current_y + RECT_H,
                RECT_W,
                amount,
                RECT_COLOR
            )

            -- Remove the abandoned strip at the top.
            gfx.fill(
                current_x,
                current_y,
                RECT_W,
                amount,
                BACKGROUND_COLOR
            )

        else
            -- Add the new strip at the top.
            gfx.fill(
                current_x,
                new_y,
                RECT_W,
                amount,
                RECT_COLOR
            )

            -- Remove the abandoned strip at the bottom.
            gfx.fill(
                current_x,
                new_y + RECT_H,
                RECT_W,
                amount,
                BACKGROUND_COLOR
            )
        end
    end
end

-- Draw the initial rectangle on both Mode X pages.
--
-- In Nano-X, the second fill simply repeats the same drawing operation.
local function show_rectangle()
    gfx.fill(
        x,
        y,
        RECT_W,
        RECT_H,
        RECT_COLOR
    )

    gfx.present()

    -- Synchronize the other Mode X page.
    gfx.fill(
        x,
        y,
        RECT_W,
        RECT_H,
        RECT_COLOR
    )
end

-- Move the rectangle and synchronize both Mode X pages.
local function move_rectangle_on_both_pages(old_x, old_y, new_x, new_y)
    -- Update the current hidden Mode X page, or the Nano-X window.
    update_rectangle(old_x, old_y, new_x, new_y)

    -- Display the completed Mode X page or flush Nano-X.
    gfx.present()

    -- Mode X now draws to its other hidden page.
    -- Apply the identical change so that both pages remain synchronized.
    --
    -- In Nano-X, repeating the operation is safe because the strip updates
    -- are idempotent.
    update_rectangle(old_x, old_y, new_x, new_y)
end

-- Draw the rectangle immediately and synchronize both Mode X pages.
show_rectangle()
rectangle_visible = true

while running do
    local old_x = x
    local old_y = y
    local latest_direction = nil

    -- Drain all queued events but keep only the newest arrow direction.
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
    end

    -- Keep the entire rectangle inside the screen.
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

    if running and latest_direction ~= nil then
        if not rectangle_visible then
            show_rectangle()
            rectangle_visible = true

        elseif x ~= old_x or y ~= old_y then
            move_rectangle_on_both_pages(
                old_x,
                old_y,
                x,
                y
            )
        end
    end

    gfx.sleep(FRAME_DELAY_MS)
end

gfx.close()