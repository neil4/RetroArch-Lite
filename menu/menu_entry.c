/*  RetroArch - A frontend for libretro.
 *  Copyright (C) 2014-2015 - Jay McCarthy
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
#include <string/string_list.h>

#include "menu.h"
#include "menu_display.h"
#include "menu_entry.h"
#include "menu_navigation.h"
#include "menu_setting.h"
#include "menu_input.h"
#include "../runloop_data.h"
#include "../runloop.h"

/* This file provides an abstraction of the currently displayed
 * menu.
 *
 * It is organized into an event-based system where the UI companion
 * calls this functions and RetroArch responds by changing the global
 * state (including arranging for these functions to return different
 * values).
 *
 * Its only interaction back to the UI is to arrange for
 * notify_list_loaded on the UI companion.
 */

/* Clicks the back button */
int menu_entry_go_back(void)
{
   menu_list_t *menu_list = menu_list_get_ptr();
   if (!menu_list)
      return -1;

   menu_setting_apply_deferred();
   menu_list_pop_stack(menu_list);

   if (menu_entries_needs_refresh())
      menu_entries_refresh(MENU_ACTION_CANCEL);

   rarch_main_data_iterate();

   return 0;
}

int menu_entry_pathdir_set_value(uint32_t i, const char *s)
{
   const char *menu_label   = NULL;
   const char *menu_path    = NULL;
   rarch_setting_t *setting = NULL;
   menu_list_t *menu_list   = menu_list_get_ptr();

   menu_list_get_last_stack(menu_list,
         &menu_path, &menu_label, NULL, NULL);

   setting = menu_setting_find(menu_label);

   if (!setting)
      return -1;

   if (setting->type != ST_DIR)
      return -1;

   (void)s;
   setting_set_with_string_representation(setting, menu_path);

   menu_setting_generic(setting, false);

   menu_list_pop_stack_by_needle(menu_list, setting->name);

   return 0;
}

static inline void menu_entry_clear(menu_entry_t *entry)
{
   entry->path[0] = '\0';
   entry->label[0] = '\0';
   entry->value[0] = '\0';
   entry->entry_idx = 0;
   entry->idx = 0;
   entry->type = 0;
   entry->spacing = 0;
}

void menu_entry_get(menu_entry_t *entry, size_t i,
      void *userdata, bool use_representation)
{
   const char *label         = NULL;
   const char *path          = NULL;
   const char *entry_label   = NULL;
   menu_file_list_cbs_t *cbs = NULL;
   file_list_t *list         = NULL;
   menu_list_t *menu_list    = menu_list_get_ptr();

   menu_entry_clear(entry);

   if (!menu_list)
      return;

   menu_list_get_last_stack(menu_list, NULL, &label, NULL, NULL);
   
   list = userdata ? (file_list_t*)userdata : menu_list->selection_buf;

   if (!list)
      return;

   menu_list_get_at_offset(list, i, &path, &entry_label, &entry->type,
         &entry->entry_idx);

   cbs = menu_list_get_actiondata_at_offset(list, i);

   if (cbs && cbs->action_get_value && use_representation)
      cbs->action_get_value(list,
            &entry->spacing, entry->type, i, label,
            entry->value,  sizeof(entry->value), 
            entry_label, path,
            entry->path, sizeof(entry->path));

   entry->idx         = i;

   if (path && !use_representation)
      strlcpy(entry->path,  path,        sizeof(entry->path));
   if (entry_label)
      strlcpy(entry->label, entry_label, sizeof(entry->label));
}

bool menu_entry_is_currently_selected(unsigned id)
{
   menu_navigation_t *nav = menu_navigation_get_ptr();
   if (!nav)
      return false;
   return (id == nav->selection_ptr);
}

/* Performs whatever actions are associated with menu entry 'i'.
 *
 * This is the most important function because it does all the work
 * associated with clicking on things in the UI.
 *
 * This includes loading cores and updating the 
 * currently displayed menu. */
