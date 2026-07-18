/*
** Lightweight keyboard input for luagfx.
*/

#include "keyboard.h"

#ifdef USE_NANOX_BACKEND

#include "nanox.h"

static int nanox_keyboard_ready;
static char nanox_character_name[2];

/*

Translate one Nano-X key code to the name returned

by gfx.keypressed().
*/
static const char *nanox_translate_key(GR_KEY key)
{
switch (key) {
case MWKEY_ESCAPE:
return "escape";

case MWKEY_ENTER:
return "return";

#ifdef MWKEY_KP_ENTER
case MWKEY_KP_ENTER:
return "return";
#endif

case MWKEY_TAB:
    return "tab";

case MWKEY_BACKSPACE:
    return "backspace";

case MWKEY_LEFT:
    return "left";

case MWKEY_RIGHT:
    return "right";

case MWKEY_UP:
    return "up";

case MWKEY_DOWN:
    return "down";

case MWKEY_HOME:
    return "home";

case MWKEY_END:
    return "end";

case MWKEY_INSERT:
    return "insert";

case MWKEY_DELETE:
    return "delete";

case MWKEY_PAGEUP:
    return "pageup";

case MWKEY_PAGEDOWN:
    return "pagedown";

case ' ':
    return "space";

default:
    break;
}

/*
 * Printable ASCII characters.
 *
 * Convert upper-case letters to lower-case so the returned names
 * match the Mode X implementation.
 */
if (key >= 32 && key <= 126) {
    unsigned char value;

    value = (unsigned char)key;

    if (value >= (unsigned char)'A' &&
        value <= (unsigned char)'Z') {
        value =
            (unsigned char)(value - (unsigned char)'A' +
                            (unsigned char)'a');
    }

    nanox_character_name[0] = (char)value;
    nanox_character_name[1] = '\0';

    return nanox_character_name;
}

return (const char *)0;

}

int keyboard_open(void)
{
nanox_keyboard_ready = 1;
return 0;
}

void keyboard_close(void)
{
nanox_keyboard_ready = 0;
}

const char *keyboard_keypressed(void)
{
GR_KEY key;
const char *name;

if (!nanox_keyboard_ready)
    return (const char *)0;

/*
 * nanox.c is the only Nano-X event consumer.
 *
 * Events received by nanox_sleep_ms() or by this nonblocking poll
 * are stored in the Nano-X keyboard queue and returned here.
 *
 * Unsupported key codes are discarded while continuing to inspect
 * the remaining buffered events.
 */
while (nanox_get_key(&key)) {
    name = nanox_translate_key(key);

    if (name != (const char *)0)
        return name;
}

return (const char *)0;

}

const char *keyboard_error(void)
{
return "";
}

#else /* Mode X console keyboard */

#include <termios.h>
#include <fcntl.h>
#include <unistd.h>

#ifndef STDIN_FILENO
#define STDIN_FILENO 0
#endif

#define KEYBOARD_KEYBUF_SIZE 16

static int keyboard_ready;
static int keyboard_old_stdin_flags;
static struct termios keyboard_old_termios;

static unsigned char keyboard_keybuf[KEYBOARD_KEYBUF_SIZE];
static unsigned char keyboard_keybuf_start;
static unsigned char keyboard_keybuf_count;

/*
 * A lone ESC is held for one poll so that an ANSI cursor-key sequence
 * such as ESC [ A has a chance to arrive completely.
 */
static unsigned char keyboard_escape_waiting;

static char keyboard_character_name[2];
static const char *keyboard_last_error = "";

/*
 * Reset only the internal event queue.
 */
static void keyboard_reset_queue(void)
{
    keyboard_keybuf_start = 0;
    keyboard_keybuf_count = 0;
    keyboard_escape_waiting = 0;
}

/*
 * Return a queued byte without consuming it.
 * The caller must ensure offset < keyboard_keybuf_count.
 */
static unsigned char keyboard_peek(unsigned int offset)
{
    unsigned int index;

    index = (unsigned int)keyboard_keybuf_start + offset;
    index %= KEYBOARD_KEYBUF_SIZE;

    return keyboard_keybuf[index];
}

/*
 * Remove bytes from the front of the queue.
 */
