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

#include <file/file_path.h>

#include "../menu.h"
#include "../menu_hash.h"
#include "../menu_displaylist.h"

#include "../../general.h"
#include "../../file_ext.h"
#include "../../gfx/video_shader_driver.h"

static int deferred_push_core_information(menu_displaylist_info_t *info)
{
   return menu_displaylist_push_list(info, DISPLAYLIST_CORE_INFO);
}

static int deferred_push_system_information(menu_displaylist_info_t *info)
{
   return menu_displaylist_push_list(info, DISPLAYLIST_SYSTEM_INFO);
}

static int deferred_push_core_list_deferred(menu_displaylist_info_t *info)
{
   return menu_displaylist_push_list(info, DISPLAYLIST_CORES_SUPPORTED);
}

static int deferred_push_performance_counters(menu_displaylist_info_t *info)
{
   return menu_displaylist_push_list(info, DISPLAYLIST_PERFCOUNTER_SELECTION);
}

static int deferred_push_video_shader_parameters(menu_displaylist_info_t *info)
{
   return menu_displaylist_push_list(info, DISPLAYLIST_SHADER_PARAMETERS);
}

static int deferred_push_settings(menu_displaylist_info_t *info)
{
   return menu_displaylist_push_list(info, DISPLAYLIST_SETTINGS_ALL);
}

static int deferred_push_category(menu_displaylist_info_t *info)
{
   info->flags = SL_FLAG_ALL_SETTINGS;

   return menu_displaylist_push_list(info, DISPLAYLIST_SETTINGS);
}

static int deferred_push_shader_options(menu_displaylist_info_t *info)
{
   return menu_displaylist_push_list(info, DISPLAYLIST_OPTIONS_SHADERS);
}

static int deferred_push_options(menu_displaylist_info_t *info)
{
   return menu_displaylist_push_list(info, DISPLAYLIST_CORE_OPTIONS);
}

static int deferred_push_core_counters(menu_displaylist_info_t *info)
{
   return menu_displaylist_push_list(info, DISPLAYLIST_PERFCOUNTERS_CORE);
}

static int deferred_push_frontend_counters(menu_displaylist_info_t *info)
{
   return menu_displaylist_push_list(info, DISPLAYLIST_PERFCOUNTERS_FRONTEND);
}

static int deferred_push_core_cheat_options(menu_displaylist_info_t *info)
{
   return menu_displaylist_push_list(info, DISPLAYLIST_OPTIONS_CHEATS);
}

static int deferred_push_core_input_remapping_options(menu_displaylist_info_t *info)
{
   return menu_displaylist_push_list(info, DISPLAYLIST_OPTIONS_REMAPPINGS);
}

static int deferred_push_core_options(menu_displaylist_info_t *info)
{
   return menu_displaylist_push_list(info, DISPLAYLIST_CORE_OPTIONS);
}

static int deferred_push_core_options_category(menu_displaylist_info_t *info)
{
   return menu_displaylist_push_list(info, DISPLAYLIST_CORE_OPTIONS_CATEGORY);
}

static int deferred_push_disk_options(menu_displaylist_info_t *info)
{
   return menu_displaylist_push_list(info, DISPLAYLIST_OPTIONS_DISK);
}

#ifdef HAVE_NETWORKING
/* HACK - we have to find some way to pass state inbetween
 * function pointer callback functions that don't necessarily
 * call each other. */
char *core_buf;
size_t core_len;

int cb_core_updater_list(void *data_, size_t len)
{
   char             *data = (char*)data_;

   menu_entries_unset_nonblocking_refresh();
   
   if (!data)
      return -1;

   if (core_buf)
      free(core_buf);

   core_buf = (char*)malloc((len+1) * sizeof(char));

   if (!core_buf)
      return -1;

   memcpy(core_buf, data, len * sizeof(char));
   core_buf[len] = '\0';
   core_len = len;

   return 0;
}

