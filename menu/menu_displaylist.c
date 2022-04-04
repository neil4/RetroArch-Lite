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

#include <stddef.h>

#include <file/file_list.h>
#include <file/file_path.h>
#include <file/file_extract.h>
#include <file/dir_list.h>

#include "menu.h"
#include "menu_hash.h"
#include "menu_display.h"
#include "menu_displaylist.h"
#include "menu_navigation.h"
#include "menu_setting.h"

#include "../general.h"
#include "../retroarch.h"
#include "../gfx/video_shader_driver.h"
#include "../config.features.h"
#include "../git_version.h"
#include "../performance.h"
#include "../tasks/tasks.h"
#include "../input/input_remapping.h"
#include "../input/input_keymaps.h"
#include "../input/input_joypad_to_keyboard.h"

#ifdef HAVE_NETWORKING
extern char *core_buf;
extern size_t core_len;

static void print_buf_lines(file_list_t *list, char *buf, int buf_size,
      unsigned type)
{
   int i;
   char c;
   char *line_start = buf;

   for (i = 0; i < buf_size; i++)
   {
      size_t ln;

      /* The end of the buffer, print the last bit */
      if (*(buf + i) == '\0')
         break;

      if (*(buf + i) != '\n')
         continue;

      /* Found a line ending, print the line and compute new line_start */

      /* Save the next char  */
      c = *(buf + i + 1);
      /* replace with \0 */
      *(buf + i + 1) = '\0';

      /* We need to strip the newline. */
      ln = strlen(line_start) - 1;
      if (line_start[ln] == '\n')
         line_start[ln] = '\0';

      menu_list_push(list, line_start,
            menu_hash_to_str(MENU_LABEL_DOWNLOADABLE_CORE), type, 0, 0);

      /* Restore the saved char */
      *(buf + i + 1) = c;
      line_start = buf + i + 1;
   }
   /* If the buffer was completely full, and didn't end with a newline, just
    * ignore the partial last line.
    */
}
#endif

static void menu_displaylist_push_perfcounter(
      menu_displaylist_info_t *info,
      const struct retro_perf_counter **counters,
      unsigned num, unsigned id)
{
   unsigned i;
   if (!counters || num == 0)
      return;

   for (i = 0; i < num; i++)
      if (counters[i] && counters[i]->ident)
         menu_list_push(info->list,
               counters[i]->ident, "", id + i, 0, 0);
}

static bool core_is_installed(const char* libretro_name)
{
   global_t* global = global_get_ptr();
   char buf[NAME_MAX_LENGTH];
   unsigned i;

   if (!global->core_info)
      return false;

   for (i = 0; i < global->core_info->count; i += 1)
   {
      path_libretro_name(buf, global->core_info->list[i].path);

      if (!strcmp(buf, libretro_name))
         return true;
   }

   return false;
}

static void menu_displaylist_get_downloadable_core_info(file_list_t* list)
{
   global_t *global  = global_get_ptr();
   unsigned num_missing_info = 0;
   char buf[NAME_MAX_LENGTH];
   char* path;
   int i;

   static uint8_t num_calls;
   static bool need_update;

   if (global->core_info_dl == NULL || (need_update && num_calls < 2))
   {
      core_info_list_free(global->core_info_dl);
      global->core_info_dl = core_info_list_new(DOWNLOADABLE_CORES);
   }

   for (i = 1; i < list->size; i += 1)  /* info.zip is i == 0 */
   {
      path = list->list[i].path;
      if (!path)
         return;

      path_libretro_name(buf, path);

      /* mark with [#] if this core is installed */
      if (core_is_installed(buf))
         file_list_set_userdata(list, i, strdup("[#]"));

      /* put display_name in 'alt' */
      if (!core_info_list_get_display_name(
               global->core_info_dl, buf, buf, NAME_MAX_LENGTH))
          num_missing_info++;
      menu_list_set_alt_at_offset(list, i, buf);
   }
   
   /* auto download info if too many missing */
   if (num_missing_info > list->size/2 && num_calls == 0)
   {
      core_info_queue_download();
      need_update = true;
   }

   num_calls++;
}

static int menu_displaylist_parse_core_info(menu_displaylist_info_t *info)
{
   unsigned i;
   char tmp[PATH_MAX_LENGTH] = {0};
   settings_t *settings      = config_get_ptr();
   global_t *global          = global_get_ptr();
   core_info_t *core_info    = global ? (core_info_t*)global->core_info_current : NULL;

   if (!core_info || !core_info->data)
   {
      menu_list_push(info->list,
            menu_hash_to_str(MENU_LABEL_VALUE_NO_CORE_INFORMATION_AVAILABLE),
            "", 0, 0, 0);
      return 0;
   }

   if (global->content_is_init && *global->fullpath)
   {
      strlcpy(tmp, "Loaded ROM", sizeof(tmp));
      strlcat(tmp, ": ", sizeof(tmp));
      strlcat(tmp, path_basename(global->fullpath), sizeof(tmp));
      menu_list_push(info->list, tmp, "info",
            MENU_SETTINGS_CORE_INFO_NONE, 0, 0);
   }

   strlcpy(tmp, "Core name", sizeof(tmp));
   strlcat(tmp, ": ", sizeof(tmp));
   if (core_info->core_name)
      strlcat(tmp, core_info->core_name, sizeof(tmp));

   menu_list_push(info->list, tmp, "info",
         MENU_SETTINGS_CORE_INFO_NONE, 0, 0);

   strlcpy(tmp, "Core label", sizeof(tmp));
   strlcat(tmp, ": ", sizeof(tmp));
   if (core_info->display_name)
      strlcat(tmp, core_info->display_name, sizeof(tmp));
   menu_list_push(info->list, tmp, "info",
         MENU_SETTINGS_CORE_INFO_NONE, 0, 0);

   if (core_info->systemname)
   {
      strlcpy(tmp, "System name", sizeof(tmp));
      strlcat(tmp, ": ", sizeof(tmp));
      strlcat(tmp, core_info->systemname, sizeof(tmp));
      menu_list_push(info->list, tmp, "info",
            MENU_SETTINGS_CORE_INFO_NONE, 0, 0);
   }

   if (core_info->system_manufacturer_list)
   {
      strlcpy(tmp, "System manufacturer", sizeof(tmp));
      strlcat(tmp, ": ", sizeof(tmp));
      string_list_join_concat(tmp, sizeof(tmp),
            core_info->system_manufacturer_list, ", ");
      menu_list_push(info->list, tmp, "info",
            MENU_SETTINGS_CORE_INFO_NONE, 0, 0);
   }

   if (core_info->categories_list)
   {
      strlcpy(tmp, "Categories", sizeof(tmp));
      strlcat(tmp, ": ", sizeof(tmp));
      string_list_join_concat(tmp, sizeof(tmp),
            core_info->categories_list, ", ");
      menu_list_push(info->list, tmp, "info",
            MENU_SETTINGS_CORE_INFO_NONE, 0, 0);
   }

   if (core_info->authors_list)
   {
      strlcpy(tmp, "Authors", sizeof(tmp));
      strlcat(tmp, ": ", sizeof(tmp));
      string_list_join_concat(tmp, sizeof(tmp),
            core_info->authors_list, ", ");
      menu_list_push(info->list, tmp, "info",
            MENU_SETTINGS_CORE_INFO_NONE, 0, 0);
   }

   if (core_info->permissions_list)
   {
      strlcpy(tmp, "Permissions", sizeof(tmp));
      strlcat(tmp, ": ", sizeof(tmp));
      string_list_join_concat(tmp, sizeof(tmp),
            core_info->permissions_list, ", ");
      menu_list_push(info->list, tmp, "info",
            MENU_SETTINGS_CORE_INFO_NONE, 0, 0);
   }

   if (core_info->licenses_list)
   {
      strlcpy(tmp, "License(s)", sizeof(tmp));
      strlcat(tmp, ": ", sizeof(tmp));
      string_list_join_concat(tmp, sizeof(tmp),
            core_info->licenses_list, ", ");
      menu_list_push(info->list, tmp, "info",
            MENU_SETTINGS_CORE_INFO_NONE, 0, 0);
   }

   if (core_info->supported_extensions_list)
   {
      strlcpy(tmp, "Supported extensions", sizeof(tmp));
      strlcat(tmp, ": ", sizeof(tmp));
      string_list_join_concat(tmp, sizeof(tmp),
            core_info->supported_extensions_list, ", ");
      menu_list_push(info->list, tmp, "info",
            MENU_SETTINGS_CORE_INFO_NONE, 0, 0);
   }

   if (core_info->required_hw_api)
   {
      strlcpy(tmp, "Required graphics API", sizeof(tmp));
      strlcat(tmp, ": ", sizeof(tmp));
      string_list_join_concat(tmp, sizeof(tmp),
            core_info->required_hw_api_list, ", ");
      menu_list_push(info->list, tmp, "info",
            MENU_SETTINGS_CORE_INFO_NONE, 0, 0);
   }

   if (core_info->firmware_count > 0)
   {
      core_info_list_update_missing_firmware(
            global->core_info, core_info->path,
            settings->system_directory);

      strlcpy(tmp, "Firmware", sizeof(tmp));
      strlcat(tmp, ": ", sizeof(tmp));
      menu_list_push(info->list, tmp, "info",
            MENU_SETTINGS_CORE_INFO_NONE, 0, 0);
      for (i = 0; i < core_info->firmware_count; i++)
      {
         if (core_info->firmware[i].desc)
         {
            snprintf(tmp, sizeof(tmp), "	name: %s",
                  core_info->firmware[i].desc ?
                  core_info->firmware[i].desc : "");
            menu_list_push(info->list, tmp, "info",
                  MENU_SETTINGS_CORE_INFO_NONE, 0, 0);

            snprintf(tmp, sizeof(tmp), "	status: %s, %s",
                  core_info->firmware[i].missing ?
                  "missing" : "present",
                  core_info->firmware[i].optional ?
                  "optional" : "required");
            menu_list_push(info->list, tmp, "info",
                  MENU_SETTINGS_CORE_INFO_NONE, 0, 0);
         }
      }
   }

   if (core_info->notes)
   {
      strlcpy(tmp, "Core notes", sizeof(tmp));
      strlcat(tmp, ": ", sizeof(tmp));
      menu_list_push(info->list, tmp, "info",
            MENU_SETTINGS_CORE_INFO_NONE, 0, 0);

      for (i = 0; i < core_info->note_list->size; i++)
      {
         strlcpy(tmp,
               core_info->note_list->elems[i].data, sizeof(tmp));
         menu_list_push(info->list, tmp, "info",
               MENU_SETTINGS_CORE_INFO_NONE, 0, 0);
      }
   }

   return 0;
}

