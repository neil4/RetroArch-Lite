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

#include <compat/strl.h>
#include <file/file_path.h>
#include <retro_inline.h>

#include "../menu.h"
#include "../menu_display.h"
#include "../menu_hash.h"
#include "../menu_entry.h"
#include "../menu_setting.h"
#include "../menu_input.h"
#include "../menu_shader.h"
#include "../menu_navigation.h"
#include "../drivers/rgui.h"

#include "../../general.h"
#include "../../retroarch.h"
#include "../../input/input_common.h"
#include "../../input/input_autodetect.h"
#include "../../performance.h"

static int archive_open(void)
{
   char cat_path[PATH_MAX_LENGTH] = {0};
   menu_displaylist_info_t info   = {0};
   const char *menu_path          = NULL;
   const char *menu_label         = NULL;
   const char* path               = NULL;
   unsigned int type              = 0;
   size_t entry_idx               = 0;
   menu_navigation_t *nav         = menu_navigation_get_ptr();
   menu_list_t *menu_list         = menu_list_get_ptr();
   menu_handle_t *menu            = menu_driver_get_ptr();

   if (!menu_list || !nav)
      return -1;

   menu_list_pop_stack(menu_list);

   menu_list_get_last_stack(menu_list,
         &menu_path, &menu_label, NULL, NULL);

   if (menu_list_get_size(menu_list) == 0)
      return 0;

   menu_list_get_at_offset(menu_list->selection_buf,
         nav->selection_ptr, &path, NULL, &type, &entry_idx);

   fill_pathname_join(cat_path, menu_path, path, sizeof(cat_path));

   fill_pathname_join(menu->detect_content_path, menu_path, path,
         sizeof(menu->detect_content_path));

   info.list          = menu_list->menu_stack;
   info.type          = type;
   info.directory_ptr = nav->selection_ptr;
   strlcpy(info.path, cat_path, sizeof(info.path));
   strlcpy(info.label, menu_label, sizeof(info.label));

   return menu_displaylist_push_list(&info, DISPLAYLIST_GENERIC);
}

static int archive_load(void)
{
   int ret = 0;
   menu_displaylist_info_t info = {0};
   const char *menu_path  = NULL;
   const char *menu_label = NULL;
   const char* path       = NULL;
   size_t entry_idx       = 0;
   settings_t *settings   = config_get_ptr();
   global_t      *global  = global_get_ptr();
   size_t selected        = menu_navigation_get_current_selection();
   menu_handle_t *menu    = menu_driver_get_ptr();
   menu_list_t *menu_list = menu_list_get_ptr();

   if (!menu || !menu_list)
      return -1;

   menu_list_pop_stack(menu_list);

   menu_list_get_last_stack(menu_list, &menu_path, &menu_label, NULL, NULL);

   if (menu_list_get_size(menu_list) == 0)
      return 0;

   menu_list_get_at_offset(menu_list->selection_buf,
         selected, &path, NULL, NULL, &entry_idx);

   ret = rarch_defer_core(global->core_info, menu_path, path, menu_label,
         menu->deferred_path, sizeof(menu->deferred_path));

   fill_pathname_join(menu->detect_content_path, menu_path, path,
         sizeof(menu->detect_content_path));

   switch (ret)
   {
      case -1:
         event_command(EVENT_CMD_LOAD_CORE);
         menu_common_load_content(false);
         break;
      case 0:
         info.list          = menu_list->menu_stack;
         info.type          = 0;
         info.directory_ptr = selected;
         strlcpy(info.path, settings->libretro_directory, sizeof(info.path));
         strlcpy(info.label,
               menu_hash_to_str(MENU_LABEL_DEFERRED_CORE_LIST), sizeof(info.label));

         ret = menu_displaylist_push_list(&info, DISPLAYLIST_GENERIC);
         break;
   }

   return ret;
}

static int load_or_open_zip_iterate(char *s, size_t len, unsigned action)
{
   snprintf(s, len, "Opening compressed file\n"
         " \n"

         " - OK to open as Folder\n"
         " - Cancel/Back to Load \n");

   menu_driver_render_messagebox(s);

   switch (action)
   {
      case MENU_ACTION_OK:
         archive_open();
         break;
      case MENU_ACTION_CANCEL:
         archive_load();
         break;
   }

   return 0;
}

