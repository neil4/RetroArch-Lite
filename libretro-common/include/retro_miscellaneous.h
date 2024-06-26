/* Copyright  (C) 2010-2015 The RetroArch team
 *
 * ---------------------------------------------------------------------------------------
 * The following license statement only applies to this file (retro_miscellaneous.h).
 * ---------------------------------------------------------------------------------------
 *
 * Permission is hereby granted, free of charge,
 * to any person obtaining a copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation the rights to
 * use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of the Software,
 * and to permit persons to whom the Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.
 * IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY,
 * WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 */

#ifndef __RARCH_MISCELLANEOUS_H
#define __RARCH_MISCELLANEOUS_H

#if defined(__CELLOS_LV2__) && !defined(__PSL1GHT__)
#include <sys/timer.h>
#elif defined(XENON)
#include <time/time.h>
#elif defined(GEKKO) || defined(__PSL1GHT__) || defined(__QNX__)
#include <unistd.h>
#elif defined(PSP)
#include <pspthreadman.h>
#elif defined(_3DS)
#include <3ds.h>
#else
#include <time.h>
#endif

#if defined(_WIN32) && !defined(_XBOX)
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
typedef unsigned int        uint;
#elif defined(_WIN32) && defined(_XBOX)
#include <Xtl.h>
typedef unsigned int        uint;
#endif
#include <compat/msvc.h>

#if defined(RARCH_INTERNAL) || defined(IS_SALAMANDER)
/* TODO/FIXME - dirty hack */
#include "../../retroarch_logger.h"
#endif
#include <retro_inline.h>
#include <retro_endianness.h>
#include <limits.h>
#include <inttypes.h>

#define U64_FMT "%" PRIu64

#ifndef PATH_MAX_LENGTH
#define PATH_MAX_LENGTH 4096
#endif

#ifndef NAME_MAX_LENGTH
#define NAME_MAX_LENGTH 256
#endif

#ifndef max
#define max(a, b) ((a) > (b) ? (a) : (b))
#endif

#ifndef min
#define min(a, b) ((a) < (b) ? (a) : (b))
#endif

#ifdef RARCH_INTERNAL
#define rarch_assert(cond) do { \
   if (!(cond)) { RARCH_ERR("Assertion failed at %s:%d.\n", __FILE__, __LINE__); abort(); } \
} while(0)
#else
#include <assert.h>
#define rarch_assert(cond) assert(cond)
#endif

#define ARRAY_SIZE(a) (sizeof(a) / sizeof((a)[0]))
#define RARCH_SCALE_BASE 256

/**
 * rarch_sleep:
 * @msec         : amount in milliseconds to sleep
 *
 * Sleeps for a specified amount of milliseconds (@msec).
 **/
static INLINE void rarch_sleep(unsigned msec)
{
#if defined(__CELLOS_LV2__) && !defined(__PSL1GHT__)
   sys_timer_usleep(1000 * msec);
#elif defined(PSP)
   sceKernelDelayThread(1000 * msec);
#elif defined(_3DS)
   svcSleepThread(1000000 * (s64)msec);
#elif defined(_WIN32)
   Sleep(msec);
#elif defined(XENON)
   udelay(1000 * msec);
#elif defined(GEKKO) || defined(__PSL1GHT__) || defined(__QNX__)
   usleep(1000 * msec);
#else
   struct timespec tv = {0};
   tv.tv_sec = msec / 1000;
   tv.tv_nsec = (msec % 1000) * 1000000;
   nanosleep(&tv, NULL);
#endif
}

/**
 * next_pow2:
 * @v         : initial value
 *
 * Get next power of 2 value based on  initial value.
 *
 * Returns: next power of 2 value (derived from @v).
 **/
static INLINE uint32_t next_pow2(uint32_t v)
{
   v--;
   v |= v >> 1;
   v |= v >> 2;
   v |= v >> 4;
   v |= v >> 8;
   v |= v >> 16;
   v++;
   return v;
}

/**
 * prev_pow2:
 * @v         : initial value
 *
 * Get previous power of 2 value based on initial value.
 *
 * Returns: previous power of 2 value (derived from @v).
 **/
static INLINE uint32_t prev_pow2(uint32_t v)
{
   v |= v >> 1;
   v |= v >> 2;
   v |= v >> 4;
   v |= v >> 8;
   v |= v >> 16;
   return v - (v >> 1);
}

/* Helper macros and struct to keep track of many booleans.
 * To check for multiple bits, use &&, not &.
 * For OR, | can be used. */
typedef struct
{
   uint32_t data[8];
} rarch_bits_t;

#define BIT_SET(a, bit)   ((a)[(bit) >> 3] |=  (1 << ((bit) & 7)))
#define BIT_CLEAR(a, bit) ((a)[(bit) >> 3] &= ~(1 << ((bit) & 7)))
#define BIT_GET(a, bit)   ((a)[(bit) >> 3] &   (1 << ((bit) & 7)))

#define BIT16_SET(a, bit)    ((a) |=  (1 << ((bit) & 15)))
#define BIT16_CLEAR(a, bit)  ((a) &= ~(1 << ((bit) & 15)))
#define BIT16_GET(a, bit) (!!((a) &   (1 << ((bit) & 15))))
#define BIT16_CLEAR_ALL(a)   ((a) = 0)

#define BIT32_SET(a, bit)    ((a) |=  (1 << ((bit) & 31)))
#define BIT32_CLEAR(a, bit)  ((a) &= ~(1 << ((bit) & 31)))
#define BIT32_GET(a, bit) (!!((a) &   (1 << ((bit) & 31))))
#define BIT32_CLEAR_ALL(a)   ((a) = 0)

#define BIT64_SET(a, bit)    ((a) |=  (1ULL << ((bit) & 63)))
#define BIT64_CLEAR(a, bit)  ((a) &= ~(1ULL << ((bit) & 63)))
#define BIT64_GET(a, bit) (!!((a) &   (1ULL << ((bit) & 63))))
#define BIT64_CLEAR_ALL(a)   ((a) = 0)

/* A default keyboard device type to ensure a retropad-keyboard mapping is
 * available. Always maps from pad 1 to augment the port 1 libretro device */
#define RETRO_DEVICE_KEYBOARD_DEFAULT RETRO_DEVICE_SUBCLASS(RETRO_DEVICE_KEYBOARD, 0xdef)

#endif