static int menu_displaylist_parse_system_info(menu_displaylist_info_t *info)
{
   char feat_str[PATH_MAX_LENGTH]        = {0};
   char tmp[PATH_MAX_LENGTH]             = {0};
   char tmp2[PATH_MAX_LENGTH]            = {0};
   const char *tmp_string                = NULL;
   const frontend_ctx_driver_t *frontend = frontend_get_ptr();

   snprintf(tmp, sizeof(tmp), "%s: %s", "Build date", __DATE__);
   menu_list_push(info->list, tmp, "info",
         MENU_SETTINGS_CORE_INFO_NONE, 0, 0);

   (void)tmp_string;

#ifdef HAVE_GIT_VERSION
   strlcpy(tmp, "Git version", sizeof(tmp));
   strlcat(tmp, ": ", sizeof(tmp));
   strlcat(tmp, rarch_git_version, sizeof(tmp));
   menu_list_push(info->list, tmp, "info",
         MENU_SETTINGS_CORE_INFO_NONE, 0, 0);
#endif

   rarch_info_get_capabilities(RARCH_CAPABILITIES_COMPILER, tmp, sizeof(tmp));
   menu_list_push(info->list, tmp, "info", MENU_SETTINGS_CORE_INFO_NONE, 0, 0);

   {
      char cpu_str[PATH_MAX_LENGTH] = {0};

      strlcpy(cpu_str, "CPU Features", sizeof(cpu_str));
      strlcat(cpu_str, ": ", sizeof(cpu_str));

      rarch_info_get_capabilities(RARCH_CAPABILITIES_CPU, cpu_str, sizeof(cpu_str));
      menu_list_push(info->list, cpu_str, "info",
            MENU_SETTINGS_CORE_INFO_NONE, 0, 0);
   }

   if (frontend)
   {
      int major = 0, minor = 0;

      strlcpy(tmp, "Frontend identifier", sizeof(tmp));
      strlcat(tmp, ": ", sizeof(tmp));
      strlcat(tmp, frontend->ident, sizeof(tmp));
      menu_list_push(info->list, tmp, "info",
            MENU_SETTINGS_CORE_INFO_NONE, 0, 0);

      if (frontend->get_name)
      {
         frontend->get_name(tmp2, sizeof(tmp2));
         strlcpy(tmp, "Frontend name", sizeof(tmp));
         strlcat(tmp, ": ", sizeof(tmp));
         strlcat(tmp, frontend->get_name ? tmp2 : "N/A", sizeof(tmp));
         menu_list_push(info->list, tmp, "info",
               MENU_SETTINGS_CORE_INFO_NONE, 0, 0);
      }

      if (frontend->get_os)
      {
         frontend->get_os(tmp2, sizeof(tmp2), &major, &minor);
         snprintf(tmp, sizeof(tmp), "%s: %s (v%d.%d)",
               "Frontend OS",
               frontend->get_os ? tmp2 : "N/A", major, minor);
         menu_list_push(info->list, tmp, "info",
               MENU_SETTINGS_CORE_INFO_NONE, 0, 0);
      }
      
      {
         unsigned int memory_used;
         unsigned int memory_total;
         rarch_get_memory_use_megabytes(&memory_total, &memory_used);

         if (memory_used != 0 && memory_total != 0)
         {
            snprintf(tmp, sizeof(tmp), "%s: %u/%u MB",
               "Memory Use", memory_used, memory_total);
            menu_list_push(info->list, tmp, "info",
               MENU_SETTINGS_CORE_INFO_NONE, 0, 0);
         }
      }

      snprintf(tmp, sizeof(tmp), "%s: %d",
            "RetroRating level",
            frontend->get_rating ? frontend->get_rating() : -1);
      menu_list_push(info->list, tmp, "info",
            MENU_SETTINGS_CORE_INFO_NONE, 0, 0);

      if (frontend->get_powerstate)
      {
         int seconds = 0, percent = 0;
         enum frontend_powerstate state = frontend->get_powerstate(&seconds, &percent);

         tmp2[0] = '\0';

         if (percent != 0)
            snprintf(tmp2, sizeof(tmp2), "%d%%", percent);

         switch (state)
         {
            case FRONTEND_POWERSTATE_NONE:
               strlcat(tmp2, " N/A", sizeof(tmp));
               break;
            case FRONTEND_POWERSTATE_NO_SOURCE:
               strlcat(tmp2, " (No source)", sizeof(tmp));
               break;
            case FRONTEND_POWERSTATE_CHARGING:
               strlcat(tmp2, " (Charging)", sizeof(tmp));
               break;
            case FRONTEND_POWERSTATE_CHARGED:
               strlcat(tmp2, " (Charged)", sizeof(tmp));
               break;
            case FRONTEND_POWERSTATE_ON_POWER_SOURCE:
               strlcat(tmp2, " (Discharging)", sizeof(tmp));
               break;
         }

         strlcpy(tmp, "Power source", sizeof(tmp));
         strlcat(tmp, ": ", sizeof(tmp));
         strlcat(tmp, tmp2, sizeof(tmp));
         menu_list_push(info->list, tmp, "info",
               MENU_SETTINGS_CORE_INFO_NONE, 0, 0);
      }
   }

#if defined(HAVE_OPENGL) || defined(HAVE_GLES)
   tmp_string = gfx_ctx_get_ident();

   strlcpy(tmp, "Video context driver", sizeof(tmp));
   strlcat(tmp, ": ", sizeof(tmp));
   strlcat(tmp, tmp_string ? tmp_string : "N/A", sizeof(tmp));
   menu_list_push(info->list, tmp, "info",
         MENU_SETTINGS_CORE_INFO_NONE, 0, 0);

   {
      float val = 0.0f;
      if (gfx_ctx_get_metrics(DISPLAY_METRIC_MM_WIDTH, &val))
      {
         snprintf(tmp, sizeof(tmp), "%s: %.2f",
               "Display metric width (mm)", val);
         menu_list_push(info->list, tmp, "info",
               MENU_SETTINGS_CORE_INFO_NONE, 0, 0);
      }

      if (gfx_ctx_get_metrics(DISPLAY_METRIC_MM_HEIGHT, &val))
      {
         snprintf(tmp, sizeof(tmp), "Display metric height (mm): %.2f",
               val);
         menu_list_push(info->list, tmp, "info",
               MENU_SETTINGS_CORE_INFO_NONE, 0, 0);
      }

      if (gfx_ctx_get_metrics(DISPLAY_METRIC_DPI, &val))
      {
         snprintf(tmp, sizeof(tmp), "Display metric DPI: %.2f",
               val);
         menu_list_push(info->list, tmp, "info",
               MENU_SETTINGS_CORE_INFO_NONE, 0, 0);
      }
   }
#endif

   strlcpy(feat_str, "LibretroDB support", sizeof(feat_str));
   strlcat(feat_str, ": ", sizeof(feat_str));
   strlcat(feat_str, "false", sizeof(feat_str));
   menu_list_push(info->list, feat_str, "info",
         MENU_SETTINGS_CORE_INFO_NONE, 0, 0);

   strlcpy(feat_str, "Overlay support", sizeof(feat_str));
   strlcat(feat_str, ": ", sizeof(feat_str));
   strlcat(feat_str, _overlay_supp ? "true" : "false", sizeof(feat_str));
   menu_list_push(info->list, feat_str, "info",
         MENU_SETTINGS_CORE_INFO_NONE, 0, 0);

   strlcpy(feat_str, "Command interface support", sizeof(feat_str));
   strlcat(feat_str, ": ", sizeof(feat_str));
   strlcat(feat_str, _command_supp ? "true" : "false", sizeof(feat_str));
   menu_list_push(info->list, feat_str, "info",
         MENU_SETTINGS_CORE_INFO_NONE, 0, 0);

   snprintf(feat_str, sizeof(feat_str),
         "%s: %s",
         "Network Command interface support",
         _network_command_supp ? "true" : "false");
   menu_list_push(info->list, feat_str, "info",
         MENU_SETTINGS_CORE_INFO_NONE, 0, 0);

   snprintf(feat_str, sizeof(feat_str),
         "Cocoa support: %s", _cocoa_supp ? "true" : "false");
   menu_list_push(info->list, feat_str, "info",
         MENU_SETTINGS_CORE_INFO_NONE, 0, 0);

   snprintf(feat_str, sizeof(feat_str),
         "PNG support (RPNG): %s", _rpng_supp ? "true" : "false");
   menu_list_push(info->list, feat_str, "info",
         MENU_SETTINGS_CORE_INFO_NONE, 0, 0);

   snprintf(feat_str, sizeof(feat_str),
         "SDL1.2 support: %s", _sdl_supp ? "true" : "false");
   menu_list_push(info->list, feat_str, "info",
         MENU_SETTINGS_CORE_INFO_NONE, 0, 0);

   snprintf(feat_str, sizeof(feat_str),
         "SDL2 support: %s", _sdl2_supp ? "true" : "false");
   menu_list_push(info->list, feat_str, "info",
         MENU_SETTINGS_CORE_INFO_NONE, 0, 0);


   snprintf(feat_str, sizeof(feat_str),
         "OpenGL support: %s", _opengl_supp ? "true" : "false");
   menu_list_push(info->list, feat_str, "info",
         MENU_SETTINGS_CORE_INFO_NONE, 0, 0);

   snprintf(feat_str, sizeof(feat_str),
         "OpenGL ES support: %s", _opengles_supp ? "true" : "false");
   menu_list_push(info->list, feat_str, "info",
         MENU_SETTINGS_CORE_INFO_NONE, 0, 0);

   snprintf(feat_str, sizeof(feat_str),
         "Threading support: %s", _thread_supp ? "true" : "false");
   menu_list_push(info->list, feat_str, "info",
         MENU_SETTINGS_CORE_INFO_NONE, 0, 0);

   snprintf(feat_str, sizeof(feat_str),
         "KMS/EGL support: %s", _kms_supp ? "true" : "false");
   menu_list_push(info->list, feat_str, "info",
         MENU_SETTINGS_CORE_INFO_NONE, 0, 0);

   snprintf(feat_str, sizeof(feat_str),
         "Udev support: %s", _udev_supp ? "true" : "false");
   menu_list_push(info->list, feat_str, "info",
         MENU_SETTINGS_CORE_INFO_NONE, 0, 0);

   snprintf(feat_str, sizeof(feat_str),
         "OpenVG support: %s", _vg_supp ? "true" : "false");
   menu_list_push(info->list, feat_str, "info",
         MENU_SETTINGS_CORE_INFO_NONE, 0, 0);

   snprintf(feat_str, sizeof(feat_str),
         "EGL support: %s", _egl_supp ? "true" : "false");
   menu_list_push(info->list, feat_str, "info",
         MENU_SETTINGS_CORE_INFO_NONE, 0, 0);

   snprintf(feat_str, sizeof(feat_str),
         "X11 support: %s", _x11_supp ? "true" : "false");
   menu_list_push(info->list, feat_str, "info",
         MENU_SETTINGS_CORE_INFO_NONE, 0, 0);

   snprintf(feat_str, sizeof(feat_str),
         "Wayland support: %s", _wayland_supp ? "true" : "false");
   menu_list_push(info->list, feat_str, "info",
         MENU_SETTINGS_CORE_INFO_NONE, 0, 0);

   snprintf(feat_str, sizeof(feat_str),
         "XVideo support: %s", _xvideo_supp ? "true" : "false");
   menu_list_push(info->list, feat_str, "info",
         MENU_SETTINGS_CORE_INFO_NONE, 0, 0);

   snprintf(feat_str, sizeof(feat_str),
         "ALSA support: %s", _alsa_supp ? "true" : "false");
   menu_list_push(info->list, feat_str, "info",
         MENU_SETTINGS_CORE_INFO_NONE, 0, 0);

   snprintf(feat_str, sizeof(feat_str),
         "OSS support: %s", _oss_supp ? "true" : "false");
   menu_list_push(info->list, feat_str, "info",
         MENU_SETTINGS_CORE_INFO_NONE, 0, 0);

   snprintf(feat_str, sizeof(feat_str),
         "OpenAL support: %s", _al_supp ? "true" : "false");
   menu_list_push(info->list, feat_str, "info",
         MENU_SETTINGS_CORE_INFO_NONE, 0, 0);

   snprintf(feat_str, sizeof(feat_str),
         "OpenSL support: %s", _sl_supp ? "true" : "false");
   menu_list_push(info->list, feat_str, "info",
         MENU_SETTINGS_CORE_INFO_NONE, 0, 0);

   snprintf(feat_str, sizeof(feat_str),
         "RSound support: %s", _rsound_supp ? "true" : "false");
   menu_list_push(info->list, feat_str, "info",
         MENU_SETTINGS_CORE_INFO_NONE, 0, 0);

   snprintf(feat_str, sizeof(feat_str),
         "RoarAudio support: %s", _roar_supp ? "true" : "false");
   menu_list_push(info->list, feat_str, "info",
         MENU_SETTINGS_CORE_INFO_NONE, 0, 0);

   snprintf(feat_str, sizeof(feat_str),
         "JACK support: %s", _jack_supp ? "true" : "false");
   menu_list_push(info->list, feat_str, "info",
         MENU_SETTINGS_CORE_INFO_NONE, 0, 0);

   snprintf(feat_str, sizeof(feat_str),
         "PulseAudio support: %s", _pulse_supp ? "true" : "false");
   menu_list_push(info->list, feat_str, "info",
         MENU_SETTINGS_CORE_INFO_NONE, 0, 0);

   snprintf(feat_str, sizeof(feat_str),
         "DirectSound support: %s", _dsound_supp ? "true" : "false");
   menu_list_push(info->list, feat_str, "info",
         MENU_SETTINGS_CORE_INFO_NONE, 0, 0);

   snprintf(feat_str, sizeof(feat_str),
         "XAudio2 support: %s", _xaudio_supp ? "true" : "false");
   menu_list_push(info->list, feat_str, "info",
         MENU_SETTINGS_CORE_INFO_NONE, 0, 0);

   snprintf(feat_str, sizeof(feat_str),
         "Zlib support: %s", _zlib_supp ? "true" : "false");
   menu_list_push(info->list, feat_str, "info",
         MENU_SETTINGS_CORE_INFO_NONE, 0, 0);

   snprintf(feat_str, sizeof(feat_str),
         "7zip support: %s", _7zip_supp ? "true" : "false");
   menu_list_push(info->list, feat_str, "info",
         MENU_SETTINGS_CORE_INFO_NONE, 0, 0);

   snprintf(feat_str, sizeof(feat_str),
         "Dynamic library support: %s", _dylib_supp ? "true" : "false");
   menu_list_push(info->list, feat_str, "info",
         MENU_SETTINGS_CORE_INFO_NONE, 0, 0);

   snprintf(feat_str, sizeof(feat_str),
         "Cg support: %s", _cg_supp ? "true" : "false");
   menu_list_push(info->list, feat_str, "info",
         MENU_SETTINGS_CORE_INFO_NONE, 0, 0);

   snprintf(feat_str, sizeof(feat_str),
         "GLSL support: %s", _glsl_supp ? "true" : "false");
   menu_list_push(info->list, feat_str, "info",
         MENU_SETTINGS_CORE_INFO_NONE, 0, 0);

   snprintf(feat_str, sizeof(feat_str),
         "HLSL support: %s", _hlsl_supp ? "true" : "false");
   menu_list_push(info->list, feat_str, "info",
         MENU_SETTINGS_CORE_INFO_NONE, 0, 0);

   snprintf(feat_str, sizeof(feat_str),
         "libxml2 XML parsing support: %s", _libxml2_supp ? "true" : "false");
   menu_list_push(info->list, feat_str, "info",
         MENU_SETTINGS_CORE_INFO_NONE, 0, 0);

   snprintf(feat_str, sizeof(feat_str),
         "SDL image support: %s", _sdl_image_supp ? "true" : "false");
   menu_list_push(info->list, feat_str, "info",
         MENU_SETTINGS_CORE_INFO_NONE, 0, 0);

   snprintf(feat_str, sizeof(feat_str),
         "OpenGL/Direct3D render-to-texture (multi-pass shaders) support: %s", _fbo_supp ? "true" : "false");
   menu_list_push(info->list, feat_str, "info",
         MENU_SETTINGS_CORE_INFO_NONE, 0, 0);

   snprintf(feat_str, sizeof(feat_str),
         "FFmpeg support: %s", _ffmpeg_supp ? "true" : "false");
   menu_list_push(info->list, feat_str, "info",
         MENU_SETTINGS_CORE_INFO_NONE, 0, 0);

   snprintf(feat_str, sizeof(feat_str),
         "CoreText support: %s", _coretext_supp ? "true" : "false");
   menu_list_push(info->list, feat_str, "info",
         MENU_SETTINGS_CORE_INFO_NONE, 0, 0);

   snprintf(feat_str, sizeof(feat_str),
         "FreeType support: %s", _freetype_supp ? "true" : "false");
   menu_list_push(info->list, feat_str, "info",
         MENU_SETTINGS_CORE_INFO_NONE, 0, 0);

   snprintf(feat_str, sizeof(feat_str),
         "Netplay (peer-to-peer) support: %s", _netplay_supp ? "true" : "false");
   menu_list_push(info->list, feat_str, "info",
         MENU_SETTINGS_CORE_INFO_NONE, 0, 0);

   snprintf(feat_str, sizeof(feat_str),
         "Python support (in shaders): %s", _python_supp ? "true" : "false");
   menu_list_push(info->list, feat_str, "info",
         MENU_SETTINGS_CORE_INFO_NONE, 0, 0);

   snprintf(feat_str, sizeof(feat_str),
         "Video4Linux2 support: %s", _v4l2_supp ? "true" : "false");
   menu_list_push(info->list, feat_str, "info",
         MENU_SETTINGS_CORE_INFO_NONE, 0, 0);

   snprintf(feat_str, sizeof(feat_str),
         "Libusb support: %s", _libusb_supp ? "true" : "false");
   menu_list_push(info->list, feat_str, "info",
         MENU_SETTINGS_CORE_INFO_NONE, 0, 0);

   return 0;
}