static int deferred_push_core_updater_list(menu_displaylist_info_t *info)
{
   global_t *global  = global_get_ptr();

   global->menu.block_push = false;
   return menu_displaylist_push_list(info, DISPLAYLIST_CORES_UPDATER);
}
#endif

int deferred_push_content_list(void *data, void *userdata, const char *path,
      const char *label, unsigned type)
{
   menu_list_t *menu_list = menu_list_get_ptr();
   if (!menu_list)
      return -1;
   return menu_displaylist_push((file_list_t*)data, menu_list->selection_buf);
}

static int deferred_push_core_list(menu_displaylist_info_t *info)
{
   info->type_default = MENU_FILE_PLAIN;
   strlcpy(info->exts, EXT_EXECUTABLES, sizeof(info->exts));

   return menu_displaylist_push_list(info, DISPLAYLIST_CORES);
}

static int deferred_push_configurations(menu_displaylist_info_t *info)
{
   info->type_default = MENU_FILE_CONFIG;
   strlcpy(info->exts, "cfg", sizeof(info->exts));

   return menu_displaylist_push_list(info, DISPLAYLIST_CONFIG_FILES);
}

static int deferred_push_video_shader_preset(menu_displaylist_info_t *info)
{
   info->type_default = MENU_FILE_SHADER_PRESET;
   strlcpy(info->exts, "cgp|glslp", sizeof(info->exts));

   return menu_displaylist_push_list(info, DISPLAYLIST_SHADER_PRESET);
}

static int deferred_push_video_shader_pass(menu_displaylist_info_t *info)
{
   info->type_default = MENU_FILE_SHADER;
   strlcpy(info->exts, "cg|glsl", sizeof(info->exts));

   return menu_displaylist_push_list(info, DISPLAYLIST_SHADER_PASS);
}

static int deferred_push_video_filter(menu_displaylist_info_t *info)
{
   info->type_default = MENU_FILE_VIDEOFILTER;
   strlcpy(info->exts, "filt", sizeof(info->exts));

   return menu_displaylist_push_list(info, DISPLAYLIST_VIDEO_FILTERS);
}

static int deferred_push_themes(menu_displaylist_info_t *info)
{
   info->type_default = MENU_FILE_THEME;
   strlcpy(info->exts, "cfg", sizeof(info->exts));

   return menu_displaylist_push_list(info, DISPLAYLIST_THEMES);
}

static int deferred_push_images(menu_displaylist_info_t *info)
{
   info->type_default = MENU_FILE_IMAGE;
   strlcpy(info->exts, "png", sizeof(info->exts));

   return menu_displaylist_push_list(info, DISPLAYLIST_IMAGES);
}

static int deferred_push_audio_dsp_plugin(menu_displaylist_info_t *info)
{
   info->type_default = MENU_FILE_AUDIOFILTER;
   strlcpy(info->exts, "dsp", sizeof(info->exts));

   return menu_displaylist_push_list(info, DISPLAYLIST_AUDIO_FILTERS);
}

static int deferred_push_cheat_file_load(menu_displaylist_info_t *info)
{
   info->type_default = MENU_FILE_CHEAT;
   strlcpy(info->exts, "cht", sizeof(info->exts));

   return menu_displaylist_push_list(info, DISPLAYLIST_CHEAT_FILES);
}

static int deferred_push_remap_file_load(menu_displaylist_info_t *info)
{
   info->type_default = MENU_FILE_REMAP;
   strlcpy(info->exts, "rmp", sizeof(info->exts));

   return menu_displaylist_push_list(info, DISPLAYLIST_REMAP_FILES);
}

static int deferred_push_options_file_load(menu_displaylist_info_t *info)
{
   info->type_default = MENU_FILE_CORE_OPTIONS;
   strlcpy(info->exts, "opt", sizeof(info->exts));

   return menu_displaylist_push_list(info, DISPLAYLIST_OPTIONS_FILES);
}

