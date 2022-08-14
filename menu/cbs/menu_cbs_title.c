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

#include <string/string_list.h>
#include <string/stdstring.h>
#include <file/file_path.h>

#include "../menu.h"
#include "../menu_hash.h"

#include "../../general.h"

static INLINE void replace_chars(char *str, char c1, char c2)
{
   char *pos = NULL;
   while((pos = strchr(str, c1)))
      *pos = c2;
}

static INLINE void sanitize_to_string(char *s, const char *label, size_t len)
{
   char new_label[NAME_MAX_LENGTH];

   strlcpy(new_label, label, sizeof(new_label));
   strlcpy(s, string_to_upper(new_label), len);
   replace_chars(s, '_', ' ');
}

static int action_get_title_options_file_load(const char *path, const char *label,
      unsigned menu_type, char *s, size_t len)
{
   snprintf(s, len, "OPTION FILE %s", path);
   return 0;
}

static int action_get_title_disk_image_append(const char *path, const char *label, 
      unsigned menu_type, char *s, size_t len)
{
   snprintf(s, len, "DISC LOAD %s", path);
   return 0;
}

static int action_get_title_remap_file_load(const char *path, const char *label, 
      unsigned menu_type, char *s, size_t len)
{
   snprintf(s, len, "REMAP FILE %s", path);
   return 0;
}

static int action_get_title_help(const char *path, const char *label, 
      unsigned menu_type, char *s, size_t len)
{
   strlcpy(s, "HELP", len);
   return 0;
}

static int action_get_title_osk_load(const char *path, const char *label, 
      unsigned menu_type, char *s, size_t len)
{
   snprintf(s, len, "ONSCREEN KEYBOARD %s", path);
   return 0;
}

static int action_get_title_video_filter(const char *path, const char *label, 
      unsigned menu_type, char *s, size_t len)
{
   snprintf(s, len, "VIDEO FILTER %s", path);
   return 0;
}

static int action_get_title_cheat_directory(const char *path, const char *label, 
      unsigned menu_type, char *s, size_t len)
{
   snprintf(s, len, "CHEAT DIR %s", path);
   return 0;
}

static int action_get_title_core_directory(const char *path, const char *label, 
      unsigned menu_type, char *s, size_t len)
{
   snprintf(s, len, "CORE LIB DIR %s", path);
   return 0;
}

static int action_get_title_core_info_directory(const char *path, const char *label, 
      unsigned menu_type, char *s, size_t len)
{
   snprintf(s, len, "CORE INFO DIR %s", path);
   return 0;
}

static int action_get_title_audio_filter(const char *path, const char *label, 
      unsigned menu_type, char *s, size_t len)
{
   snprintf(s, len, "AUDIO FILTER %s", path);
   return 0;
}

static int action_get_title_font_path(const char *path, const char *label, 
      unsigned menu_type, char *s, size_t len)
{
   snprintf(s, len, "FONT %s", path);
   return 0;
}

static int action_get_title_path_load_generic(const char *path, 
      const char *label, unsigned menu_type, char *s, size_t len)
{
   char new_label[NAME_MAX_LENGTH];

   strlcpy(new_label, label, NAME_MAX_LENGTH);
   string_to_upper(new_label);
   replace_chars(new_label, '_', ' ');

   snprintf(s, len, "%s %s", new_label, path);
   return 0;
}

static int action_get_title_custom_viewport(const char *path, const char *label, 
      unsigned menu_type, char *s, size_t len)
{
   strlcpy(s, "CUSTOM VIEWPORT", len);
   return 0;
}

static int action_get_title_video_shader_preset(const char *path, const char *label, 
      unsigned menu_type, char *s, size_t len)
{
   snprintf(s, len, "SHADER PRESET %s", path);
   return 0;
}