static int menu_displaylist_parse_shader_options(menu_displaylist_info_t *info)
{
   unsigned i;
   struct video_shader *shader = NULL;
   menu_handle_t         *menu = menu_driver_get_ptr();

   if (!menu)
      return -1;

   shader = menu->shader;

   if (!shader)
      return -1;

   menu_list_push(info->list,
         "Shader Parameters",
         menu_hash_to_str(MENU_LABEL_VIDEO_SHADER_PARAMETERS),
         MENU_SETTING_ACTION, 0, 0);
   menu_list_push(info->list,
         menu_hash_to_str(MENU_LABEL_VALUE_SHADER_APPLY_CHANGES),
         menu_hash_to_str(MENU_LABEL_SHADER_APPLY_CHANGES),
         MENU_SETTING_ACTION, 0, 0);
   menu_list_push(info->list,
         menu_hash_to_str(MENU_LABEL_VALUE_VIDEO_SHADER_PRESET_SAVE_AS),
         menu_hash_to_str(MENU_LABEL_VIDEO_SHADER_PRESET_SAVE_AS),
         MENU_SETTING_ACTION, 0, 0);
   menu_list_push(info->list,
         menu_hash_to_str(MENU_LABEL_VALUE_VIDEO_SHADER_NUM_PASSES),
         menu_hash_to_str(MENU_LABEL_VIDEO_SHADER_NUM_PASSES),
         0, 0, 0);

   for (i = 0; i < shader->passes; i++)
   {
      char buf[64] = {0};

      snprintf(buf, sizeof(buf), "Shader #%u", i);
      menu_list_push(info->list, buf,
            menu_hash_to_str(MENU_LABEL_VIDEO_SHADER_PASS),
            MENU_SETTINGS_SHADER_PASS_0 + i, 0, 0);

      snprintf(buf, sizeof(buf), "Shader #%u Filter", i);
      menu_list_push(info->list, buf,
            menu_hash_to_str(MENU_LABEL_VIDEO_SHADER_FILTER_PASS),
            MENU_SETTINGS_SHADER_PASS_FILTER_0 + i, 0, 0);

      snprintf(buf, sizeof(buf), "Shader #%u Scale", i);
      menu_list_push(info->list, buf,
            menu_hash_to_str(MENU_LABEL_VIDEO_SHADER_SCALE_PASS),
            MENU_SETTINGS_SHADER_PASS_SCALE_0 + i, 0, 0);
   }

   return 0;
}