static int deferred_push_input_overlay(menu_displaylist_info_t *info)
{
   info->type_default = MENU_FILE_OVERLAY;
   strlcpy(info->exts, "cfg", sizeof(info->exts));

   return menu_displaylist_push_list(info, DISPLAYLIST_OVERLAYS);
}

static int deferred_push_input_osk_overlay(menu_displaylist_info_t *info)
{
   info->type_default = MENU_FILE_OVERLAY;
   strlcpy(info->exts, "cfg", sizeof(info->exts));

   return menu_displaylist_push_list(info, DISPLAYLIST_OVERLAYS);
}

static int deferred_push_video_font_path(menu_displaylist_info_t *info)
{
   info->type_default = MENU_FILE_FONT;
   strlcpy(info->exts, "ttf", sizeof(info->exts));

   return menu_displaylist_push_list(info, DISPLAYLIST_FONTS);
}

static int deferred_push_detect_core_list(menu_displaylist_info_t *info)
{
   global_t *global = global_get_ptr();

   info->type_default = MENU_FILE_PLAIN;
   if (global->core_info)
      strlcpy(info->exts, core_info_list_get_all_extensions(
         global->core_info), sizeof(info->exts));
   
   return menu_displaylist_push_list(info, DISPLAYLIST_CORES_DETECTED);
}

static int deferred_push_default(menu_displaylist_info_t *info)
{
   global_t *global         = global_get_ptr();

   info->type_default = MENU_FILE_PLAIN;
   info->setting      = menu_setting_find(info->label);

   if (info->setting && info->setting->browser_selection_type == ST_DIR) {}
   else if (global->menu.info.valid_extensions)
   {
      if (*global->menu.info.valid_extensions)
         snprintf(info->exts, sizeof(info->exts), "%s",
               global->menu.info.valid_extensions);
   }
   else
      strlcpy(info->exts, global->system.valid_extensions, sizeof(info->exts));

   return menu_displaylist_push_list(info, DISPLAYLIST_DEFAULT);
}

