/*  RetroArch - A frontend for libretro.
 *  Copyright (C) 2011-2015 - Daniel De Matteis
 *
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

#include <stdint.h>
#include <string.h>
#include <rhash.h>

#include "menu_hash.h"

#include "../configuration.h"

static const char *menu_hash_to_str_english(uint32_t hash)
{
   switch (hash)
   {
      case MENU_LABEL_VALUE_SHADER_APPLY_CHANGES:
         return "Save Preset";
      case MENU_LABEL_SHADER_APPLY_CHANGES:
         return "shader_apply_changes";
      case MENU_LABEL_REWIND_ENABLE:
         return "rewind_enable";
      case MENU_LABEL_VALUE_REWIND_ENABLE:
         return "Rewind Enable";
      case MENU_LABEL_DETECT_CORE_LIST:
         return "detect_core_list";
      case MENU_LABEL_VALUE_DETECT_CORE_LIST:
         return "Load ROM (Detect Core)";
      case MENU_LABEL_AUDIO_ENABLE:
         return "audio_enable";
      case MENU_LABEL_VALUE_AUDIO_ENABLE:
         return "Audio Enable";
      case MENU_LABEL_FPS_SHOW:
         return "fps_show";
      case MENU_LABEL_VALUE_FPS_SHOW:
         return "Display Framerate";
      case MENU_LABEL_AUDIO_MUTE:
         return "audio_mute_enable";
      case MENU_LABEL_VALUE_AUDIO_MUTE:
         return "Audio Mute";
      case MENU_LABEL_VIDEO_SHADER_PASS:
         return "video_shader_pass";
      case MENU_LABEL_AUDIO_VOLUME:
         return "audio_volume";
      case MENU_LABEL_VALUE_AUDIO_VOLUME:
         return "Adjust Volume";
      case MENU_LABEL_AUDIO_SYNC:
         return "audio_sync";
      case MENU_LABEL_VALUE_AUDIO_SYNC:
         return "Audio Sync";
      case MENU_LABEL_AUDIO_RATE_CONTROL_DELTA:
         return "audio_rate_control_delta";
      case MENU_LABEL_VALUE_AUDIO_RATE_CONTROL_DELTA:
         return "Audio Rate Control Delta";
      case MENU_LABEL_VIDEO_SHADER_FILTER_PASS:
         return "video_shader_filter_pass";
      case MENU_LABEL_VIDEO_SHADER_SCALE_PASS:
         return "video_shader_scale_pass";
      case MENU_LABEL_VALUE_VIDEO_SHADER_NUM_PASSES:
         return "Shader Passes";
      case MENU_LABEL_VIDEO_SHADER_NUM_PASSES:
         return "video_shader_num_passes";
      case MENU_LABEL_CONFIGURATIONS:
         return "configurations";
      case MENU_LABEL_VALUE_CONFIGURATIONS:
         return "Configuration Files";
      case MENU_LABEL_LOAD_OPEN_ZIP:
         return "load_open_zip";
      case MENU_LABEL_REWIND_GRANULARITY:
         return "rewind_granularity";
      case MENU_LABEL_VALUE_REWIND_GRANULARITY:
         return "Rewind Granularity";
      case MENU_LABEL_REMAP_FILE_LOAD:
         return "remap_file_load";
      case MENU_LABEL_REMAP_FILE_SAVE:
         return "remap_file_save";
      case MENU_LABEL_VALUE_REMAP_FILE_LOAD:
         return "Load Remap File";
      case MENU_LABEL_OPTIONS_SCOPE:
         return "options_scope";
      case MENU_LABEL_OPTIONS_FILE_LOAD:
         return "options_load";
      case MENU_LABEL_CUSTOM_RATIO:
         return "custom_ratio";
      case MENU_LABEL_VALUE_CUSTOM_RATIO:
         return "  Set Custom Aspect";
      case MENU_LABEL_VALUE_USE_THIS_DIRECTORY:
         return "<Use this directory>";
      case MENU_LABEL_USE_THIS_DIRECTORY:
         return "use_this_directory";
      case MENU_LABEL_CUSTOM_BIND:
         return "custom_bind";
      case MENU_LABEL_CUSTOM_BIND_ALL:
         return "custom_bind_all";
      case MENU_LABEL_DISK_OPTIONS:
         return "core_disc_options";
      case MENU_LABEL_VALUE_DISK_OPTIONS:
         return "Disc Control";
      case MENU_LABEL_VALUE_CORE_CHEAT_OPTIONS:
         return "Core Cheat Options";
      case MENU_LABEL_CORE_CHEAT_OPTIONS:
         return "core_cheat_options";
      case MENU_LABEL_CORE_OPTIONS:
         return "core_options";
      case MENU_LABEL_VALUE_CHEAT_FILE_LOAD:
         return "Cheat File Load";
      case MENU_LABEL_CHEAT_FILE_LOAD:
         return "cheat_file_load";
      case MENU_LABEL_CHEAT_FILE_SAVE_AS:
         return "cheat_file_save_as";
      case MENU_LABEL_VALUE_CHEAT_FILE_SAVE_AS:
         return "Cheat File Save As";
      case MENU_LABEL_FRONTEND_COUNTERS:
         return "frontend_counters";
      case MENU_LABEL_CORE_COUNTERS:
         return "core_counters";
      case MENU_LABEL_VALUE_CORE_COUNTERS:
         return "Core Counters";
      case MENU_LABEL_DISK_CYCLE_TRAY_STATUS:
         return "disk_cycle_tray_status";
      case MENU_LABEL_DISK_IMAGE_APPEND:
         return "disk_image_append";
      case MENU_LABEL_DEFERRED_CORE_LIST:
         return "deferred_core_list";
      case MENU_LABEL_VALUE_TAKE_SCREENSHOT:
         return "Take Screenshot";
      case MENU_LABEL_INFO:
         return "info";
      case MENU_LABEL_INFO_SCREEN:
         return "info_screen";
      case MENU_LABEL_VALUE_RESUME:
         return "Resume";
      case MENU_LABEL_DISK_INDEX:
         return "disk_index";
      case MENU_LABEL_VALUE_DISK_INDEX:
         return "Disc Index";
      case MENU_LABEL_VALUE_FRONTEND_COUNTERS:
         return "Frontend Counters";
      case MENU_LABEL_VALUE_DISK_IMAGE_APPEND:
         return "Load New Disc Image";
      case MENU_LABEL_VALUE_DISK_CYCLE_TRAY_STATUS:
         return "Cycle Disc Tray Status";
      case MENU_LABEL_VALUE_NO_CORE_INFORMATION_AVAILABLE:
         return "No core information available.";
      case MENU_LABEL_VALUE_NO_CORE_OPTIONS_AVAILABLE:
         return "No core options available.";
      case MENU_LABEL_VALUE_NO_CORES_AVAILABLE:
         return "No cores available.";
      case MENU_VALUE_NO_CORE:
         return "No Core";
      case MENU_LABEL_VALUE_CURSOR_MANAGER:
         return "Cursor Manager";
      case MENU_VALUE_RECORDING_SETTINGS:
         return "Recording Settings";
      case MENU_VALUE_MAIN_MENU:
         return "Main Menu";
      case MENU_LABEL_SETTINGS:
         return "settings";
      case MENU_LABEL_VALUE_SETTINGS:
         return "Settings";
      case MENU_LABEL_MENU_SETTINGS:
         return "Menu Settings";
      case MENU_LABEL_INPUT_SETTINGS:
         return "Input Settings";
      case MENU_LABEL_VIDEO_SETTINGS:
         return "Video Settings";
      case MENU_LABEL_QUIT_RETROARCH:
         return "quit_retroarch";
      case MENU_LABEL_VALUE_QUIT_RETROARCH:
         return "Quit RetroArch Lite";
      case MENU_LABEL_VALUE_HELP:
         return "Help";
      case MENU_LABEL_HELP:
         return "help";
      case MENU_LABEL_RESTART_CONTENT:
         return "restart_content";
      case MENU_LABEL_TAKE_SCREENSHOT:
         return "take_screenshot";
      case MENU_LABEL_CORE_UPDATER_LIST:
         return "core_updater_list";
      case MENU_LABEL_VALUE_CORE_UPDATER_LIST:
         return "Core Updater";
      case MENU_LABEL_SYSTEM_INFORMATION:
         return "system_information";
      case MENU_LABEL_VALUE_SYSTEM_INFORMATION:
         return "System Information";
      case MENU_LABEL_OPTIONS:
         return "core_options";
      case MENU_LABEL_VALUE_OPTIONS:
         return "Core Options";
      case MENU_LABEL_CORE_INFORMATION:
         return "core_information";
      case MENU_LABEL_VALUE_CORE_INFORMATION:
         return "Core Information";
      case MENU_LABEL_VALUE_DIRECTORY_NOT_FOUND:
         return "Directory not found.";
      case MENU_LABEL_VALUE_NO_ITEMS:
         return "No items.";
      case MENU_LABEL_CORE_LIST:
         return "Load Core";
      case MENU_LABEL_LOAD_CONTENT:
         return "load_content";
      case MENU_LABEL_VALUE_LOAD_CONTENT:
         return "Load ROM";
      case MENU_LABEL_UNLOAD_CORE:
         return "unload_core";
      case MENU_LABEL_VALUE_UNLOAD_CORE:
         return "Unload Core";
      case MENU_LABEL_PERFORMANCE_COUNTERS:
         return "performance_counters";
      case MENU_LABEL_VALUE_PERFORMANCE_COUNTERS:
         return "Performance Counters";
      case MENU_LABEL_SAVE_STATE:
         return "savestate";
      case MENU_LABEL_VALUE_SAVE_STATE:
         return "Save State";
      case MENU_LABEL_LOAD_STATE:
         return "loadstate";
      case MENU_LABEL_VALUE_LOAD_STATE:
         return "Load State";
      case MENU_LABEL_VALUE_RESUME_CONTENT:
         return "Resume ROM";
      case MENU_LABEL_RESUME_CONTENT:
         return "resume_content";
      case MENU_LABEL_DRIVER_SETTINGS:
         return "Driver Settings";
      case MENU_LABEL_OVERLAY_SETTINGS:
         return "Onscreen Overlay Settings";
      case MENU_LABEL_VALUE_UNABLE_TO_READ_COMPRESSED_FILE:
         return "Unable to read compressed file.";
      case MENU_LABEL_OVERLAY_SCALE:
         return "input_overlay_scale";
      case MENU_LABEL_VALUE_OVERLAY_SCALE:
         return "  Scale";
      case MENU_LABEL_VALUE_OVERLAY_PRESET:
         return "Overlay Preset";
      case MENU_LABEL_OVERLAY_PRESET:
         return "input_overlay";
      case MENU_LABEL_KEYBOARD_OVERLAY_PRESET:
         return "input_osk_overlay";
      case MENU_LABEL_AUDIO_DEVICE:
         return "audio_device";
      case MENU_LABEL_AUDIO_LATENCY:
         return "audio_latency";
      case MENU_LABEL_VALUE_AUDIO_LATENCY:
         return "Audio Latency";
      case MENU_LABEL_VALUE_AUDIO_DEVICE:
         return "Audio Device";
      case MENU_LABEL_VALUE_KEYBOARD_OVERLAY_PRESET:
         return "Keyboard Overlay";
      case MENU_LABEL_OVERLAY_OPACITY:
         return "input_overlay_opacity";
      case MENU_LABEL_VALUE_OVERLAY_OPACITY:
         return "Overlay Opacity";
      case MENU_LABEL_MENU_THEME:
         return "menu_theme";
      case MENU_LABEL_VALUE_MENU_THEME:
         return "Menu Theme";
      case MENU_LABEL_MENU_WALLPAPER:
         return "menu_wallpaper";
      case MENU_LABEL_VALUE_MENU_WALLPAPER:
         return "Menu Wallpaper test";
      case MENU_LABEL_DYNAMIC_WALLPAPER:
         return "menu_dynamic_wallpaper_enable";
      case MENU_LABEL_VALUE_DYNAMIC_WALLPAPER:
         return "Dynamic Wallpaper";
      case MENU_LABEL_BOXART:
         return "menu_boxart_enable";
      case MENU_LABEL_VALUE_BOXART:
         return "Display Boxart";
      case MENU_LABEL_CORE_INPUT_REMAPPING_OPTIONS:
         return "core_input_remapping_options";
      case MENU_LABEL_VALUE_CORE_INPUT_REMAPPING_OPTIONS:
         return "Input Remapping";
      case MENU_LABEL_VALUE_VIDEO_OPTIONS:
         return "Video Options";
      case MENU_LABEL_VALUE_SHADER_OPTIONS:
         return "  Shader Options";
      case MENU_LABEL_SHADER_OPTIONS:
         return "shader_options";
      case MENU_LABEL_VIDEO_SHADER_PARAMETERS:
         return "video_shader_parameters";
      case MENU_LABEL_VALUE_VIDEO_SHADER_PRESET_SAVE_AS:
         return "Save Preset As";
      case MENU_LABEL_VIDEO_SHADER_PRESET_SAVE_AS:
         return "video_shader_preset_save_as";
      case MENU_LABEL_VALUE_NO_SHADER_PARAMETERS:
         return "No shader parameters.";
      case MENU_LABEL_VIDEO_SHADER_PRESET:
         return "video_shader_preset";
      case MENU_LABEL_VALUE_VIDEO_SHADER_PRESET:
         return "HW Shader Preset";
      case MENU_LABEL_VIDEO_FILTER:
         return "video_filter";
      case MENU_LABEL_VALUE_VIDEO_FILTER:
         return "Video Filter";
      case MENU_LABEL_DEFERRED_VIDEO_FILTER:
         return "deferred_video_filter";
      case MENU_LABEL_DEFERRED_CORE_UPDATER_LIST:
         return "deferred_core_updater_list";
      case MENU_LABEL_AUDIO_DSP_PLUGIN:
         return "audio_dsp_plugin";
      case MENU_LABEL_VALUE_AUDIO_DSP_PLUGIN:
         return "Audio DSP Plugin";
      case MENU_LABEL_VALUE_STARTING_DOWNLOAD:
         return "Starting download: ";
      case MENU_LABEL_SHARED_CONTEXT:
         return "video_shared_context";
      case MENU_LABEL_DUMMY_ON_CORE_SHUTDOWN:
         return "dummy_on_core_shutdown";
      case MENU_LABEL_LIBRETRO_DEVICE_SCOPE:
         return "libretro_device_scope";
      case MENU_VALUE_SECONDS:
         return "seconds";
      case MENU_VALUE_OFF:
         return "OFF";
      case MENU_VALUE_ON:
         return "ON";
      default:
         break;
   }

   return "null";
}

const char *menu_hash_to_str(uint32_t hash)
{
#if 0
   const char *ret = NULL;
#endif
   settings_t *settings = config_get_ptr();

   if (!settings)
      return "null";

#if 0
   switch (settings->user_language)
   {
      default:
         break;
   }

   if (ret && strcmp(ret, "null") != 0)
      return ret;
#endif

   return menu_hash_to_str_english(hash);
}

uint32_t menu_hash_calculate(const char *s)
{
   return djb2_calculate(s);
}