#ifdef HAVE_SHADER_MANAGER
static int deferred_push_video_shader_parameters_common(
      menu_displaylist_info_t *info,
      struct video_shader *shader, unsigned base_parameter)
{
   unsigned i;
   size_t list_size = shader ? shader->num_parameters : 0;

   if (list_size <= 0)
   {
      menu_list_push(info->list,
            menu_hash_to_str(MENU_LABEL_VALUE_NO_SHADER_PARAMETERS),
            "", 0, 0, 0);
      return 0;
   }

   for (i = 0; i < list_size; i++)
      menu_list_push(info->list, shader->parameters[i].desc,
            "", base_parameter + i, 0, 0);

   return 0;
}
#endif

static void menu_displaylist_realloc_settings(menu_entries_t *entries, unsigned flags)
{
   if (!entries)
      return;

   if (entries->list_settings)
      menu_setting_free(entries->list_settings);

   entries->list_settings      = menu_setting_new(flags);
}

static int menu_displaylist_parse_settings(menu_handle_t *menu,
      menu_displaylist_info_t *info, unsigned setting_flags)
{
   rarch_setting_t *setting = NULL;
   settings_t *settings     = config_get_ptr();

   menu_displaylist_realloc_settings(&menu->entries, setting_flags);

   setting                  = menu_setting_find(info->label);

   if (!setting)
      return -1;


   for (; setting->type != ST_END_GROUP; setting++)
   {
      switch (setting->type)
      {
         case ST_GROUP:
         case ST_SUB_GROUP:
         case ST_END_SUB_GROUP:
            continue;
         default:
            break;
      }

      if (setting->flags & SD_FLAG_ADVANCED &&
            !settings->menu.show_advanced_settings)
         continue;

      menu_list_push(info->list, setting->short_description,
            setting->name, menu_setting_set_flags(setting),
            info->directory_ptr, 0);
   }

   return 0;
}