static int action_iterate_help(char *s, size_t len, const char *label)
{
   unsigned i;
   static const unsigned binds[] = {
      RETRO_DEVICE_ID_JOYPAD_UP,
      RETRO_DEVICE_ID_JOYPAD_DOWN,
      RETRO_DEVICE_ID_JOYPAD_A,
      RETRO_DEVICE_ID_JOYPAD_B,
      RETRO_DEVICE_ID_JOYPAD_SELECT,
      RARCH_MENU_TOGGLE,
      RARCH_QUIT_KEY,
      RETRO_DEVICE_ID_JOYPAD_X,
   };
   char desc[ARRAY_SIZE(binds)][64] = {{0}};
   settings_t *settings             = config_get_ptr();

   menu_driver_render();

   for (i = 0; i < ARRAY_SIZE(binds); i++)
   {
      const struct retro_keybind *keybind = (const struct retro_keybind*)
         &settings->input.binds[0][binds[i]];

      input_get_bind_string(desc[i], keybind, NULL, sizeof(desc[i]));
   }

   snprintf(s, len,
         "-- Welcome to RetroArch Lite --\n"
         " \n" // strtok_r doesn't split empty strings.

         "Basic Menu controls:\n"
         "    Scroll (Up): %-20s\n"
         "  Scroll (Down): %-20s\n"
         "      Accept/OK: %-20s\n"
         "           Back: %-20s\n"
         "           Info: %-20s\n"
         "Enter/Exit Menu: %-20s\n"
         " Exit RetroArch: %-20s\n"
         " \n"

         "To run content:\n"
         "Load a libretro core (Load Core).\n"
         "Load a content file (Load ROM).\n"
         " \n"
         "See Directory Settings to set paths\n"
         "for faster access to files.\n"
         " \n"

         "Press Accept/OK to continue.",
      desc[0], desc[1], desc[2], desc[3], desc[4], desc[5], desc[6]);

   return 0;
}

static int action_iterate_info(char *s, size_t len, const char *label)
{
   int ret = 0;
   char needle[NAME_MAX_LENGTH]     = {0};
   unsigned info_type               = 0;
   size_t   entry_idx               = 0;
   rarch_setting_t *current_setting = NULL;
   file_list_t *list                = NULL;
   menu_list_t *menu_list           = menu_list_get_ptr();
   size_t selection                 = menu_navigation_get_current_selection();
   const char *path;

   if (!menu_list)
      return 0;

   list = (file_list_t*)menu_list->selection_buf;

   menu_driver_render();

   current_setting = menu_setting_find(list->list[selection].label);

   if (current_setting)
      strlcpy(needle, current_setting->name, sizeof(needle));
   else
   {
      const char *lbl = NULL;
      menu_list_get_at_offset(list, selection, &path, &lbl,
            &info_type, &entry_idx);

      if (lbl)
         strlcpy(needle, lbl, sizeof(needle));
   }

   setting_get_description(needle, s, len, path, info_type, entry_idx);

   return ret;
}

static int action_iterate_load_open_zip(const char *label, char *s, size_t len, unsigned action)
{
   settings_t *settings   = config_get_ptr();

   switch (settings->archive.mode)
   {
      case 0:
         return load_or_open_zip_iterate(s, len, action);
      case 1:
         return archive_load();
      case 2:
         return archive_open();
      default:
         break;
   }

   return 0;
}

