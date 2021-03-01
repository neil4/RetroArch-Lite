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

#include <rhash.h>

#include "../../libretro-common/include/file/file_path.h"
#include "../menu.h"
#include "../menu_cbs.h"
#include "../menu_input.h"
#include "../menu_setting.h"
#include "../menu_shader.h"
#include "../menu_hash.h"
#include "../menu_entry.h"

#include "../../general.h"
#include "../../retroarch.h"
#include "../../performance.h"

#include "../../input/input_remapping.h"
#include "../../input/input_joypad_to_keyboard.h"

extern int setting_action_start_libretro_device_type(void *data);

static int action_start_remap_file_load(unsigned type, const char *label)
{
   settings_t *settings = config_get_ptr();

   if (!settings)
      return -1;

   settings->input.remapping_path[0] = '\0';
   input_remapping_set_defaults();
   rarch_main_msg_queue_push("Default input map applied", 1, 100, true);
   return 0;
}

static int action_start_remap_file_delete(unsigned type, const char *label)
{
   char buf[PATH_MAX_LENGTH]       = {0};
   char fullpath[PATH_MAX_LENGTH]  = {0};
   global_t *global                = global_get_ptr();
   settings_t *settings            = config_get_ptr();
   char *scope                     = NULL;
   uint32_t hash                   = djb2_calculate(label);

   if (!global || !settings)
      return 0;

   fill_pathname_join(fullpath, settings->input_remapping_directory,
                      global->libretro_name, PATH_MAX_LENGTH);
   strlcat(fullpath, path_default_slash(), PATH_MAX_LENGTH);

   switch(hash)
   {
      case MENU_LABEL_REMAP_FILE_SAVE_GAME:
         strlcat(fullpath, path_basename(global->basename), PATH_MAX_LENGTH);
         scope = "ROM";
         break;

      case MENU_LABEL_REMAP_FILE_SAVE_DIR:
         if (!path_parent_dir_name(buf, global->basename))
            strcpy(buf, "root");
         strlcat(fullpath, buf, PATH_MAX_LENGTH);
         scope = "Directory";
         break;

      case MENU_LABEL_REMAP_FILE_SAVE_CORE:
         strlcat(fullpath, global->libretro_name, PATH_MAX_LENGTH);
         scope = "Core";
         break;

      default:
         return -1;
   }

   strlcat(fullpath, ".rmp", PATH_MAX_LENGTH);

   if (!path_file_exists(fullpath))
      snprintf(buf, PATH_MAX_LENGTH, "%s remap file does not exist.", scope);
   else if (remove(fullpath))
      snprintf(buf, PATH_MAX_LENGTH, "Error deleting %s remap file", scope);
   else
   {
      snprintf(buf, PATH_MAX_LENGTH, "Deleted %s remap file", scope);
      if (!strcmp(fullpath, settings->input.remapping_path))
         settings->input.remapping_path[0] = '\0';
   }

   rarch_main_msg_queue_push(buf, 1, 100, true);
   return 0;
}

static int action_start_options_file_delete_game(unsigned type, const char *label)
{
   char directory[PATH_MAX_LENGTH] = {0};
   char abs_path[PATH_MAX_LENGTH]  = {0};
   global_t *global                = global_get_ptr();
   settings_t *settings            = config_get_ptr();
   const char *core_name           = global ? global->libretro_name
                                              : NULL;
   const char *game_name           = global ? path_basename(global->basename)
                                              : NULL;
   char *opt_path;
   
   if (!global || !settings)
      return 0;

   fill_pathname_join(directory, settings->menu_config_directory,
                      core_name, PATH_MAX_LENGTH);
   fill_pathname_join(abs_path, directory, game_name, PATH_MAX_LENGTH);
   strlcat(abs_path, ".opt", PATH_MAX_LENGTH);

   if(!path_file_exists(abs_path))
   {
      rarch_main_msg_queue_push("ROM Options file does not exist", 1, 100, true);
      return 0;
   }
   
   if (remove(abs_path))
      rarch_main_msg_queue_push("Error deleting ROM Options file", 1, 100, true);
   else
      rarch_main_msg_queue_push("Deleted ROM Options file", 1, 100, true);
   
   opt_path = core_option_conf_path(global->system.core_options);
   core_option_get_core_conf_path(opt_path);
   
   core_options_conf_reload(global->system.core_options);
   options_touched = true;
   
   return 0;
}

