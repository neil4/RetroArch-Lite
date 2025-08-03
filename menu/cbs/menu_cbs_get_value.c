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
#include "../menu_cbs.h"
#include "../menu_shader.h"
#include "../menu_hash.h"

#include "../../input/input_common.h"
#include "../../general.h"
#include "../../performance.h"
#include "../../intl/intl.h"

extern unsigned input_remapping_scope;
extern void setting_get_string_representation_uint_libretro_device(void *data,
      char *s, size_t len);

const char axis_labels[4][16] = {
   RETRO_LBL_ANALOG_LEFT_X,
   RETRO_LBL_ANALOG_LEFT_Y,
   RETRO_LBL_ANALOG_RIGHT_X,
   RETRO_LBL_ANALOG_RIGHT_Y
};

static void menu_action_setting_disp_set_label_cheat_num_passes(
      file_list_t* list,
      unsigned *w, unsigned type, unsigned i,
      const char *label,
      char *s, size_t len,
      const char *entry_label,
      const char *path,
      char *s2, size_t len2)
{
   global_t *global = global_get_ptr();

   *w = MENU_NARROW_ENTRY_SPACING;
   strlcpy(s2, path, len2);
   snprintf(s, len, "%u", global->cheat->buf_size);
}

static void menu_action_setting_disp_set_label_core_options_scope(
      file_list_t* list,
      unsigned *w, unsigned type, unsigned i,
      const char *label,
      char *s, size_t len,
      const char *entry_label,
      const char *path,
      char *s2, size_t len2)
{
   *w = MENU_DEFAULT_ENTRY_SPACING;
   strlcpy(s2, path, len2);
   strlcpy(s, scope_lut[core_options_scope].name, len);
}

static void menu_action_setting_disp_set_label_shader_filter_pass(
      file_list_t* list,
      unsigned *w, unsigned type, unsigned i,
      const char *label,
      char *s, size_t len,
      const char *entry_label,
      const char *path,
      char *s2, size_t len2)
{
   unsigned pass = 0;
   static const char *modes[] = {
      "Don't care",
      "Linear",
      "Nearest"
   };
   menu_handle_t *menu    = menu_driver_get_ptr();
   if (!menu)
      return;

   (void)pass;
   (void)modes;
   (void)menu;

   *s = '\0';
   *w = MENU_DEFAULT_ENTRY_SPACING;
   strlcpy(s2, path, len2);

#if defined(HAVE_CG) || defined(HAVE_GLSL) || defined(HAVE_HLSL)
   if (!menu->shader)
      return;

  pass = (type - MENU_SETTINGS_SHADER_PASS_FILTER_0);

  strlcpy(s, modes[menu->shader->pass[pass].filter],
        len);
#endif
}

static void menu_action_setting_disp_set_label_filter(
      file_list_t* list,
      unsigned *w, unsigned type, unsigned i,
      const char *label,
      char *s, size_t len,
      const char *entry_label,
      const char *path,
      char *s2, size_t len2)
{
   settings_t *settings = config_get_ptr();

   *s = '\0';
   *w = MENU_DEFAULT_ENTRY_SPACING;
   strlcpy(s2, path, len2);
   strlcpy(s, "None", len);

   if (*settings->video.softfilter_plugin)
   strlcpy(s, path_basename(settings->video.softfilter_plugin),
         len);
}

static void menu_action_setting_disp_set_label_shader_preset(
      file_list_t* list,
      unsigned *w, unsigned type, unsigned i,
      const char *label,
      char *s, size_t len,
      const char *entry_label,
      const char *path,
      char *s2, size_t len2)
{
   settings_t *settings = config_get_ptr();

   *s = '\0';
   *w = MENU_DEFAULT_ENTRY_SPACING;
   strlcpy(s2, path, len2);
   strlcpy(s, "None", len);

   if (*settings->video.shader_path)
   strlcpy(s, path_basename(settings->video.shader_path),
         len);
}

static void menu_action_setting_disp_set_label_shader_num_passes(
      file_list_t* list,
      unsigned *w, unsigned type, unsigned i,
      const char *label,
      char *s, size_t len,
      const char *entry_label,
      const char *path,
      char *s2, size_t len2)
{
   menu_handle_t *menu    = menu_driver_get_ptr();
   if (!menu)
      return;

   (void)menu;

   *s = '\0';
   *w = MENU_DEFAULT_ENTRY_SPACING;
   strlcpy(s2, path, len2);
#if defined(HAVE_CG) || defined(HAVE_GLSL) || defined(HAVE_HLSL)
   snprintf(s, len, "%u", menu->shader->passes);
#endif
}