#if 0
static int action_get_title_generic(char *s, size_t len, const char *path,
      const char *text)
{
   char elem0_path[PATH_MAX_LENGTH] = {0};
   char elem1_path[PATH_MAX_LENGTH] = {0};
   struct string_list *list_path    = string_split(path, "|");

   if (list_path)
   {
      if (list_path->size > 0)
      {
         strlcpy(elem0_path, list_path->elems[0].data, sizeof(elem0_path));
         if (list_path->size > 1)
            strlcpy(elem1_path, list_path->elems[1].data, sizeof(elem1_path));
      }
      string_list_free(list_path);
   }

   snprintf(s, len, "%s - %s", text,
         (elem0_path[0] != '\0') ? path_basename(elem0_path) : "");

   return 0;
}
#endif

static int action_get_title_deferred_core_list(const char *path, const char *label, 
      unsigned menu_type, char *s, size_t len)
{
   snprintf(s, len, "DETECTED CORES %s", path);
   return 0;
}

static int action_get_title_default(const char *path, const char *label, 
      unsigned menu_type, char *s, size_t len)
{
   driver_t               *driver = driver_get_ptr();
   if (driver->menu->defer_core)
      snprintf(s, len, "CONTENT %s", path);
   else
   {
      global_t *global      = global_get_ptr();
      const char *core_name = global->menu.info.library_name;

      if (!core_name)
         core_name = global->system.info.library_name;
      if (!core_name)
         core_name = menu_hash_to_str(MENU_VALUE_NO_CORE);
      snprintf(s, len, "CONTENT (%s) %s", core_name, path);
   }


   return 0;
}

static int action_get_title_group_settings(const char *path, const char *label, 
      unsigned menu_type, char *s, size_t len)
{
   struct string_list *list_label = string_split(label, "|");
   char *elem0                    = string_alloc(PATH_MAX_LENGTH);
   char *elem1                    = string_alloc(PATH_MAX_LENGTH);

   if (list_label)
   {
      if (list_label->size > 0)
      {
         strlcpy(elem0, list_label->elems[0].data, PATH_MAX_LENGTH);
         if (list_label->size > 1)
            strlcpy(elem1, list_label->elems[1].data, PATH_MAX_LENGTH);
      }
      string_list_free(list_label);
   }

   strlcpy(s, string_to_upper(elem0), len);

   if (elem1[0] != '\0')
   {
      strlcat(s, " - ", len);
      strlcat(s, string_to_upper(elem1), len);
   }

   free(elem0);
   free(elem1);
   return 0;
}

static int action_get_title_action_generic(const char *path, const char *label, 
      unsigned menu_type, char *s, size_t len)
{
   sanitize_to_string(s, label, len);
   return 0;
}

static int action_get_title_core_option(const char *path, const char *label,
      unsigned menu_type, char *s, size_t len)
{
   global_t *global = global_get_ptr();
   const char *desc = core_option_category_desc(global->system.core_options);

   sanitize_to_string(s, desc, len);

   return 0;
}

static int action_get_title_core_updater(const char *path, const char *label, 
      unsigned menu_type, char *s, size_t len)
{
   strlcpy(s, "CORE UPDATER", len);
   return 0;
}

static int action_get_title_config_directory(const char *path, const char *label, 
      unsigned menu_type, char *s, size_t len)
{
   snprintf(s, len, "CONFIG DIR %s", path);
   return 0;
}

static int action_get_title_browser_directory(const char *path, const char *label, 
      unsigned menu_type, char *s, size_t len)
{
   snprintf(s, len, "BROWSER DIR %s", path);
   return 0;
}

static int action_get_title_menu(const char *path, const char *label, 
      unsigned menu_type, char *s, size_t len)
{
   snprintf(s, len, "MAIN MENU");
   return 0;
}

static int action_get_title_waiting_for_input(const char *path, const char *label, 
      unsigned menu_type, char *s, size_t len)
{
   strlcpy(s, "-- WAITING FOR INPUT --", len);
   return 0;
}

