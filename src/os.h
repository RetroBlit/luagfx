#ifndef LUAGFX_OS_H
#define LUAGFX_OS_H

/*
 Precise busy-wait delay. Uses the ELKS precision timer and 
 does not relinquish the CPU. It is to be used for small delays.
 */
void precise_elks_delay(unsigned int ms);


/*
  Hybrid precise sleep. ELKS sleep is complete bonkers for short delays.
  Requesting 1 ms can result in a delay of roughly 10 to 20 ms! :(
  This required the implementation of this function.
  
  - Short delays use precise_elks_delay().
  - Longer delays use normal ELKS usleep() for the coarse portion,
    measure the actual elapsed time with get_ptime(), then busy-wait
    only for the remaining time.
*/
void precise_elks_sleep(unsigned int ms);

#endif