static void menu_action_setting_disp_set_label_shader_pass(
      file_list_t* list,
      unsigned *w, unsigned type, unsigned i,
      const char *label,
      char *s, size_t len,
      const char *entry_label,
      const char *path,
      char *s2, size_t len2)
{
   unsigned pass = (type - MENU_SETTINGS_SHADER_PASS_0);
   menu_handle_t *menu    = menu_driver_get_ptr();
   if (!menu)
      return;

   (void)pass;
   (void)menu;

   *s = '\0';
   *w = MENU_DEFAULT_ENTRY_SPACING;
   strlcpy(s2, path, len2);
   strlcpy(s, "N/A", len);

#if defined(HAVE_CG) || defined(HAVE_GLSL) || defined(HAVE_HLSL)
   if (*menu->shader->pass[pass].source.path)
      fill_pathname_base(s,
            menu->shader->pass[pass].source.path, len);
#endif
}

static void menu_action_setting_disp_set_label_shader_default_filter(

      file_list_t* list,
      unsigned *w, unsigned type, unsigned i,
      const char *label,
      char *s, size_t len,
      const char *entry_label,
      const char *path,
      char *s2, size_t len2)
{
   settings_t *settings = config_get_ptr();

   *s = '\0';
   *w = MENU_DEFAULT_ENTRY_SPACING;
   snprintf(s, len, "%s",
         settings->video.smooth ? "Linear" : "Nearest");
}

static void menu_action_setting_disp_set_label_shader_parameter(
      file_list_t* list,
      unsigned *w, unsigned type, unsigned i,
      const char *label,
      char *s, size_t len,
      const char *entry_label,
      const char *path,
      char *s2, size_t len2)
{
#if defined(HAVE_CG) || defined(HAVE_GLSL) || defined(HAVE_HLSL)
   const struct video_shader_parameter *param = NULL;
   struct video_shader *shader = NULL;
#endif
   driver_t *driver = driver_get_ptr();

   if (!driver->video_poke)
      return;
   if (!driver->video_data)
      return;

   *s = '\0';
   *w = MENU_DEFAULT_ENTRY_SPACING;
   strlcpy(s2, path, len2);

#if defined(HAVE_CG) || defined(HAVE_GLSL) || defined(HAVE_HLSL)
   shader = video_shader_driver_get_current_shader();

   if (!shader)
      return;

   param = &shader->parameters[type - MENU_SETTINGS_SHADER_PARAMETER_0];

   if (!param)
      return;

   snprintf(s, len, "%.2f [%.2f %.2f]",
         param->current, param->minimum, param->maximum);
#endif
}

static void menu_action_setting_disp_set_label_shader_scale_pass(
      file_list_t* list,
      unsigned *w, unsigned type, unsigned i,
      const char *label,
      char *s, size_t len,
      const char *entry_label,
      const char *path,
      char *s2, size_t len2)
{
   unsigned pass = 0;
   unsigned scale_value = 0;
   menu_handle_t *menu    = menu_driver_get_ptr();
   if (!menu)
      return;

   *s = '\0';
   *w = MENU_DEFAULT_ENTRY_SPACING;
   strlcpy(s2, path, len2);

   (void)pass;
   (void)scale_value;
   (void)menu;

#if defined(HAVE_CG) || defined(HAVE_GLSL) || defined(HAVE_HLSL)
   if (!menu->shader)
      return;

   pass        = (type - MENU_SETTINGS_SHADER_PASS_SCALE_0);
   scale_value = menu->shader->pass[pass].fbo.scale_x;

   if (!scale_value)
      strlcpy(s, "Don't care", len);
   else
      snprintf(s, len, "%ux", scale_value);
#endif
}

static void menu_action_setting_disp_set_label_menu_file_core(
      file_list_t* list,
      unsigned *w, unsigned type, unsigned i,
      const char *label,
      char *s, size_t len,
      const char *entry_label,
      const char *path,
      char *s2, size_t len2)
{
   const char *alt = NULL;
   menu_list_get_alt_at_offset(list, i, &alt);
   if (alt)
      strlcpy(s2, alt, len2);
}

static void menu_action_setting_turbo_id(
      file_list_t* list,
      unsigned *w, unsigned type, unsigned i,
      const char *label,
      char *s, size_t len,
      const char *entry_label,
      const char *path,
      char *s2, size_t len2)
{
   global_t *global     = global_get_ptr();
   settings_t *settings = config_get_ptr();
   unsigned turbo_id    = settings->input.turbo_id[type];

   if (turbo_id < TURBO_ID_ALL
         && global->system.input_desc_btn[type][turbo_id])
      strlcpy(s, global->system.input_desc_btn[type][turbo_id], len);
   else if (turbo_id == TURBO_ID_ALL)
      strlcpy(s, "All", len);
   else
      strlcpy(s, "---", len);

   *w = MENU_DEFAULT_ENTRY_SPACING;
   strlcpy(s2, path, len2);
}

