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
#include "../menu_input.h"
#include "../menu_setting.h"
#include "../menu_shader.h"
#include "../menu_navigation.h"

#include "../../input/input_joypad_to_keyboard.h"

#include "../../general.h"
#include "../../retroarch.h"

extern unsigned input_remapping_scope;
extern bool input_remapping_touched;
extern int setting_action_left_libretro_device_type(
      void *data, bool wraparound);

#ifdef HAVE_SHADER_MANAGER
static void shader_action_parameter_left_common(
      struct video_shader_parameter *param,
      struct video_shader *shader)
{
   if (!shader)
      return;

   param->current -= param->step;
   param->current = min(max(param->minimum, param->current), param->maximum);
}

static void shader_action_parameter_l_common(
      struct video_shader_parameter *param,
      struct video_shader *shader)
{
   if (!shader)
      return;

   param->current = param->minimum;
}
#endif

static int shader_action_parameter_left(unsigned type, const char *label,
      bool wraparound)
{
#ifdef HAVE_SHADER_MANAGER
   struct video_shader *shader = video_shader_driver_get_current_shader();
   struct video_shader_parameter *param =
      &shader->parameters[type - MENU_SETTINGS_SHADER_PARAMETER_0];

   shader_action_parameter_left_common(param, shader);
#endif
   return 0;
}

static int shader_action_parameter_l(unsigned type, const char *label)
{
#ifdef HAVE_SHADER_MANAGER
   struct video_shader *shader = video_shader_driver_get_current_shader();
   struct video_shader_parameter *param =
      &shader->parameters[type - MENU_SETTINGS_SHADER_PARAMETER_0];

   shader_action_parameter_l_common(param, shader);
#endif
   return 0;
}

static int action_left_cheat(unsigned type, const char *label,
      bool wraparound)
{
   global_t *global       = global_get_ptr();
   cheat_manager_t *cheat = global->cheat;
   size_t idx             = type - MENU_SETTINGS_CHEAT_BEGIN;

   if (!cheat)
      return -1;

   cheat->cheats[idx].state = !cheat->cheats[idx].state;
   cheat_manager_update(cheat, idx);

   return 0;
}

static int action_left_input_desc(unsigned type, const char *label,
      bool wraparound)
{
   unsigned inp_desc_index_offset = type - MENU_SETTINGS_INPUT_DESC_BEGIN;
   unsigned inp_desc_user         = inp_desc_index_offset / (RARCH_FIRST_CUSTOM_BIND + 4);
   unsigned inp_desc_button_index_offset = inp_desc_index_offset - (inp_desc_user * (RARCH_FIRST_CUSTOM_BIND + 4));
   settings_t *settings = config_get_ptr();
   unsigned *mapped_id;

   if (label[0] == 'T')
      mapped_id = &settings->input.turbo_remap_id[inp_desc_user];
   else
      mapped_id = &settings->input.remap_ids[inp_desc_user][inp_desc_button_index_offset];

   (*mapped_id)--;

   /* Treat NO_BTN as leftmost value */
   if (*mapped_id > RARCH_FIRST_CUSTOM_BIND + 3)
      *mapped_id = NO_BTN;

   input_remapping_touched = true;
   return 0;
}

static int action_l_input_desc(unsigned type, const char *label)
{
   unsigned inp_desc_index_offset = type - MENU_SETTINGS_INPUT_DESC_BEGIN;
   unsigned inp_desc_user         = inp_desc_index_offset / (RARCH_FIRST_CUSTOM_BIND + 4);
   unsigned inp_desc_button_index_offset = inp_desc_index_offset - (inp_desc_user * (RARCH_FIRST_CUSTOM_BIND + 4));
   settings_t *settings = config_get_ptr();
   unsigned *mapped_id;

   if (label[0] == 'T')
      mapped_id = &settings->input.turbo_remap_id[inp_desc_user];
   else
      mapped_id = &settings->input.remap_ids[inp_desc_user][inp_desc_button_index_offset];

   /* Treat NO_BTN as leftmost value */
   *mapped_id = NO_BTN;

   input_remapping_touched = true;
   return 0;
}

