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

#define MAX_PREEMPT_FRAMES 6

#include "libretro_version_1.h"

typedef struct preempt preempt_t;

/**
 * preempt_skip_av:
 * @preempt       : pointer to preempt object
 * 
 * Returns: true if audio & video will be skipped this frame, false if not
 **/
bool preempt_skip_av(preempt_t *preempt);

void input_poll_preempt(void);

int16_t input_state_preempt(unsigned port, unsigned device,
      unsigned idx, unsigned id);

void video_frame_preempt(const void *data, unsigned width,
      unsigned height, size_t pitch);

void audio_sample_preempt(int16_t left, int16_t right);

size_t audio_sample_batch_preempt(const int16_t *data, size_t frames);

/**
 * preempt_pre_frame:   
 * @preempt         : pointer to preempt object
 *
 * Pre-frame for preempt.
 * Call this before running retro_run().
 **/
void preempt_pre_frame(preempt_t *preempt);

void deinit_preempt(void);

/**
 * init_preempt:
 *
 * Initializes preempt. Skips init if preempt_frames == 0.
 *
 * Returns: true on success, false on failure
 **/
bool init_preempt(void);

/**
 * update_preempt_frames
 * 
 * Inits/Deinits/Reinits preempt as needed.
 */
void update_preempt_frames();

/**
 * preempt_reset_buffer
 * 
 * Fills preempt buffer with current state, to prevent potentially loading a
 * bad state after init or user state-load.
 */
void preempt_reset_buffer();

/* Overrides for libretro callbacks
 */
void input_poll_preempt(void);
int16_t input_state_preempt(unsigned port, unsigned device,
      unsigned idx, unsigned id);
void video_frame_preempt(const void *data, unsigned width,
      unsigned height, size_t pitch);
void audio_sample_preempt(int16_t left, int16_t right);
size_t audio_sample_batch_preempt(const int16_t *data, size_t frames);

#endif /* PREEMPT_H */