static void
keyboard_consume(unsigned int count)
{
    if (count >= (unsigned int)keyboard_keybuf_count) {
        keyboard_keybuf_start = 0;
        keyboard_keybuf_count = 0;
        return;
    }

    keyboard_keybuf_start =
        (unsigned char)(((unsigned int)keyboard_keybuf_start + count) %
                        KEYBOARD_KEYBUF_SIZE);

    keyboard_keybuf_count =
        (unsigned char)((unsigned int)keyboard_keybuf_count - count);
}

/*
 * Read all bytes currently available from stdin into the ring buffer.
 * read() is nonblocking, so zero or a negative result simply means that
 * no additional input is currently available.
 */
static void keyboard_fill_queue(void)
{
    unsigned int tail;
    unsigned int free_count;
    unsigned int contiguous_count;
    int result;

    while (keyboard_keybuf_count < KEYBOARD_KEYBUF_SIZE) {
        tail = ((unsigned int)keyboard_keybuf_start +
                (unsigned int)keyboard_keybuf_count) %
               KEYBOARD_KEYBUF_SIZE;

        free_count =
            KEYBOARD_KEYBUF_SIZE - (unsigned int)keyboard_keybuf_count;

        contiguous_count = KEYBOARD_KEYBUF_SIZE - tail;
        if (contiguous_count > free_count)
            contiguous_count = free_count;

        result = (int)read(STDIN_FILENO,
                           keyboard_keybuf + tail,
                           contiguous_count);

        if (result <= 0)
            break;

        keyboard_keybuf_count =
            (unsigned char)((unsigned int)keyboard_keybuf_count +
                            (unsigned int)result);
    }
}


/*
 * Return a static one-character key name.
 */
static const char *
keyboard_printable_name(unsigned char value)
{
    if (value >= (unsigned char)'A' &&
        value <= (unsigned char)'Z')
        value = (unsigned char)(value - 'A' + 'a');

    keyboard_character_name[0] = (char)value;
    keyboard_character_name[1] = '\0';

    return keyboard_character_name;
}

/*
 * Decode ESC [ <code> and ESC O <code> cursor-key sequences.
 *
 * Returns:
 *   key name when a complete supported sequence was consumed;
 *   NULL when the sequence is incomplete or unsupported.
 *
 * An unsupported sequence is not consumed here. The caller will treat
 * its initial ESC as an ordinary Escape key.
 */
static const char *keyboard_decode_escape_sequence(void)
{
    unsigned char prefix;
    unsigned char code;

    if (keyboard_keybuf_count < 2)
        return (const char *)0;

    prefix = keyboard_peek(1);

    if (prefix != (unsigned char)'[' &&
        prefix != (unsigned char)'O')
        return (const char *)0;

    if (keyboard_keybuf_count < 3)
        return (const char *)0;

    code = keyboard_peek(2);

    switch (code) {
    case 'A':
        keyboard_consume(3);
        return "up";

    case 'B':
        keyboard_consume(3);
        return "down";

    case 'C':
        keyboard_consume(3);
        return "right";

    case 'D':
        keyboard_consume(3);
        return "left";

    case 'H':
        keyboard_consume(3);
        return "home";

    case 'F':
        keyboard_consume(3);
        return "end";

    default:
        break;
    }

    /*
     * Common four-byte ANSI sequences:
     *
     *   ESC [ 1 ~   Home
     *   ESC [ 2 ~   Insert
     *   ESC [ 3 ~   Delete
     *   ESC [ 4 ~   End
     *   ESC [ 5 ~   Page Up
     *   ESC [ 6 ~   Page Down
     *   ESC [ 7 ~   Home
     *   ESC [ 8 ~   End
     */
    if (prefix == (unsigned char)'[' &&
        keyboard_keybuf_count >= 4 &&
        keyboard_peek(3) == (unsigned char)'~') {
        switch (code) {
        case '1':
        case '7':
            keyboard_consume(4);
            return "home";

        case '2':
            keyboard_consume(4);
            return "insert";

        case '3':
            keyboard_consume(4);
            return "delete";

        case '4':
        case '8':
            keyboard_consume(4);
            return "end";

        case '5':
            keyboard_consume(4);
            return "pageup";

        case '6':
            keyboard_consume(4);
            return "pagedown";

        default:
            break;
        }
    }

    return (const char *)0;
}