static void menu_action_setting_disp_set_label_input_desc(
      file_list_t* list,
      unsigned *w, unsigned type, unsigned i,
      const char *label,
      char *s, size_t len,
      const char *entry_label,
      const char *path,
      char *s2, size_t len2)
{
   settings_t *settings = config_get_ptr();
   unsigned inp_desc_index_offset = type - MENU_SETTINGS_INPUT_DESC_BEGIN;
   unsigned inp_desc_user         = inp_desc_index_offset /
      (RARCH_FIRST_CUSTOM_BIND + 4);
   unsigned inp_desc_button_index_offset = inp_desc_index_offset -
      (inp_desc_user * (RARCH_FIRST_CUSTOM_BIND + 4));
   unsigned mapped_id;

   if (entry_label[0] == 'T')
      mapped_id = settings->input.turbo_remap_id[inp_desc_user];
   else
      mapped_id = settings->input.remap_ids[inp_desc_user][inp_desc_button_index_offset];

   if (mapped_id > RARCH_FIRST_CUSTOM_BIND + 3)
      snprintf(s, len, "---");
   else if (inp_desc_button_index_offset < RARCH_FIRST_CUSTOM_BIND)
      snprintf(s, len, "%s",
            settings->input.binds[inp_desc_user][mapped_id].desc);
   else
      snprintf(s, len, "%s",
            axis_labels[mapped_id]);

   *w = MENU_DEFAULT_ENTRY_SPACING;
   strlcpy(s2, path, len2);
}

static void menu_action_setting_disp_set_label_joykbd_input_desc(
      file_list_t* list,
      unsigned *w, unsigned type, unsigned i,
      const char *label,
      char *s, size_t len,
      const char *entry_label,
      const char *path,
      char *s2, size_t len2)
{
   settings_t *settings = config_get_ptr();
   unsigned joykbd_list_offset = type - MENU_SETTINGS_INPUT_JOYKBD_LIST_BEGIN;
   uint16_t joy_id             = joykbd_bind_list[joykbd_list_offset].btn;

   if (joy_id < RARCH_FIRST_CUSTOM_BIND)
      snprintf(s, len, "%s",
            settings->input.binds[0][joy_id].desc);
   else if (joy_id < NUM_JOYKBD_BTNS)
      snprintf(s, len, "%s",
            input_config_bind_map[joy_id].desc);
   else
      snprintf(s, len, "---");

   *w = MENU_DEFAULT_ENTRY_SPACING;
   strlcpy(s2, path, len2);
}

static void menu_action_setting_disp_set_label_cheat(
      file_list_t* list,
      unsigned *w, unsigned type, unsigned i,
      const char *label,
      char *s, size_t len,
      const char *entry_label,
      const char *path,
      char *s2, size_t len2)
{
   global_t *global     = global_get_ptr();
   unsigned cheat_index = type - MENU_SETTINGS_CHEAT_BEGIN;

   if (cheat_index < global->cheat->buf_size)
      snprintf(s, len, "%s : %s",
            global->cheat->cheats[cheat_index].state ? 
            menu_hash_to_str(MENU_VALUE_ON) :
            menu_hash_to_str(MENU_VALUE_OFF),
            (global->cheat->cheats[cheat_index].code != NULL)
            ? global->cheat->cheats[cheat_index].code : "N/A"
            );
   *w = MENU_NARROW_ENTRY_SPACING;
   strlcpy(s2, path, len2);
}

static void menu_action_setting_disp_set_label_perf_counters_common(
      const struct retro_perf_counter **counters,
      unsigned offset, char *s, size_t len
      )
{
   if (!counters[offset])
      return;
   if (!counters[offset]->call_cnt)
      return;

   snprintf(s, len,
         U64_FMT " ticks, " U64_FMT " runs.",
         ((uint64_t)counters[offset]->total /
          (uint64_t)counters[offset]->call_cnt),
         (uint64_t)counters[offset]->call_cnt);
}

static void menu_action_setting_disp_set_label_perf_counters(
      file_list_t* list,
      unsigned *w, unsigned type, unsigned i,
      const char *label,
      char *s, size_t len,
      const char *entry_label,
      const char *path,
      char *s2, size_t len2)
{
   menu_animation_t *anim = menu_animation_get_ptr();
   const struct retro_perf_counter **counters =
      (const struct retro_perf_counter **)perf_counters_rarch;
   unsigned offset = type - MENU_SETTINGS_PERF_COUNTERS_BEGIN;

   *s = '\0';
   *w = MENU_DEFAULT_ENTRY_SPACING;
   strlcpy(s2, path, len2);

   menu_action_setting_disp_set_label_perf_counters_common(
         counters, offset, s, len);

   anim->label.is_updated = true;
}

static void menu_action_setting_disp_set_label_libretro_perf_counters(
      file_list_t* list,
      unsigned *w, unsigned type, unsigned i,
      const char *label,
      char *s, size_t len,
      const char *entry_label,
      const char *path,
      char *s2, size_t len2)
{
   menu_animation_t *anim = menu_animation_get_ptr();
   const struct retro_perf_counter **counters =
      (const struct retro_perf_counter **)perf_counters_libretro;
   unsigned offset = type - MENU_SETTINGS_LIBRETRO_PERF_COUNTERS_BEGIN;

   *s = '\0';
   *w = MENU_DEFAULT_ENTRY_SPACING;
   strlcpy(s2, path, len2);

   menu_action_setting_disp_set_label_perf_counters_common(
         counters, offset, s, len);

   anim->label.is_updated = true;
}

