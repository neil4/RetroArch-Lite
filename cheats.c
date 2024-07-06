/*  RetroArch - A frontend for libretro.
 *  Copyright (C) 2010-2014 - Hans-Kristian Arntzen
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

#include "cheats.h"
#include "general.h"
#include "runloop.h"
#include "dynamic.h"
#include <file/config_file.h>
#include <file/file_path.h>
#include <compat/strl.h>
#include <compat/posix_string.h>
#include <string/stdstring.h>
#include "menu/menu.h"

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <stdlib.h>
#include <stddef.h>
#include <string.h>

void cheat_manager_apply_cheats(cheat_manager_t *handle)
{
   unsigned i, idx = 0;

   if (!handle)
      return;

   pretro_cheat_reset();

   for (i = 0; i < handle->size; i++)
   {
      if (handle->cheats[i].state
            && !string_is_empty(handle->cheats[i].code))
         pretro_cheat_set(idx++, true, handle->cheats[i].code);
   }
}

/**
 * cheat_manager_save:
 * @path                      : Path to cheats file (relative path).
 *
 * Saves cheats to file on disk.
 *
 * Returns: true (1) if successful, otherwise false (0).
 **/
bool cheat_manager_save(cheat_manager_t *handle, const char *path)
{
   bool ret;
   unsigned i;
   config_file_t *conf  = NULL;
   char *cheats_file    = string_alloc(PATH_MAX_LENGTH);
   char *buf            = string_alloc(PATH_MAX_LENGTH);
   settings_t *settings = config_get_ptr();

   fill_pathname_join(buf, settings->cheat_database,
         path, PATH_MAX_LENGTH);

   fill_pathname_noext(cheats_file, buf, ".cht", PATH_MAX_LENGTH);
   
   conf = config_file_new(cheats_file);

   if (!conf)
      conf = config_file_new(NULL);

   if (!conf)
      goto error;

   if (!handle)
   {
      config_file_free(conf);
      goto error;
   }

   config_set_int(conf, "cheats", handle->size);

   for (i = 0; i < handle->size; i++)
   {
      char desc_key[32];
      char code_key[32];
      char enable_key[32];

      if (string_is_empty(handle->cheats[i].code))
         continue;

      snprintf(desc_key, sizeof(desc_key), "cheat%u_desc", i);
      snprintf(code_key, sizeof(code_key), "cheat%u_code", i);
      snprintf(enable_key, sizeof(enable_key), "cheat%u_enable", i);

      if (handle->cheats[i].desc)
         config_set_string(conf, desc_key, handle->cheats[i].desc);
      else
         config_set_string(conf, desc_key, handle->cheats[i].code);
      config_set_string(conf, code_key, handle->cheats[i].code);
      config_set_bool(conf, enable_key, handle->cheats[i].state);
   }

   ret = config_file_write(conf, cheats_file);
   config_file_free(conf);

   if (ret)
      snprintf(buf, 128, "Saved %s", path_basename(cheats_file));
   else
      snprintf(buf, 128, "Error saving %s", path_basename(cheats_file));

   rarch_main_msg_queue_push(buf, 2, 180, true);

   free(buf);
   free(cheats_file);
   return ret;

error:
   free(buf);
   free(cheats_file);
   return false;
}

cheat_manager_t *cheat_manager_load(const char *path)
{
   unsigned cheats = 0, i;
   cheat_manager_t *cheat = NULL;
   config_file_t *conf = config_file_new(path);

   if (!conf)
      return NULL;

   config_get_uint(conf, "cheats", &cheats);

   if (cheats == 0)
      return NULL;

   cheats = min(cheats, MAX_CHEAT_COUNTERS);
   cheat  = cheat_manager_new(cheats);

   if (!cheat)
      return NULL;

   for (i = 0; i < cheats; i++)
   {
      char desc_key[32];
      char code_key[32];
      char enable_key[32];
      char *tmp            = NULL;
      bool tmp_bool        = false;

      snprintf(desc_key, sizeof(desc_key), "cheat%u_desc", i);
      snprintf(code_key, sizeof(code_key), "cheat%u_code", i);
      snprintf(enable_key, sizeof(enable_key), "cheat%u_enable", i);

      if (config_get_string(conf, desc_key, &tmp))
      {
         cheat->cheats[i].desc   = strdup(tmp);
         free(tmp);
         tmp = NULL;
      }

      if (config_get_string(conf, code_key, &tmp))
      {
         cheat->cheats[i].code   = strdup(tmp);
         free(tmp);
         tmp = NULL;
      }

      if (config_get_bool(conf, enable_key, &tmp_bool))
         cheat->cheats[i].state  = tmp_bool;
   }

   config_file_free(conf);

   return cheat;
}