static int action_iterate_menu_viewport(char *s, size_t len, const char *label, unsigned action, uint32_t hash)
{
   int stride_x = 1, stride_y = 1;
   menu_displaylist_info_t info     = {0};
   struct retro_game_geometry *geom = NULL;
   const char *fmt                  = NULL;
   unsigned type                    = 0;
   video_viewport_t *custom         = video_viewport_get_custom();
   menu_display_t *disp             = menu_display_get_ptr();
   menu_navigation_t *nav           = menu_navigation_get_ptr();
   menu_list_t *menu_list           = menu_list_get_ptr();
   settings_t *settings             = config_get_ptr();
   struct retro_system_av_info *av_info = video_viewport_get_system_av_info();

   static video_viewport_t start_vp;

   if (!menu_list)
      return -1;

   menu_list_get_last_stack(menu_list, NULL, NULL, &type, NULL);

   geom = (struct retro_game_geometry*)&av_info->geometry;

   if (settings->video.scale_integer)
   {
      stride_x = geom->base_width;
      stride_y = geom->base_height;
   }

   switch (action)
   {
      case MENU_ACTION_UP:
         if (hash == MENU_LABEL_CUSTOM_VIEWPORT_1)
         {
            custom->y      -= stride_y;
            custom->height += stride_y;
         }
         else if (custom->height >= (unsigned)stride_y)
            custom->height -= stride_y;

         start_vp.width = 0;  /* flag start_vp reset */
         break;

      case MENU_ACTION_DOWN:
         if (hash == MENU_LABEL_CUSTOM_VIEWPORT_1)
         {
            custom->y += stride_y;
            if (custom->height >= (unsigned)stride_y)
               custom->height -= stride_y;
         }
         else
            custom->height += stride_y;

         start_vp.width = 0;
         break;

      case MENU_ACTION_LEFT:
         if (hash == MENU_LABEL_CUSTOM_VIEWPORT_1)
         {
            custom->x     -= stride_x;
            custom->width += stride_x;
         }
         else if (custom->width >= (unsigned)stride_x)
            custom->width -= stride_x;

         start_vp.width = 0;
         break;

      case MENU_ACTION_RIGHT:
         if (hash == MENU_LABEL_CUSTOM_VIEWPORT_1)
         {
            custom->x += stride_x;
            if (custom->width >= (unsigned)stride_x)
               custom->width -= stride_x;
         }
         else
            custom->width += stride_x;

         start_vp.width = 0;
         break;

      case MENU_ACTION_CANCEL:
         menu_list_pop_stack(menu_list);
         menu_entries_unset_refresh();

         if (hash == MENU_LABEL_CUSTOM_VIEWPORT_2)
         {
            info.list          = menu_list->menu_stack;
            info.type          = MENU_SETTINGS_CUSTOM_VIEWPORT;
            info.directory_ptr = nav->selection_ptr;
            strlcpy(info.label, "custom_viewport_1", sizeof(info.label));

            menu_displaylist_push_list(&info, DISPLAYLIST_INFO);
         }
         else
            start_vp.width = 0;
         break;

      case MENU_ACTION_OK:
         menu_list_pop_stack(menu_list);
         menu_entries_unset_refresh();

         if (hash == MENU_LABEL_CUSTOM_VIEWPORT_1
               && !settings->video.scale_integer)
         {
            info.list          = menu_list->menu_stack;
            info.type          = MENU_SETTINGS_CUSTOM_VIEWPORT;
            info.directory_ptr = nav->selection_ptr;
            strlcpy(info.label, "custom_viewport_2", sizeof(info.label));

            menu_displaylist_push_list(&info, DISPLAYLIST_INFO);
         }
         else
            start_vp.width = 0;
         break;

      case MENU_ACTION_START:  /* reset to default aspect */
         if (!settings->video.scale_integer)
         {
            video_viewport_t vp;
            video_driver_viewport_info(&vp);
            float default_aspect = aspectratio_lut[ASPECT_RATIO_CORE].value;

            custom->width   = vp.full_height * default_aspect;
            custom->height  = vp.full_height;
            custom->x       = (vp.full_width - custom->width) / 2;
            custom->y       = 0;

            start_vp = *custom;
         }
         break;

      case MENU_ACTION_L:  /* zoom out */
         if (!settings->video.scale_integer && custom->height > RGUI_HEIGHT)
         {
            if (!start_vp.width)
               start_vp = *custom;

            custom->height -= 2;
            custom->y      += 1;

            custom->width   = custom->height *
               (start_vp.width / (float)start_vp.height) + 0.5f;
            custom->x       = start_vp.x +
               ((start_vp.width - (float)custom->width) / 2.0f) + 0.5f;
         }
         break;
         
      case MENU_ACTION_R:  /* zoom in */
         if (!settings->video.scale_integer)
         {
            if (!start_vp.width)
               start_vp = *custom;

            custom->height += 2;
            custom->y      -= 1;

            custom->width   = custom->height *
               (start_vp.width / (float)start_vp.height) + 0.5f;
            custom->x       = start_vp.x +
               ((start_vp.width - (float)custom->width) / 2.0f) + 0.5f;
         }

         break;

      case MENU_ACTION_L2:  /* shift down or left */
         if (!settings->video.scale_integer)
         {
            if (hash == MENU_LABEL_CUSTOM_VIEWPORT_1)
            {
               custom->y  += 1;
               start_vp.y += 1;
            }
            else
            {
               custom->x  -= 1;
               start_vp.x -= 1;
            }
         }

         break;

      case MENU_ACTION_R2:  /* shift up or right */
         if (!settings->video.scale_integer)
         {
            if (hash == MENU_LABEL_CUSTOM_VIEWPORT_1)
            {
               custom->y  -= 1;
               start_vp.y -= 1;
            }
            else
            {
               custom->x  += 1;
               start_vp.x += 1;
            }
         }

         break;

      case MENU_ACTION_MESSAGE:
         if (disp)
            disp->msg_force = true;
         break;

      default:
         break;
   }

   menu_list_get_last_stack(menu_list, NULL, &label, &type, NULL);

   menu_driver_render();

   if (settings->video.scale_integer)
   {
      custom->x      = 0;
      custom->y      = 0;
      custom->width  = ((custom->width + geom->base_width - 1) /
            geom->base_width) * geom->base_width;
      custom->height = ((custom->height + geom->base_height - 1) /
            geom->base_height) * geom->base_height;
      fmt            = "Set scale (%ux%u, %u x %u scale)";
       
      snprintf(s, len, fmt,
            custom->width, custom->height,
            custom->width / geom->base_width,
            custom->height / geom->base_height);
   }
   else
   {
      if (hash == MENU_LABEL_CUSTOM_VIEWPORT_1)
         fmt = "Set Upper-Left Corner (%d, %d : %ux%u)\n"
               " D-Pad : Move Corner\n"
               " L / R : Zoom       \n"
               "L2 / R2: Shift Y    ";
      else
         fmt = "Set Bottom-Right Corner (%d, %d : %ux%u)\n"
               " D-Pad : Move Corner\n"
               " L / R : Zoom       \n"
               "L2 / R2: Shift X    ";

      snprintf(s, len, fmt,
            custom->x, custom->y, custom->width, custom->height);
   }

   menu_driver_render_messagebox(s);

   if (!custom->width)
      custom->width = stride_x;
   if (!custom->height)
      custom->height = stride_y;

   aspectratio_lut[ASPECT_RATIO_CUSTOM].value =
      (float)custom->width / custom->height;

   event_command(EVENT_CMD_VIDEO_APPLY_STATE_CHANGES);

   return 0;
}