static int menu_displaylist_parse_options_cheats(menu_displaylist_info_t *info)
{
   unsigned i;
   global_t *global       = global_get_ptr();
   cheat_manager_t *cheat = global ? global->cheat : NULL;

   if (!cheat)
   {
      global->cheat = cheat_manager_new(0);

      if (!global->cheat)
         return -1;
      cheat = global->cheat;
   }

   menu_list_push(info->list,
         menu_hash_to_str(MENU_LABEL_VALUE_CHEAT_FILE_LOAD),
         menu_hash_to_str(MENU_LABEL_CHEAT_FILE_LOAD),
         MENU_SETTING_ACTION, 0, 0);
   menu_list_push(info->list,
         menu_hash_to_str(MENU_LABEL_VALUE_CHEAT_FILE_SAVE_AS),
         menu_hash_to_str(MENU_LABEL_CHEAT_FILE_SAVE_AS),
         MENU_SETTING_ACTION, 0, 0);
   menu_list_push(info->list,
         "Cheat Passes",
         "cheat_num_passes",
         0, 0, 0);
   menu_list_push(info->list,
         "Apply Cheat Changes",
         "cheat_apply_changes",
         MENU_SETTING_ACTION, 0, 0);

   for (i = 0; i < cheat->size; i++)
   {
      char cheat_label[64] = {0};

      snprintf(cheat_label, sizeof(cheat_label), "Cheat #%u: ", i);
      if (cheat->cheats[i].desc)
         strlcat(cheat_label, cheat->cheats[i].desc, sizeof(cheat_label));
      menu_list_push(info->list, cheat_label, "", MENU_SETTINGS_CHEAT_BEGIN + i, 0, 0);
   }

   return 0;
}

static INLINE void menu_displaylist_push_joykbd_binds(
      menu_displaylist_info_t *info)
{
   char desc_label[64];
   char rk_buf[64];
   unsigned i;

   for (i = 0; i < JOYKBD_LIST_LEN; i++)
   {
      enum retro_key rk = joykbd_bind_list[i].rk;

      input_keymaps_translate_rk_to_str(rk, rk_buf, sizeof(rk_buf));
      rk_buf[0] -= 32; /* uppercase 1st letter */
      snprintf(desc_label, sizeof(desc_label), "Keyboard %s: ", rk_buf);

      menu_list_push(info->list, desc_label,
         menu_hash_to_str(MENU_LABEL_JOYPAD_TO_KEYBOARD_BIND),
         MENU_SETTINGS_INPUT_JOYKBD_LIST_BEGIN + i, 0, 0);
   }
}

static INLINE bool menu_displaylist_push_turbo_input(
      menu_displaylist_info_t *info, unsigned p)
{
   global_t *global     = global_get_ptr();
   settings_t *settings = config_get_ptr();
   unsigned id          = settings->input.turbo_id[p];
   unsigned user        = p + 1;
   bool turbo_all       = (id == TURBO_ID_ALL);
   char desc_label[64];
   const char *description;

   snprintf(desc_label, sizeof(desc_label), "User %u Turbo Bind: ", user);
   menu_list_push(info->list, desc_label,
         menu_hash_to_str(MENU_LABEL_INPUT_TURBO_ID), p, 0, 0);

   if (id > TURBO_ID_ALL)
      return false;

   if (!turbo_all)
   {
      description = global->system.input_desc_btn[p][id];
      if (!description)
         return false;

      snprintf(desc_label, sizeof(desc_label),
            "User %u Turbo %s: ", user, description);
      menu_list_push(info->list, desc_label, "TS",
            MENU_SETTINGS_INPUT_DESC_BEGIN +
            (p * (RARCH_FIRST_CUSTOM_BIND + 4)) + id, 0, 0);
   }

   return turbo_all;
}

static INLINE void menu_displaylist_push_remap(menu_displaylist_info_t *info,
                                               unsigned p, unsigned retro_id)
{
   global_t *global        = global_get_ptr();
   char desc_label[64];
   unsigned user           = p + 1;
   unsigned desc_offset    = retro_id;
   const char *description = NULL;

   if (desc_offset >= RARCH_FIRST_CUSTOM_BIND)
      desc_offset = RARCH_FIRST_CUSTOM_BIND + (desc_offset - RARCH_FIRST_CUSTOM_BIND) * 2;

   description = global->system.input_desc_btn[p][desc_offset];

   if (!description)
      return;

   snprintf(desc_label, sizeof(desc_label),
         "User %u %s: ", user, description);
   menu_list_push(info->list, desc_label, "",
         MENU_SETTINGS_INPUT_DESC_BEGIN +
         (p * (RARCH_FIRST_CUSTOM_BIND + 4)) +  retro_id, 0, 0);
}

static void menu_displaylist_push_core_options(menu_displaylist_info_t *info)
{
   size_t i, j, opts_size;
   global_t              *global  = global_get_ptr();
   core_option_manager_t *opt_mgr = global->system.core_options;
   const char *desc;

   core_option_update_category_visibilities(opt_mgr);

   opts_size = core_option_size(opt_mgr);

   for (i = 0, j = 0; i < opts_size; i++)
   {
      if (core_option_is_hidden(opt_mgr, i))
         continue;

      desc = core_option_desc(opt_mgr, i);

      if (core_option_is_category(opt_mgr, i))
         menu_list_push(info->list,
               desc, menu_hash_to_str(MENU_LABEL_CORE_OPTION_CATEGORY),
               MENU_SETTING_ACTION, 0, i);
      else
         menu_list_push(info->list,
               desc, menu_hash_to_str(MENU_LABEL_CORE_OPTION),
               MENU_SETTINGS_CORE_OPTION_START + j, 0, 0);

      core_option_set_menu_offset(opt_mgr, i, j++);
   }

   if (info->list->size == 0)
      menu_list_push(info->list,
            menu_hash_to_str(MENU_LABEL_VALUE_NO_CORE_OPTIONS_AVAILABLE),
            "", MENU_SETTINGS_CORE_OPTION_NONE, 0, 0);
}