static int action_left_joykbd_input_desc(unsigned type, const char *label,
      bool wraparound)
{
   unsigned joykbd_list_offset = type - MENU_SETTINGS_INPUT_JOYKBD_LIST_BEGIN;
   uint16_t joy_btn  = joykbd_bind_list[joykbd_list_offset].btn;
   enum retro_key rk = joykbd_bind_list[joykbd_list_offset].rk;

   /* Treat NO_BTN as leftmost value */
   if (joy_btn < NUM_JOYKBD_BTNS)
   {
      input_joykbd_remove_bind(rk, joy_btn);
      joy_btn--;
      input_joykbd_add_bind(rk, joy_btn);
   }

   input_remapping_touched = true;
   return 0;
}

static int action_l_joykbd_input_desc(unsigned type, const char *label)
{
   unsigned joykbd_list_offset = type - MENU_SETTINGS_INPUT_JOYKBD_LIST_BEGIN;
   uint16_t joy_btn  = joykbd_bind_list[joykbd_list_offset].btn;
   enum retro_key rk = joykbd_bind_list[joykbd_list_offset].rk;

  /* Treat NO_BTN as leftmost value */
   if (joy_btn < NUM_JOYKBD_BTNS)
      input_joykbd_remove_bind(rk, joy_btn);

   input_remapping_touched = true;
   return 0;
}

static int action_left_save_state(unsigned type, const char *label,
      bool wraparound)
{
   settings_t *settings = config_get_ptr();

   /* Slot -1 is (auto) slot. */
   if (settings->state_slot >= 0)
      settings->state_slot--;

   return 0;
}

static int action_l_save_state(unsigned type, const char *label)
{
   settings_t *settings = config_get_ptr();

   /* Slot -1 is (auto) slot. */
   if (settings->state_slot >= 9)
      settings->state_slot -= 10;
   else
      settings->state_slot = -1;

   return 0;
}

static int action_left_scroll(unsigned type, const char *label,
      bool wraparound)
{
   menu_navigation_t *nav = menu_navigation_get_ptr();
   menu_list_t *menu_list = menu_list_get_ptr();
   if (!nav || !menu_list)
      return -1;

   if (nav->selection_ptr > 8)
      menu_navigation_set(nav, nav->selection_ptr - 8, true);
   else
      menu_navigation_clear(nav, false);

   return 0;
}

static int action_left_mainmenu(unsigned type, const char *label,
      bool wraparound)
{
   menu_file_list_cbs_t *cbs = NULL;
   unsigned        push_list = 0;
   menu_list_t    *menu_list = menu_list_get_ptr();
   menu_handle_t       *menu = menu_driver_get_ptr();
   menu_navigation_t   *nav  = menu_navigation_get_ptr();
   unsigned           action = MENU_ACTION_LEFT;
   size_t          list_size = menu_driver_list_get_size(MENU_LIST_PLAIN);
   if (!menu)
      return -1;

   if (list_size == 1)
   {
      nav->selection_ptr = 0;
      if (menu_driver_list_get_selection() != 0)
         push_list = 1;
   }
   else
      push_list = 2;

   cbs = menu_list_get_actiondata_at_offset(menu_list->selection_buf,
         nav->selection_ptr);

   switch (push_list)
   {
      case 1:
         menu_driver_list_cache(MENU_LIST_HORIZONTAL, action);

         if (cbs && cbs->action_content_list_switch)
            return cbs->action_content_list_switch(
                  menu_list->selection_buf, menu_list->menu_stack,
                  "", "", 0);
         break;
      case 2:
         action_left_scroll(0, "", false);
         break;
      case 0:
      default:
         break;
   }

   return 0;
}

static int action_left_shader_scale_pass(unsigned type, const char *label,
      bool wraparound)
{
#ifdef HAVE_SHADER_MANAGER
   unsigned pass = type - MENU_SETTINGS_SHADER_PASS_SCALE_0;
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

   {
      unsigned current_scale   = shader_pass->fbo.scale_x;
      unsigned delta           = 5;
      current_scale            = (current_scale + delta) % 6;

      shader_pass->fbo.valid   = current_scale;
      shader_pass->fbo.scale_x = shader_pass->fbo.scale_y = current_scale;

   }
#endif
   return 0;
}