static void delete_shader_preset(menu_list_t *menu_list)
{
   char file_path[PATH_MAX_LENGTH] = {0};
   char msg[NAME_MAX_LENGTH]       = {0};
   const char *menu_dir            = NULL;
   const char *menu_label          = NULL;
   settings_t *settings            = config_get_ptr();
   struct video_shader *shader     = video_shader_driver_get_current_shader();
   menu_entry_t entry;
   size_t selected;

   /* get dir */
   menu_list_get_last_stack(menu_list, &menu_dir, &menu_label, NULL, NULL);

   /* get filename */
   selected = menu_navigation_get_current_selection();
   selected = max(min(selected, menu_list_get_size(menu_list)-1), 0);
   menu_entry_get(&entry, selected, NULL, false);

   fill_pathname_join(file_path, menu_dir, entry.path, PATH_MAX_LENGTH);

   /* delete file */
   if (remove(file_path))
      rarch_main_msg_queue_push("Error deleting shader preset", 1, 100, true);
   else
   {
      snprintf(msg, NAME_MAX_LENGTH, "Deleted %s", path_basename(file_path));
      rarch_main_msg_queue_push(msg, 1, 100, true);
      menu_entries_set_refresh();

      if (!strcmp(file_path, settings->video.shader_path))
      {
         settings->video.shader_path[0] = '\0';
         scoped_settings_touched = true;
         settings_touched = true;

         if (shader)
            video_driver_set_shader(shader->type, NULL);
      }
   }
}