static void menu_action_setting_disp_set_label_menu_more(
      file_list_t* list,
      unsigned *w, unsigned type, unsigned i,
      const char *label,
      char *s, size_t len,
      const char *entry_label,
      const char *path,
      char *s2, size_t len2)
{
   strlcpy(s, "...", len);
   *w = MENU_DEFAULT_ENTRY_SPACING;
   strlcpy(s2, path, len2);
}

static void menu_action_setting_disp_set_narrow_label_menu_more(
      file_list_t* list,
      unsigned *w, unsigned type, unsigned i,
      const char *label,
      char *s, size_t len,
      const char *entry_label,
      const char *path,
      char *s2, size_t len2)
{
   strlcpy(s, "...", len);
   *w = MENU_NARROW_ENTRY_SPACING;
   strlcpy(s2, path, len2);
}

static void menu_action_setting_disp_set_label_menu_disk_tray_status(
      file_list_t* list,
      unsigned *w, unsigned type, unsigned i,
      const char *label,
      char *s, size_t len,
      const char *entry_label,
      const char *path,
      char *s2, size_t len2)
{
   global_t *global     = global_get_ptr();
   const struct retro_disk_control_callback *control =
      (const struct retro_disk_control_callback*)
      &global->system.disk_control;

   *w = MENU_DEFAULT_ENTRY_SPACING;
   *s = '\0';
   strlcpy(s2, path, len2);
   if (!control)
      return;

   strlcpy(s, control->get_eject_state() ? "(Ejected)" : "(Closed)", len);
}

static void menu_action_setting_disp_set_label_menu_disk_index(
      file_list_t* list,
      unsigned *w, unsigned type, unsigned i,
      const char *label,
      char *s, size_t len,
      const char *entry_label,
      const char *path,
      char *s2, size_t len2)
{
   unsigned images = 0, current = 0;
   global_t *global     = global_get_ptr();
   const struct retro_disk_control_callback *control =
      (const struct retro_disk_control_callback*)
      &global->system.disk_control;

   *w = MENU_DEFAULT_ENTRY_SPACING;
   *s = '\0';
   strlcpy(s2, path, len2);
   if (!control)
      return;

   images = control->get_num_images();
   current = control->get_image_index();

   if (current >= images)
      strlcpy(s, "No Disc", len);
   else
      snprintf(s, len, "%u of %u", current + 1, images);
}

static void menu_action_setting_disp_set_label_menu_video_resolution(
      file_list_t* list,
      unsigned *w, unsigned type, unsigned i,
      const char *label,
      char *s, size_t len,
      const char *entry_label,
      const char *path,
      char *s2, size_t len2)
{
   unsigned width = 0, height = 0;

   *w = MENU_DEFAULT_ENTRY_SPACING;
   *s = '\0';

   (void)width;
   (void)height;

   strlcpy(s2, path, len2);

   if (video_driver_get_video_output_size(&width, &height))
      snprintf(s, len, "%ux%u", width, height);
   else
      strlcpy(s, "N/A", len);
}

static void menu_action_setting_generic_disp_set_label(
      unsigned *w, char *s, size_t len,
      const char *path, const char *label,
      char *s2, size_t len2)
{
   *s = '\0';

   if (label)
      strlcpy(s, label, len);
   *w = strlen(s);

   strlcpy(s2, path, len2);
}

static void menu_action_setting_disp_set_label_menu_file_plain(
      file_list_t* list,
      unsigned *w, unsigned type, unsigned i,
      const char *label,
      char *s, size_t len,
      const char *entry_label,
      const char *path,
      char *s2, size_t len2)
{
   char* alt = list->list[i].alt;
   menu_action_setting_generic_disp_set_label(w, s, len,
         (alt? alt : path), "(FILE)", s2, len2);
}

static void menu_action_setting_disp_set_label_menu_file_remap(
      file_list_t* list,
      unsigned *w, unsigned type, unsigned i,
      const char *label,
      char *s, size_t len,
      const char *entry_label,
      const char *path,
      char *s2, size_t len2)
{
   menu_action_setting_generic_disp_set_label(w, s, len,
         path, "(REMAP)", s2, len2);
}

static void menu_action_setting_disp_set_label_menu_file_core_option(
      file_list_t* list,
      unsigned *w, unsigned type, unsigned i,
      const char *label,
      char *s, size_t len,
      const char *entry_label,
      const char *path,
      char *s2, size_t len2)
{
   menu_action_setting_generic_disp_set_label(w, s, len,
         path, "(OPTION)", s2, len2);
}

static void menu_action_setting_disp_set_label_menu_file_image(
      file_list_t* list,
      unsigned *w, unsigned type, unsigned i,
      const char *label,
      char *s, size_t len,
      const char *entry_label,
      const char *path,
      char *s2, size_t len2)
{
   menu_action_setting_generic_disp_set_label(w, s, len,
         path, "(IMG)", s2, len2);
}

