--
-- showtext.lua
--
-- LuaGFX text-rendering demonstration.
--
-- Controls:
--   Left / Right  Move the text sample
--   Up / Down     Move the text sample
--   Space         Toggle the measurement box
--   Escape or Q   Exit
--

local SCREEN_W = 320
local SCREEN_H = 240

local COLOR_BLACK     = 0
local COLOR_SKY       = 1
local COLOR_GREEN     = 2
local COLOR_RED       = 6
local COLOR_BLUE      = 7
local COLOR_YELLOW    = 8
local COLOR_WHITE     = 10
local COLOR_DARK      = 11
local COLOR_GRAY      = 12
local COLOR_HIGHLIGHT = 15

local running = true
local show_box = true

local sample = "LuaGFX text"
local sample_x = 112
local sample_y = 111

local function center_x(text)
    return math.floor((SCREEN_W - gfx.text_width(text)) / 2)
end

local function draw_frame()
    local title = "LuaGFX 8x8 Text Demo"
    local instruction1 = "Arrow keys move the text"
    local instruction2 = "Space toggles its box"
    local instruction3 = "Q or Escape exits"

    local sample_w = gfx.text_width(sample)
    local sample_h = gfx.text_height()

    --
    -- Each back page must be cleared before drawing the new frame.
    -- Otherwise pixels from two frames earlier can remain after flipping.
    --
    gfx.clear(COLOR_SKY)

    -- Header.
    gfx.fill(0, 0, SCREEN_W, 22, COLOR_BLUE)
    gfx.print(title, center_x(title), 7, COLOR_WHITE)

    -- Built-in 8x8 font information.
    gfx.print("Font:", 8, 31, COLOR_DARK)
    gfx.print("builtin 8x8", 56, 31, COLOR_WHITE)

    gfx.print("Height:", 8, 43, COLOR_DARK)
    gfx.print(tostring(gfx.text_height()) .. " pixels", 64, 43, COLOR_WHITE)

    gfx.print("Width:", 8, 55, COLOR_DARK)
    gfx.print(tostring(gfx.text_width("ABC abc 123")) .. " pixels",
              56, 55, COLOR_WHITE)

    -- Character samples.
    gfx.print("Upper: ABCDEFGHIJKLMNOPQRSTUVWXYZ",
              8, 72, COLOR_WHITE)

    gfx.print("Lower: abcdefghijklmnopqrstuvwxyz",
              8, 84, COLOR_WHITE)

    gfx.print("Digit: 0123456789  !?+-=*/()[]",
              8, 96, COLOR_YELLOW)

    -- Movable text and optional measurement rectangle.
    if show_box then
        gfx.fill(sample_x - 3,
                 sample_y - 3,
                 sample_w + 6,
                 sample_h + 6,
                 COLOR_GRAY)

        gfx.rect(sample_x - 4,
                 sample_y - 4,
                 sample_w + 8,
                 sample_h + 8,
                 COLOR_DARK)
    end

    gfx.print(sample, sample_x, sample_y, COLOR_HIGHLIGHT)

    -- Multiline and tab handling.
    gfx.fill(7, 135, 146, 39, COLOR_GREEN)
    gfx.rect(6, 134, 148, 41, COLOR_DARK)
    gfx.print("Multiple lines:\nLine 2\tTab", 11, 140, COLOR_WHITE)

    -- Text near the right side demonstrates clipping.
    gfx.fill(165, 135, 148, 39, COLOR_RED)
    gfx.rect(164, 134, 150, 41, COLOR_DARK)
    gfx.print("Clipped at screen edge --->", 173, 150, COLOR_WHITE)

    -- Instructions.
    gfx.fill(0, 190, SCREEN_W, 50, COLOR_DARK)
    gfx.print(instruction1, center_x(instruction1), 197, COLOR_WHITE)
    gfx.print(instruction2, center_x(instruction2), 209, COLOR_WHITE)
    gfx.print(instruction3, center_x(instruction3), 221, COLOR_YELLOW)

    gfx.present()
end

local function handle_keys()
    while true do
        local key = gfx.keypressed()

        if key == nil then
            break
        end

        if key == "escape" or key == "q" then
            running = false
        elseif key == "left" then
            sample_x = sample_x - 2
        elseif key == "right" then
            sample_x = sample_x + 2
        elseif key == "up" then
            sample_y = sample_y - 2
        elseif key == "down" then
            sample_y = sample_y + 2
        elseif key == "space" then
            show_box = not show_box
        end
    end

    -- Keep enough of the sample visible to continue moving it.
    local sample_w = gfx.text_width(sample)
    local sample_h = gfx.text_height()

    if sample_x < -sample_w + 8 then
        sample_x = -sample_w + 8
    elseif sample_x > SCREEN_W - 8 then
        sample_x = SCREEN_W - 8
    end

    if sample_y < 22 then
        sample_y = 22
    elseif sample_y > 184 - sample_h then
        sample_y = 184 - sample_h
    end
end

gfx.open(SCREEN_W, SCREEN_H)
gfx.set_font(gfx.FONT_8X8)

while running do
    handle_keys()
    draw_frame()
    gfx.sleep(16)
end

gfx.close()