static int menu_cbs_init_bind_title_compare_label(menu_file_list_cbs_t *cbs,
      const char *label, uint32_t label_hash, const char *elem1)
{
   rarch_setting_t *setting = menu_setting_find(label);

   if (setting)
   {
      uint32_t parent_group_hash = menu_hash_calculate(setting->parent_group);

      if ((parent_group_hash == MENU_VALUE_MAIN_MENU) && setting->type == ST_GROUP)
      {
         cbs->action_get_title = action_get_title_group_settings;
         return 0;
      }
   }

   switch (label_hash)
   {
      case MENU_LABEL_DEFERRED_CORE_LIST:
         cbs->action_get_title = action_get_title_deferred_core_list;
         break;
      case MENU_LABEL_RGUI_BROWSER_DIRECTORY:
         cbs->action_get_title = action_get_title_browser_directory;
         break;
      case MENU_LABEL_REMAP_FILE_LOAD:
         cbs->action_get_title = action_get_title_remap_file_load;
         break;
       case MENU_LABEL_OPTIONS_FILE_LOAD:
         cbs->action_get_title = action_get_title_options_file_load;
         break;
      case MENU_LABEL_VIDEO_SHADER_PRESET:
         cbs->action_get_title = action_get_title_video_shader_preset;
         break;
      case MENU_LABEL_RGUI_CONFIG_DIRECTORY:
         cbs->action_get_title = action_get_title_config_directory;
         break;
      case MENU_LABEL_KEYBOARD_OVERLAY_PRESET:
         cbs->action_get_title = action_get_title_osk_load;
         break;
      case MENU_LABEL_DISK_IMAGE_APPEND:
         cbs->action_get_title = action_get_title_disk_image_append;
         break;
      case MENU_LABEL_VIDEO_SHADER_DIR:
      case MENU_LABEL_JOYPAD_AUTOCONFIG_DIR:
      case MENU_LABEL_EXTRACTION_DIRECTORY:
      case MENU_LABEL_SYSTEM_DIRECTORY:
      case MENU_LABEL_ASSETS_DIRECTORY:
      case MENU_LABEL_SAVEFILE_DIRECTORY:
      case MENU_LABEL_OVERLAY_DIRECTORY:
      case MENU_LABEL_CONTENT_DIRECTORY:
      case MENU_LABEL_SCREENSHOT_DIRECTORY:
      case MENU_LABEL_VIDEO_FILTER_DIR:
      case MENU_LABEL_AUDIO_FILTER_DIR:
      case MENU_LABEL_RECORDING_CONFIG_DIRECTORY:
      case MENU_LABEL_RECORDING_OUTPUT_DIRECTORY:
      case MENU_LABEL_OSK_OVERLAY_DIRECTORY:
      case MENU_LABEL_INPUT_REMAPPING_DIRECTORY:
      case MENU_LABEL_SAVESTATE_DIRECTORY:
      case MENU_LABEL_DYNAMIC_WALLPAPERS_DIRECTORY:
      case MENU_LABEL_CORE_ASSETS_DIRECTORY:
      case MENU_LABEL_CHEAT_FILE_LOAD:
      case MENU_LABEL_MENU_THEME:
      case MENU_LABEL_MENU_THEME_DIRECTORY:
      case MENU_LABEL_CORE_CONTENT_DIRECTORY:
      case MENU_LABEL_INPUT_OVERLAY:
         cbs->action_get_title = action_get_title_path_load_generic;
         break;
      case MENU_LABEL_PERFORMANCE_COUNTERS:
      case MENU_LABEL_CORE_LIST:
      case MENU_LABEL_CONFIRM_CORE_DELETION:
      case MENU_LABEL_CONFIRM_SHADER_PRESET_DELETION:
      case MENU_LABEL_SETTINGS:
      case MENU_LABEL_INPUT_SETTINGS:
      case MENU_LABEL_FRONTEND_COUNTERS:
      case MENU_LABEL_CORE_COUNTERS:
      case MENU_LABEL_INFO_SCREEN:
      case MENU_LABEL_SYSTEM_INFORMATION:
      case MENU_LABEL_CORE_INFORMATION:
      case MENU_LABEL_VIDEO_SHADER_PARAMETERS:
      case MENU_LABEL_DISK_CONTROL:
      case MENU_LABEL_CORE_OPTIONS:
      case MENU_LABEL_SHADER_OPTIONS:
      case MENU_LABEL_CORE_CHEAT_OPTIONS:
      case MENU_LABEL_INPUT_REMAPPING:
      case MENU_LABEL_OVERLAY_MOUSE_SETTINGS:
      case MENU_LABEL_OVERLAY_KEYBOARD_SETTINGS:
      case MENU_LABEL_OVERLAY_LIGHTGUN_SETTINGS:
      case MENU_LABEL_MENU_VISIBILITIES:
         cbs->action_get_title = action_get_title_action_generic;
         break;
      case MENU_LABEL_CORE_OPTION_CATEGORY:
         cbs->action_get_title = action_get_title_core_option;
         break;
      case MENU_LABEL_DEFERRED_CORE_UPDATER_LIST:
         cbs->action_get_title = action_get_title_core_updater;
         break;
      case MENU_LABEL_CUSTOM_VIEWPORT_2:
         cbs->action_get_title = action_get_title_custom_viewport;
         break;
      case MENU_LABEL_HELP:
         cbs->action_get_title = action_get_title_help;
         break;
      case MENU_LABEL_VIDEO_FONT_PATH:
         cbs->action_get_title = action_get_title_font_path;
         break;
      case MENU_LABEL_VIDEO_FILTER:
         cbs->action_get_title = action_get_title_video_filter;
         break;
      case MENU_LABEL_AUDIO_DSP_PLUGIN:
         cbs->action_get_title = action_get_title_audio_filter;
         break;
      case MENU_LABEL_CHEAT_DATABASE_PATH:
         cbs->action_get_title = action_get_title_cheat_directory;
         break;
      case MENU_LABEL_LIBRETRO_DIR_PATH:
         cbs->action_get_title = action_get_title_core_directory;
         break;
      case MENU_LABEL_LIBRETRO_INFO_PATH:
         cbs->action_get_title = action_get_title_core_info_directory;
         break;
      default:
         return -1;
   }

   return 0;
}

