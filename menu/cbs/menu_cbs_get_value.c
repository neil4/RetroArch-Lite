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

#include "../../general.h"
#include "../../performance.h"
#include "../../intl/intl.h"

extern void setting_get_string_representation_uint_libretro_device(void *data,
      char *s, size_t len);

const char axis_labels[4][128] = {
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

   *w = 19;
   strlcpy(s2, path, len2);
   snprintf(s, len, "%u", global->cheat->buf_size);
}

static void menu_action_setting_disp_set_label_remap_file_load(
      file_list_t* list,
      unsigned *w, unsigned type, unsigned i,
      const char *label,
      char *s, size_t len,
      const char *entry_label,
      const char *path,
      char *s2, size_t len2)
{
   settings_t *settings = config_get_ptr();

   *w = 19;
   strlcpy(s2, path, len2);
   fill_pathname_base(s, settings->input.remapping_path,
         len);
}

static void menu_action_setting_disp_set_label_game_option_file(
      file_list_t* list,
      unsigned *w, unsigned type, unsigned i,
      const char *label,
      char *s, size_t len,
      const char *entry_label,
      const char *path,
      char *s2, size_t len2)
{
   global_t *global = global_get_ptr();
   const char *opt_name = path_basename
                          (core_option_conf_path(global->system.core_options));
   char buf[PATH_MAX_LENGTH] = {0};

   *w = 19;
   strlcpy(s2, path, len2);
   
   core_option_get_core_conf_path(buf);
   if (strncmp(opt_name, path_basename(buf), NAME_MAX_LENGTH))
      strlcpy(s, opt_name, NAME_MAX_LENGTH);
   else
      *s = '\0';
}

static void menu_action_setting_disp_set_label_configurations(
      file_list_t* list,
      unsigned *w, unsigned type, unsigned i,
      const char *label,
      char *s, size_t len,
      const char *entry_label,
      const char *path,
      char *s2, size_t len2)
{
   global_t *global = global_get_ptr();

   *w = 19;
   strlcpy(s2, path, len2);
   if (*global->config_path)
      fill_pathname_base(s, global->config_path,
            len);
   else
      strlcpy(s, "<default>", len);
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
   *w = 19;
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
   *w = 19;
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
   *w = 19;
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
   *w = 19;
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
   *w = 19;
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
   *w = 19;
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
   *w = 19;
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
   *w = 19;
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
   unsigned remap_id = settings->input.remap_ids
      [inp_desc_user][inp_desc_button_index_offset];

   if (inp_desc_button_index_offset < RARCH_FIRST_CUSTOM_BIND)
      snprintf(s, len, "%s",
            settings->input.binds[inp_desc_user][remap_id].desc);
   else
      snprintf(s, len, "%s",
            axis_labels[remap_id]);

   *w = 19;
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
      snprintf(s, len, "%s : (%s)",
            (global->cheat->cheats[cheat_index].code != NULL)
            ? global->cheat->cheats[cheat_index].code : "N/A",
            global->cheat->cheats[cheat_index].state ? 
            menu_hash_to_str(MENU_VALUE_ON) :
            menu_hash_to_str(MENU_VALUE_OFF)
            );
   *w = 19;
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
#ifdef _WIN32
         "%I64u ticks, %I64u runs.",
#else
         "%llu ticks, %llu runs.",
#endif
         ((unsigned long long)counters[offset]->total /
          (unsigned long long)counters[offset]->call_cnt),
         (unsigned long long)counters[offset]->call_cnt);
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
   *w = 19;
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
   *w = 19;
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
   *w = 19;
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

   *w = 19;
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

   *w = 19;
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

   *w = 19;
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
   menu_action_setting_generic_disp_set_label(w, s, len,
         path, "(FILE)", s2, len2);
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