static void delete_core_file(menu_list_t *menu_list)
{
   global_t *global                = global_get_ptr();
   char core_name[NAME_MAX_LENGTH] = {0};
   char buf[PATH_MAX_LENGTH]       = {0};
   const char *menu_dir            = NULL;
   const char *menu_label          = NULL;
   menu_entry_t entry;
   size_t selected;
   
   /* get dir */
   menu_list_get_last_stack(menu_list, &menu_dir, &menu_label, NULL, NULL);
   
   /* get filename */
   selected = menu_navigation_get_current_selection();
   selected = max(min(selected, menu_list_get_size(menu_list)-1), 0);
   menu_entry_get(&entry, selected, NULL, false);
   
   fill_pathname_join(buf, menu_dir, entry.path, PATH_MAX_LENGTH);
   
   /* delete core */
   if (remove(buf))
      rarch_main_msg_queue_push("Error deleting core", 1, 100, true);
   else
   {
      if (!core_info_list_get_core_name(global->core_info,
            entry.path, core_name, PATH_MAX_LENGTH))
         strlcpy(core_name, entry.path, NAME_MAX_LENGTH);

      snprintf(buf, PATH_MAX_LENGTH, "Deleted %s", core_name);
      rarch_main_msg_queue_push(buf, 1, 100, true);

      menu_entries_set_refresh();
      event_command(EVENT_CMD_CORE_INFO_INIT);
   }
}

static bool menu_input_file_delete_hold(char *s, size_t len,
                                        menu_list_t *menu_list, char* type)
{
   settings_t *settings = config_get_ptr();
   int timeout          = 0;
   static int64_t end_time;
   
   if (!end_time)
      end_time = rarch_get_time_usec() + 1999999;
   timeout = (end_time - rarch_get_time_usec()) / 500000;
   
   if (input_driver_key_pressed(settings->menu_default_btn))
   {
      if (timeout > 0)
      {
         snprintf(s, len, "Hold for %d"
                          "\nto DELETE this %s.", timeout, type);
         menu_driver_render_messagebox(s);
         return false;
      }
      else
      {
         menu_list_pop_stack(menu_list);
         end_time = 0;
         return true;  /* trigger deletion */
      }
   }

   /* button released */
   menu_list_pop_stack(menu_list);
   end_time = 0;
   return false;
}

enum action_iterate_type
{
   ITERATE_TYPE_DEFAULT = 0,
   ITERATE_TYPE_HELP,
   ITERATE_TYPE_INFO,
   ITERATE_TYPE_ZIP,
   ITERATE_TYPE_MESSAGE,
   ITERATE_TYPE_VIEWPORT,
   ITERATE_TYPE_BIND,
   ITERATE_TYPE_CONFIRM_CORE_DELETE,
   ITERATE_TYPE_CONFIRM_SHADER_PRESET_DELETE,
};

static enum action_iterate_type action_iterate_type(uint32_t hash)
{
   switch (hash)
   {
      case MENU_LABEL_HELP:
         return ITERATE_TYPE_HELP;
      case MENU_LABEL_INFO_SCREEN:
         return ITERATE_TYPE_INFO;
      case MENU_LABEL_LOAD_OPEN_ZIP:
         return ITERATE_TYPE_ZIP;
      case MENU_LABEL_MESSAGE:
         return ITERATE_TYPE_MESSAGE;
      case MENU_LABEL_CUSTOM_VIEWPORT_1:
      case MENU_LABEL_CUSTOM_VIEWPORT_2:
         return ITERATE_TYPE_VIEWPORT;
      case MENU_LABEL_CUSTOM_BIND:
      case MENU_LABEL_CUSTOM_BIND_ALL:
      case MENU_LABEL_CUSTOM_BIND_DEFAULTS:
         return ITERATE_TYPE_BIND;
      case MENU_LABEL_CONFIRM_CORE_DELETION:
         return ITERATE_TYPE_CONFIRM_CORE_DELETE;
      case MENU_LABEL_CONFIRM_SHADER_PRESET_DELETION:
         return ITERATE_TYPE_CONFIRM_SHADER_PRESET_DELETE;
   }

   return ITERATE_TYPE_DEFAULT;
}