cheat_manager_t *cheat_manager_new(unsigned size)
{
   unsigned i;
   cheat_manager_t *handle = (cheat_manager_t*)
      calloc(1, sizeof(struct cheat_manager));

   if (!handle)
      return NULL;

   size = min(size, MAX_CHEAT_COUNTERS);

   handle->buf_size = size;
   handle->size     = size;
   handle->cheats   = (struct item_cheat*)
      calloc(handle->buf_size, sizeof(struct item_cheat));

   if (!handle->cheats)
   {
      handle->buf_size = 0;
      handle->size = 0;
      handle->cheats = NULL;
      return handle;
   }

   for (i = 0; i < handle->size; i++)
   {
      handle->cheats[i].desc   = NULL;
      handle->cheats[i].code   = NULL;
      handle->cheats[i].state  = false;
   }

   return handle;
}

bool cheat_manager_realloc(cheat_manager_t *handle, int new_size)
{
   int old_size;
   int i;

   if (!handle)
      return false;

   new_size = min(new_size, MAX_CHEAT_COUNTERS);
   old_size = handle->size;

   if (!handle->cheats)
      handle->cheats = (struct item_cheat*)
         calloc(new_size, sizeof(struct item_cheat));
   else
   {
      for (i = old_size; i-- > new_size;)
      {
         free(handle->cheats[i].desc);
         free(handle->cheats[i].code);
      }
      handle->cheats = (struct item_cheat*)
         realloc(handle->cheats, new_size * sizeof(struct item_cheat));
   }

   if (!handle->cheats)
   {
      handle->buf_size = handle->size = 0;
      handle->cheats = NULL;
      return false;
   }

   handle->buf_size = new_size;
   handle->size     = new_size;

   for (i = old_size; i < new_size; i++)
   {
      handle->cheats[i].desc    = NULL;
      handle->cheats[i].code    = NULL;
      handle->cheats[i].state   = false;
   }

   return true;
}

void cheat_manager_free(cheat_manager_t *handle)
{
   unsigned i;
   if (!handle)
      return;

   if (handle->cheats)
   {
      for (i = 0; i < handle->size; i++)
      {
         free(handle->cheats[i].desc);
         free(handle->cheats[i].code);
      }

      free(handle->cheats);
   }

   free(handle);
}

void cheat_manager_update(cheat_manager_t *handle, unsigned handle_idx)
{
   char msg[256];

   if (!handle)
      return;

   snprintf(msg, sizeof(msg), "Cheat: #%u [%s]: %s",
         handle_idx, handle->cheats[handle_idx].state ? "ON" : "OFF",
         (handle->cheats[handle_idx].desc) ? 
         (handle->cheats[handle_idx].desc) : (handle->cheats[handle_idx].code)
         );
   rarch_main_msg_queue_push(msg, 1, 180, true);
   RARCH_LOG("%s\n", msg);
}


void cheat_manager_toggle(cheat_manager_t *handle)
{
   if (!handle)
      return;

   handle->cheats[handle->ptr].state ^= true;
   cheat_manager_apply_cheats(handle);
   cheat_manager_update(handle, handle->ptr);
}

void cheat_manager_index_next(cheat_manager_t *handle)
{
   if (!handle)
      return;

   handle->ptr = (handle->ptr + 1) % handle->size;
   cheat_manager_apply_cheats(handle);
   cheat_manager_update(handle, handle->ptr);
}

void cheat_manager_index_prev(cheat_manager_t *handle)
{
   if (!handle)
      return;

   if (handle->ptr == 0)
      handle->ptr = handle->size - 1;
   else
      handle->ptr--;

   cheat_manager_apply_cheats(handle);
   cheat_manager_update(handle, handle->ptr);
}
