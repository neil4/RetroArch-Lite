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

#include "core_info.h"
#include "general.h"
#include <file/file_path.h>
#include "file_ext.h"
#include <file/file_extract.h>
#include "dir_list_special.h"
#include "config.def.h"

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

static void core_info_list_resolve_all_extensions(
      core_info_list_t *core_info_list)
{
   size_t i, all_ext_len = 0;

   if (!core_info_list)
      return;

   for (i = 0; i < core_info_list->count; i++)
   {
      if (core_info_list->list[i].supported_extensions)
         all_ext_len += 
            (strlen(core_info_list->list[i].supported_extensions) + 2);
   }

   if (all_ext_len)
      core_info_list->all_ext = (char*)calloc(1, all_ext_len);

   if (!core_info_list->all_ext)
      return;

   for (i = 0; i < core_info_list->count; i++)
   {
      if (!core_info_list->list[i].supported_extensions)
         continue;

      strlcat(core_info_list->all_ext,
            core_info_list->list[i].supported_extensions, all_ext_len);
      strlcat(core_info_list->all_ext, "|", all_ext_len);
   }
}

static void core_info_list_resolve_all_firmware(
      core_info_list_t *core_info_list)
{
   size_t i;
   unsigned c;

   if (!core_info_list)
      return;

   for (i = 0; i < core_info_list->count; i++)
   {
      unsigned count    = 0;
      core_info_t *info = (core_info_t*)&core_info_list->list[i];

      if (!info || !info->data)
         continue;

      if (!config_get_uint(info->data, "firmware_count", &count))
         continue;

      info->firmware = (core_info_firmware_t*)
         calloc(count, sizeof(*info->firmware));

      if (!info->firmware)
         continue;

      for (c = 0; c < count; c++)
      {
         char path_key[64] = {0};
         char desc_key[64] = {0};
         char opt_key[64]  = {0};

         snprintf(path_key, sizeof(path_key), "firmware%u_path", c);
         snprintf(desc_key, sizeof(desc_key), "firmware%u_desc", c);
         snprintf(opt_key, sizeof(opt_key), "firmware%u_opt", c);

         config_get_string(info->data, path_key, &info->firmware[c].path);
         config_get_string(info->data, desc_key, &info->firmware[c].desc);
         config_get_bool(info->data, opt_key , &info->firmware[c].optional);
      }
   }
}

static void core_info_parse_installed(core_info_t *core_info)
{
   unsigned fw_count = 0;

   config_get_string(core_info->data, "display_name",
         &core_info->display_name);
   config_get_string(core_info->data, "corename",
         &core_info->core_name);
   config_get_string(core_info->data, "systemname",
         &core_info->systemname);

   if (config_get_string(core_info->data, "manufacturer",
            &core_info->system_manufacturer) &&
         core_info->system_manufacturer)
      core_info->system_manufacturer_list =
         string_split(core_info->system_manufacturer, "|");

   config_get_uint(core_info->data, "firmware_count", &fw_count);
   core_info->firmware_count = fw_count;
   if (config_get_string(core_info->data, "supported_extensions",
            &core_info->supported_extensions) &&
         core_info->supported_extensions)
      core_info->supported_extensions_list =
         string_split(core_info->supported_extensions, "|");

   if (config_get_string(core_info->data, "authors",
            &core_info->authors) &&
         core_info->authors)
      core_info->authors_list =
         string_split(core_info->authors, "|");

   if (config_get_string(core_info->data, "permissions",
            &core_info->permissions) &&
         core_info->permissions)
      core_info->permissions_list =
         string_split(core_info->permissions, "|");

   if (config_get_string(core_info->data, "license",
            &core_info->licenses) &&
         core_info->licenses)
      core_info->licenses_list =
         string_split(core_info->licenses, "|");

   if (config_get_string(core_info->data, "categories",
            &core_info->categories) &&
         core_info->categories)
      core_info->categories_list =
         string_split(core_info->categories, "|");

   if (config_get_string(core_info->data, "database",
            &core_info->databases) &&
         core_info->databases)
      core_info->databases_list =
         string_split(core_info->databases, "|");

   if (config_get_string(core_info->data, "notes",
            &core_info->notes) &&
         core_info->notes)
      core_info->note_list = string_split(core_info->notes, "|");

   if (config_get_string(core_info->data, "required_hw_api",
            &core_info->required_hw_api) &&
         core_info->required_hw_api)
      core_info->required_hw_api_list = string_split(
            core_info->required_hw_api, "|");

   config_get_bool(core_info->data, "supports_no_game",
         &core_info->supports_no_game);
}