static int menu_displaylist_parse_options_remappings(menu_displaylist_info_t *info)
{
   unsigned p, retro_id;
   settings_t *settings   = config_get_ptr();
   bool        kbd_shown  = false;
   char buf[32];

   for (p = 0; p < settings->input.max_users; p++)
   {
      snprintf(buf, sizeof(buf), "User %u Virtual Device", p+1);
      menu_list_push(info->list, buf, "",
         MENU_SETTINGS_LIBRETRO_DEVICE_INDEX_BEGIN + p, 0, 0);
   }

   menu_list_push(info->list, settings->input.max_users > 1 ?
         "Virtual Devices Scope" : "Virtual Device Scope",
         menu_hash_to_str(MENU_LABEL_LIBRETRO_DEVICE_SCOPE), 0, 0, 0);

   menu_list_push(info->list,
         "Remapping Scope",
         menu_hash_to_str(MENU_LABEL_REMAPPING_SCOPE),
         MENU_SETTING_ACTION, 0, 0);
   menu_list_push(info->list,
         menu_hash_to_str(MENU_LABEL_VALUE_REMAP_FILE_LOAD),
         menu_hash_to_str(MENU_LABEL_REMAP_FILE_LOAD),
         MENU_SETTING_ACTION, 0, 0);

   for (p = 0; p < settings->input.max_users; p++)
   {
      if ((RETRO_DEVICE_MASK & settings->input.libretro_device[p])
             == RETRO_DEVICE_KEYBOARD && !kbd_shown)
      {
         menu_displaylist_push_joykbd_binds(info);
         kbd_shown = true;
         continue;
      }

      if (settings->input.turbo_binds_enable)
         menu_displaylist_push_turbo_input(info, p);

      menu_displaylist_push_remap(info, p, RETRO_DEVICE_ID_JOYPAD_B);
      menu_displaylist_push_remap(info, p, RETRO_DEVICE_ID_JOYPAD_A);
      menu_displaylist_push_remap(info, p, RETRO_DEVICE_ID_JOYPAD_Y);
      menu_displaylist_push_remap(info, p, RETRO_DEVICE_ID_JOYPAD_X);

      for (retro_id = RETRO_DEVICE_ID_JOYPAD_L;
           retro_id < RARCH_FIRST_CUSTOM_BIND + 4; retro_id++)
         menu_displaylist_push_remap(info, p, retro_id);
      for (retro_id = RETRO_DEVICE_ID_JOYPAD_SELECT;
           retro_id <= RETRO_DEVICE_ID_JOYPAD_RIGHT; retro_id++)
         menu_displaylist_push_remap(info, p, retro_id);
   }

   return 0;
}

static int menu_displaylist_parse_generic(menu_displaylist_info_t *info, bool *need_sort)
{
   bool path_is_compressed, push_dir;
   size_t i, list_size;
   struct string_list *str_list = NULL;
   int                   device = 0;
   menu_list_t *menu_list       = menu_list_get_ptr();
   global_t *global             = global_get_ptr();
   settings_t *settings         = config_get_ptr();
   uint32_t hash_label          = menu_hash_calculate(info->label);

   (void)device;

   if (!*info->path)
   {
      if (frontend_driver_parse_drive_list(info->list) != 0)
         menu_list_push(info->list, "/", "",
               MENU_FILE_DIRECTORY, 0, 0);
      return 0;
   }

#if defined(GEKKO) && defined(HW_RVL)
   slock_lock(gx_device_mutex);
   device = gx_get_device_from_path(info->path);

   if (device != -1 && !gx_devices[device].mounted &&
         gx_devices[device].interface->isInserted())
      fatMountSimple(gx_devices[device].name, gx_devices[device].interface);

   slock_unlock(gx_device_mutex);
#endif

   path_is_compressed = path_is_compressed_file(info->path);
   push_dir           = (info->setting
                         && info->setting->browser_selection_type == ST_DIR);

   if (path_is_compressed)
      str_list = compressed_file_list_new(info->path, info->exts);
   else
      str_list = dir_list_new(info->path,
            settings->menu.navigation.browser.filter.supported_extensions_enable
            ? info->exts : NULL, true);

   if (push_dir)
      menu_list_push(info->list,
            menu_hash_to_str(MENU_LABEL_VALUE_USE_THIS_DIRECTORY),
            menu_hash_to_str(MENU_LABEL_USE_THIS_DIRECTORY),
            MENU_FILE_USE_DIRECTORY, 0 ,0);

   if (!str_list)
   {
      const char *str = path_is_compressed
         ? menu_hash_to_str(MENU_LABEL_VALUE_UNABLE_TO_READ_COMPRESSED_FILE)
         : menu_hash_to_str(MENU_LABEL_VALUE_DIRECTORY_NOT_FOUND);

      menu_list_push(info->list, str, "", 0, 0, 0);
      return 0;
   }

   dir_list_sort(str_list, true);

   list_size = str_list->size;

   if (list_size <= 0)
   {
      if (!(info->flags & SL_FLAG_ALLOW_EMPTY_LIST))
      {
         menu_list_push(info->list,
               menu_hash_to_str(MENU_LABEL_VALUE_NO_ITEMS),
               "", 0, 0, 0);
      }

      string_list_free(str_list);

      return 0;
   }

   for (i = 0; i < list_size; i++)
   {
      bool is_dir;
      const char *path            = NULL;
      char label[PATH_MAX_LENGTH] = {0};
      menu_file_type_t file_type  = MENU_FILE_NONE;

      switch (str_list->elems[i].attr.i)
      {
         case RARCH_DIRECTORY:
            file_type = MENU_FILE_DIRECTORY;
            break;
         case RARCH_COMPRESSED_ARCHIVE:
            file_type = MENU_FILE_CARCHIVE;
            break;
         case RARCH_COMPRESSED_FILE_IN_ARCHIVE:
            file_type = MENU_FILE_IN_CARCHIVE;
            break;
         case RARCH_PLAIN_FILE:
         default:
            if (hash_label == MENU_LABEL_DETECT_CORE_LIST)
            {
               if (path_is_compressed_file(str_list->elems[i].data))
               {
                  /* in case of deferred_core_list we have to interpret
                   * every archive as an archive to disallow instant loading
                   */
                  file_type = MENU_FILE_CARCHIVE;
                  break;
               }
            }
            file_type = (menu_file_type_t)info->type_default;
            break;
      }

      is_dir = (file_type == MENU_FILE_DIRECTORY);

      if (push_dir && !is_dir)
         continue;

      /* Need to preserve slash first time. */
      path = str_list->elems[i].data;

      if (*info->path && !path_is_compressed)
         path = path_basename(path);
      
      /* Push type further down in the chain.
       * Needed for shader manager currently. */
      switch (hash_label)
      {
         case MENU_LABEL_CORE_LIST:
#ifdef HAVE_LIBRETRO_MANAGEMENT
#ifdef RARCH_CONSOLE
            if (is_dir || strcasecmp(path, SALAMANDER_FILE) == 0)
               continue;
#endif
#endif
            /* Compressed cores are unsupported */
            if (file_type == MENU_FILE_CARCHIVE)
               continue;

            file_type = is_dir ? MENU_FILE_DIRECTORY : MENU_FILE_CORE;
            break;
      }

      menu_list_push(info->list, path, label,
            file_type, 0, 0);
   }

   string_list_free(str_list);

   switch (hash_label)
   {
      case MENU_LABEL_CORE_LIST:
         {
            const char *dir = NULL;

            menu_list_get_last_stack(menu_list, &dir, NULL, NULL, NULL);

            list_size = file_list_get_size(info->list);

            for (i = 0; i < list_size; i++)
            {
               unsigned type = 0;
               char core_path[PATH_MAX_LENGTH]    = {0};
               char display_name[PATH_MAX_LENGTH] = {0};
               const char *path                   = NULL;

               menu_list_get_at_offset(info->list, i, &path, NULL, &type, NULL);

               if (type != MENU_FILE_CORE)
                  continue;

               fill_pathname_join(core_path, dir, path, sizeof(core_path));

               if (global->core_info &&
                     core_info_list_get_display_name(global->core_info,
                        core_path, display_name, sizeof(display_name)))
                  menu_list_set_alt_at_offset(info->list, i, display_name);
            }
            *need_sort = true;
         }
         break;
   }

   return 0;
}