static int action_left_shader_filter_pass(unsigned type, const char *label,
      bool wraparound)
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

   {
      unsigned delta = 2;
      shader_pass->filter = ((shader_pass->filter + delta) % 3);

   }
#endif
   return 0;
}

static int action_left_shader_filter_default(unsigned type, const char *label,
      bool wraparound)
{
#ifdef HAVE_SHADER_MANAGER
   rarch_setting_t *setting = menu_setting_find("video_smooth");
   if (!setting)
      return -1;
   return menu_action_handle_setting(setting,
         setting->type, MENU_ACTION_LEFT, wraparound);
#else
   return 0;
#endif
}

static int action_left_cheat_num_passes(unsigned type, const char *label,
      bool wraparound)
{
   unsigned new_size = 0;
   global_t *global       = global_get_ptr();
   cheat_manager_t *cheat = global->cheat;

   if (!cheat)
      return -1;

   if (cheat->size)
      new_size = cheat->size - 1;
   menu_entries_set_refresh();
   cheat_manager_realloc(cheat, new_size);

   return 0;
}

static int action_l_cheat_num_passes(unsigned type, const char *label)
{
   global_t *global       = global_get_ptr();
   cheat_manager_t *cheat = global->cheat;

   if (!cheat)
      return -1;

   menu_entries_set_refresh();
   cheat_manager_realloc(cheat, 0);

   return 0;
}

static int action_left_shader_num_passes(unsigned type, const char *label,
      bool wraparound)
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
      shader->passes--;
   menu_entries_set_refresh();
   video_shader_resolve_parameters(NULL, menu->shader);

#endif
   return 0;
}

static int action_l_shader_num_passes(unsigned type, const char *label)
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

static int action_left_video_resolution(unsigned type, const char *label,
      bool wraparound)
{
   global_t *global = global_get_ptr();

   (void)global;

#if defined(__CELLOS_LV2__)
   if (global->console.screen.resolutions.current.idx)
   {
      global->console.screen.resolutions.current.idx--;
      global->console.screen.resolutions.current.id =
         global->console.screen.resolutions.list
         [global->console.screen.resolutions.current.idx];
   }
#else
   video_driver_get_video_output_prev();
#endif

   return 0;
}

static int core_setting_left(unsigned type, const char *label,
      bool wraparound)
{
   global_t *global = global_get_ptr();

   (void)label;

   core_option_prev(global->system.core_options, type);

   return 0;
}

static int core_setting_l(unsigned type, const char *label)
{
   global_t *global = global_get_ptr();

   (void)label;

   core_option_first(global->system.core_options, type);

   return 0;
}

static int disk_options_disk_idx_left(unsigned type, const char *label,
      bool wraparound)
{
   event_command(EVENT_CMD_DISK_PREV);

   return 0;
}

static int action_left_libretro_device_type(unsigned type, const char *label,
      bool wraparound)
{
   rarch_setting_t setting;
   setting.index_offset = type - MENU_SETTINGS_LIBRETRO_DEVICE_INDEX_BEGIN;

   menu_entries_set_refresh();

   return setting_action_left_libretro_device_type(&setting, wraparound);
}

static int action_left_remap_file_scope(unsigned type, const char *label,
      bool wraparound)
{
   if (input_remapping_scope > THIS_CORE)
   {
      input_remapping_scope -= 1;
      input_remapping_touched    = true;
   }

   return 0;
}

static int action_l_remap_file_scope(unsigned type, const char *label)
{
   if (input_remapping_scope != THIS_CORE)
   {
      input_remapping_scope = THIS_CORE;
      input_remapping_touched   = true;
   }
   return 0;
}

static int action_left_options_file_scope(unsigned type, const char *label,
      bool wraparound)
{
   if (core_options_scope > THIS_CORE)
   {
      core_options_scope   -= 1;
      core_options_touched  = true;
   }
   return 0;
}

static int action_l_options_file_scope(unsigned type, const char *label)
{
   if (core_options_scope > THIS_CORE)
   {
      core_options_scope   = THIS_CORE;
      core_options_touched = true;
   }
   return 0;
}