static void menu_action_setting_disp_set_label_menu_file_use_directory(
      file_list_t* list,
      unsigned *w, unsigned type, unsigned i,
      const char *label,
      char *s, size_t len,
      const char *entry_label,
      const char *path,
      char *s2, size_t len2)
{
   menu_action_setting_generic_disp_set_label(w, s, len,
         path, NULL, s2, len2);
}

static void menu_action_setting_disp_set_label_menu_file_directory(
      file_list_t* list,
      unsigned *w, unsigned type, unsigned i,
      const char *label,
      char *s, size_t len,
      const char *entry_label,
      const char *path,
      char *s2, size_t len2)
{
   menu_action_setting_generic_disp_set_label(w, s, len,
         path, "(DIR)", s2, len2);
}

static void menu_action_setting_disp_set_label_menu_file_carchive(
      file_list_t* list,
      unsigned *w, unsigned type, unsigned i,
      const char *label,
      char *s, size_t len,
      const char *entry_label,
      const char *path,
      char *s2, size_t len2)
{
   menu_action_setting_generic_disp_set_label(w, s, len,
         path, "(COMP)", s2, len2);
}

static void menu_action_setting_disp_set_label_menu_file_shader(
      file_list_t* list,
      unsigned *w, unsigned type, unsigned i,
      const char *label,
      char *s, size_t len,
      const char *entry_label,
      const char *path,
      char *s2, size_t len2)
{
   menu_action_setting_generic_disp_set_label(w, s, len,
         path, "(SHADER)", s2, len2);
}

static void menu_action_setting_disp_set_label_menu_file_shader_preset(
      file_list_t* list,
      unsigned *w, unsigned type, unsigned i,
      const char *label,
      char *s, size_t len,
      const char *entry_label,
      const char *path,
      char *s2, size_t len2)
{
   menu_action_setting_generic_disp_set_label(w, s, len,
         path, "(PRESET)", s2, len2);
}

static void menu_action_setting_disp_set_label_menu_file_in_carchive(
      file_list_t* list,
      unsigned *w, unsigned type, unsigned i,
      const char *label,
      char *s, size_t len,
      const char *entry_label,
      const char *path,
      char *s2, size_t len2)
{
   menu_action_setting_generic_disp_set_label(w, s, len,
         path, "(CFILE)", s2, len2);
}

static void menu_action_setting_disp_set_label_menu_file_overlay(
      file_list_t* list,
      unsigned *w, unsigned type, unsigned i,
      const char *label,
      char *s, size_t len,
      const char *entry_label,
      const char *path,
      char *s2, size_t len2)
{
   menu_action_setting_generic_disp_set_label(w, s, len,
         path, "(OVERLAY)", s2, len2);
}

static void menu_action_setting_disp_set_label_menu_file_theme(
      file_list_t* list,
      unsigned *w, unsigned type, unsigned i,
      const char *label,
      char *s, size_t len,
      const char *entry_label,
      const char *path,
      char *s2, size_t len2)
{
   menu_action_setting_generic_disp_set_label(w, s, len,
         path, "(THEME)", s2, len2);
}

static void menu_action_setting_disp_set_label_menu_file_font(
      file_list_t* list,
      unsigned *w, unsigned type, unsigned i,
      const char *label,
      char *s, size_t len,
      const char *entry_label,
      const char *path,
      char *s2, size_t len2)
{
   menu_action_setting_generic_disp_set_label(w, s, len,
         path, "(FONT)", s2, len2);
}

static void menu_action_setting_disp_set_label_menu_file_filter(
      file_list_t* list,
      unsigned *w, unsigned type, unsigned i,
      const char *label,
      char *s, size_t len,
      const char *entry_label,
      const char *path,
      char *s2, size_t len2)
{
   menu_action_setting_generic_disp_set_label(w, s, len,
         path, "(FILTER)", s2, len2);
}

#if 0
static void menu_action_setting_disp_set_label_menu_file_url(
      file_list_t* list,
      unsigned *w, unsigned type, unsigned i,
      const char *label,
      char *s, size_t len,
      const char *entry_label,
      const char *path,
      char *s2, size_t len2)
{
   menu_action_setting_generic_disp_set_label(w, s, len,
         path, "(URL)", s2, len2);
}
#endif

static void menu_action_setting_disp_set_label_menu_core_url(
      file_list_t* list,
      unsigned *w, unsigned type, unsigned i,
      const char *label,
      char *s, size_t len,
      const char *entry_label,
      const char *path,
      char *s2, size_t len2)
{
   char *alt = list->list[i].alt;

   menu_action_setting_generic_disp_set_label(w, s, len,
         (alt? alt : path), list->list[i].entry_idx ? "[#]" : NULL, s2, len2);
}

static void menu_action_setting_disp_set_label_menu_core_info(
      file_list_t* list,
      unsigned *w, unsigned type, unsigned i,
      const char *label,
      char *s, size_t len,
      const char *entry_label,
      const char *path,
      char *s2, size_t len2)
{
   menu_action_setting_generic_disp_set_label(w, s, len,
         "Update Core Info Files", "", s2, len2);
}

