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

#include "core_history.h"

#include <file/file_path.h>
#include <string/stdstring.h>

static char core_history_core[NAME_MAX_LENGTH];
static bool core_history_dirty;

/**
 * core_history_get_path
 * @out : PATH_MAX_LENGTH size buffer
 */
void core_history_get_path(char *out)
{
   global_t   *global   = global_get_ptr();
   settings_t *settings = config_get_ptr();

   if (!*global->libretro_name)
   {
      *out = 0;
      return;
   }

   fill_pathname_join(out, settings->menu_config_directory,
         global->libretro_name, PATH_MAX_LENGTH);
   fill_pathname_slash(out, PATH_MAX_LENGTH);
   strlcat(out, global->libretro_name, PATH_MAX_LENGTH);
   strlcat(out, "_history.txt", PATH_MAX_LENGTH);
}

void core_history_free()
{
   global_t *global = global_get_ptr();
   int i;

   for (i = 0; i < global->history_size; i++)
      free(global->history[i]);
   free(global->history);

   global->history      = NULL;
   global->history_size = 0;
   *core_history_core   = '\0';
}

bool core_history_erase()
{
   bool success = true;
   char *path   = string_alloc(PATH_MAX_LENGTH);

   core_history_free();

   core_history_get_path(path);

   if (*path && path_file_exists(path))
   {
      RARCH_LOG("Removing history file at path: \"%s\"\n", path);
      if (remove(path) != 0)
         success = false;
   }

   *core_history_core = '\0';
   core_history_dirty = false;
   free(path);
   return success;
}

void core_history_remove(int entry_idx)
{
   global_t *global = global_get_ptr();
   size_t new_size, old_size;

   old_size = global->history_size;
   new_size = old_size - 1;

   if (!new_size)
   {
      core_history_erase();
      return;
   }

   /* Free entry */
   free(global->history[entry_idx]);

   /* Move older entries up */
   if (entry_idx < new_size)
      memmove(global->history + entry_idx, global->history + entry_idx + 1,
            (new_size - entry_idx) * sizeof(char**));

   /* Trim array */
   global->history = realloc(global->history, new_size * sizeof(char**));
   global->history_size = new_size;

   core_history_dirty = true;
}

static void core_history_read()
{
   global_t *global = global_get_ptr();
   FILE *file       = NULL;
   char *path       = string_alloc(PATH_MAX_LENGTH);
   char *line       = string_alloc(PATH_MAX_LENGTH);
   size_t temp_size = 0;
   int num_lines    = 0;

   core_history_free();

   core_history_get_path(path);
   if (!*path)
      goto finish;

   file = fopen(path, "r");
   if (!file)
      goto finish;

   while (!feof(file))
   {
      int in = getc(file);
      int c  = 0;
      *line  = '\0';

      /* Get next line */
      while (in != '\n' && in != EOF && c < PATH_MAX_LENGTH)
      {
         line[c++] = (char)in;
         in        = getc(file);
      }
      num_lines++;

      /* Sanity checks */
      if (c == 0)
         continue;
      if (c == PATH_MAX_LENGTH)
      {
         RARCH_ERR("[Core History] %s line %i exceeds PATH_MAX_LENGTH",
               path_basename(path), num_lines);
         /* Something wrong. Give up */
         break;
      }
      if (num_lines > MAX_HISTORY_SIZE)
         break;

      /* Grow array */
      if (++global->history_size > temp_size)
      {
         temp_size += 64;
         global->history = realloc(global->history, temp_size * sizeof(char**));
         if (!global->history)
         {
            RARCH_ERR("[Core History] Error allocating memory");
            core_history_free();
            goto finish;
         }
      }

      /* Trim any CR */
      if (line[c-1] == '\r')
         c--;

      /* Add to history */
      line[c] = '\0';
      global->history[global->history_size - 1] = strdup(line);
   }

finish:
   strlcpy(core_history_core, global->libretro_name, sizeof(core_history_core));

   if (file)
      fclose(file);
   if (global->history)
      global->history = realloc(
            global->history, global->history_size * sizeof(char**));
   free(line);
   free(path);
}

/**
 * core_history_refresh:
 *
 * Adds or moves the loaded content to top of history list.
 * Can grow the history list past the user setting.
 */
void core_history_refresh()
{
   global_t *global = global_get_ptr();
   char **new_history;
   size_t new_size, old_size;
   int i, j;

   /* Read from file if necessary */
   if (strcmp(core_history_core, global->libretro_name))
   {
      core_history_read();
      core_history_dirty = false;
   }
   if (!*core_history_core)
      return;

   old_size = global->history_size;
   new_size = old_size + 1;

   /* Skip if no changes needed */
   if (!*global->fullpath ||
         (old_size && !strcmp(global->fullpath, global->history[0])))
      return;

   /* Allocate array for resorted entries */
   new_history = malloc(new_size * sizeof(char**));

   /* If loaded content is in list, move to top */
   for (i = 0; i < old_size; i++)
   {
      if (!strcmp(global->fullpath, global->history[i]))
      {
         new_history[0] = global->history[i];
         global->history[i] = NULL;
         new_history = realloc(new_history, (--new_size) * sizeof(char**));
         break;
      }
   }

   /* Otherwise, add to top */
   if (i == old_size)
      new_history[0] = strdup(global->fullpath);

   /* Move old entries */
   for (i = 0, j = 1; i < old_size; i++)
   {
      if (global->history[i])
         new_history[j++] = global->history[i];
   }
   
   /* Don't grow forever */
   while (new_size > MAX_HISTORY_SIZE)
   {
      free(new_history[--new_size]);
      new_history = realloc(new_history, new_size * sizeof(char**));
   }

   free(global->history);
   global->history = new_history;
   global->history_size = new_size;

   core_history_dirty = true;
}

void core_history_write()
{
   settings_t *settings = config_get_ptr();
   global_t *global     = global_get_ptr();
   FILE *file           = NULL;
   char *path           = string_alloc(PATH_MAX_LENGTH);
   size_t size;
   int i;

   core_history_refresh();

   /* Skip if no changes needed */
   if (!core_history_dirty
         && global->history_size <= settings->core.history_size)
      goto finish;

   core_history_get_path(path);
   if (!*path)
      goto finish;

   file = fopen(path, "w");
   if (!file)
   {
      RARCH_ERR("[Core History] Unable to open %s for writing",
            path_basename(path));
      goto finish;
   }

   /* Delete if empty */
   if (!global->history_size)
   {
      remove(path);
      goto finish;
   }

   /* Write entries */
   size = min(global->history_size, settings->core.history_size);
   for (i = 0; i < size; i++)
      fprintf(file, "%s\n", global->history[i]);

   core_history_dirty = false;

finish:
   if (file)
      fclose(file);
   free(path);
}

void core_history_init()
{
   settings_t *settings = config_get_ptr();

   if (settings->core.history_write)
      core_history_write();
   else
      core_history_refresh();
}

void core_history_deinit()
{
   settings_t *settings = config_get_ptr();

   if (settings->core.history_write)
      core_history_write();
   core_history_free();
}
