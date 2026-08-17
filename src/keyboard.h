/*
** Lightweight keyboard input for luagfx.
**
** Mode X backend:
**   Uses the console through noncanonical, nonblocking stdin.
**
** Nano-X backend:
**   Retrieves buffered key events from nanox.c.
**   nanox.c remains the only consumer of the Nano-X event stream.
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
 * For Nano-X, this enables keyboard event retrieval from the event
 * queue maintained by nanox.c.
 */
int keyboard_open(void);

/*
 * Restore keyboard state and discard backend-specific input state.
 * Safe to call when keyboard handling is not open.
 *
 * The Mode X backend restores the console terminal settings.
 * The Nano-X backend disables keyboard event retrieval.
 */
void keyboard_close(void);

/*
 * Return one pending key-press event using a LÖVE-style key name.
 *
 * Examples:
 *   "a", "0", "space", "return", "escape",
 *   "left", "right", "up", "down"
 *
 * For Nano-X, key events buffered by nanox.c are translated here.
 * A Nano-X window close request is reported once as "escape".
 *
 * Returns NULL when no supported key event is available.
 * The returned pointer refers to static storage and must not be freed.
 */
const char *keyboard_keypressed(void);

/*
 * Return the most recent keyboard initialization error,
 * or an empty string when no error is available.
 */
const char *keyboard_error(void);

#endif /* keyboard_h */