int keyboard_open(void)
{
    struct termios new_termios;
    int flags;

    if (keyboard_ready)
        return 0;

    keyboard_last_error = "";
    keyboard_reset_queue();

    if (tcgetattr(STDIN_FILENO, &keyboard_old_termios) < 0) {
        keyboard_last_error =
            "unable to read keyboard terminal settings";
        return -1;
    }

    flags = fcntl(STDIN_FILENO, F_GETFL, 0);
    if (flags < 0) {
        keyboard_last_error =
            "unable to read keyboard input flags";
        return -1;
    }

    keyboard_old_stdin_flags = flags;
    new_termios = keyboard_old_termios;

    /*
     * Disable line buffering and local echo.
     * Keep ISIG enabled so Ctrl-C can still interrupt the program.
     */
    new_termios.c_lflag &= ~(ICANON | ECHO);

#ifdef ECHONL
    new_termios.c_lflag &= ~ECHONL;
#endif

#ifdef IXON
    /*
     * Do not let Ctrl-S/Ctrl-Q pause and resume game input.
     */
    new_termios.c_iflag &= ~IXON;
#endif

#ifdef IXOFF
    new_termios.c_iflag &= ~IXOFF;
#endif

    new_termios.c_cc[VMIN] = 0;
    new_termios.c_cc[VTIME] = 0;

    if (tcsetattr(STDIN_FILENO, TCSANOW, &new_termios) < 0) {
        keyboard_last_error =
            "unable to configure keyboard terminal";
        return -1;
    }

    if (fcntl(STDIN_FILENO,
              F_SETFL,
              keyboard_old_stdin_flags | O_NONBLOCK) < 0) {
        /*
         * Undo the terminal change if nonblocking mode cannot be set.
         */
        tcsetattr(STDIN_FILENO, TCSANOW, &keyboard_old_termios);

        keyboard_last_error =
            "unable to enable nonblocking keyboard input";
        return -1;
    }

    keyboard_ready = 1;
    return 0;
}

void keyboard_close(void)
{
    if (!keyboard_ready) {
        keyboard_reset_queue();
        return;
    }

    /*
     * Restore both pieces of console state. Continue with the second
     * restoration even if the first one fails.
     */
    tcsetattr(STDIN_FILENO, TCSANOW, &keyboard_old_termios);
    fcntl(STDIN_FILENO, F_SETFL, keyboard_old_stdin_flags);

    keyboard_ready = 0;
    keyboard_reset_queue();
}


const char *keyboard_keypressed(void)
{
    unsigned char value;
    const char *name;

    if (!keyboard_ready)
        return (const char *)0;

    keyboard_fill_queue();

    while (keyboard_keybuf_count != 0) {
        value = keyboard_peek(0);

        if (value == 27) {
            name = keyboard_decode_escape_sequence();

            if (name != (const char *)0) {
                keyboard_escape_waiting = 0;
                return name;
            }

            /*
             * Delay a lone or incomplete ESC once. This prevents the
             * first byte of an arrow-key sequence from being reported
             * prematurely as "escape".
             */
            if (!keyboard_escape_waiting &&
                (keyboard_keybuf_count == 1 ||
                 (keyboard_keybuf_count == 2 &&
                  (keyboard_peek(1) == (unsigned char)'[' ||
                   keyboard_peek(1) == (unsigned char)'O')))) {
                keyboard_escape_waiting = 1;
                return (const char *)0;
            }

            keyboard_escape_waiting = 0;
            keyboard_consume(1);
            return "escape";
        }

        keyboard_escape_waiting = 0;
        keyboard_consume(1);

        switch (value) {
        case '\r':
        case '\n':
            return "return";

        case '\t':
            return "tab";

        case '\b':
        case 127:
            return "backspace";

        case ' ':
            return "space";

        default:
            break;
        }

        if (value >= 32 && value <= 126)
            return keyboard_printable_name(value);

        /*
         * Ignore unsupported control bytes and continue looking for
         * another queued event instead of returning a false "no key".
         */
    }

    return (const char *)0;
}

const char * keyboard_error(void)
{
    return keyboard_last_error;
}

#endif /* USE_NANOX_BACKEND */
