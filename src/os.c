#include "os.h"
#include <unistd.h>
#include <linuxmt/prectimer.h>

/*
 * ELKS PIT runs at approximately 1.193182 MHz.
 *
 * 11932 precision-timer ticks = 10 ms.
 */
#define PTICKS_PER_10MS          11932UL

/*
 * ELKS usleep() has approximately 10 ms scheduler granularity and
 * normally overshoots by about one scheduler tick.
 *
 * Reserve 20 ms for the precise finishing phase.
 */
#define PRECISE_SLEEP_MARGIN_MS  20U


static int ptime_initialized;


/*
 * Initialise the ELKS precision timer once.
 */
static int ensure_ptime_initialized(void)
{
    if (ptime_initialized)
        return 1;

    if (!init_ptime())
        return 0;

    /*
     * Establish the initial get_ptime() reference.
     */
    (void)get_ptime();

    ptime_initialized = 1;

    return 1;
}

/*
 * Convert milliseconds to ELKS precision-timer ticks.
 *
 * Round upward so integer truncation cannot shorten a delay.
 */
static unsigned long ms_to_pticks(unsigned int ms)
{
    return ((unsigned long)ms * PTICKS_PER_10MS + 9UL) / 10UL;
}

/*
 * Busy-wait for 'remaining' pticks.
 *
 * A get_ptime() reference must already have been established before
 * entering this function.
 */
static void delay_remaining(unsigned long remaining)
{
    unsigned long elapsed;

    while (remaining != 0) {
        elapsed = get_ptime();

        if (elapsed >= remaining)
            return;

        remaining -= elapsed;
    }
}

void precise_elks_delay(unsigned int ms)
{
    unsigned long remaining;

    if (ms == 0)
        return;

    /*
     * Fall back to normal ELKS sleep if the precision timer
     * cannot be initialised.
     */
    if (!ensure_ptime_initialized()) {
        (void)usleep((unsigned long)ms * 1000UL);
        return;
    }

    remaining = ms_to_pticks(ms);

    /*
     * Establish the start of the measured interval.
     */
    (void)get_ptime();

    delay_remaining(remaining);
}


void precise_elks_sleep(unsigned int ms)
{
    unsigned long target;
    unsigned long elapsed;
    unsigned long remaining;
    unsigned long coarse_ms;

    if (ms == 0)
        return;

    /*
     * For short delays ELKS usleep() is too coarse.
     */
    if (ms <= PRECISE_SLEEP_MARGIN_MS) {
        precise_elks_delay(ms);
        return;
    }

    if (!ensure_ptime_initialized()) {
        (void)usleep((unsigned long)ms * 1000UL);
        return;
    }

    target = ms_to_pticks(ms);

    /*
     * Start measuring BEFORE the coarse sleep.
     */
    (void)get_ptime();

    /*
     * Leave a 20 ms safety margin for the precise finishing phase.
     */
    coarse_ms = (unsigned long)ms - PRECISE_SLEEP_MARGIN_MS;

    /*
     * Relinquish the CPU for most of the requested interval.
     */
    (void)usleep(coarse_ms * 1000UL);

    /*
     * Measure how much time ELKS usleep() actually consumed.
     */
    elapsed = get_ptime();

    /*
     * usleep() may already have reached or exceeded the requested
     * duration.
     */
    if (elapsed >= target)
        return;

    remaining = target - elapsed;

    /*
     * get_ptime() above established the reference for the precise
     * finishing phase, so continue directly from it.
     */
    delay_remaining(remaining);
}