/* Returns nav index of @path, or nav index of the directory leading to it. */
static unsigned menu_displaylist_path_nav_idx(file_list_t *list, char *path)
{
   menu_list_t *menu_list = menu_list_get_ptr();
   const char *menu_dir   = NULL;
   const char *menu_label = NULL;
   unsigned start, i;

   menu_list_get_last_stack(menu_list, &menu_dir, &menu_label, NULL, NULL);
   if (strstr(path, menu_dir) != path)
      return 0;

   start = strlen(menu_dir) + 1;
   if (path[start-1] == '\0')
      return 0;

   /* Look for exact match. */
   for (i = 0; i < list->size; i++)
   {
      if (list->list[i].type == MENU_FILE_DIRECTORY)
         continue;

      if (!strcmp(path + start, list->list[i].path))
         return i;
   }

   /* Look for directory leading to path. */
   for (i = list->size - 1; i < list->size; i--)
   {
      if (list->list[i].type != MENU_FILE_DIRECTORY)
         continue;

      if (strstr(path + start, list->list[i].path) == path + start)
         return i;
   }

   return 0;
}

int menu_displaylist_push_list(menu_displaylist_info_t *info, unsigned type)
{
   size_t i, list_size;
   int ret                  = 0;
   bool need_sort           = false;
   bool need_refresh        = false;
   bool need_push           = false;
   rarch_setting_t *setting = NULL;
   menu_handle_t    *menu   = menu_driver_get_ptr();
   menu_navigation_t *nav   = menu_navigation_get_ptr();
   global_t       *global   = global_get_ptr();
   settings_t   *settings   = config_get_ptr();
   char *buf;

   switch (type)
   {
      case DISPLAYLIST_NONE:
         break;
      case DISPLAYLIST_INFO:
         menu_list_push(info->list, info->path, info->label, info->type, info->directory_ptr, 0);
         break;
      case DISPLAYLIST_GENERIC:
         menu_driver_list_cache(MENU_LIST_PLAIN, 0);

         menu_list_push(info->list, info->path, info->label, info->type, info->directory_ptr, 0);
         menu_navigation_clear(nav, true);
         menu_entries_set_refresh();
         break;
      case DISPLAYLIST_HELP:
         menu_list_push(info->list, info->path, info->label, info->type, info->directory_ptr, 0);
         menu->push_start_screen = false;
         menu_display_fb_set_dirty();
         break;
      case DISPLAYLIST_MAIN_MENU:
      case DISPLAYLIST_SETTINGS:
         menu_list_clear(info->list);
         ret = menu_displaylist_parse_settings(menu, info, info->flags);
         need_push    = true;
         break;
      case DISPLAYLIST_OPTIONS_CHEATS:
         menu_list_clear(info->list);
         ret = menu_displaylist_parse_options_cheats(info);

         need_push    = true;
         break;
      case DISPLAYLIST_OPTIONS_REMAPPINGS:
         menu_list_clear(info->list);
         if (!global->has_set_input_descriptors)
         {
            if (!joykbd_enabled)
               rarch_main_msg_queue_push("Defaulting to RetroPad input "
                                         "descriptors.", 1, 180, true);
            input_remapping_set_default_desc();
         }
         ret = menu_displaylist_parse_options_remappings(info);

         need_push    = true;
         break;
      case DISPLAYLIST_SHADER_PARAMETERS:
#ifdef HAVE_SHADER_MANAGER
         menu_list_clear(info->list);
         {
            struct video_shader *shader = video_shader_driver_get_current_shader();

            ret = deferred_push_video_shader_parameters_common(info, shader,
                  MENU_SETTINGS_SHADER_PARAMETER_0);

            need_push = true;
         }
#endif
         break;
      case DISPLAYLIST_PERFCOUNTERS_CORE:
      case DISPLAYLIST_PERFCOUNTERS_FRONTEND:
         menu_list_clear(info->list);
         menu_displaylist_push_perfcounter(info,
               (type == DISPLAYLIST_PERFCOUNTERS_CORE) ?
               perf_counters_libretro : perf_counters_rarch,
               (type == DISPLAYLIST_PERFCOUNTERS_CORE) ?
               perf_ptr_libretro : perf_ptr_rarch ,
               (type == DISPLAYLIST_PERFCOUNTERS_CORE) ?
               MENU_SETTINGS_LIBRETRO_PERF_COUNTERS_BEGIN : 
               MENU_SETTINGS_PERF_COUNTERS_BEGIN);
         ret = 0;

         need_refresh = false;
         need_push    = true;
         break;
      case DISPLAYLIST_OPTIONS_SHADERS:
         menu_list_clear(info->list);
         ret = menu_displaylist_parse_shader_options(info);

         need_push    = true;
         break;
      case DISPLAYLIST_CORES_UPDATER:
#ifdef HAVE_NETWORKING
         buf = core_buf ? strdup(core_buf) : calloc(1,1);
         menu_list_clear(info->list);

         /* First entry is option to update info files */
         menu_list_push(info->list, "", "", MENU_FILE_DOWNLOAD_CORE_INFO, 0, 0);

         /* Add downloadable core file names */
         print_buf_lines(info->list, buf, core_len, MENU_FILE_DOWNLOAD_CORE);
         free(buf);

         if (info->list->size > 1)
         {
            /* Get display names and descriptions */
            menu_displaylist_get_downloadable_core_info(info->list);
            need_sort    = true;
            need_push    = true;
            need_refresh = true;
         }
         else menu_reset();
#endif
         break;
      case DISPLAYLIST_PERFCOUNTER_SELECTION:
         menu_list_clear(info->list);
         menu_list_push(info->list,
               menu_hash_to_str(MENU_LABEL_VALUE_FRONTEND_COUNTERS),
               menu_hash_to_str(MENU_LABEL_FRONTEND_COUNTERS),
               MENU_SETTING_ACTION, 0, 0);
         menu_list_push(info->list,
               menu_hash_to_str(MENU_LABEL_VALUE_CORE_COUNTERS),
               menu_hash_to_str(MENU_LABEL_CORE_COUNTERS),
               MENU_SETTING_ACTION, 0, 0);

         need_refresh = true;
         need_push    = true;
         break;
      case DISPLAYLIST_SETTINGS_ALL:
         menu_list_clear(info->list);
         menu_displaylist_realloc_settings(&menu->entries, SL_FLAG_ALL_SETTINGS);

#ifdef HAVE_OVERLAY
         if (settings->menu.show_overlay_menu)
            setting = menu_setting_find(menu_hash_to_str(MENU_LABEL_OVERLAY_SETTINGS));
         else
#endif
         setting = menu_setting_find(menu_hash_to_str(MENU_LABEL_VIDEO_SETTINGS));

         for (; setting->type != ST_NONE; setting++)
         {
            if (setting->type == ST_GROUP)
            {
               if (setting->flags & SD_FLAG_ADVANCED &&
                     !settings->menu.show_advanced_settings)
                  continue;
               menu_list_push(info->list, setting->short_description,
                     setting->name, menu_setting_set_flags(setting), 0, 0);
            }
         }

         need_push    = true;
         break;
      case DISPLAYLIST_OPTIONS_DISK:
         menu_list_clear(info->list);
         menu_list_push(info->list,
               menu_hash_to_str(MENU_LABEL_VALUE_DISK_CYCLE_TRAY_STATUS),
               menu_hash_to_str(MENU_LABEL_DISK_CYCLE_TRAY_STATUS),
               MENU_SETTINGS_CORE_DISK_OPTIONS_DISK_CYCLE_TRAY_STATUS, 0, 0);
         menu_list_push(info->list,
               menu_hash_to_str(MENU_LABEL_VALUE_DISK_INDEX),
               menu_hash_to_str(MENU_LABEL_DISK_INDEX),
               MENU_SETTINGS_CORE_DISK_OPTIONS_DISK_INDEX, 0, 0);
         menu_list_push(info->list,
               menu_hash_to_str(MENU_LABEL_VALUE_DISK_IMAGE_APPEND),
               menu_hash_to_str(MENU_LABEL_DISK_IMAGE_APPEND),
               MENU_SETTINGS_CORE_DISK_OPTIONS_DISK_IMAGE_APPEND, 0, 0);

         need_push    = true;
         break;
      case DISPLAYLIST_SYSTEM_INFO:
         menu_list_clear(info->list);
         menu_displaylist_parse_system_info(info);
         need_push    = true;
         break;
      case DISPLAYLIST_CORES_SUPPORTED:
         menu_list_clear(info->list);
         need_sort    = true;
         need_refresh = true;
         need_push    = true;

         {
            const core_info_t *core_info = NULL;
            core_info_list_get_supported_cores(global->core_info,
                  menu->deferred_path, &core_info, &list_size);

            if (list_size <= 0)
            {
               menu_list_push(info->list,
                     menu_hash_to_str(MENU_LABEL_VALUE_NO_CORES_AVAILABLE),
                     "",
                     0, 0, 0);
               return 0;
            }

            for (i = 0; i < list_size; i++)
            {
               menu_list_push(info->list, core_info[i].path,
                     menu_hash_to_str(MENU_LABEL_DETECT_CORE_LIST_OK),
                     MENU_FILE_CORE, 0, 0);
               menu_list_set_alt_at_offset(info->list, i,
                     core_info[i].display_name);
            }
         }
         break;
      case DISPLAYLIST_CORE_INFO:
         menu_list_clear(info->list);
         menu_displaylist_parse_core_info(info);
         need_push = true;
         break;
      case DISPLAYLIST_CORE_OPTIONS:
         menu_list_clear(info->list);
         
         menu_list_push(info->list,
               menu_hash_to_str(MENU_LABEL_VALUE_INPUT_REMAPPING),
               menu_hash_to_str(MENU_LABEL_INPUT_REMAPPING),
               MENU_SETTING_ACTION, 0, 0);

         if (!global->libretro_dummy && global->system.disk_control.get_num_images)
            menu_list_push(info->list,
                  menu_hash_to_str(MENU_LABEL_VALUE_DISK_CONTROL),
                  menu_hash_to_str(MENU_LABEL_DISK_CONTROL),
                  MENU_SETTING_ACTION_CORE_DISK_OPTIONS, 0, 0);

         if (settings->menu.show_cheat_options)
            menu_list_push(info->list,
                  menu_hash_to_str(MENU_LABEL_VALUE_CORE_CHEAT_OPTIONS),
                  menu_hash_to_str(MENU_LABEL_CORE_CHEAT_OPTIONS),
                  MENU_SETTING_ACTION, 0, 0);

         if (global->system.core_options)
         {
            size_t opt_size = core_option_size(global->system.core_options);
            
            if (opt_size > 0)
            {
               menu_list_push(info->list,
                     "Core Options Scope",
                     menu_hash_to_str(MENU_LABEL_OPTIONS_SCOPE),
                     MENU_SETTING_ACTION, 0, 0);

               menu_list_push(info->list,
                     "Load Options File",
                     menu_hash_to_str(MENU_LABEL_OPTIONS_FILE_LOAD),
                     MENU_SETTING_ACTION, 0, 0);
            }
         }

         /* clear category */
         core_option_set_category(global->system.core_options, NULL, NULL);

         menu_displaylist_push_core_options(info);

         need_push = true;
         break;
      case DISPLAYLIST_CORE_OPTIONS_CATEGORY:
         menu_list_clear(info->list);
         menu_displaylist_push_core_options(info);
         need_push = true;
         break;
      case DISPLAYLIST_DEFAULT:
         need_sort = true; /* fall-through */
      case DISPLAYLIST_CORES:
      case DISPLAYLIST_CORES_DETECTED:
      case DISPLAYLIST_SHADER_PASS:
      case DISPLAYLIST_SHADER_PRESET:
      case DISPLAYLIST_VIDEO_FILTERS:
      case DISPLAYLIST_AUDIO_FILTERS:
      case DISPLAYLIST_IMAGES:
      case DISPLAYLIST_FONTS:
      case DISPLAYLIST_CHEAT_FILES:
      case DISPLAYLIST_OPTIONS_FILES:
      case DISPLAYLIST_REMAP_FILES:
      case DISPLAYLIST_THEMES:
      case DISPLAYLIST_OVERLAYS:
         menu_list_clear(info->list);
         if (menu_displaylist_parse_generic(info, &need_sort) == 0)
         {
            need_refresh = true;
            need_push    = true;
         }

         if (menu->input.last_action != MENU_ACTION_OK)
            break;

         /* Set nav index leading to in-use path */
         switch (type)
         {
            case DISPLAYLIST_OVERLAYS:
               buf = settings->input.overlay;
               break;
            case DISPLAYLIST_SHADER_PRESET:
               buf = settings->video.shader_path;
               break;
            case DISPLAYLIST_VIDEO_FILTERS:
               buf = settings->video.softfilter_plugin;
               break;
            case DISPLAYLIST_AUDIO_FILTERS:
               buf = settings->audio.dsp_plugin;
               break;
            case DISPLAYLIST_OPTIONS_FILES:
               core_option_get_conf_path(info->path_b, core_options_scope);
               buf = info->path_b;
               break;
            case DISPLAYLIST_REMAP_FILES:
               buf = settings->input.remapping_path;
               break;
            case DISPLAYLIST_THEMES:
               buf = settings->menu.theme;
               break;
            case DISPLAYLIST_CHEAT_FILES:
               buf = settings->cheat_database;
               break;
            default:
               buf = NULL;
         }

         if (buf)
         {
            i = menu_displaylist_path_nav_idx(info->list, buf);
            if (i != 0)
               menu_navigation_set(nav, i, true);
         }
         break;
   }

   if (need_sort)
      file_list_sort_on_alt(info->list);

   if (need_push)
   {
      driver_t *driver                = driver_get_ptr();
      const ui_companion_driver_t *ui = ui_companion_get_ptr();

      if (need_refresh)
         menu_list_refresh(info->list);
      menu_driver_populate_entries(info->path, info->label, info->type);

      if (ui && driver)
         ui->notify_list_loaded(driver->ui_companion_data,
               info->list, info->menu_list);
   }

   return ret;
}