static void core_info_parse_downloadable(core_info_t *core_info)
{
   config_get_string(core_info->data, "display_name",
         &core_info->display_name);
   config_get_string(core_info->data, "description",
         &core_info->description);
}

core_info_list_t *core_info_list_new(enum info_list_target target)
{
   size_t i;
   core_info_t *core_info = NULL;
   core_info_list_t *core_info_list = NULL;
   settings_t *settings = config_get_ptr();
   global_t *global     = global_get_ptr();
   struct string_list *contents;
   
   if (target == DOWNLOADABLE_CORES)
      contents = dir_list_new(settings->libretro_info_path, "info", false);
   else
      contents = dir_list_new_special(NULL, DIR_LIST_CORES);

   if (!contents)
      return NULL;

   core_info_list = (core_info_list_t*)calloc(1, sizeof(*core_info_list));
   if (!core_info_list)
      goto error;

   core_info = (core_info_t*)calloc(contents->size, sizeof(*core_info));
   if (!core_info)
      goto error;

   core_info_list->list = core_info;
   core_info_list->count = contents->size;

   for (i = 0; i < contents->size; i++)
   {
      char info_path_base[NAME_MAX_LENGTH];
      char info_path[PATH_MAX_LENGTH];

      /* get platform-free name */
      info_path_base[0] = '\0';
      path_libretro_name(info_path_base, contents->elems[i].data);
      
      /* set path (search key) */
      if (target == DOWNLOADABLE_CORES)
         core_info[i].path = strdup(info_path_base); /* key on libretro name */
      else
         core_info[i].path = strdup(contents->elems[i].data); /* key on lib path */

      if (target == LAUNCHED_CORE
          && strncmp(info_path_base, global->libretro_name, NAME_MAX_LENGTH))
         continue;
      
      /* get info file path */
      strlcat(info_path_base, "_libretro.info", sizeof(info_path_base));
      fill_pathname_join(info_path, (*settings->libretro_info_path) ?
            settings->libretro_info_path : settings->libretro_directory,
            info_path_base, sizeof(info_path));

      core_info[i].data = config_file_new(info_path);

      if (core_info[i].data)
      {
         if (target == DOWNLOADABLE_CORES)
            core_info_parse_downloadable(&core_info[i]);
         else
            core_info_parse_installed(&core_info[i]);
      }

      if (!core_info[i].display_name)
         core_info[i].display_name = strdup(path_basename(core_info[i].path));
   }

   core_info_list_resolve_all_extensions(core_info_list);
   core_info_list_resolve_all_firmware(core_info_list);

   dir_list_free(contents);
   return core_info_list;

error:
   if (contents)
      dir_list_free(contents);
   core_info_list_free(core_info_list);
   return NULL;
}

void core_info_list_free(core_info_list_t *core_info_list)
{
   size_t i, j;

   if (!core_info_list)
      return;

   for (i = 0; i < core_info_list->count; i++)
   {
      core_info_t *info = (core_info_t*)&core_info_list->list[i];

      if (!info)
         continue;

      free(info->path);
      free(info->core_name);
      free(info->systemname);
      free(info->system_manufacturer);
      free(info->display_name);
      free(info->supported_extensions);
      free(info->authors);
      free(info->permissions);
      free(info->licenses);
      free(info->categories);
      free(info->databases);
      free(info->notes);
      if (info->supported_extensions_list)
         string_list_free(info->supported_extensions_list);
      string_list_free(info->system_manufacturer_list);
      string_list_free(info->authors_list);
      string_list_free(info->note_list);
      string_list_free(info->permissions_list);
      string_list_free(info->licenses_list);
      string_list_free(info->categories_list);
      string_list_free(info->databases_list);
      config_file_free(info->data);

      for (j = 0; j < info->firmware_count; j++)
      {
         free(info->firmware[j].path);
         free(info->firmware[j].desc);
      }
      free(info->firmware);
   }

   free(core_info_list->all_ext);
   free(core_info_list->list);
   free(core_info_list);
}