static void menu_action_setting_disp_set_label_menu_file_cheat(
      file_list_t* list,
      unsigned *w, unsigned type, unsigned i,
      const char *label,
      char *s, size_t len,
      const char *entry_label,
      const char *path,
      char *s2, size_t len2)
{
   menu_action_setting_generic_disp_set_label(w, s, len,
         path, "(CHEAT)", s2, len2);
}

static void menu_action_setting_disp_set_label_directory_setting(file_list_t* list,
      unsigned *w, unsigned type, unsigned i,
      const char *label,
      char *s, size_t len,
      const char *entry_label,
      const char *path,
      char *s2, size_t len2)
{
   *s = '\0';
   *w = MENU_WIDE_ENTRY_SPACING;

   setting_get_label(list, s, len, w, type, label, entry_label, i);

   strlcpy(s2, path, len2);
}

static void menu_action_setting_disp_set_label(file_list_t* list,
      unsigned *w, unsigned type, unsigned i,
      const char *label,
      char *s, size_t len,
      const char *entry_label,
      const char *path,
      char *s2, size_t len2)
{
   *s = '\0';
   *w = MENU_DEFAULT_ENTRY_SPACING;

   setting_get_label(list, s, len, w, type, label, entry_label, i);

   strlcpy(s2, path, len2);
}

static void menu_action_setting_disp_set_label_info(file_list_t* list,
      unsigned *w, unsigned type, unsigned i,
      const char *label,
      char *s, size_t len,
      const char *entry_label,
      const char *path,
      char *s2, size_t len2)
{
   *s = '\0';
   *w = 0;

   setting_get_label(list, s, len, w, type, label, entry_label, i);
   strlcpy(s2, path, len2);
}

static void menu_action_setting_disp_set_label_core_option(
      file_list_t* list,
      unsigned *w, unsigned type, unsigned i,
      const char *label,
      char *s, size_t len,
      const char *entry_label,
      const char *path,
      char *s2, size_t len2)
{
   global_t *global = global_get_ptr();

   *w = MENU_DEFAULT_ENTRY_SPACING;
   strlcpy(s2, path, len2);
   strlcpy(s, core_option_label(global->system.core_options, type), len);
}

static void menu_action_setting_disp_set_label_libretro_device(
      file_list_t* list,
      unsigned *w, unsigned type, unsigned i,
      const char *label,
      char *s, size_t len,
      const char *entry_label,
      const char *path,
      char *s2, size_t len2)
{
   rarch_setting_t setting;
   setting.index_offset = type - MENU_SETTINGS_LIBRETRO_DEVICE_INDEX_BEGIN;

   *w = MENU_DEFAULT_ENTRY_SPACING;
   strlcpy(s2, path, len2);
   setting_get_string_representation_uint_libretro_device(&setting,
         s, len);
}

static void menu_action_setting_disp_set_label_remapping_scope(
      file_list_t* list,
      unsigned *w, unsigned type, unsigned i,
      const char *label,
      char *s, size_t len,
      const char *entry_label,
      const char *path,
      char *s2, size_t len2)
{
   *w = MENU_DEFAULT_ENTRY_SPACING;
   strlcpy(s2, path, len2);
   strlcpy(s, scope_lut[input_remapping_scope].name, len);
}