int menu_displaylist_push(file_list_t *list, file_list_t *menu_list)
{
   menu_file_list_cbs_t *cbs    = NULL;
   const char *path             = NULL;
   const char *label            = NULL;
   uint32_t          hash_label = 0;
   unsigned type                = 0;
   menu_displaylist_info_t info = {0};
   menu_entries_t *entries      = menu_entries_get_ptr();

   menu_list_get_last_stack(entries->menu_list, &path, &label, &type, NULL);

   info.list      = list;
   info.menu_list = menu_list;
   info.type      = type;
   strlcpy(info.path, path, sizeof(info.path));
   strlcpy(info.label, label, sizeof(info.label));

   hash_label     = menu_hash_calculate(label);

   if (!info.list)
      return -1;

   switch (hash_label)
   {
      case MENU_VALUE_MAIN_MENU:
         info.flags = SL_FLAG_MAIN_MENU | SL_FLAG_MAIN_MENU_SETTINGS;
         return menu_displaylist_push_list(&info, DISPLAYLIST_MAIN_MENU);
   }

   cbs = (menu_file_list_cbs_t*)
      menu_list_get_last_stack_actiondata(entries->menu_list);

   if (cbs->action_deferred_push)
      return cbs->action_deferred_push(&info);

   return 0;
}

/**
 * menu_displaylist_init:
 *
 * Creates and initializes menu display list.
 *
 * Returns: true (1) if successful, otherwise false (0).
 **/
bool menu_displaylist_init()
{
   menu_list_t       *menu_list = menu_list_get_ptr();
   menu_navigation_t       *nav = menu_navigation_get_ptr();
   menu_displaylist_info_t info = {0};
   if (!menu_list)
      return false;

   info.list  = menu_list->selection_buf;
   info.type  = MENU_SETTINGS;
   info.flags = SL_FLAG_MAIN_MENU | SL_FLAG_MAIN_MENU_SETTINGS;
   strlcpy(info.label, menu_hash_to_str(MENU_VALUE_MAIN_MENU), sizeof(info.label));

   menu_list_push(menu_list->menu_stack,
         info.path, info.label, info.type, info.flags, 0);
   menu_displaylist_push_list(&info, DISPLAYLIST_MAIN_MENU);
   menu_navigation_clear(nav, true);

   return true;
}