static int menu_cbs_init_bind_deferred_push_compare_label(menu_file_list_cbs_t *cbs, 
      const char *label, uint32_t label_hash)
{
   switch (label_hash)
   {
      case MENU_LABEL_DEFERRED_CORE_UPDATER_LIST:
#ifdef HAVE_NETWORKING
         cbs->action_deferred_push = deferred_push_core_updater_list;
#endif
         break;
      case MENU_LABEL_CHEAT_FILE_LOAD:
         cbs->action_deferred_push = deferred_push_cheat_file_load;
         break;
      case MENU_LABEL_OPTIONS_FILE_LOAD:
         cbs->action_deferred_push = deferred_push_options_file_load;
         break;
      case MENU_LABEL_REMAP_FILE_LOAD:
         cbs->action_deferred_push = deferred_push_remap_file_load;
         break;
      case MENU_LABEL_SHADER_OPTIONS:
         cbs->action_deferred_push = deferred_push_shader_options;
         break;
      case MENU_LABEL_OPTIONS:
         cbs->action_deferred_push = deferred_push_options;
         break;
      case MENU_LABEL_DEFERRED_CORE_LIST:
         cbs->action_deferred_push = deferred_push_core_list_deferred;
         break;
      case MENU_LABEL_DEFERRED_VIDEO_FILTER:
         cbs->action_deferred_push = deferred_push_video_filter;
         break;
      case MENU_LABEL_CORE_INFORMATION:
         cbs->action_deferred_push = deferred_push_core_information;
         break;
      case MENU_LABEL_SYSTEM_INFORMATION:
         cbs->action_deferred_push = deferred_push_system_information;
         break;
      case MENU_LABEL_PERFORMANCE_COUNTERS:
         cbs->action_deferred_push = deferred_push_performance_counters;
         break;
      case MENU_LABEL_CORE_COUNTERS:
         cbs->action_deferred_push = deferred_push_core_counters;
         break;
      case MENU_LABEL_VIDEO_SHADER_PARAMETERS:
         cbs->action_deferred_push = deferred_push_video_shader_parameters;
         break;
      case MENU_LABEL_SETTINGS:
         cbs->action_deferred_push = deferred_push_settings;
         break;
      case MENU_LABEL_FRONTEND_COUNTERS:
         cbs->action_deferred_push = deferred_push_frontend_counters;
         break;
      case MENU_LABEL_CORE_OPTIONS:
         cbs->action_deferred_push = deferred_push_core_options;
         break;
      case MENU_LABEL_CORE_OPTION_CATEGORY:
         cbs->action_deferred_push = deferred_push_core_options_category;
         break;
      case MENU_LABEL_CORE_CHEAT_OPTIONS:
         cbs->action_deferred_push = deferred_push_core_cheat_options;
         break;
      case MENU_LABEL_CORE_INPUT_REMAPPING_OPTIONS:
         cbs->action_deferred_push = deferred_push_core_input_remapping_options;
         break;
      case MENU_LABEL_DISK_OPTIONS:
         cbs->action_deferred_push = deferred_push_disk_options;
         break;
      case MENU_LABEL_CORE_LIST:
         cbs->action_deferred_push = deferred_push_core_list;
         break;
      case MENU_LABEL_CONFIGURATIONS:
         cbs->action_deferred_push = deferred_push_configurations;
         break;
      case MENU_LABEL_VIDEO_SHADER_PRESET:
         cbs->action_deferred_push = deferred_push_video_shader_preset;
         break;
      case MENU_LABEL_VIDEO_SHADER_PASS:
         cbs->action_deferred_push = deferred_push_video_shader_pass;
         break;
      case MENU_LABEL_VIDEO_FILTER:
         cbs->action_deferred_push = deferred_push_video_filter;
         break;
      case MENU_LABEL_MENU_THEME:
         cbs->action_deferred_push = deferred_push_themes;
         break;
      case MENU_LABEL_MENU_WALLPAPER:
         cbs->action_deferred_push = deferred_push_images;
         break;
      case MENU_LABEL_AUDIO_DSP_PLUGIN:
         cbs->action_deferred_push = deferred_push_audio_dsp_plugin;
         break;
      case MENU_LABEL_INPUT_OVERLAY:
         cbs->action_deferred_push = deferred_push_input_overlay;
         break;
      case MENU_LABEL_INPUT_OSK_OVERLAY:
         cbs->action_deferred_push = deferred_push_input_osk_overlay;
         break;
      case MENU_LABEL_VIDEO_FONT_PATH:
         cbs->action_deferred_push = deferred_push_video_font_path;
         break;
      case MENU_LABEL_DETECT_CORE_LIST:
         cbs->action_deferred_push = deferred_push_detect_core_list;
         break;
      default:
         return -1;
   }

   return 0;
}

static int menu_cbs_init_bind_deferred_push_compare_type(menu_file_list_cbs_t *cbs, unsigned type,
      uint32_t label_hash)
{
   if (type == MENU_SETTING_GROUP)
      cbs->action_deferred_push = deferred_push_category;
   else if (type == MENU_SETTING_ACTION_CORE_DISK_OPTIONS)
      cbs->action_deferred_push = deferred_push_disk_options;
   else
      return -1;

   return 0;
}

int menu_cbs_init_bind_deferred_push(menu_file_list_cbs_t *cbs,
      const char *path, const char *label, unsigned type, size_t idx,
      const char *elem0, const char *elem1,
      uint32_t label_hash, uint32_t menu_label_hash)
{
   if (!cbs)
      return -1;

   cbs->action_deferred_push = deferred_push_default;

   if (menu_cbs_init_bind_deferred_push_compare_label(cbs, label, label_hash) == 0)
      return 0;

   if (menu_cbs_init_bind_deferred_push_compare_type(cbs, type, label_hash) == 0)
      return 0;

   return -1;
}