size_t core_info_list_num_info_files(core_info_list_t *core_info_list)
{
   size_t i, num = 0;

   if (!core_info_list)
      return 0;

   for (i = 0; i < core_info_list->count; i++)
      num += !!core_info_list->list[i].data;

   return num;
}

bool core_info_list_get_display_name(core_info_list_t *core_info_list,
      const char *path, char *buf, size_t size)
{
   size_t i;

   if (!core_info_list)
      return false;

   for (i = 0; i < core_info_list->count; i++)
   {
      const core_info_t *info = &core_info_list->list[i];
      if (!strcmp(path_basename(info->path), path_basename(path)))
      {
         if (!info->display_name)
            return false;

         strlcpy(buf, info->display_name, size);
         return true;
      }
   }

   return false;
}

bool core_info_list_get_core_name(core_info_list_t *core_info_list,
      const char *path, char *buf, size_t size)
{
   size_t i;

   if (!core_info_list)
      return false;

   for (i = 0; i < core_info_list->count; i++)
   {
      const core_info_t *info = &core_info_list->list[i];
      if (!strcmp(path_basename(info->path), path_basename(path)))
      {
         if (!info->core_name)
            return false;

         strlcpy(buf, info->core_name, size);
         return true;
      }
   }

   return false;
}

bool core_info_list_get_description(core_info_list_t *core_info_list,
      const char *path, char *buf, size_t size, bool as_messagebox)
{
   global_t *global = global_get_ptr();
   size_t i, len;

   if (!core_info_list)
      return false;

   for (i = 0; i < core_info_list->count; i++)
   {
      const core_info_t *info = &core_info_list->list[i];
      if (!strcmp(path_basename(info->path), path_basename(path)))
      {
         if (!info->description)
            return false;

         len = strlcpy(buf, info->description, size);
         if (as_messagebox)
            menu_driver_wrap_text(buf, len, global->menu.msg_box_width);

         return true;
      }
   }

   return false;
}

bool core_info_list_get_info(core_info_list_t *core_info_list,
      core_info_t *out_info, const char *path)
{
   size_t i;
   if (!core_info_list || !out_info)
      return false;

   memset(out_info, 0, sizeof(*out_info));

   for (i = 0; i < core_info_list->count; i++)
   {
      const core_info_t *info = &core_info_list->list[i];
      if (!strcmp(path_basename(info->path), path_basename(path)))
      {
         *out_info = *info;
         return true;
      }
   }

   return false;
}

bool core_info_does_support_any_file(const core_info_t *core,
      const struct string_list *list)
{
   size_t i;
   if (!list || !core || !core->supported_extensions_list)
      return false;

   for (i = 0; i < list->size; i++)
      if (string_list_find_elem_prefix(core->supported_extensions_list,
               ".", path_get_extension(list->elems[i].data)))
         return true;
   return false;
}

bool core_info_does_support_file(const core_info_t *core, const char *path)
{
   if (!path || !core || !core->supported_extensions_list)
      return false;
   return string_list_find_elem_prefix(
         core->supported_extensions_list, ".", path_get_extension(path));
}

const char *core_info_list_get_all_extensions(core_info_list_t *core_info_list)
{
   if (!core_info_list || !core_info_list->all_ext)
      return "";
   return core_info_list->all_ext;
}

/* qsort_r() is not in standard C, sadly. */
static const char *core_info_tmp_path;
static const struct string_list *core_info_tmp_list;