static int action_start_shader_preset(unsigned type, const char *label)
{
   settings_t *settings = config_get_ptr();
   struct video_shader *shader = video_shader_driver_get_current_shader();

   if (!settings)
      return -1;

   settings->video.shader_path[0] = '\0';
   scoped_settings_touched = true;
   settings_touched = true;

   if (shader)
      video_driver_set_shader(shader->type, NULL);

   return 0;
}

static int action_start_shader_preset_delete(unsigned type, const char *label)
{
   menu_displaylist_info_t info = {0};
   int ret                  = 0;
   menu_list_t   *menu_list = menu_list_get_ptr();
   menu_navigation_t *nav   = menu_navigation_get_ptr();

   if (!menu_list)
      return -1;
   
   info.list          = menu_list->menu_stack;
   info.directory_ptr = nav->selection_ptr;
   strlcpy(info.label, "confirm_shader_preset_deletion", sizeof(info.label));

   ret = menu_displaylist_push_list(&info, DISPLAYLIST_INFO);

   return ret;
}

static int action_start_performance_counters_core(unsigned type, const char *label)
{
   struct retro_perf_counter **counters = (struct retro_perf_counter**)
      perf_counters_libretro;
   unsigned offset = type - MENU_SETTINGS_LIBRETRO_PERF_COUNTERS_BEGIN;

   (void)label;

   if (counters[offset])
   {
      counters[offset]->total = 0;
      counters[offset]->call_cnt = 0;
   }

   return 0;
}

static int action_start_input_desc(unsigned type, const char *label)
{
   settings_t           *settings = config_get_ptr();
   unsigned inp_desc_index_offset = type - MENU_SETTINGS_INPUT_DESC_BEGIN;
   unsigned inp_desc_user         = inp_desc_index_offset / (RARCH_FIRST_CUSTOM_BIND + 4);
   unsigned inp_desc_button_index_offset = inp_desc_index_offset - (inp_desc_user * (RARCH_FIRST_CUSTOM_BIND + 4));

   (void)label;

   if (inp_desc_button_index_offset < RARCH_FIRST_CUSTOM_BIND)
      settings->input.remap_ids[inp_desc_user][inp_desc_button_index_offset] =
         settings->input.binds[inp_desc_user][inp_desc_button_index_offset].id;
   else
      settings->input.remap_ids[inp_desc_user][inp_desc_button_index_offset] =
         inp_desc_button_index_offset - RARCH_FIRST_CUSTOM_BIND;

   return 0;
}

static int action_start_joykey_input_desc(unsigned type, const char *label)
{
   unsigned joykey_list_offset = type - MENU_SETTINGS_INPUT_JOYKBD_LIST_BEGIN;
   uint16_t joy_btn  = joykbd_bind_list[joykey_list_offset].btn;
   enum retro_key rk = joykbd_bind_list[joykey_list_offset].rk;

   (void)label;

   if (joy_btn < RARCH_FIRST_CUSTOM_BIND)
      input_joykbd_remove_bind(rk, joy_btn);

   return 0;
}

static int action_start_shader_action_parameter(unsigned type, const char *label)
{
#ifdef HAVE_SHADER_MANAGER
   struct video_shader_parameter *param = NULL;
   struct video_shader *shader = video_shader_driver_get_current_shader();

   if (!shader)
      return 0;

   param = &shader->parameters[type - MENU_SETTINGS_SHADER_PARAMETER_0];
   param->current = param->initial;
   param->current = min(max(param->minimum, param->current), param->maximum);

#endif

   return 0;
}

static int action_start_shader_pass(unsigned type, const char *label)
{
#ifdef HAVE_SHADER_MANAGER
   struct video_shader *shader = NULL;
   struct video_shader_pass *shader_pass = NULL;
   menu_handle_t *menu = menu_driver_get_ptr();
   if (!menu)
      return -1;

   shader = menu->shader;
   menu->shader->pass_idx = type - MENU_SETTINGS_SHADER_PASS_0;

   if (shader)
      shader_pass = &shader->pass[menu->shader->pass_idx];

   if (shader_pass)
      *shader_pass->source.path = '\0';
#endif

   return 0;
}


