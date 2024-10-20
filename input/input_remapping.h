/*  RetroArch - A frontend for libretro.
 *  Copyright (C) 2011-2015 - Daniel De Matteis
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

#ifndef _INPUT_REMAPPING_H
#define _INPUT_REMAPPING_H

#include <stdint.h>
#include <stddef.h>
#include <boolean.h>

#ifdef __cplusplus
extern "C" {
#endif

extern unsigned input_remapping_scope;
extern bool input_remapping_touched;
extern const unsigned input_remapping_btn_order[];

/**
 * input_remapping_load_file:
 * @path                     : Path to remapping file (absolute path).
 *
 * Loads a remap file from disk to memory.
 *
 * Returns: true (1) if successful, otherwise false (0).
 **/
bool input_remapping_load_file(const char *path);

/**
 * remap_file_load_auto
 * 
 * Attempt to load ROM-, Directory-, then Core-specific input remap file.
 * If unsuccessful, initialize defaults.
 **/
void remap_file_load_auto();

/**
 * input_remapping_save:
 *
 * Saves remapping values to file based on input_remapping_scope.
 * Also deletes remap files as necessary if input_remapping_scope was changed.
 *
 * Returns: true (1) if successful, otherwise false (0).
 **/
bool input_remapping_save();

void input_remapping_set_defaults(void);

void input_remapping_state(unsigned port,
      unsigned *device, unsigned *idx, unsigned *id);

void input_remapping_set_default_desc();

/**
 * input_remapping_get_path:
 * @path                   : PATH_MAX_LENGTH buffer
 * @scope                  : THIS_CORE, THIS_CONTENT_DIR, or THIS_CONTENT_ONLY
 *
 * Sets @path to remapping file path for the @scope given.
 */
void input_remapping_get_path(char* path, unsigned scope);

unsigned input_remapping_next_id(unsigned id, bool digital_only);
unsigned input_remapping_prev_id(unsigned id, bool digital_only);
unsigned input_remapping_last_id(bool digital_only);

#ifdef __cplusplus
}
#endif

#endif
