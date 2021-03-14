/*  RetroArch - A frontend for libretro.
 *  Copyright (C) 2021 - Neil Fore
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

#ifndef _INPUT_JOYKBD_H
#define _INPUT_JOYKBD_H

#include <stdint.h>
#include <stddef.h>
#include <boolean.h>

#include "../libretro.h"
#include "../general.h"

#ifdef __cplusplus
extern "C" {
#endif

#define JOYKBD_LIST_LEN 101
#define NUM_JOYKBD_BTNS (RARCH_FIRST_CUSTOM_BIND + 8)

struct joykbd_bind
{
   const enum retro_key rk;
   uint16_t btn;
   struct joykbd_bind *next;
};

extern bool joykbd_enabled;
extern struct joykbd_bind joykbd_bind_list[];

void input_joykbd_update_enabled();

void input_joykbd_init_binds();

void input_joykbd_add_bind(enum retro_key rk, uint8_t btn);

void input_joykbd_remove_bind(enum retro_key rk, uint8_t btn);

void input_joykbd_poll();

int16_t input_joykbd_state(enum retro_key rk);


#ifdef __cplusplus
}
#endif

#endif