static int action_start_shader_scale_pass(unsigned type, const char *label)
{
#ifdef HAVE_SHADER_MANAGER
   struct video_shader *shader = NULL;
   struct video_shader_pass *shader_pass = NULL;
   unsigned pass = type - MENU_SETTINGS_SHADER_PASS_SCALE_0;
   menu_handle_t *menu = menu_driver_get_ptr();
   if (!menu)
      return -1;

   shader      = menu->shader;

   if (shader)
      shader_pass = &shader->pass[pass];

   if (shader_pass)
   {
      shader_pass->fbo.scale_x = shader_pass->fbo.scale_y = 0;
      shader_pass->fbo.valid = false;
   }
#endif

   return 0;
}

static int action_start_shader_filter_pass(unsigned type, const char *label)
{
#ifdef HAVE_SHADER_MANAGER
   unsigned pass = type - MENU_SETTINGS_SHADER_PASS_FILTER_0;
   struct video_shader *shader = NULL;
   struct video_shader_pass *shader_pass = NULL;
   menu_handle_t *menu = menu_driver_get_ptr();
   if (!menu)
      return -1;

   shader = menu->shader;
   if (!shader)
      return -1;
   shader_pass = &shader->pass[pass];
   if (!shader_pass)
      return -1;

   shader_pass->filter = RARCH_FILTER_UNSPEC;
#endif

   return 0;
}

static int action_start_shader_num_passes(unsigned type, const char *label)
{
#ifdef HAVE_SHADER_MANAGER
   struct video_shader *shader = NULL;
   menu_handle_t *menu = menu_driver_get_ptr();
   if (!menu)
      return -1;

   shader = menu->shader;
   if (!shader)
      return -1;
   if (shader->passes)
      shader->passes = 0;

   menu_entries_set_refresh();
   video_shader_resolve_parameters(NULL, menu->shader);
#endif
   return 0;
}

static int action_start_cheat_num_passes(unsigned type, const char *label)
{
   global_t *global       = global_get_ptr();
   cheat_manager_t *cheat = global->cheat;

   if (!cheat)
      return -1;

   if (cheat->size)
   {
      menu_entries_set_refresh();
      cheat_manager_realloc(cheat, 0);
   }

   return 0;
}

static int action_start_performance_counters_frontend(unsigned type,
      const char *label)
{
   struct retro_perf_counter **counters = (struct retro_perf_counter**)
      perf_counters_rarch;
   unsigned offset = type - MENU_SETTINGS_PERF_COUNTERS_BEGIN;

   (void)label;

   if (counters[offset])
   {
      counters[offset]->total = 0;
      counters[offset]->call_cnt = 0;
   }

   return 0;
}

static int action_start_core_setting(unsigned type,
      const char *label)
{
   global_t *global       = global_get_ptr();

   (void)label;

   core_option_set_default(global->system.core_options, type);

   return 0;
}

static int action_start_core_delete(unsigned type, const char *label)
{
   menu_displaylist_info_t info = {0};
   int ret                  = 0;
   menu_list_t   *menu_list = menu_list_get_ptr();
   menu_navigation_t *nav   = menu_navigation_get_ptr();

   if (!menu_list)
      return -1;
   
   info.list          = menu_list->menu_stack;
   info.directory_ptr = nav->selection_ptr;
   strlcpy(info.label, "confirm_core_deletion", sizeof(info.label));

   ret = menu_displaylist_push_list(&info, DISPLAYLIST_INFO);

   return ret;
}

static int action_start_lookup_setting(unsigned type, const char *label)
{
   return menu_setting_set(type, label, MENU_ACTION_START, false);
}

static int action_start_libretro_device_type(unsigned type, const char *label)
{
   rarch_setting_t setting;
   setting.index_offset = type - MENU_SETTINGS_LIBRETRO_DEVICE_INDEX_BEGIN;

   menu_entries_set_refresh();
   scoped_settings_touched = true;
   settings_touched = true;

   return setting_action_start_libretro_device_type(&setting);
}

static int action_start_libretro_device_scope(unsigned type, const char *label)
{
   settings_t *settings = config_get_ptr();

   settings->input.libretro_device_scope = THIS_CORE;
   return 0;
}

