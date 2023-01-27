/*  RetroArch - A frontend for libretro.
 *  2019 - Neil Fore
 *
 *  RetroArch is free software: you can redistribute it and/or modify it under the terms
 *  of the GNU General Public License as published by the Free Software Found-
 *  ation, either version 3 of the License, or (at your option) any later version.
 *
 *  RetroArch is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY;
 *  without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR
 *  PURPOSE.  See the GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License along with RetroArch.
 *  If not, see <http://www.gnu.org/licenses/>.
 */

#ifndef PREEMPT_H
#define PREEMPT_H

#define MAX_PREEMPT_FRAMES 10

#include "libretro_version_1.h"

typedef struct preempt_data preempt_t;

/**
 * preempt_in_preframe:
 * @preempt       : pointer to preempt object
 * 
 * Returns: true if audio & video should be skipped and fast savestates used
 **/
bool preempt_in_preframe(preempt_t *preempt);

void input_poll_preempt(void);

int16_t input_state_preempt(unsigned port, unsigned device,
      unsigned idx, unsigned id);

/**
 * preempt_pre_frame:   
 * @preempt         : pointer to preempt object
 *
 * Pre-frame for preempt.
 * Call this before running retro_run().
 **/
void preempt_pre_frame(preempt_t *preempt);

void preempt_deinit(void);

/**
 * preempt_init:
 *
 * Initializes preempt. Skips init if preempt_frames == 0.
 *
 * Returns: true on success, false on failure
 **/
bool preempt_init(void);

/**
 * preempt_reset_buffer
 * 
 * Forces preempt to refill its state buffer before replaying frames.
 */
void preempt_reset_buffer(preempt_t *preempt);

#endif /* PREEMPT_H */