static int core_info_qsort_cmp(const void *a_, const void *b_)
{
   const core_info_t *a = (const core_info_t*)a_;
   const core_info_t *b = (const core_info_t*)b_;
   int support_a        = core_info_does_support_any_file(a, core_info_tmp_list) ||
      core_info_does_support_file(a, core_info_tmp_path);
   int support_b        = core_info_does_support_any_file(b, core_info_tmp_list) ||
      core_info_does_support_file(b, core_info_tmp_path);

   if (support_a != support_b)
      return support_b - support_a;
   return strcasecmp(a->display_name, b->display_name);
}

void core_info_list_get_supported_cores(core_info_list_t *core_info_list,
      const char *path, const core_info_t **infos, size_t *num_infos)
{
   struct string_list *list = NULL;
   size_t supported = 0, i;

   if (!core_info_list)
      return;

   (void)list;

   core_info_tmp_path = path;

#ifdef HAVE_ZLIB
   if (!strcasecmp(path_get_extension(path), "zip"))
      list = zlib_get_file_list(path, NULL);
   core_info_tmp_list = list;
#endif

   /* Let supported core come first in list so we can return 
    * a pointer to them. */
   qsort(core_info_list->list, core_info_list->count,
         sizeof(core_info_t), core_info_qsort_cmp);

   for (i = 0; i < core_info_list->count; i++, supported++)
   {
      const core_info_t *core = &core_info_list->list[i];

      if (!core)
         continue;

      if (core_info_does_support_file(core, path))
         continue;

#ifdef HAVE_ZLIB
      if (core_info_does_support_any_file(core, list))
         continue;
#endif

      break;
   }

#ifdef HAVE_ZLIB
   if (list)
      string_list_free(list);
#endif

   *infos = core_info_list->list;
   *num_infos = supported;
}

static core_info_t *find_core_info(core_info_list_t *list,
      const char *core)
{
   size_t i;

   for (i = 0; i < list->count; i++)
   {
      core_info_t *info = (core_info_t*)&list->list[i];

      if (!info)
         continue;
      if (!info->path)
         continue;
      if (!strcmp(info->path, core))
         return info;
   }

   return NULL;
}

static int core_info_firmware_cmp(const void *a_, const void *b_)
{
   const core_info_firmware_t *a = (const core_info_firmware_t*)a_;
   const core_info_firmware_t *b = (const core_info_firmware_t*)b_;
   int order = b->missing - a->missing;

   if (order)
      return order;
   return strcasecmp(a->path, b->path);
}

void core_info_list_update_missing_firmware(core_info_list_t *core_info_list,
      const char *core, const char *systemdir)
{
   size_t i;
   char path[PATH_MAX_LENGTH];
   core_info_t *info = NULL;

   if (!core_info_list || !core)
      return;

   if (!(info = find_core_info(core_info_list, core)))
      return;

   for (i = 0; i < info->firmware_count; i++)
   {
      if (!info->firmware[i].path)
         continue;

      fill_pathname_join(path, systemdir,
            info->firmware[i].path, sizeof(path));
      info->firmware[i].missing = !path_exists(path);
   }
}

void core_info_list_get_missing_firmware(core_info_list_t *core_info_list,
      const char *core, const char *systemdir,
      const core_info_firmware_t **firmware, size_t *num_firmware)
{
   size_t i;
   char path[PATH_MAX_LENGTH];
   core_info_t          *info = NULL;

   if (!core_info_list || !core)
      return;

   *firmware = NULL;
   *num_firmware = 0;

   if (!(info = find_core_info(core_info_list, core)))
      return;

   *firmware = info->firmware;

   for (i = 1; i < info->firmware_count; i++)
   {
      fill_pathname_join(path, systemdir, info->firmware[i].path, sizeof(path));
      info->firmware[i].missing = !path_exists(path);
      *num_firmware += info->firmware[i].missing;
   }

   qsort(info->firmware, info->firmware_count, sizeof(*info->firmware),
         core_info_firmware_cmp);
}