static int menu_cbs_init_bind_get_string_representation_compare_label(
      menu_file_list_cbs_t *cbs, uint32_t label_hash)
{
   switch (label_hash)
   {
      case MENU_LABEL_CHEAT_NUM_PASSES:
         cbs->action_get_value =
            menu_action_setting_disp_set_label_cheat_num_passes;
         break;
      case MENU_LABEL_OPTIONS_SCOPE:
         cbs->action_get_value =
            menu_action_setting_disp_set_label_core_options_scope;
         break;
      case MENU_LABEL_VIDEO_SHADER_FILTER_PASS:
         cbs->action_get_value =
            menu_action_setting_disp_set_label_shader_filter_pass;
         break;
      case MENU_LABEL_VIDEO_SHADER_SCALE_PASS:
         cbs->action_get_value =
            menu_action_setting_disp_set_label_shader_scale_pass;
         break;
      case MENU_LABEL_VIDEO_SHADER_NUM_PASSES:
         cbs->action_get_value =
            menu_action_setting_disp_set_label_shader_num_passes;
         break;
      case MENU_LABEL_VIDEO_SHADER_PASS:
         cbs->action_get_value =
            menu_action_setting_disp_set_label_shader_pass;
         break;
      case MENU_LABEL_VIDEO_SHADER_DEFAULT_FILTER:
         cbs->action_get_value =
            menu_action_setting_disp_set_label_shader_default_filter;
         break;
      case MENU_LABEL_VIDEO_FILTER:
         cbs->action_get_value =
            menu_action_setting_disp_set_label_filter;
         break;
      case MENU_LABEL_SHADER_APPLY_CHANGES:
      case MENU_LABEL_VIDEO_SHADER_PRESET:
         cbs->action_get_value =
            menu_action_setting_disp_set_label_shader_preset;
         break;
      case MENU_LABEL_LIBRETRO_DEVICE_SCOPE:
      case MENU_LABEL_REMAPPING_SCOPE:
         cbs->action_get_value =
            menu_action_setting_disp_set_label_remapping_scope;
         break;
      case MENU_LABEL_INPUT_TURBO_ID:
         cbs->action_get_value = menu_action_setting_turbo_id;
         break;
      case MENU_LABEL_JOYPAD_TO_KEYBOARD_BIND:
         cbs->action_get_value =
            menu_action_setting_disp_set_label_joykbd_input_desc;
         break;
      case MENU_LABEL_OSK_OVERLAY_DIRECTORY:
      case MENU_LABEL_RECORDING_OUTPUT_DIRECTORY:
      case MENU_LABEL_RECORDING_CONFIG_DIRECTORY:
      case MENU_LABEL_RGUI_BROWSER_DIRECTORY:
      case MENU_LABEL_CORE_ASSETS_DIRECTORY:
      case MENU_LABEL_CONTENT_DIRECTORY:
      case MENU_LABEL_CORE_CONTENT_DIRECTORY:
      case MENU_LABEL_CORE_CONTENT_DIRECTORY_QUICKSET:
      case MENU_LABEL_SCREENSHOT_DIRECTORY:
      case MENU_LABEL_INPUT_REMAPPING_DIRECTORY:
      case MENU_LABEL_SAVESTATE_DIRECTORY:
      case MENU_LABEL_RGUI_CONFIG_DIRECTORY:
      case MENU_LABEL_SAVEFILE_DIRECTORY:
      case MENU_LABEL_OVERLAY_DIRECTORY:
      case MENU_LABEL_SYSTEM_DIRECTORY:
      case MENU_LABEL_ASSETS_DIRECTORY:
      case MENU_LABEL_EXTRACTION_DIRECTORY:
      case MENU_LABEL_DYNAMIC_WALLPAPERS_DIRECTORY:
      case MENU_LABEL_JOYPAD_AUTOCONFIG_DIR:
      case MENU_LABEL_LIBRETRO_DIR_PATH:
      case MENU_LABEL_AUDIO_FILTER_DIR:
      case MENU_LABEL_VIDEO_FILTER_DIR:
      case MENU_LABEL_VIDEO_SHADER_DIR:
      case MENU_LABEL_LIBRETRO_INFO_PATH:
      case MENU_LABEL_MENU_THEME_DIRECTORY:
      case MENU_LABEL_CHEAT_DATABASE_PATH:
         cbs->action_get_value =
            menu_action_setting_disp_set_label_directory_setting;
         break;
      case MENU_LABEL_INPUT_REMAPPING:
      case MENU_LABEL_DISK_CONTROL:
      case MENU_LABEL_VIDEO_SHADER_PARAMETERS:
      case MENU_LABEL_OPTIONS_FILE_LOAD:
      case MENU_LABEL_REMAP_FILE_LOAD:
      case MENU_LABEL_CORE_OPTION_CATEGORY:
      case MENU_LABEL_CORE_CHEAT_OPTIONS:
      case MENU_LABEL_CORE_HISTORY:
         cbs->action_get_value =
            menu_action_setting_disp_set_label_menu_more;
         break;
      case MENU_LABEL_CHEAT_FILE_LOAD:
         cbs->action_get_value =
            menu_action_setting_disp_set_narrow_label_menu_more;
         break;
      default:
         return - 1;
   }

   return 0;
}

