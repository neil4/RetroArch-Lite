/*  RetroArch - A frontend for libretro.
 *  Copyright (C) 2024 - Neil Fore
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


#ifndef CORE_HISTORY_H
#define CORE_HISTORY_H

#include "general.h"

#define MAX_HISTORY_SIZE 256

#ifdef __cplusplus
extern "C" {
#endif

void core_history_get_path(char *buf);

void core_history_free();

bool core_history_erase();

void core_history_refresh();

void core_history_write();

void core_history_remove(int entry_idx);

void core_history_init();

void core_history_deinit();

#ifdef __cplusplus
}
#endif

#endif /* CORE_HISTORY_H */