static int action_left_turbo_id(unsigned type, const char *label,
      bool wraparound)
{
   global_t *global     = global_get_ptr();
   settings_t *settings = config_get_ptr();
   unsigned *turbo_id   = &settings->input.turbo_id[type];

   menu_entries_set_refresh();
   input_remapping_touched = true;

   /* Find mappable id */
   while (--(*turbo_id) < RARCH_FIRST_CUSTOM_BIND)
   {
      if ((1 << *turbo_id) & TURBO_ID_MASK
            && global->system.input_desc_btn[type][*turbo_id])
         return 0;
   }

   /* Treat NO_BTN as leftmost value */
   *turbo_id = NO_BTN;
   return 0;
}

static int action_l_turbo_id(unsigned type, const char *label)
{
   settings_t *settings = config_get_ptr();

   menu_entries_set_refresh();
   input_remapping_touched = true;

   /* Treat NO_BTN as leftmost value */
   settings->input.turbo_id[type] = NO_BTN;
   return 0;
}

static int bind_left_generic(unsigned type, const char *label,
      bool wraparound)
{
   unsigned action = MENU_ACTION_LEFT;
   return menu_setting_set(type, label, action, wraparound);
}

static int bind_l_generic(unsigned type, const char *label)
{
   return menu_setting_set(type, label, MENU_ACTION_L, false);
}

static int menu_cbs_init_bind_left_compare_label(menu_file_list_cbs_t *cbs,
      const char *label, uint32_t label_hash, const char *elem0)
{
   unsigned i;
   rarch_setting_t *setting = menu_setting_find(label);

   if (setting)
   {
      uint32_t parent_group_hash = menu_hash_calculate(setting->parent_group);

      if ((parent_group_hash == MENU_VALUE_MAIN_MENU) && (setting->type == ST_GROUP))
      {
         cbs->action_left = action_left_scroll;
         return 0;
      }
   }

   if (!strncmp(label,"input_", 6))
   {
      for (i = 0; i < MAX_USERS; i++)
      {
         uint32_t label_setting_hash;
         char label_setting[NAME_MAX_LENGTH];

         label_setting[0] = '\0';
         snprintf(label_setting, sizeof(label_setting), "input_player%d_joypad_index", i + 1);

         label_setting_hash = menu_hash_calculate(label_setting);

         if (label_hash != label_setting_hash)
            continue;

         cbs->action_left = bind_left_generic;
         return 0;
      }
   }

   switch (label_hash)
   {
      case MENU_LABEL_SAVESTATE:
      case MENU_LABEL_LOADSTATE:
         cbs->action_left = action_left_save_state;
         cbs->action_l = action_l_save_state;
         break;
      case MENU_LABEL_VIDEO_SHADER_SCALE_PASS:
         cbs->action_left = action_left_shader_scale_pass;
         break;
      case MENU_LABEL_VIDEO_SHADER_FILTER_PASS:
         cbs->action_left = action_left_shader_filter_pass;
         break;
      case MENU_LABEL_VIDEO_SHADER_DEFAULT_FILTER:
         cbs->action_left = action_left_shader_filter_default;
         break;
      case MENU_LABEL_VIDEO_SHADER_NUM_PASSES:
         cbs->action_left = action_left_shader_num_passes;
         cbs->action_l = action_l_shader_num_passes;
         break;
      case MENU_LABEL_CHEAT_NUM_PASSES:
         cbs->action_left = action_left_cheat_num_passes;
         cbs->action_l = action_l_cheat_num_passes;
         break;
      case MENU_LABEL_INFO:
         cbs->action_left = action_left_scroll;
         break;
      case MENU_LABEL_OPTIONS_SCOPE:
         cbs->action_left = action_left_options_file_scope;
         cbs->action_l = action_l_options_file_scope;
         break;
      case MENU_LABEL_LIBRETRO_DEVICE_SCOPE:
      case MENU_LABEL_REMAPPING_SCOPE:
         cbs->action_left = action_left_remap_file_scope;
         cbs->action_l = action_l_remap_file_scope;
         break;
      case MENU_LABEL_INPUT_TURBO_ID:
         cbs->action_left = action_left_turbo_id;
         cbs->action_l = action_l_turbo_id;
         break;
      case MENU_LABEL_JOYPAD_TO_KEYBOARD_BIND:
         cbs->action_left = action_left_joykbd_input_desc;
         cbs->action_l = action_l_joykbd_input_desc;
         break;
      default:
         return -1;
   }

   return 0;
}