static int menu_cbs_init_bind_get_string_representation_compare_type(
      menu_file_list_cbs_t *cbs, unsigned type)
{
   if (type >= MENU_SETTINGS_INPUT_DESC_BEGIN
         && type <= MENU_SETTINGS_INPUT_DESC_END)
      cbs->action_get_value =
         menu_action_setting_disp_set_label_input_desc;
   else if (type >= MENU_SETTINGS_CHEAT_BEGIN
         && type <= MENU_SETTINGS_CHEAT_END)
      cbs->action_get_value =
         menu_action_setting_disp_set_label_cheat;
   else if (type >= MENU_SETTINGS_PERF_COUNTERS_BEGIN
         && type <= MENU_SETTINGS_PERF_COUNTERS_END)
      cbs->action_get_value =
         menu_action_setting_disp_set_label_perf_counters;
   else if (type >= MENU_SETTINGS_LIBRETRO_PERF_COUNTERS_BEGIN
         && type <= MENU_SETTINGS_LIBRETRO_PERF_COUNTERS_END)
      cbs->action_get_value =
         menu_action_setting_disp_set_label_libretro_perf_counters;
   else if (type >= MENU_SETTINGS_SHADER_PARAMETER_0
         && type <= MENU_SETTINGS_SHADER_PARAMETER_LAST)
      cbs->action_get_value =
         menu_action_setting_disp_set_label_shader_parameter;
   else if (type >= MENU_SETTINGS_CORE_OPTION_START)
      cbs->action_get_value =
         menu_action_setting_disp_set_label_core_option;
   else if (type >= MENU_SETTINGS_LIBRETRO_DEVICE_INDEX_BEGIN
         && type <= MENU_SETTINGS_LIBRETRO_DEVICE_INDEX_END)
      cbs->action_get_value =
         menu_action_setting_disp_set_label_libretro_device;
   else
   {
      switch (type)
      {
         case MENU_FILE_CORE:
            cbs->action_get_value =
               menu_action_setting_disp_set_label_menu_file_core;
            break;
         case MENU_FILE_REMAP:
            cbs->action_get_value =
               menu_action_setting_disp_set_label_menu_file_remap;
            break;
         case MENU_FILE_CORE_OPTIONS:
            cbs->action_get_value =
               menu_action_setting_disp_set_label_menu_file_core_option;
            break;
         case MENU_FILE_PLAIN:
            cbs->action_get_value =
               menu_action_setting_disp_set_label_menu_file_plain;
            break;
         case MENU_FILE_IMAGE:
            cbs->action_get_value =
               menu_action_setting_disp_set_label_menu_file_image;
            break;
         case MENU_FILE_USE_DIRECTORY:
            cbs->action_get_value =
               menu_action_setting_disp_set_label_menu_file_use_directory;
            break;
         case MENU_FILE_DIRECTORY:
            cbs->action_get_value =
               menu_action_setting_disp_set_label_menu_file_directory;
            break;
         case MENU_FILE_CARCHIVE:
            cbs->action_get_value =
               menu_action_setting_disp_set_label_menu_file_carchive;
            break;
         case MENU_FILE_OVERLAY:
            cbs->action_get_value =
               menu_action_setting_disp_set_label_menu_file_overlay;
            break;
         case MENU_FILE_FONT:
            cbs->action_get_value =
               menu_action_setting_disp_set_label_menu_file_font;
            break;
         case MENU_FILE_SHADER:
            cbs->action_get_value =
               menu_action_setting_disp_set_label_menu_file_shader;
            break;
         case MENU_FILE_SHADER_PRESET:
            cbs->action_get_value =
               menu_action_setting_disp_set_label_menu_file_shader_preset;
            break;
         case MENU_FILE_THEME:
            cbs->action_get_value =
               menu_action_setting_disp_set_label_menu_file_theme;
            break;
         case MENU_FILE_IN_CARCHIVE:
            cbs->action_get_value =
               menu_action_setting_disp_set_label_menu_file_in_carchive;
            break;
         case MENU_FILE_VIDEOFILTER:
         case MENU_FILE_AUDIOFILTER:
            cbs->action_get_value =
               menu_action_setting_disp_set_label_menu_file_filter;
            break;
         case MENU_FILE_DOWNLOAD_CORE:
            cbs->action_get_value =
               menu_action_setting_disp_set_label_menu_core_url;
            break;
         case MENU_FILE_DOWNLOAD_CORE_INFO:
            cbs->action_get_value =
               menu_action_setting_disp_set_label_menu_core_info;
            break;
         case MENU_FILE_CHEAT:
            cbs->action_get_value =
               menu_action_setting_disp_set_label_menu_file_cheat;
            break;
         case MENU_SETTING_SUBGROUP:
         case MENU_SETTINGS_CUSTOM_VIEWPORT:
         case MENU_SETTINGS_CUSTOM_BIND_ALL:
         case MENU_SETTINGS_CORE_DISK_OPTIONS_DISK_IMAGE_APPEND:
         case MENU_SETTINGS_CUSTOM_BIND_DEFAULT_ALL:
            cbs->action_get_value =
               menu_action_setting_disp_set_label_menu_more;
            break;
         case MENU_SETTINGS_CORE_DISK_OPTIONS_DISK_INDEX:
            cbs->action_get_value =
               menu_action_setting_disp_set_label_menu_disk_index;
            break;
         case MENU_SETTINGS_CORE_DISK_OPTIONS_DISK_CYCLE_TRAY_STATUS:
            cbs->action_get_value =
               menu_action_setting_disp_set_label_menu_disk_tray_status;
            break;
         case MENU_SETTINGS_VIDEO_RESOLUTION:
            cbs->action_get_value =
               menu_action_setting_disp_set_label_menu_video_resolution;
            break;
         case MENU_SETTINGS_CORE_INFO_NONE:
            cbs->action_get_value =
               menu_action_setting_disp_set_label_info;
            break;
         default:
            cbs->action_get_value = menu_action_setting_disp_set_label;
            break;
      }
   }

   return 0;
}

int menu_cbs_init_bind_get_string_representation(menu_file_list_cbs_t *cbs,
      const char *path, const char *label, unsigned type, size_t idx,
      const char *elem0, const char *elem1,
      uint32_t label_hash, uint32_t menu_label_hash)
{
   if (!cbs)
      return -1;

   if (menu_cbs_init_bind_get_string_representation_compare_label(cbs, label_hash) == 0)
      return 0;

   if (menu_cbs_init_bind_get_string_representation_compare_type(cbs, type) == 0)
      return 0;

   return -1;
}