static int action_iterate_main(const char *label, unsigned action)
{
   menu_entry_t entry;
   static bool did_messagebox = false;
   char msg[PATH_MAX_LENGTH]      = {0};
   enum action_iterate_type iterate_type;
   size_t selected, *pop_selected = false;
   bool do_messagebox        = false;
   bool do_pop_stack         = false;
   bool do_post_iterate      = false;
   bool do_render            = false;
   int ret                   = 0;
   global_t *global          = global_get_ptr();
   menu_handle_t *menu       = menu_driver_get_ptr();
   menu_navigation_t *nav    = menu_navigation_get_ptr();
   menu_display_t *disp      = menu_display_get_ptr();
   menu_list_t *menu_list    = menu_list_get_ptr();
   uint32_t hash             = menu_hash_calculate(label);
   if (!menu || !menu_list)
      return 0;
   
   iterate_type              = action_iterate_type(hash);

   switch (iterate_type)
   {
      case ITERATE_TYPE_HELP:
         ret = action_iterate_help(msg, sizeof(msg), label);
         pop_selected    = NULL;
         do_messagebox   = true;
         do_pop_stack    = true;
         do_post_iterate = true;
         break;
      case ITERATE_TYPE_BIND:
         if (menu_input_bind_iterate(hash))
            menu_list_pop_stack(menu_list);
         break;
      case ITERATE_TYPE_VIEWPORT:
         ret = action_iterate_menu_viewport(msg, sizeof(msg), label, action, hash);
         break;
      case ITERATE_TYPE_INFO:
         ret = action_iterate_info(msg, sizeof(msg), label);
         pop_selected    = &nav->selection_ptr;
         do_messagebox   = true;
         do_pop_stack    = true;
         do_post_iterate = true;
         break;
      case ITERATE_TYPE_ZIP:
         ret = action_iterate_load_open_zip(label, msg, sizeof(msg), action);
         break;
      case ITERATE_TYPE_MESSAGE:
         strlcpy(msg, disp->message_contents, sizeof(msg));
         pop_selected    = &nav->selection_ptr;
         do_messagebox   = true;
         do_pop_stack    = true;
         break;
      case ITERATE_TYPE_CONFIRM_CORE_DELETE:
         if (menu_input_file_delete_hold(msg, sizeof(msg), menu_list, "core"))
            delete_core_file(menu_list);
         break;
      case ITERATE_TYPE_CONFIRM_SHADER_PRESET_DELETE:
         if (menu_input_file_delete_hold(msg, sizeof(msg), menu_list, "preset"))
            delete_shader_preset(menu_list);
         break;
      case ITERATE_TYPE_DEFAULT:
         selected = menu_navigation_get_current_selection();
         /* FIXME: Crappy hack, needed for mouse controls
          * to not be completely broken in case we press back.
          *
          * We need to fix this entire mess, mouse controls
          * should not rely on a hack like this in order to work. */
         selected = max(min(selected, menu_list_get_size(menu_list)-1), 0);

         menu_entry_get(&entry,    selected, NULL, false);
         ret = menu_entry_action(&entry, selected, (enum menu_action)action);

         if (ret)
            return ret;

         do_post_iterate = true;
         do_render       = true;

         /* Have to defer it so we let settings refresh. */
         if (menu->push_start_screen)
         {
            menu_displaylist_info_t info = {0};

            info.list = menu_list->menu_stack;
            strlcpy(info.label,
                  menu_hash_to_str(MENU_LABEL_HELP),
                  sizeof(info.label));

            menu_displaylist_push_list(&info, DISPLAYLIST_HELP);
         }
         break;
   }

   if (did_messagebox && !do_messagebox)
   {
      menu_display_fb_set_dirty();
      global->menu.block_push = false;
   }

   if (do_messagebox)
      menu_driver_render_messagebox(msg);

   if (do_pop_stack && (action == MENU_ACTION_OK
                        || action == MENU_ACTION_CANCEL))
      menu_list_pop(menu_list->menu_stack, pop_selected);
   
   if (do_post_iterate)
      menu_input_post_iterate(&ret, action);

   if (do_render)
      menu_driver_render();

   did_messagebox = do_messagebox;

   return ret;
}

int menu_cbs_init_bind_iterate(menu_file_list_cbs_t *cbs,
      const char *path, const char *label, unsigned type, size_t idx,
      const char *elem0, const char *elem1,
      uint32_t label_hash, uint32_t menu_label_hash)
{
   if (!cbs)
      return -1;

   cbs->action_iterate = action_iterate_main;

   return -1;
}