int menu_cbs_init_bind_start_compare_label(menu_file_list_cbs_t *cbs,
      uint32_t hash)
{
   switch (hash)
   {
      case MENU_LABEL_REMAP_FILE_LOAD:
         cbs->action_start = action_start_remap_file_load;
         break;
      case MENU_LABEL_REMAP_FILE_SAVE_CORE:
      case MENU_LABEL_REMAP_FILE_SAVE_DIR:
      case MENU_LABEL_REMAP_FILE_SAVE_GAME:
         cbs->action_start = action_start_remap_file_delete;
         break;
      case MENU_LABEL_OPTIONS_FILE_SAVE_GAME:
         cbs->action_start = action_start_options_file_delete_game;
         break;
      case MENU_LABEL_VIDEO_SHADER_PRESET:
         cbs->action_start = action_start_shader_preset;
         break;
      case MENU_LABEL_VIDEO_SHADER_PASS:
         cbs->action_start = action_start_shader_pass;
         break;
      case MENU_LABEL_VIDEO_SHADER_SCALE_PASS:
         cbs->action_start = action_start_shader_scale_pass;
         break;
      case MENU_LABEL_VIDEO_SHADER_FILTER_PASS:
         cbs->action_start = action_start_shader_filter_pass;
         break;
      case MENU_LABEL_VIDEO_SHADER_NUM_PASSES:
         cbs->action_start = action_start_shader_num_passes;
         break;
      case MENU_LABEL_CHEAT_NUM_PASSES:
         cbs->action_start = action_start_cheat_num_passes;
         break;
      case MENU_LABEL_LIBRETRO_DEVICE_SCOPE:
         cbs->action_start = action_start_libretro_device_scope;
         break;
      default:
         return -1;
   }

   return 0;
}

static int menu_cbs_init_bind_start_compare_type(menu_file_list_cbs_t *cbs,
      unsigned type)
{
   if (type == MENU_FILE_CORE)
      cbs->action_start = action_start_core_delete;
   else if (type == MENU_FILE_SHADER_PRESET)
         cbs->action_start = action_start_shader_preset_delete;
   else if (type >= MENU_SETTINGS_SHADER_PARAMETER_0
         && type <= MENU_SETTINGS_SHADER_PARAMETER_LAST)
      cbs->action_start = action_start_shader_action_parameter;
   else if (type >= MENU_SETTINGS_LIBRETRO_PERF_COUNTERS_BEGIN &&
         type <= MENU_SETTINGS_LIBRETRO_PERF_COUNTERS_END)
      cbs->action_start = action_start_performance_counters_core;
   else if (type >= MENU_SETTINGS_INPUT_DESC_BEGIN
         && type <= MENU_SETTINGS_INPUT_DESC_END)
      cbs->action_start = action_start_input_desc;
   else if (type >= MENU_SETTINGS_PERF_COUNTERS_BEGIN &&
         type <= MENU_SETTINGS_PERF_COUNTERS_END)
      cbs->action_start = action_start_performance_counters_frontend;
   else if ((type >= MENU_SETTINGS_CORE_OPTION_START))
      cbs->action_start = action_start_core_setting;
   else if (type >= MENU_SETTINGS_LIBRETRO_DEVICE_INDEX_BEGIN
         && type <= MENU_SETTINGS_LIBRETRO_DEVICE_INDEX_END)
      cbs->action_start = action_start_libretro_device_type;
   else if (type >= MENU_SETTINGS_INPUT_JOYKBD_LIST_BEGIN
         && type <= MENU_SETTINGS_INPUT_JOYKBD_LIST_END)
      cbs->action_start = action_start_joykey_input_desc;
   else
      return -1;

   return 0;
}

int menu_cbs_init_bind_start(menu_file_list_cbs_t *cbs,
      const char *path, const char *label, unsigned type, size_t idx,
      const char *elem0, const char *elem1,
      uint32_t label_hash, uint32_t menu_label_hash)
{
   if (!cbs)
      return -1;

   cbs->action_start = action_start_lookup_setting;
   
   if (menu_cbs_init_bind_start_compare_label(cbs, label_hash) == 0)
      return 0;

   if (menu_cbs_init_bind_start_compare_type(cbs, type) == 0)
      return 0;

   return -1;
}
