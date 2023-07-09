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

#ifndef _MENU_DISPLAYLIST_H
#define _MENU_DISPLAYLIST_H

#include <stdint.h>

#include <retro_miscellaneous.h>
#include <file/file_list.h>

#include "menu_setting.h"

#ifdef __cplusplus
extern "C" {
#endif

enum
{
   DISPLAYLIST_NONE = 0,
   DISPLAYLIST_INFO,
   DISPLAYLIST_HELP,
   DISPLAYLIST_MAIN_MENU,
   DISPLAYLIST_GENERIC,
   DISPLAYLIST_SETTINGS,
   DISPLAYLIST_SETTINGS_ALL,
   DISPLAYLIST_DEFAULT,
   DISPLAYLIST_CORES,
   DISPLAYLIST_CORES_SUPPORTED,
   DISPLAYLIST_CORES_UPDATER,
   DISPLAYLIST_CORES_DETECTED,
   DISPLAYLIST_CORE_OPTIONS,
   DISPLAYLIST_CORE_OPTIONS_CATEGORY,
   DISPLAYLIST_CORE_INFO,
   DISPLAYLIST_PERFCOUNTER_SELECTION,
   DISPLAYLIST_PERFCOUNTERS_CORE,
   DISPLAYLIST_PERFCOUNTERS_FRONTEND,
   DISPLAYLIST_SHADER_PASS,
   DISPLAYLIST_SHADER_PRESET,
   DISPLAYLIST_AUDIO_FILTERS,
   DISPLAYLIST_VIDEO_FILTERS,
   DISPLAYLIST_CHEAT_FILES,
   DISPLAYLIST_OPTIONS_FILES,
   DISPLAYLIST_REMAP_FILES,
   DISPLAYLIST_THEMES,
   DISPLAYLIST_IMAGES,
   DISPLAYLIST_FONTS,
   DISPLAYLIST_OVERLAYS,
   DISPLAYLIST_OSK_OVERLAYS,
   DISPLAYLIST_SHADER_PARAMETERS,
   DISPLAYLIST_SYSTEM_INFO,
   DISPLAYLIST_OPTIONS_CHEATS,
   DISPLAYLIST_OPTIONS_REMAPPINGS,
   DISPLAYLIST_OPTIONS_DISK,
   DISPLAYLIST_OPTIONS_SHADERS,
};

typedef struct menu_displaylist_info
{
   file_list_t *list;
   file_list_t *menu_list;
   char path[PATH_MAX_LENGTH];
   char path_b[PATH_MAX_LENGTH];
   char path_c[PATH_MAX_LENGTH];
   char label[NAME_MAX_LENGTH];
   char exts[PATH_MAX_LENGTH];
   unsigned type;
   unsigned type_default;
   size_t directory_ptr;
   unsigned flags;
   rarch_setting_t *setting;
} menu_displaylist_info_t;

int menu_displaylist_push_list(menu_displaylist_info_t *info, unsigned type);

int menu_displaylist_push(file_list_t *list, file_list_t *menu_list);

/**
 * menu_displaylist_init:
 *
 * Creates and initializes menu display list.
 *
 * Returns: true (1) if successful, otherwise false (0).
 **/
bool menu_displaylist_init();

void menu_displaylist_info_clear(menu_displaylist_info_t *info);

menu_displaylist_info_t *menu_displaylist_info_new();

#ifdef __cplusplus
}
#endif

#endif