static int menu_cbs_init_bind_left_compare_type(menu_file_list_cbs_t *cbs,
      unsigned type, uint32_t menu_label_hash)
{
   if (type >= MENU_SETTINGS_SHADER_PARAMETER_0
         && type <= MENU_SETTINGS_SHADER_PARAMETER_LAST)
   {
      cbs->action_left = shader_action_parameter_left;
      cbs->action_l = shader_action_parameter_l;
   }
   else if (type >= MENU_SETTINGS_CHEAT_BEGIN
         && type <= MENU_SETTINGS_CHEAT_END)
      cbs->action_left = action_left_cheat;
   else if (type >= MENU_SETTINGS_INPUT_DESC_BEGIN
         && type <= MENU_SETTINGS_INPUT_DESC_END)
   {
      cbs->action_left = action_left_input_desc;
      cbs->action_l = action_l_input_desc;
   }
   else if (type == MENU_SETTINGS_VIDEO_RESOLUTION)
      cbs->action_left = action_left_video_resolution;
   else if (type >= MENU_SETTINGS_CORE_OPTION_START)
   {
      cbs->action_left = core_setting_left;
      cbs->action_l    = core_setting_l;
   }
   else if (type >= MENU_SETTINGS_LIBRETRO_DEVICE_INDEX_BEGIN
         && type <= MENU_SETTINGS_LIBRETRO_DEVICE_INDEX_END)
      cbs->action_left = action_left_libretro_device_type;
   else
   {
      switch (type)
      {
         case MENU_SETTINGS_CORE_DISK_OPTIONS_DISK_INDEX:
            cbs->action_left = disk_options_disk_idx_left;
            break;
         case MENU_FILE_PLAIN:
         case MENU_FILE_DIRECTORY:
         case MENU_FILE_CARCHIVE:
         case MENU_FILE_CORE:
         case MENU_FILE_SHADER:
         case MENU_FILE_SHADER_PRESET:
         case MENU_FILE_IMAGE:
         case MENU_FILE_THEME:
         case MENU_FILE_OVERLAY:
         case MENU_FILE_VIDEOFILTER:
         case MENU_FILE_AUDIOFILTER:
         case MENU_FILE_USE_DIRECTORY:
         case MENU_FILE_DOWNLOAD_CORE:
         case MENU_FILE_DOWNLOAD_CORE_INFO:
         case MENU_FILE_CHEAT:
         case MENU_FILE_CORE_OPTIONS:
         case MENU_FILE_REMAP:
         case MENU_SETTING_GROUP:
            switch (menu_label_hash)
            {
               case MENU_VALUE_HORIZONTAL_MENU:
               case MENU_VALUE_MAIN_MENU:
                  cbs->action_left = action_left_mainmenu;
                  break;
               default:
                  cbs->action_left = action_left_scroll;
                  break;
            }
            break;
         case MENU_SETTING_ACTION:
         case MENU_FILE_CONTENTLIST_ENTRY:
            cbs->action_left = action_left_mainmenu;
            break;
         default:
            return -1;
      }
   }

   return 0;
}

int menu_cbs_init_bind_left(menu_file_list_cbs_t *cbs,
      const char *path, const char *label, unsigned type, size_t idx,
      const char *elem0, const char *elem1, const char *menu_label,
      uint32_t label_hash, uint32_t menu_label_hash)
{
   if (!cbs)
      return -1;

   cbs->action_left = bind_left_generic;
   cbs->action_l = bind_l_generic;
   cbs->action_l2 = action_left_scroll;

   if (menu_cbs_init_bind_left_compare_label(cbs, label, label_hash, elem0) == 0)
      return 0;

   if (menu_cbs_init_bind_left_compare_type(cbs, type, menu_label_hash) == 0)
      return 0;

   return -1;
}