static int menu_cbs_init_bind_title_compare_type(menu_file_list_cbs_t *cbs,
      unsigned type)
{
   switch (type)
   {
      case MENU_SETTINGS_CUSTOM_VIEWPORT:
         cbs->action_get_title = action_get_title_custom_viewport;
         break;
      case MENU_SETTINGS:
         cbs->action_get_title = action_get_title_menu;
         break;
      case MENU_SETTINGS_CUSTOM_BIND:
      case MENU_SETTINGS_CUSTOM_BIND_KEYBOARD:
         cbs->action_get_title = action_get_title_waiting_for_input;
         break;
      case MENU_SETTING_ACTION_CORE_DISK_OPTIONS:
         cbs->action_get_title = action_get_title_action_generic;
         break;
      default:
         return -1;
   }

   return 0;
}

int menu_cbs_init_bind_title(menu_file_list_cbs_t *cbs,
      const char *path, const char *label, unsigned type, size_t idx,
      const char *elem0, const char *elem1,
      uint32_t label_hash, uint32_t menu_label_hash)
{
   if (!cbs)
      return -1;

   cbs->action_get_title = action_get_title_default;

   if (menu_cbs_init_bind_title_compare_label(cbs, label, label_hash, elem1) == 0)
      return 0;

   if (menu_cbs_init_bind_title_compare_type(cbs, type) == 0)
      return 0;

   return -1;
}
