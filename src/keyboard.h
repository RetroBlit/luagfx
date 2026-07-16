/*
** Lightweight keyboard input for luagfx.
**
** Mode X backend:
**   Uses the console through noncanonical, nonblocking stdin.
**
** Nano-X backend:
**   Stub implementation. keyboard_keypressed() always returns NULL.
*/

#ifndef keyboard_h
#define keyboard_h

/*
 * Initialize keyboard handling.
 *
 * Returns:
 *   0  success
 *  -1  initialization failed
 *
 * The Nano-X stub returns 0 so that gfx.open() can still succeed.
 */
int keyboard_open(void);

/*
 * Restore the terminal state and discard queued input.
 * Safe to call when the keyboard is not open.
 */
void keyboard_close(void);

/*
 * Return one pending key-press event using a LÖVE-style key name.
 *
 * Examples:
 *   "a", "0", "space", "return", "escape",
 *   "left", "right", "up", "down"
 *
 * Returns NULL when no complete key event is available.
 * The returned pointer refers to static storage and must not be freed.
 */
const char *keyboard_keypressed(void);

/*
 * Return the most recent initialization error, or an empty string.
 */
const char *keyboard_error(void);

#endif /* keyboard_h */