int menu_entry_select(uint32_t i)
{
   menu_entry_t entry;
   menu_navigation_t *nav = menu_navigation_get_ptr();
    
   nav->selection_ptr = i;
   menu_entry_get(&entry, i, NULL, false);

   return menu_entry_action(&entry, i, MENU_ACTION_SELECT);
}

int menu_entry_iterate(unsigned action)
{
   const char *label         = NULL;
   menu_file_list_cbs_t *cbs = NULL;
   menu_list_t *menu_list    = menu_list_get_ptr();

   if (!menu_list)
      return -1;

   if (action != MENU_ACTION_NOOP || menu_entries_needs_refresh() || menu_display_update_pending())
      menu_display_fb_set_dirty();

   cbs = (menu_file_list_cbs_t*)menu_list_get_last_stack_actiondata(menu_list);

   menu_list_get_last_stack(menu_list, NULL, &label, NULL, NULL);

   if (cbs && cbs->action_iterate)
      return cbs->action_iterate(label, action);

   return -1;
}

int menu_entry_action(menu_entry_t *entry, unsigned i, enum menu_action action)
{
   int ret                   = 0;
   global_t *global          = global_get_ptr();
   menu_input_t *menu_input  = menu_input_get_ptr();
   menu_display_t *disp      = menu_display_get_ptr();
   menu_list_t *menu_list    = menu_list_get_ptr();
   menu_file_list_cbs_t *cbs = menu_list_get_actiondata_at_offset(menu_list->selection_buf, i);

   switch (action)
   {
      case MENU_ACTION_UP:
         if (cbs && cbs->action_up)
            ret = cbs->action_up(entry->type, entry->label);
         break;
      case MENU_ACTION_DOWN:
         if (cbs && cbs->action_down)
            ret = cbs->action_down(entry->type, entry->label);
         break;

      case MENU_ACTION_CANCEL:
         if (cbs && cbs->action_cancel)
         {
            ret = cbs->action_cancel(entry->path, entry->label, entry->type, i);
            global->menu.block_push = false;
         }
         break;

      case MENU_ACTION_OK:
         if (cbs && cbs->action_ok && !global->menu.block_push)
            ret = cbs->action_ok(entry->path, entry->label, entry->type, i, entry->entry_idx);
         break;
      case MENU_ACTION_START:
         if (cbs && cbs->action_start)
            ret = cbs->action_start(entry->type, entry->label);
         break;
      case MENU_ACTION_LEFT:
         if (cbs && cbs->action_left)
            ret = cbs->action_left(entry->type, entry->label, false);
         break;
      case MENU_ACTION_RIGHT:
         if (cbs && cbs->action_right)
            ret = cbs->action_right(entry->type, entry->label, false);
         break;
      case MENU_ACTION_L:
         if (cbs && cbs->action_l)
            ret = cbs->action_l(entry->type, entry->label);
         break;
      case MENU_ACTION_R:
         if (cbs && cbs->action_r)
            ret = cbs->action_r(entry->type, entry->label);
         break;
      case MENU_ACTION_L2:
         if (cbs && cbs->action_l2)
            ret = cbs->action_l2(entry->type, entry->label, false);
         break;
      case MENU_ACTION_R2:
         if (cbs && cbs->action_r2)
            ret = cbs->action_r2(entry->type, entry->label, false);
         break;
      case MENU_ACTION_INFO:
         if (cbs && cbs->action_info)
            ret = cbs->action_info(entry->type, entry->label);
         break;
      case MENU_ACTION_SELECT:
         if (cbs && cbs->action_select)
            ret = cbs->action_select(entry->path, entry->label, entry->type, i);
         break;

      case MENU_ACTION_REFRESH:
         if (cbs && cbs->action_refresh)
         {
            ret = cbs->action_refresh(menu_list->selection_buf, menu_list->menu_stack);
            menu_entries_unset_refresh();
         }
         break;

      case MENU_ACTION_MESSAGE:
         if (disp)
            disp->msg_force = true;
         break;

      default:
         break;
   }

   menu_input->last_action = action;

   return ret;
}
