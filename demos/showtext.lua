--
-- showtext.lua
--
-- LuaGFX text demo.
-- Escape, Q, or the Nano-X close button exits.
--

local SCREEN_W = 320
local SCREEN_H = 240

local BACKGROUND_COLOR = 1
local BOX_COLOR = 12
local BORDER_COLOR = 11
local TEXT_COLOR = 15

local MESSAGE = "Hello from LuaGFX"

local running = true

local function draw_screen()
    local text_w = gfx.text_width(MESSAGE)
    local text_h = gfx.text_height()

    local box_w = text_w + 16
    local box_h = text_h + 16

    local box_x = math.floor((SCREEN_W - box_w) / 2)
    local box_y = math.floor((SCREEN_H - box_h) / 2)

    gfx.clear(BACKGROUND_COLOR)

    gfx.fill(
        box_x,
        box_y,
        box_w,
        box_h,
        BOX_COLOR
    )

    gfx.rect(
        box_x,
        box_y,
        box_w,
        box_h,
        BORDER_COLOR
    )

    gfx.print(
        MESSAGE,
        box_x + 8,
        box_y + 8,
        TEXT_COLOR
    )

    gfx.present()
end

gfx.open(SCREEN_W, SCREEN_H)
gfx.set_font(gfx.FONT_8X8)

-- Draw only once. There is no continuous screen redrawing.
draw_screen()

while running do
    while true do
        local key = gfx.keypressed()

        if key == nil then
            break
        end

        if key == "escape" or key == "q" then
            running = false
            break
        end
    end

    gfx.sleep(4)
end

gfx.close()