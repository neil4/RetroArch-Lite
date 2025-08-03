/*  RetroArch - A frontend for libretro.
 *  Copyright (C) 2010-2014 - Hans-Kristian Arntzen
 *  Copyright (C) 2011-2015 - Daniel De Matteis
 *  Copyright (C) 2013-2015 - Jason Fetters
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

#ifndef CORE_INFO_H_
#define CORE_INFO_H_

#include <file/config_file.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct
{
   char *path;
   char *desc;
   /* Set missing once to avoid opening
    * the same file several times. */
   bool missing;
   bool optional;
} core_info_firmware_t;

typedef struct
{
   char *path;
   config_file_t *data;
   char *display_name;
   char *core_name;
   char *system_manufacturer;
   char *systemname;
   char *supported_extensions;
   char *authors;
   char *permissions;
   char *licenses;
   char *categories;
   char *databases;
   char *notes;
   char *required_hw_api;
   char *description;
   struct string_list *system_manufacturer_list;
   struct string_list *categories_list;
   struct string_list *databases_list;
   struct string_list *note_list;   
   struct string_list *supported_extensions_list;
   struct string_list *authors_list;
   struct string_list *permissions_list;
   struct string_list *licenses_list;
   struct string_list *required_hw_api_list;

   core_info_firmware_t *firmware;
   size_t firmware_count;
   bool supports_no_game;
   void *userdata;
} core_info_t;

typedef struct
{
   core_info_t *list;
   size_t count;
   char *all_ext;
} core_info_list_t;

enum info_list_target
{
   INSTALLED_CORES,
   DOWNLOADABLE_CORES,
   LAUNCHED_CORE
};

core_info_list_t *core_info_list_new(enum info_list_target target);
void core_info_list_free(core_info_list_t *list);

size_t core_info_list_num_info_files(core_info_list_t *list);

bool core_info_does_support_file(const core_info_t *info,
      const char *path);

bool core_info_does_support_any_file(const core_info_t *info,
      const struct string_list *list);

/* Non-reentrant, does not allocate. Returns pointer to internal state. */
void core_info_list_get_supported_cores(core_info_list_t *list,
      const char *path, const core_info_t **infos, size_t *num_infos);

/* Non-reentrant, does not allocate. Returns pointer to internal state. */
void core_info_list_get_missing_firmware(core_info_list_t *list,
      const char *core, const char *systemdir,
      const core_info_firmware_t **firmware, size_t *num_firmware);

void core_info_list_update_missing_firmware(core_info_list_t *list,
      const char *core, const char *systemdir);

/* Shallow-copies internal state. Data in *info is invalidated when the
 * core_info_list is freed. */
bool core_info_list_get_info(core_info_list_t *list,
      core_info_t *info, const char *path);

const char *core_info_list_get_all_extensions(core_info_list_t *list);

bool core_info_list_get_display_name(core_info_list_t *core_info_list,
      const char *path, char *buf, size_t size);

bool core_info_list_get_core_name(core_info_list_t *core_info_list,
      const char *path, char *buf, size_t size);

bool core_info_list_get_description(core_info_list_t *core_info_list,
      const char *path, char *buf, size_t size, bool as_messagebox);

const char *core_info_lib_path(const char *libretro_name);

#ifdef __cplusplus
}
#endif

#endif /* CORE_INFO_H_ */