static void menu_action_setting_disp_set_label_menu_file_config(
      file_list_t* list,
      unsigned *w, unsigned type, unsigned i,
      const char *label,
      char *s, size_t len,
      const char *entry_label,
      const char *path,
      char *s2, size_t len2)
{
   menu_action_setting_generic_disp_set_label(w, s, len,
         path, "(CONFIG)", s2, len2);
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
   menu_action_setting_generic_disp_set_label
                        (w, s, len,
                         ((*list->list[i].alt && list->list[i].alt[0]) ?
                            list->list[i].alt : path),
                         (char*)list->list[i].userdata, s2, len2);
}

static void menu_action_setting_disp_set_alt_label_menu_file_plain(
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

static void menu_action_setting_disp_set_label(file_list_t* list,
      unsigned *w, unsigned type, unsigned i,
      const char *label,
      char *s, size_t len,
      const char *entry_label,
      const char *path,
      char *s2, size_t len2)
{
   global_t *global     = global_get_ptr();
   uint32_t hash_label  = menu_hash_calculate(label);

   *s = '\0';
   *w = 19;

   switch (hash_label)
   {
      case MENU_LABEL_PERFORMANCE_COUNTERS:
         *w = strlen(label);
         break;
      case MENU_LABEL_INFO_SCREEN:
      {
         /* todo: something less kludgey */
         file_list_t *stack = menu_list_get_ptr()->menu_stack;
         if (stack->size > 1)
            hash_label = menu_hash_calculate(stack->list[stack->size-2].label);

         if (hash_label != MENU_LABEL_CORE_INFORMATION
             && hash_label != MENU_LABEL_SYSTEM_INFORMATION)
            break;  /* else fall through */
      }
      case MENU_LABEL_CORE_INFORMATION:
      case MENU_LABEL_SYSTEM_INFORMATION:
         *w = 0;
         break;
   }

   if (type >= MENU_SETTINGS_CORE_OPTION_START)
      strlcpy(
            s,
            core_option_get_label(global->system.core_options, type),
            len);
   else
      setting_get_label(list, s,
            len, w, type, label, entry_label, i);

   strlcpy(s2, path, len2);
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

   *w = 19;
   strlcpy(s2, path, len2);
   setting_get_string_representation_uint_libretro_device(&setting,
         s, len);
}

static void menu_action_setting_disp_set_label_libretro_scope(
      file_list_t* list,
      unsigned *w, unsigned type, unsigned i,
      const char *label,
      char *s, size_t len,
      const char *entry_label,
      const char *path,
      char *s2, size_t len2)
{
   settings_t *settings = config_get_ptr();

   *w = 19;
   strlcpy(s2, path, len2);
   strlcpy(s, scope_lut[settings->input.libretro_device_scope].name, len);
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
      case MENU_LABEL_REMAP_FILE_LOAD:
         cbs->action_get_value =
            menu_action_setting_disp_set_label_remap_file_load;
         break;
      case MENU_LABEL_OPTIONS_FILE_SAVE_GAME:
         cbs->action_get_value =
            menu_action_setting_disp_set_label_game_option_file;
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
      case MENU_LABEL_CONFIGURATIONS:
         cbs->action_get_value =
            menu_action_setting_disp_set_label_configurations;
         break;
      case MENU_LABEL_LIBRETRO_DEVICE_SCOPE:
         cbs->action_get_value =
            menu_action_setting_disp_set_label_libretro_scope;
         break;
      default:
         return - 1;
   }

   return 0;
}

static int menu_cbs_init_bind_get_string_representation_compare_type(
      menu_file_list_cbs_t *cbs, unsigned type)
{
   settings_t *settings;
   
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
         case MENU_FILE_PLAIN:
            settings = config_get_ptr();
            if (settings->menu.mame_titles)
               cbs->action_get_value =
                  menu_action_setting_disp_set_alt_label_menu_file_plain;
            else
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
            settings = config_get_ptr();
            if (settings->menu.mame_titles)
               cbs->action_get_value =
                  menu_action_setting_disp_set_alt_label_menu_file_plain;
            else
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
         case MENU_FILE_CONFIG:
            cbs->action_get_value =
               menu_action_setting_disp_set_label_menu_file_config;
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
