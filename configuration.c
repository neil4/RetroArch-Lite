/*  RetroArch - A frontend for libretro.
 *  Copyright (C) 2010-2014 - Hans-Kristian Arntzen
 *  Copyright (C) 2011-2015 - Daniel De Matteis
 *                2019 - Neil Fore
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

#include <file/config_file.h>
#include <compat/strl.h>
#include <compat/posix_string.h>
#include <ctype.h>
#include "config.def.h"
#include <file/file_path.h>
#include "input/input_common.h"
#include "input/input_keymaps.h"
#include "input/input_remapping.h"
#include "input/input_joypad.h"
#include "configuration.h"
#include "general.h"

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#define CONFIG_GET_BOOL_BASE(conf, base, var, key) do { \
   bool tmp = false; \
   if (config_get_bool(conf, key, &tmp)) \
      base->var = tmp; \
} while(0)

#define CONFIG_GET_INT_BASE(conf, base, var, key) do { \
   int tmp = 0; \
   if (config_get_int(conf, key, &tmp)) \
      base->var = tmp; \
} while(0)

#define CONFIG_GET_UINT64_BASE(conf, base, var, key) do { \
   uint64_t tmp = 0; \
   if (config_get_int(conf, key, &tmp)) \
      base->var = tmp; \
} while(0)

#define CONFIG_GET_HEX_BASE(conf, base, var, key) do { \
   unsigned tmp = 0; \
   if (config_get_hex(conf, key, &tmp)) \
      base->var = tmp; \
} while(0)

#define CONFIG_GET_FLOAT_BASE(conf, base, var, key) do { \
   float tmp = 0.0f; \
   if (config_get_float(conf, key, &tmp)) \
      base->var = tmp; \
} while(0)

#define CONFIG_GET_STRING_BASE(conf, base, var, key) config_get_array(conf, key, base->var, sizeof(base->var))
#define CONFIG_GET_PATH_BASE(conf, base, var, key)   config_get_path (conf, key, base->var, sizeof(base->var))

static settings_t *g_config = NULL;
struct defaults g_defaults;

static config_file_t* scoped_conf[NUM_SETTING_SCOPES];

struct enum_lut scope_lut[NUM_SETTING_SCOPES] = {
   { "Global", GLOBAL },
   { "This Core", THIS_CORE },
   { "This ROM directory", THIS_CONTENT_DIR },
   { "This ROM only", THIS_CONTENT_ONLY }
};

bool settings_touched = false;
bool scoped_settings_touched = false;

/**
 * config_get_default_audio:
 *
 * Gets default audio driver.
 *
 * Returns: Default audio driver.
 **/
const char *config_get_default_audio(void)
{
   switch (AUDIO_DEFAULT_DRIVER)
   {
      case AUDIO_RSOUND:
         return "rsound";
      case AUDIO_OSS:
         return "oss";
      case AUDIO_ALSA:
         return "alsa";
      case AUDIO_ALSATHREAD:
         return "alsathread";
      case AUDIO_ROAR:
         return "roar";
      case AUDIO_COREAUDIO:
         return "coreaudio";
      case AUDIO_AL:
         return "openal";
      case AUDIO_SL:
         return "opensl";
      case AUDIO_SDL:
         return "sdl";
      case AUDIO_SDL2:
         return "sdl2";
      case AUDIO_DSOUND:
         return "dsound";
      case AUDIO_XAUDIO:
         return "xaudio";
      case AUDIO_PULSE:
         return "pulse";
      case AUDIO_EXT:
         return "ext";
      case AUDIO_XENON360:
         return "xenon360";
      case AUDIO_PS3:
         return "ps3";
      case AUDIO_WII:
         return "gx";
      case AUDIO_PSP1:
         return "psp1";
      case AUDIO_CTR:
         return "ctr";
      case AUDIO_RWEBAUDIO:
         return "rwebaudio";
      default:
         break;
   }

   return "null";
}

const char *config_get_default_record(void)
{
   switch (RECORD_DEFAULT_DRIVER)
   {
      case RECORD_FFMPEG:
         return "ffmpeg";
      default:
         break;
   }

   return "null";
}

/**
 * config_get_default_audio_resampler:
 *
 * Gets default audio resampler driver.
 *
 * Returns: Default audio resampler driver.
 **/
const char *config_get_default_audio_resampler(void)
{
   switch (AUDIO_DEFAULT_RESAMPLER_DRIVER)
   {
      case AUDIO_RESAMPLER_CC:
         return "cc";
      case AUDIO_RESAMPLER_SINC:
         return "sinc";
      case AUDIO_RESAMPLER_NEAREST:
         return "nearest";
      default:
         break;
   }

   return "null";
}

/**
 * config_get_default_video:
 *
 * Gets default video driver.
 *
 * Returns: Default video driver.
 **/
const char *config_get_default_video(void)
{
   switch (VIDEO_DEFAULT_DRIVER)
   {
      case VIDEO_GL:
         return "gl";
      case VIDEO_WII:
         return "gx";
      case VIDEO_XENON360:
         return "xenon360";
      case VIDEO_XDK_D3D:
      case VIDEO_D3D9:
         return "d3d";
      case VIDEO_PSP1:
         return "psp1";
      case VIDEO_VITA:
         return "vita";
      case VIDEO_CTR:
         return "ctr";
      case VIDEO_XVIDEO:
         return "xvideo";
      case VIDEO_SDL:
         return "sdl";
      case VIDEO_SDL2:
         return "sdl2";
      case VIDEO_EXT:
         return "ext";
      case VIDEO_VG:
         return "vg";
      case VIDEO_OMAP:
         return "omap";
      case VIDEO_EXYNOS:
         return "exynos";
      case VIDEO_DISPMANX:
         return "dispmanx";
      case VIDEO_SUNXI:
         return "sunxi";
      default:
         break;
   }

   return "null";
}

/**
 * config_get_default_input:
 *
 * Gets default input driver.
 *
 * Returns: Default input driver.
 **/
const char *config_get_default_input(void)
{
   switch (INPUT_DEFAULT_DRIVER)
   {
      case INPUT_ANDROID:
         return "android";
      case INPUT_PS3:
         return "ps3";
      case INPUT_PSP:
         return "psp";
      case INPUT_CTR:
         return "ctr";
      case INPUT_SDL:
         return "sdl";
      case INPUT_SDL2:
         return "sdl2";
      case INPUT_DINPUT:
         return "dinput";
      case INPUT_X:
         return "x";
      case INPUT_WAYLAND:
         return "wayland";
      case INPUT_XENON360:
         return "xenon360";
      case INPUT_XINPUT:
         return "xinput";
      case INPUT_WII:
         return "gx";
      case INPUT_LINUXRAW:
         return "linuxraw";
      case INPUT_UDEV:
         return "udev";
      case INPUT_COCOA:
         return "cocoa";
      case INPUT_QNX:
          return "qnx_input";
      case INPUT_RWEBINPUT:
          return "rwebinput";
      default:
         break;
   }

   return "null";
}

/**
 * config_get_default_joypad:
 *
 * Gets default input joypad driver.
 *
 * Returns: Default input joypad driver.
 **/
const char *config_get_default_joypad(void)
{
   switch (JOYPAD_DEFAULT_DRIVER)
   {
      case JOYPAD_PS3:
         return "ps3";
      case JOYPAD_XINPUT:
         return "xinput";
      case JOYPAD_GX:
         return "gx";
      case JOYPAD_XDK:
         return "xdk";
      case JOYPAD_PSP:
         return "psp";
      case JOYPAD_CTR:
         return "ctr";
      case JOYPAD_DINPUT:
         return "dinput";
      case JOYPAD_UDEV:
         return "udev";
      case JOYPAD_LINUXRAW:
         return "linuxraw";
      case JOYPAD_ANDROID:
         return "android";
      case JOYPAD_SDL:
#ifdef HAVE_SDL2
         return "sdl2";
#else
         return "sdl";
#endif
      case JOYPAD_HID:
         return "hid";
      case JOYPAD_QNX:
         return "qnx";
      default:
         break;
   }

   return "null";
}

#ifdef HAVE_MENU
/**
 * config_get_default_menu:
 *
 * Gets default menu driver.
 *
 * Returns: Default menu driver.
 **/
const char *config_get_default_menu(void)
{
   switch (MENU_DEFAULT_DRIVER)
   {
      case MENU_RGUI:
         return "rgui";
      case MENU_RMENU:
         return "rmenu";
      case MENU_RMENU_XUI:
         return "rmenu_xui";
      case MENU_GLUI:
         return "glui";
      case MENU_XMB:
         return "xmb";
      default:
         break;
   }

   return "null";
}
#endif

/**
 * config_get_default_camera:
 *
 * Gets default camera driver.
 *
 * Returns: Default camera driver.
 **/
const char *config_get_default_camera(void)
{
   switch (CAMERA_DEFAULT_DRIVER)
   {
      case CAMERA_V4L2:
         return "video4linux2";
      case CAMERA_RWEBCAM:
         return "rwebcam";
      case CAMERA_ANDROID:
         return "android";
      case CAMERA_AVFOUNDATION:
         return "avfoundation";
      default:
         break;
   }

   return "null";
}

/**
 * config_get_default_location:
 *
 * Gets default location driver.
 *
 * Returns: Default location driver.
 **/
const char *config_get_default_location(void)
{
   switch (LOCATION_DEFAULT_DRIVER)
   {
      case LOCATION_ANDROID:
         return "android";
      case LOCATION_CORELOCATION:
         return "corelocation";
      default:
         break;
   }

   return "null";
}

void config_check_overlay_preset()
{
   settings_t* settings = config_get_ptr();
   global_t* global     = global_get_ptr();

   if (*settings->input.overlay && !path_file_exists(settings->input.overlay))
      fill_pathname_join(settings->input.overlay,
                         global->overlay_dir, "DualShock.cfg",
                         sizeof(settings->input.overlay));
}

/**
 * config_set_defaults:
 *
 * Set 'default' configuration values.
 **/
static void config_set_defaults(void)
{
   unsigned i, j;
   settings_t *settings            = config_get_ptr();
   global_t   *global              = global_get_ptr();
   const char *def_video           = config_get_default_video();
   const char *def_audio           = config_get_default_audio();
   const char *def_audio_resampler = config_get_default_audio_resampler();
   const char *def_input           = config_get_default_input();
   const char *def_joypad          = config_get_default_joypad();
#ifdef HAVE_MENU
   const char *def_menu            = config_get_default_menu();
#endif
   const char *def_camera          = config_get_default_camera();
   const char *def_location        = config_get_default_location();
   const char *def_record          = config_get_default_record();

   if (def_camera)
      strlcpy(settings->camera.driver,
            def_camera, sizeof(settings->camera.driver));
   if (def_location)
      strlcpy(settings->location.driver,
            def_location, sizeof(settings->location.driver));
   if (def_video)
      strlcpy(settings->video.driver,
            def_video, sizeof(settings->video.driver));
   if (def_audio)
      strlcpy(settings->audio.driver,
            def_audio, sizeof(settings->audio.driver));
   if (def_audio_resampler)
      strlcpy(settings->audio.resampler,
            def_audio_resampler, sizeof(settings->audio.resampler));
   if (def_input)
      strlcpy(settings->input.driver,
            def_input, sizeof(settings->input.driver));
   if (def_joypad)
      strlcpy(settings->input.joypad_driver,
            def_joypad, sizeof(settings->input.joypad_driver));
   if (def_record)
      strlcpy(settings->record.driver,
            def_record, sizeof(settings->record.driver));
#ifdef HAVE_MENU
   if (def_menu)
      strlcpy(settings->menu.driver,
            def_menu,  sizeof(settings->menu.driver));
#endif

   settings->load_dummy_on_core_shutdown = load_dummy_on_core_shutdown;

   settings->video.scale                 = scale;
   settings->video.fullscreen            = global->force_fullscreen ? true : fullscreen;
   settings->video.windowed_fullscreen   = windowed_fullscreen;
   settings->video.monitor_index         = monitor_index;
   settings->video.fullscreen_x          = fullscreen_x;
   settings->video.fullscreen_y          = fullscreen_y;
   settings->video.disable_composition   = disable_composition;
   settings->video.vsync                 = vsync;
   settings->video.hard_sync             = hard_sync;
   settings->video.hard_sync_frames      = hard_sync_frames;
   settings->video.frame_delay           = frame_delay;
   settings->video.black_frame_insertion = black_frame_insertion;
   settings->video.swap_interval         = swap_interval;
   settings->video.fake_swap_interval    = fake_swap_interval;
   settings->video.threaded              = video_threaded;

   if (g_defaults.settings.video_threaded_enable != video_threaded)
      settings->video.threaded           = g_defaults.settings.video_threaded_enable;

   settings->video.shared_context              = video_shared_context;
   settings->video.force_srgb_disable          = false;
#ifdef GEKKO
   settings->video.viwidth                     = video_viwidth;
   settings->video.vfilter                     = video_vfilter;
#endif
   settings->video.smooth                      = video_smooth;
   settings->video.force_aspect                = force_aspect;
   settings->video.scale_integer               = scale_integer;
   settings->video.crop_overscan               = crop_overscan;
   settings->video.aspect_ratio                = aspect_ratio;
   settings->video.aspect_ratio_auto           = aspect_ratio_auto; /* Let implementation decide if automatic, or 1:1 PAR. */
   settings->video.aspect_ratio_idx            = aspect_ratio_idx;
   settings->video.allow_rotate                = allow_rotate;

   settings->video.font_enable                 = font_enable;
   settings->video.font_size                   = font_size;
   settings->video.msg_pos_x                   = message_pos_offset_x;
   settings->video.msg_pos_y                   = message_pos_offset_y;

   settings->video.msg_color_r                 = ((message_color >> 16) & 0xff) / 255.0f;
   settings->video.msg_color_g                 = ((message_color >>  8) & 0xff) / 255.0f;
   settings->video.msg_color_b                 = ((message_color >>  0) & 0xff) / 255.0f;

   settings->video.refresh_rate                = refresh_rate;

   if (g_defaults.settings.video_refresh_rate > 0.0 &&
         g_defaults.settings.video_refresh_rate != refresh_rate)
      settings->video.refresh_rate             = g_defaults.settings.video_refresh_rate;

   settings->video.post_filter_record          = post_filter_record;
   settings->video.gpu_record                  = gpu_record;
   settings->video.gpu_screenshot              = gpu_screenshot;
   settings->video.rotation                    = ORIENTATION_NORMAL;

   settings->audio.enable                      = audio_enable;
   settings->audio.mute_enable                 = false;
   settings->audio.out_rate                    = out_rate;
   settings->audio.block_frames                = 0;
   if (audio_device)
      strlcpy(settings->audio.device,
            audio_device, sizeof(settings->audio.device));

   if (!g_defaults.settings.out_latency)
      g_defaults.settings.out_latency          = out_latency;

   settings->audio.latency                     = g_defaults.settings.out_latency;
   settings->audio.sync                        = audio_sync;
   settings->audio.rate_control                = rate_control;
   settings->audio.rate_control_delta          = rate_control_delta;
   settings->audio.max_timing_skew             = max_timing_skew;
   settings->audio.volume                      = audio_volume;

   audio_driver_set_volume_gain(db_to_gain(settings->audio.volume));

   settings->rewind_enable                     = rewind_enable;
   settings->rewind_buffer_size                = rewind_buffer_size;
   settings->rewind_granularity                = rewind_granularity;
   settings->slowmotion_ratio                  = slowmotion_ratio;
   settings->fastforward_ratio                 = fastforward_ratio;
   settings->throttle_using_core_fps           = throttle_using_core_fps;
   settings->pause_nonactive                   = pause_nonactive;
   settings->autosave_interval                 = autosave_interval;

   settings->block_sram_overwrite              = block_sram_overwrite;
   settings->savestate_auto_index              = savestate_auto_index;
   settings->savestate_auto_save               = savestate_auto_save;
   settings->savestate_auto_load               = savestate_auto_load;
   settings->network_cmd_enable                = network_cmd_enable;
   settings->network_cmd_port                  = network_cmd_port;
   settings->stdin_cmd_enable                  = stdin_cmd_enable;
   settings->libretro_log_level                = libretro_log_level;

#ifdef HAVE_MENU
   settings->menu_show_start_screen            = menu_show_start_screen;
   settings->menu.pause_libretro               = true;
   settings->menu.mouse.enable                 = menu_mouse_support;
   settings->menu.pointer.enable               = pointer_enable;
   settings->menu.timedate_enable              = true;
   settings->menu.core_enable                  = true;
   settings->menu.dynamic_wallpaper_enable     = false;
   settings->menu.boxart_enable                = false;
   *settings->menu.theme_dir                   = '\0';
   *settings->menu.theme                       = '\0';
   settings->menu.wallpaper_opacity            = wallpaper_opacity;
   settings->menu.show_advanced_settings       = show_advanced_settings;
   settings->menu.ticker_speed                 = menu_ticker_speed;
#ifdef HAVE_RGUI
   settings->menu.rgui_particle_effect_speed_factor = 1.0f;
#endif

   settings->menu.dpi.override_enable          = menu_dpi_override_enable;
   settings->menu.dpi.override_value           = menu_dpi_override_value;

   settings->menu.navigation.wraparound.vertical_enable                 = true;
   settings->menu.navigation.browser.filter.supported_extensions_enable = true;
   settings->menu.mame_titles                  = mame_titles;
#ifdef HAVE_OVERLAY
   settings->menu.show_overlay_menu            = show_overlay_menu;
#endif
   settings->menu.show_frame_throttle_menu     = show_frame_throttle_menu;
   settings->menu.show_netplay_menu            = show_netplay_menu;
   settings->menu.show_saving_menu             = show_saving_menu;
   settings->menu.show_core_menu               = show_core_menu;
   settings->menu.show_driver_menu             = show_driver_menu;
   settings->menu.show_ui_menu                 = show_ui_menu;
   settings->menu.show_logging_menu            = show_logging_menu;
   settings->menu.show_hotkey_menu             = show_hotkey_menu;
   settings->menu.show_rewind_menu             = show_rewind_menu;
#ifndef EXTERNAL_LAUNCHER
   settings->menu.show_core_updater            = show_core_updater;
#endif
   settings->menu.show_core_info               = menu_show_core_info;
   settings->menu.show_system_info             = menu_show_system_info;
   settings->menu.show_cheat_options           = show_cheat_options;
   settings->menu.show_configuration_menu      = show_configuration_menu;
   settings->menu.show_user_menu               = show_user_menu;
   settings->menu.show_directory_menu          = show_directory_menu;
   settings->menu.show_privacy_menu            = show_privacy_menu;
   settings->menu.show_recording_menu          = show_recording_menu;
   settings->menu.show_core_updater_menu       = show_core_updater_menu;
#endif /* #ifdef HAVE_MENU */

   settings->ui.companion_start_on_boot             = true;
   settings->ui.menubar_enable                      = true;
   settings->ui.suspend_screensaver_enable          = true;

   settings->location.allow                         = false;
   settings->camera.allow                           = false;

   settings->input.autoconfig_descriptor_label_show = true;
   settings->input.remap_binds_enable               = true;
   settings->input.max_users                        = 2;
   settings->input.rumble_enable                    = false;

   rarch_assert(sizeof(settings->input.binds[0]) >= sizeof(retro_keybinds_1));
   rarch_assert(sizeof(settings->input.binds[1]) >= sizeof(retro_keybinds_rest));

   memcpy(settings->input.binds[0], retro_keybinds_1, sizeof(retro_keybinds_1));

   for (i = 1; i < MAX_USERS; i++)
      memcpy(settings->input.binds[i], retro_keybinds_rest,
            sizeof(retro_keybinds_rest));

   input_remapping_set_defaults();

   for (i = 0; i < MAX_USERS; i++)
   {
      for (j = 0; j < RARCH_BIND_LIST_END; j++)
      {
         settings->input.autoconf_binds[i][j].joykey  = NO_BTN;
         settings->input.autoconf_binds[i][j].joyaxis = AXIS_NONE;
      }
   }
   memset(settings->input.autoconfigured, 0,
         sizeof(settings->input.autoconfigured));

   /* Verify that binds are in proper order. */
   for (i = 0; i < MAX_USERS; i++)
      for (j = 0; j < RARCH_BIND_LIST_END; j++)
      {
         if (settings->input.binds[i][j].valid)
            rarch_assert(j == settings->input.binds[i][j].id);
      }

   settings->input.netplay_client_swap_input       = netplay_client_swap_input;
   
   settings->input.autodetect_enable               = input_autodetect_enable;
   *settings->input.keyboard_layout                = '\0';

#ifdef HAVE_OVERLAY
   settings->input.overlay_opacity                 = overlay_opacity;
   settings->input.dpad_diagonal_sensitivity       = dpad_diagonal_sensitivity;
   settings->input.abxy_diagonal_sensitivity       = abxy_diagonal_sensitivity;
   settings->input.dpad_method                     = VECTOR;
   settings->input.abxy_method                     = VECTOR_AND_AREA;
   settings->input.touch_ellipse_magnify           = 1.0f;
   settings->input.vibrate_time                    = input_vibrate_time;
   settings->input.overlay_enable                  = true;
   settings->input.overlay_scale                   = 1.0f;
   settings->input.overlay_adjust_aspect           = true;
   settings->input.overlay_aspect_ratio_index      = OVERLAY_ASPECT_RATIO_AUTO_INDEX;
   settings->input.overlay_bisect_aspect_ratio     = overlay_bisect_aspect_ratio;
   settings->input.overlay_shift_y_lock_edges      = overlay_shift_y_lock_edges;
   settings->osk.enable                            = input_osk_overlay_enable;
#endif

   strlcpy(settings->network.buildbot_url, buildbot_server_url,
         sizeof(settings->network.buildbot_url));
   strlcpy(settings->network.buildbot_assets_url, buildbot_assets_server_url,
         sizeof(settings->network.buildbot_assets_url));
   settings->network.buildbot_auto_extract_archive = true;

   for (i = 0; i < MAX_USERS; i++)
   {
      settings->input.joypad_map[i] = i;
      if (!global->has_set_libretro_device[i])
         settings->input.libretro_device[i] = RETRO_DEVICE_JOYPAD;
   }

   settings->input.axis_threshold              = axis_threshold;
   settings->input.analog_dpad_mode            = ANALOG_DPAD_NONE;
   settings->input.analog_diagonal_sensitivity = analog_diagonal_sensitivity;
   settings->input.analog_dpad_deadzone        = analog_dpad_deadzone;

   settings->core.set_supports_no_game_enable  = true;

   video_viewport_reset_custom();

   /* Make sure settings from other configs carry over into defaults
    * for another config. */
   if (!global->has_set_save_path)
      *global->savefile_dir = '\0';
   if (!global->has_set_state_path)
      *global->savestate_dir = '\0';

   *settings->libretro_info_path = '\0';
   if (!global->has_set_libretro_directory)
      *settings->libretro_directory = '\0';

   if (!global->has_set_ups_pref)
      global->ups_pref = false;
   if (!global->has_set_bps_pref)
      global->bps_pref = false;
   if (!global->has_set_ips_pref)
      global->ips_pref = false;

   *global->record.output_dir = '\0';
   *global->record.config_dir = '\0';

   *settings->cheat_database = '\0';
   *settings->cheat_settings_path = '\0';
   *settings->screenshot_directory = '\0';
   *settings->system_directory = '\0';
   *settings->extraction_directory = '\0';
   *settings->input_remapping_directory = '\0';
   *settings->input.autoconfig_dir = '\0';
   *settings->input.overlay = '\0';
   *settings->core_assets_directory = '\0';
   *settings->assets_directory = '\0';
   *settings->dynamic_wallpapers_directory = '\0';
   *settings->boxarts_directory = '\0';
   *settings->video.shader_path = '\0';
   *settings->video.shader_dir = '\0';
   *settings->video.filter_dir = '\0';
   *settings->audio.filter_dir = '\0';
   *settings->video.softfilter_plugin = '\0';
   *settings->audio.dsp_plugin = '\0';
#ifdef HAVE_MENU
   *settings->menu_content_directory = '\0';
   *settings->core_content_directory = '\0';
   *settings->menu_config_directory = '\0';
#endif
   settings->auto_remaps_enable = default_auto_remaps_enable;

   settings->sort_savefiles_enable = default_sort_savefiles_enable;
   settings->sort_savestates_enable = default_sort_savestates_enable;

   settings->savestate_file_compression = true;
   settings->sram_file_compression = true;

   settings->menu_ok_btn          = default_menu_btn_ok;
   settings->menu_cancel_btn      = default_menu_btn_cancel;
   settings->menu_search_btn      = default_menu_btn_search;
   settings->menu_default_btn     = default_menu_btn_default;
   settings->menu_info_btn        = default_menu_btn_info;
   settings->menu_scroll_down_btn = default_menu_btn_scroll_down;
   settings->menu_scroll_up_btn   = default_menu_btn_scroll_up;

   settings->user_language = 0;

   global->console.sound.system_bgm_enable = false;
#ifdef RARCH_CONSOLE
   global->console.screen.gamma_correction = DEFAULT_GAMMA;
   global->console.screen.resolutions.current.id = 0;
   global->console.sound.mode = SOUND_MODE_NORMAL;
#endif

   if (*g_defaults.extraction_dir)
      strlcpy(settings->extraction_directory,
            g_defaults.extraction_dir, sizeof(settings->extraction_directory));
   if (*g_defaults.audio_filter_dir)
      strlcpy(settings->audio.filter_dir,
            g_defaults.audio_filter_dir, sizeof(settings->audio.filter_dir));
   if (*g_defaults.video_filter_dir)
      strlcpy(settings->video.filter_dir,
            g_defaults.video_filter_dir, sizeof(settings->video.filter_dir));
   if (*g_defaults.assets_dir)
      strlcpy(settings->assets_directory,
            g_defaults.assets_dir, sizeof(settings->assets_directory));
   if (*g_defaults.core_dir)
   {
      fill_pathname_expand_special(settings->libretro_directory,
            g_defaults.core_dir, sizeof(settings->libretro_directory));
      global->has_set_libretro_directory = true;
   }
   if (*g_defaults.core_path)
      strlcpy(settings->libretro, g_defaults.core_path,
            sizeof(settings->libretro));
   if (*g_defaults.cheats_dir)
      strlcpy(settings->cheat_database, g_defaults.cheats_dir,
            sizeof(settings->cheat_database));
   if (*g_defaults.core_info_dir)
      fill_pathname_expand_special(settings->libretro_info_path,
            g_defaults.core_info_dir, sizeof(settings->libretro_info_path));
#ifdef HAVE_OVERLAY
   if (*g_defaults.overlay_dir)
   {
      fill_pathname_expand_special(global->overlay_dir,
            g_defaults.overlay_dir, sizeof(global->overlay_dir));
#ifdef RARCH_MOBILE
      if (!*settings->input.overlay)
            fill_pathname_join(settings->input.overlay,
                  global->overlay_dir,
                  "DualShock.cfg",
                  sizeof(settings->input.overlay));
#endif
   }

   if (*g_defaults.osk_overlay_dir)
   {
      fill_pathname_expand_special(global->osk_overlay_dir,
            g_defaults.osk_overlay_dir, sizeof(global->osk_overlay_dir));
#ifdef RARCH_MOBILE
      if (!*settings->osk.overlay)
            fill_pathname_join(settings->osk.overlay,
                  global->osk_overlay_dir, "/modular-keyboard/big.cfg",
                  sizeof(settings->osk.overlay));
#endif
   }
   else
      strlcpy(global->osk_overlay_dir,
            global->overlay_dir, sizeof(global->osk_overlay_dir));
#endif
#ifdef HAVE_MENU
   if (*g_defaults.menu_config_dir)
      strlcpy(settings->menu_config_directory,
            g_defaults.menu_config_dir,
            sizeof(settings->menu_config_directory));
   if (*g_defaults.menu_theme_dir)
      strlcpy(settings->menu.theme_dir,
            g_defaults.menu_theme_dir,
            sizeof(settings->menu.theme_dir));
#endif
   if (*g_defaults.shader_dir)
      fill_pathname_expand_special(settings->video.shader_dir,
            g_defaults.shader_dir, sizeof(settings->video.shader_dir));
   if (*g_defaults.autoconfig_dir)
      strlcpy(settings->input.autoconfig_dir,
            g_defaults.autoconfig_dir,
            sizeof(settings->input.autoconfig_dir));

   if (!global->has_set_state_path && *g_defaults.savestate_dir)
      strlcpy(global->savestate_dir,
            g_defaults.savestate_dir, sizeof(global->savestate_dir));
   if (!global->has_set_save_path && *g_defaults.sram_dir)
      strlcpy(global->savefile_dir,
            g_defaults.sram_dir, sizeof(global->savefile_dir));
   if (*g_defaults.system_dir)
      strlcpy(settings->system_directory,
            g_defaults.system_dir, sizeof(settings->system_directory));
   if (*g_defaults.screenshot_dir)
      strlcpy(settings->screenshot_directory,
            g_defaults.screenshot_dir, sizeof(settings->screenshot_directory));
#ifdef HAVE_MENU
   if (*g_defaults.content_dir)
      strlcpy(settings->menu_content_directory,
              g_defaults.content_dir, sizeof(settings->menu_content_directory));
#endif

#ifdef HAVE_NETPLAY
   global->netplay_sync_frames = netplay_sync_frames;
   global->netplay_port        = RARCH_DEFAULT_PORT;
#endif

   if (*g_defaults.config_path)
      fill_pathname_expand_special(global->config_path,
            g_defaults.config_path, sizeof(global->config_path));

   settings->config_save_on_exit = config_save_on_exit;

   /* Avoid reloading config on every content load */
   global->block_config_read = default_block_config_read;
}

#ifndef GLOBAL_CONFIG_DIR
#if defined(__HAIKU__)
#define GLOBAL_CONFIG_DIR "/system/settings"
#else
#define GLOBAL_CONFIG_DIR "/etc"
#endif
#endif

/**
 * open_default_config_file
 *
 * Open a default config file. Platform-specific.
 *
 * Returns: handle to config file if found, otherwise NULL.
 **/
static config_file_t *open_default_config_file(void)
{
   char conf_path[PATH_MAX_LENGTH] = {0};
   char app_path[PATH_MAX_LENGTH]  = {0};
   config_file_t *conf             = NULL;
   bool saved                      = false;
   global_t *global                = global_get_ptr();

   (void)conf_path;
   (void)app_path;
   (void)saved;

#if defined(_WIN32) && !defined(_XBOX)
   fill_pathname_application_path(app_path, sizeof(app_path));
   fill_pathname_resolve_relative(conf_path, app_path,
         "retroarch.cfg", sizeof(conf_path));

   conf = config_file_new(conf_path);

   if (!conf)
   {
      const char *appdata = getenv("APPDATA");

      if (appdata)
      {
         fill_pathname_join(conf_path, appdata,
               "retroarch.cfg", sizeof(conf_path));
         conf = config_file_new(conf_path);
      }
   }

   if (!conf)
   {
      /* Try to create a new config file. */
      conf = config_file_new(NULL);

      if (conf)
      {
         /* Since this is a clean config file, we can
          * safely use config_save_on_exit. */
         fill_pathname_resolve_relative(conf_path, app_path,
               "retroarch.cfg", sizeof(conf_path));
         config_set_bool(conf, "config_save_on_exit", true);
         saved = config_file_write(conf, conf_path);
      }

      if (!saved)
      {
         /* WARN here to make sure user has a good chance of seeing it. */
         RARCH_ERR("Failed to create new config file in: \"%s\".\n",
               conf_path);
         config_file_free(conf);
         return NULL;
      }

      RARCH_WARN("Created new config file in: \"%s\".\n", conf_path);
   }
#elif defined(OSX)
   const char *home = getenv("HOME");

   if (!home)
      return NULL;

   fill_pathname_join(conf_path, home,
         "Library/Application Support/RetroArch", sizeof(conf_path));
   path_mkdir(conf_path);

   fill_pathname_join(conf_path, conf_path,
         "retroarch.cfg", sizeof(conf_path));
   conf = config_file_new(conf_path);

   if (!conf)
   {
      conf = config_file_new(NULL);

      if (conf)
      {
         config_set_bool(conf, "config_save_on_exit", true);
         saved = config_file_write(conf, conf_path);
      }

      if (!saved)
      {
         /* WARN here to make sure user has a good chance of seeing it. */
         RARCH_ERR("Failed to create new config file in: \"%s\".\n",
               conf_path);
         config_file_free(conf);

         return NULL;
      }

      RARCH_WARN("Created new config file in: \"%s\".\n", conf_path);
   }
#elif !defined(__CELLOS_LV2__) && !defined(_XBOX)
   const char *xdg  = getenv("XDG_CONFIG_HOME");
   const char *home = getenv("HOME");

   /* XDG_CONFIG_HOME falls back to $HOME/.config. */
   if (xdg)
      fill_pathname_join(conf_path, xdg,
            "retroarch/retroarch.cfg", sizeof(conf_path));
   else if (home)
#ifdef __HAIKU__
      fill_pathname_join(conf_path, home,
            "config/settings/retroarch/retroarch.cfg", sizeof(conf_path));
#else
      fill_pathname_join(conf_path, home,
            ".config/retroarch/retroarch.cfg", sizeof(conf_path));
#endif

   if (xdg || home)
   {
      RARCH_LOG("Looking for config in: \"%s\".\n", conf_path);
      conf = config_file_new(conf_path);
   }

   /* Fallback to $HOME/.retroarch.cfg. */
   if (!conf && home)
   {
      fill_pathname_join(conf_path, home,
            ".retroarch.cfg", sizeof(conf_path));
      RARCH_LOG("Looking for config in: \"%s\".\n", conf_path);
      conf = config_file_new(conf_path);
   }

   if (!conf)
   {
      if (home || xdg)
      {
         char basedir[PATH_MAX_LENGTH] = {0};

         /* Try to create a new config file. */

         /* XDG_CONFIG_HOME falls back to $HOME/.config. */
         if (xdg)
            fill_pathname_join(conf_path, xdg,
                  "retroarch/retroarch.cfg", sizeof(conf_path));
         else if (home)
#ifdef __HAIKU__
            fill_pathname_join(conf_path, home,
                  "config/settings/retroarch/retroarch.cfg", sizeof(conf_path));
#else
         fill_pathname_join(conf_path, home,
               ".config/retroarch/retroarch.cfg", sizeof(conf_path));
#endif

         fill_pathname_basedir(basedir, conf_path, sizeof(basedir));

         if (path_mkdir(basedir))
         {
            char skeleton_conf[PATH_MAX_LENGTH] = {0};

            fill_pathname_join(skeleton_conf, GLOBAL_CONFIG_DIR,
                  "retroarch.cfg", sizeof(skeleton_conf));
            conf = config_file_new(skeleton_conf);
            if (conf)
               RARCH_WARN("Using skeleton config \"%s\" as base for a new config file.\n", skeleton_conf);
            else
               conf = config_file_new(NULL);

            if (conf)
               saved = config_file_write(conf, conf_path);

            if (!saved)
            {
               /* WARN here to make sure user has a good chance of seeing it. */
               RARCH_ERR("Failed to create new config file in: \"%s\".\n", conf_path);
               config_file_free(conf);

               return NULL;
            }

            RARCH_WARN("Created new config file in: \"%s\".\n", conf_path);
         }
      }
   }
#endif

   if (!conf)
      return NULL;

   strlcpy(global->config_path, conf_path,
         sizeof(global->config_path));

   return conf;
}

static void read_keybinds_keyboard(config_file_t *conf, unsigned user,
      unsigned idx, struct retro_keybind *bind)
{
   const char *prefix = NULL;

   if (!input_config_bind_map[idx].valid)
      return;

   if (!input_config_bind_map[idx].base)
      return;

   prefix = input_config_get_prefix(user, input_config_bind_map[idx].meta);

   if (prefix)
      input_config_parse_key(conf, prefix,
            input_config_bind_map[idx].base, bind);
}

static void read_keybinds_button(config_file_t *conf, unsigned user,
      unsigned idx, struct retro_keybind *bind)
{
   const char *prefix = NULL;

   if (!input_config_bind_map[idx].valid)
      return;
   if (!input_config_bind_map[idx].base)
      return;

   prefix = input_config_get_prefix(user,
         input_config_bind_map[idx].meta);

   if (prefix)
      input_config_parse_joy_button(conf, prefix,
            input_config_bind_map[idx].base, bind);
}

static void read_keybinds_axis(config_file_t *conf, unsigned user,
      unsigned idx, struct retro_keybind *bind)
{
   const char *prefix = NULL;

   if (!input_config_bind_map[idx].valid)
      return;
   if (!input_config_bind_map[idx].base)
      return;

   prefix = input_config_get_prefix(user,
         input_config_bind_map[idx].meta);

   if (prefix)
      input_config_parse_joy_axis(conf, prefix,
            input_config_bind_map[idx].base, bind);
}

static void read_keybinds_user(config_file_t *conf, unsigned user)
{
   unsigned i;
   settings_t *settings = config_get_ptr();

   for (i = 0; input_config_bind_map[i].valid; i++)
   {
      struct retro_keybind *bind = (struct retro_keybind*)
         &settings->input.binds[user][i];

      if (!bind->valid)
         continue;

      read_keybinds_keyboard(conf, user, i, bind);
      read_keybinds_button(conf, user, i, bind);
      read_keybinds_axis(conf, user, i, bind);
   }
}

static void config_read_keybinds_conf(config_file_t *conf)
{
   unsigned i;

   for (i = 0; i < MAX_USERS; i++)
      read_keybinds_user(conf, i);
}

/* Also dumps inherited values, useful for logging. */
#if 0
static void config_file_dump_all(config_file_t *conf)
{
   struct config_entry_list *list = NULL;
   struct config_include_list *includes = conf->includes;

   while (includes)
   {
      RARCH_LOG("#include \"%s\"\n", includes->path);
      includes = includes->next;
   }

   list = conf->entries;

   while (list)
   {
      RARCH_LOG("%s = \"%s\"%s\n", list->key,
            list->value, list->readonly ? " (included)" : "");
      list = list->next;
   }
}
#endif
/**
 * config_load:
 * @path                : path to be read from.
 * @set_defaults        : set default values first before
 *                        reading the values from the config file
 *
 * Loads a config file and reads all the values into memory.
 *
 */
static bool config_load_file(const char *path, bool set_defaults)
{
   unsigned i;
   char *save                            = NULL;
   const char *extra_path                = NULL;
   char tmp_str[PATH_MAX_LENGTH]         = {0};
   char tmp_append_path[PATH_MAX_LENGTH] = {0}; /* Don't destroy append_config_path. */
   int vp_width = 0, vp_height = 0, vp_x = 0, vp_y = 0; 
   unsigned msg_color                    = 0;
   config_file_t *conf                   = NULL;
   settings_t *settings                  = config_get_ptr();
   global_t   *global                    = global_get_ptr();
   video_viewport_t *custom_vp           = (video_viewport_t*)
      video_viewport_get_custom();

   if (scoped_conf[GLOBAL])
   {
      config_file_free(scoped_conf[GLOBAL]);
      scoped_conf[GLOBAL] = NULL;
   }

   if (path)
   {
      conf = config_file_new(path);
      if (!conf)
         return false;
   }
   else
      conf = open_default_config_file();

   if (!conf)
      return true;

   if (set_defaults)
      config_set_defaults();

   strlcpy(tmp_append_path, global->append_config_path,
         sizeof(tmp_append_path));
   extra_path = strtok_r(tmp_append_path, "|", &save);

   while (extra_path)
   {
      bool ret = false;
      RARCH_LOG("Appending config \"%s\"\n", extra_path);
      ret = config_append_file(conf, extra_path);
      if (!ret)
         RARCH_ERR("Failed to append config \"%s\"\n", extra_path);
      extra_path = strtok_r(NULL, "|", &save);
   }
#if 0
   if (global->verbosity)
   {
      RARCH_LOG_OUTPUT("=== Config ===\n");
      config_file_dump_all(conf);
      RARCH_LOG_OUTPUT("=== Config end ===\n");
   }
#endif

   CONFIG_GET_FLOAT_BASE(conf, settings, video.scale, "video_scale");
   CONFIG_GET_INT_BASE  (conf, settings, video.fullscreen_x, "video_fullscreen_x");
   CONFIG_GET_INT_BASE  (conf, settings, video.fullscreen_y, "video_fullscreen_y");

   if (!global->force_fullscreen)
      CONFIG_GET_BOOL_BASE(conf, settings, video.fullscreen, "video_fullscreen");

   CONFIG_GET_BOOL_BASE(conf, settings, video.windowed_fullscreen, "video_windowed_fullscreen");
   CONFIG_GET_INT_BASE (conf, settings, video.monitor_index, "video_monitor_index");
   CONFIG_GET_BOOL_BASE(conf, settings, video.disable_composition, "video_disable_composition");
   
   CONFIG_GET_BOOL_BASE(conf, settings, video.vsync, "video_vsync");
   CONFIG_GET_BOOL_BASE(conf, settings, video.hard_sync, "video_hard_sync");
   CONFIG_GET_INT_BASE(conf, settings, video.hard_sync_frames, "video_hard_sync_frames");
   if (settings->video.hard_sync_frames > 3)
      settings->video.hard_sync_frames = 3;

#ifdef HAVE_MENU
   CONFIG_GET_BOOL_BASE(conf, settings, menu.dpi.override_enable, "dpi_override_enable");
   CONFIG_GET_INT_BASE (conf, settings, menu.dpi.override_value,  "dpi_override_value");

   CONFIG_GET_BOOL_BASE(conf, settings, menu.pause_libretro, "menu_pause_libretro");
   CONFIG_GET_BOOL_BASE(conf, settings, menu.mouse.enable,   "menu_mouse_enable");
   CONFIG_GET_BOOL_BASE(conf, settings, menu.pointer.enable, "menu_pointer_enable");
   CONFIG_GET_BOOL_BASE(conf, settings, menu.timedate_enable,   "menu_timedate_enable");
   CONFIG_GET_BOOL_BASE(conf, settings, menu.core_enable,   "menu_core_enable");
   CONFIG_GET_BOOL_BASE(conf, settings, menu.dynamic_wallpaper_enable,   "menu_dynamic_wallpaper_enable");
   CONFIG_GET_BOOL_BASE(conf, settings, menu.boxart_enable,   "menu_boxart_enable");
#ifdef HAVE_RGUI
   CONFIG_GET_BOOL_BASE(conf, settings, menu.rgui_thick_bg_checkerboard,   "rgui_thick_background_checkerboard");
   CONFIG_GET_BOOL_BASE(conf, settings, menu.rgui_thick_bd_checkerboard,   "rgui_thick_border_checkerboard");
   CONFIG_GET_INT_BASE (conf, settings, menu.rgui_particle_effect, "rgui_particle_effect");
   CONFIG_GET_FLOAT_BASE(conf, settings, menu.rgui_particle_effect_speed_factor, "rgui_particle_effect_speed_factor");
#endif
   CONFIG_GET_FLOAT_BASE(conf, settings, menu.ticker_speed, "menu_ticker_speed");
   CONFIG_GET_BOOL_BASE(conf, settings, menu.navigation.wraparound.vertical_enable,   "menu_navigation_wraparound_vertical_enable");
   CONFIG_GET_BOOL_BASE(conf, settings, menu.navigation.browser.filter.supported_extensions_enable,   "menu_navigation_browser_filter_supported_extensions_enable");
   CONFIG_GET_BOOL_BASE(conf, settings, menu.show_advanced_settings,   "menu_show_advanced_settings");
   CONFIG_GET_BOOL_BASE(conf, settings, menu.mame_titles,   "mame_titles");
#ifdef HAVE_OVERLAY
   CONFIG_GET_BOOL_BASE(conf, settings, menu.show_overlay_menu,  "show_overlay_menu");
#endif
   CONFIG_GET_BOOL_BASE(conf, settings, menu.show_frame_throttle_menu,  "show_frame_throttle_menu");
#ifdef HAVE_NETPLAY
   CONFIG_GET_BOOL_BASE(conf, settings, menu.show_netplay_menu,  "show_netplay_menu");
#endif
   CONFIG_GET_BOOL_BASE(conf, settings, menu.show_saving_menu,  "show_saving_menu");
   CONFIG_GET_BOOL_BASE(conf, settings, menu.show_core_menu,  "show_core_menu");
   CONFIG_GET_BOOL_BASE(conf, settings, menu.show_driver_menu,  "show_driver_menu");
   CONFIG_GET_BOOL_BASE(conf, settings, menu.show_ui_menu,  "show_ui_menu");
   CONFIG_GET_BOOL_BASE(conf, settings, menu.show_logging_menu,  "show_logging_menu");
   CONFIG_GET_BOOL_BASE(conf, settings, menu.show_hotkey_menu,  "show_hotkey_menu");
   CONFIG_GET_BOOL_BASE(conf, settings, menu.show_rewind_menu,  "show_rewind_menu");
   CONFIG_GET_BOOL_BASE(conf, settings, menu.show_cheat_options,  "show_cheat_options");
   CONFIG_GET_BOOL_BASE(conf, settings, menu.swap_ok_cancel,  "menu_swap_ok_cancel");
#ifndef EXTERNAL_LAUNCHER
   CONFIG_GET_BOOL_BASE(conf, settings, menu.show_core_updater,  "show_core_updater");
#endif
   CONFIG_GET_BOOL_BASE(conf, settings, menu.show_core_info,  "menu_show_core_info");
   CONFIG_GET_BOOL_BASE(conf, settings, menu.show_system_info,  "menu_show_system_info");
   CONFIG_GET_BOOL_BASE(conf, settings, menu.show_configuration_menu,  "show_configuration_menu");
   CONFIG_GET_BOOL_BASE(conf, settings, menu.show_user_menu,  "show_user_menu");
   CONFIG_GET_BOOL_BASE(conf, settings, menu.show_directory_menu,  "show_directory_menu");
   CONFIG_GET_BOOL_BASE(conf, settings, menu.show_privacy_menu,  "show_privacy_menu");
   CONFIG_GET_BOOL_BASE(conf, settings, menu.show_recording_menu,  "show_recording_menu");
   CONFIG_GET_BOOL_BASE(conf, settings, menu.show_core_updater_menu,  "show_core_updater_menu");
   CONFIG_GET_BOOL_BASE(conf, settings, menu.show_font_menu,  "show_font_menu");
   config_get_path(conf, "menu_theme_dir", settings->menu.theme_dir, sizeof(settings->menu.theme_dir));
   if (!strcmp(settings->menu.theme_dir, "default"))
      *settings->menu.theme_dir = '\0';
   CONFIG_GET_FLOAT_BASE(conf, settings, menu.wallpaper_opacity, "menu_wallpaper_opacity");
   config_get_path(conf, "menu_theme", settings->menu.theme, sizeof(settings->menu.theme));
   if (!strcmp(settings->menu.theme, "default"))
      *settings->menu.theme = '\0';
#endif /* #ifdef HAVE_MENU */

   CONFIG_GET_INT_BASE(conf, settings, video.frame_delay, "video_frame_delay");
   if (settings->video.frame_delay > 15)
      settings->video.frame_delay = 15;

   CONFIG_GET_BOOL_BASE(conf, settings, video.black_frame_insertion, "video_black_frame_insertion");
   CONFIG_GET_INT_BASE(conf, settings, video.swap_interval, "video_swap_interval");
   settings->video.swap_interval = max(settings->video.swap_interval, 1);
   settings->video.swap_interval = min(settings->video.swap_interval, 4);    
   CONFIG_GET_BOOL_BASE(conf, settings, video.fake_swap_interval, "video_fake_swap_interval");
   CONFIG_GET_BOOL_BASE(conf, settings, video.threaded, "video_threaded");
   CONFIG_GET_BOOL_BASE(conf, settings, video.shared_context, "video_shared_context");
#ifdef GEKKO
   CONFIG_GET_INT_BASE(conf, settings, video.viwidth, "video_viwidth");
   CONFIG_GET_BOOL_BASE(conf, settings, video.vfilter, "video_vfilter");
#endif
   CONFIG_GET_BOOL_BASE(conf, settings, video.smooth, "video_smooth");
   CONFIG_GET_BOOL_BASE(conf, settings, video.force_aspect, "video_force_aspect");
   CONFIG_GET_BOOL_BASE(conf, settings, video.scale_integer, "video_scale_integer");
   CONFIG_GET_BOOL_BASE(conf, settings, video.crop_overscan, "video_crop_overscan");
   CONFIG_GET_FLOAT_BASE(conf, settings, video.aspect_ratio, "video_aspect_ratio");
   
   CONFIG_GET_INT_BASE(conf, settings, video.aspect_ratio_idx, "aspect_ratio_index");
   
   CONFIG_GET_BOOL_BASE(conf,  settings, video.aspect_ratio_auto, "video_aspect_ratio_auto");
   CONFIG_GET_FLOAT_BASE(conf, settings, video.refresh_rate, "video_refresh_rate");

   CONFIG_GET_BOOL_BASE(conf, settings, video.allow_rotate, "video_allow_rotate");

   config_get_path(conf, "video_font_path", settings->video.font_path, sizeof(settings->video.font_path));
   CONFIG_GET_FLOAT_BASE(conf, settings, video.font_size, "video_font_size");
   CONFIG_GET_BOOL_BASE(conf, settings, video.font_enable, "video_font_enable");
   CONFIG_GET_FLOAT_BASE(conf, settings, video.msg_pos_x, "video_message_pos_x");
   CONFIG_GET_FLOAT_BASE(conf, settings, video.msg_pos_y, "video_message_pos_y");
   
   CONFIG_GET_INT_BASE(conf, settings, video.rotation, "video_rotation");

   CONFIG_GET_BOOL_BASE(conf, settings, video.force_srgb_disable, "video_force_srgb_disable");

   CONFIG_GET_BOOL_BASE(conf, settings, core.set_supports_no_game_enable, "core_set_supports_no_game_enable");

#ifdef RARCH_CONSOLE
   /* TODO - will be refactored later to make it more clean - it's more
    * important that it works for consoles right now */

   CONFIG_GET_BOOL_BASE(conf, global, console.screen.gamma_correction, "gamma_correction");

   config_get_bool(conf, "custom_bgm_enable",
         &global->console.sound.system_bgm_enable);
   config_get_bool(conf, "flicker_filter_enable",
         &global->console.flickerfilter_enable);
   config_get_bool(conf, "soft_filter_enable",
         &global->console.softfilter_enable);

   CONFIG_GET_INT_BASE(conf, global, console.screen.flicker_filter_index,
         "flicker_filter_index");
   CONFIG_GET_INT_BASE(conf, global, console.screen.soft_filter_index,
         "soft_filter_index");
   CONFIG_GET_INT_BASE(conf, global, console.screen.resolutions.current.id,
         "current_resolution_id");
   CONFIG_GET_INT_BASE(conf, global, console.sound.mode, "sound_mode");
#endif
   
   config_get_int(conf, "custom_viewport_width",  &vp_width);
   config_get_int(conf, "custom_viewport_height", &vp_height);
   config_get_int(conf, "custom_viewport_x",      &vp_x);
   config_get_int(conf, "custom_viewport_y",      &vp_y);

   if (custom_vp)
   {
      custom_vp->width  = vp_width;
      custom_vp->height = vp_height;
      custom_vp->x      = vp_x;
      custom_vp->y      = vp_y;
   }

   if (config_get_hex(conf, "video_message_color", &msg_color))
   {
      settings->video.msg_color_r = ((msg_color >> 16) & 0xff) / 255.0f;
      settings->video.msg_color_g = ((msg_color >>  8) & 0xff) / 255.0f;
      settings->video.msg_color_b = ((msg_color >>  0) & 0xff) / 255.0f;
   }

   CONFIG_GET_BOOL_BASE(conf, settings, video.post_filter_record, "video_post_filter_record");
   CONFIG_GET_BOOL_BASE(conf, settings, video.gpu_record, "video_gpu_record");
   CONFIG_GET_BOOL_BASE(conf, settings, video.gpu_screenshot, "video_gpu_screenshot");

   config_get_path(conf, "video_shader_dir", settings->video.shader_dir, sizeof(settings->video.shader_dir));
   if (!strcmp(settings->video.shader_dir, "default"))
      *settings->video.shader_dir = '\0';

   config_get_path(conf, "video_filter_dir", settings->video.filter_dir, sizeof(settings->video.filter_dir));
   if (!strcmp(settings->video.filter_dir, "default"))
      *settings->video.filter_dir = '\0';

   config_get_path(conf, "audio_filter_dir", settings->audio.filter_dir, sizeof(settings->audio.filter_dir));
   if (!strcmp(settings->audio.filter_dir, "default"))
      *settings->audio.filter_dir = '\0';

   CONFIG_GET_BOOL_BASE(conf, settings, input.remap_binds_enable, "input_remap_binds_enable");
   CONFIG_GET_FLOAT_BASE(conf, settings, input.axis_threshold, "input_axis_threshold");
   CONFIG_GET_INT_BASE(conf, settings, input.analog_dpad_mode, "input_analog_dpad_mode");
   CONFIG_GET_FLOAT_BASE(conf, settings, input.analog_diagonal_sensitivity, "input_analog_diagonal_sensitivity");
   CONFIG_GET_FLOAT_BASE(conf, settings, input.analog_dpad_deadzone, "input_analog_dpad_deadzone");
   input_joypad_update_analog_dpad_params();

   CONFIG_GET_BOOL_BASE(conf, settings, input.rumble_enable, "input_rumble_enable");
   CONFIG_GET_BOOL_BASE(conf, settings, input.netplay_client_swap_input, "netplay_client_swap_input");
   CONFIG_GET_INT_BASE(conf, settings, input.max_users, "input_max_users");
   CONFIG_GET_BOOL_BASE(conf, settings, input.autoconfig_descriptor_label_show, "autoconfig_descriptor_label_show");

   CONFIG_GET_BOOL_BASE(conf, settings, ui.companion_start_on_boot, "ui_companion_start_on_boot");

   config_get_path(conf, "core_updater_buildbot_url",
         settings->network.buildbot_url, sizeof(settings->network.buildbot_url));
   config_get_path(conf, "core_updater_buildbot_assets_url",
         settings->network.buildbot_assets_url, sizeof(settings->network.buildbot_assets_url));
   CONFIG_GET_BOOL_BASE(conf, settings, network.buildbot_auto_extract_archive, "core_updater_auto_extract_archive");

   for (i = 0; i < settings->input.max_users; i++)
   {
      char buf[64] = {0};
      snprintf(buf, sizeof(buf), "input_player%u_joypad_index", i + 1);
      CONFIG_GET_INT_BASE(conf, settings, input.joypad_map[i], buf);

      if (!global->has_set_libretro_device[i])
      {
         snprintf(buf, sizeof(buf), "input_libretro_device_p%u", i + 1);
         CONFIG_GET_INT_BASE(conf, settings, input.libretro_device[i], buf);
      }
   }

   if (!global->has_set_ups_pref)
   {
      CONFIG_GET_BOOL_BASE(conf, global, ups_pref, "ups_pref");
   }
   if (!global->has_set_bps_pref)
   {
      CONFIG_GET_BOOL_BASE(conf, global, bps_pref, "bps_pref");
   }
   if (!global->has_set_ips_pref)
   {
      CONFIG_GET_BOOL_BASE(conf, global, ips_pref, "ips_pref");
   }

   /* Audio settings. */
   CONFIG_GET_BOOL_BASE(conf, settings, audio.enable, "audio_enable");
   CONFIG_GET_BOOL_BASE(conf, settings, audio.mute_enable, "audio_mute_enable");
   CONFIG_GET_INT_BASE(conf, settings, audio.out_rate, "audio_out_rate");
   CONFIG_GET_INT_BASE(conf, settings, audio.block_frames, "audio_block_frames");
   CONFIG_GET_STRING_BASE(conf, settings, audio.device, "audio_device");
   CONFIG_GET_INT_BASE(conf, settings, audio.latency, "audio_latency");
   CONFIG_GET_BOOL_BASE(conf, settings, audio.sync, "audio_sync");
   CONFIG_GET_BOOL_BASE(conf, settings, audio.rate_control, "audio_rate_control");
   CONFIG_GET_FLOAT_BASE(conf, settings, audio.rate_control_delta, "audio_rate_control_delta");
   CONFIG_GET_FLOAT_BASE(conf, settings, audio.max_timing_skew, "audio_max_timing_skew");
   CONFIG_GET_FLOAT_BASE(conf, settings, audio.volume, "audio_volume");
   CONFIG_GET_STRING_BASE(conf, settings, audio.resampler, "audio_resampler");
   audio_driver_set_volume_gain(db_to_gain(settings->audio.volume));

   CONFIG_GET_STRING_BASE(conf, settings, camera.device, "camera_device");
   CONFIG_GET_BOOL_BASE(conf, settings, camera.allow, "camera_allow");

   CONFIG_GET_BOOL_BASE(conf, settings, location.allow, "location_allow");
   CONFIG_GET_STRING_BASE(conf, settings, video.driver, "video_driver");
#ifdef HAVE_MENU
   CONFIG_GET_STRING_BASE(conf, settings, menu.driver, "menu_driver");
#endif
   CONFIG_GET_STRING_BASE(conf, settings, video.context_driver, "video_context_driver");
   CONFIG_GET_STRING_BASE(conf, settings, audio.driver, "audio_driver");

   CONFIG_GET_PATH_BASE(conf, settings, video.softfilter_plugin, "video_filter");
   CONFIG_GET_PATH_BASE(conf, settings, video.shader_path, "video_shader");
   
   CONFIG_GET_INT_BASE(conf, settings, preempt_frames, "preempt_frames");
   
   CONFIG_GET_PATH_BASE(conf, settings, audio.dsp_plugin, "audio_dsp_plugin");
   CONFIG_GET_STRING_BASE(conf, settings, input.driver, "input_driver");
   CONFIG_GET_STRING_BASE(conf, settings, input.joypad_driver, "input_joypad_driver");
   CONFIG_GET_STRING_BASE(conf, settings, input.keyboard_layout, "input_keyboard_layout");

   if (!global->core_dir_override && !global->has_set_libretro_directory)
      config_get_path(conf, "libretro_directory", settings->libretro_directory, sizeof(settings->libretro_directory));
   if (!*settings->libretro_directory)
   {
      strlcpy(settings->libretro_directory, path_default_dotslash(),
              sizeof(settings->libretro_directory));
      strlcat(settings->libretro_directory, "cores",
              sizeof(settings->libretro_directory));
      path_mkdir(settings->libretro_directory);
   }

   /* Safe-guard against older behavior. */
   if (path_is_directory(settings->libretro))
   {
      RARCH_WARN("\"libretro_path\" is a directory, using this for \"libretro_directory\" instead.\n");
      strlcpy(settings->libretro_directory, settings->libretro,
            sizeof(settings->libretro_directory));
      *settings->libretro = '\0';
      *global->libretro_name = '\0';
      *settings->core_content_directory = '\0';
   }

   CONFIG_GET_BOOL_BASE(conf, settings, ui.menubar_enable, "ui_menubar_enable");
   CONFIG_GET_BOOL_BASE(conf, settings, ui.suspend_screensaver_enable, "suspend_screensaver_enable");
   CONFIG_GET_BOOL_BASE(conf, settings, fps_show, "fps_show");
   CONFIG_GET_BOOL_BASE(conf, settings, load_dummy_on_core_shutdown, "load_dummy_on_core_shutdown");

   if (!global->info_dir_override)
      config_get_path(conf, "libretro_info_path", settings->libretro_info_path,
                      sizeof(settings->libretro_info_path));
   if (!*settings->libretro_info_path)
   {
      strlcpy(settings->libretro_info_path, path_default_dotslash(),
              sizeof(settings->libretro_info_path));
      strlcat(settings->libretro_info_path, "info",
              sizeof(settings->libretro_info_path));
   }

   config_get_path(conf, "screenshot_directory", settings->screenshot_directory, sizeof(settings->screenshot_directory));
   if (*settings->screenshot_directory)
   {
      if (!strcmp(settings->screenshot_directory, "default"))
         *settings->screenshot_directory = '\0';
      else if (!path_is_directory(settings->screenshot_directory))
      {
         RARCH_WARN("screenshot_directory is not an existing directory, ignoring ...\n");
         *settings->screenshot_directory = '\0';
      }
   }
   
   config_get_path(conf, "extraction_directory", settings->extraction_directory, sizeof(settings->extraction_directory));
   config_get_path(conf, "input_remapping_directory", settings->input_remapping_directory, sizeof(settings->input_remapping_directory));
   config_get_path(conf, "core_assets_directory", settings->core_assets_directory, sizeof(settings->core_assets_directory));
   config_get_path(conf, "assets_directory", settings->assets_directory, sizeof(settings->assets_directory));
   config_get_path(conf, "dynamic_wallpapers_directory", settings->dynamic_wallpapers_directory, sizeof(settings->dynamic_wallpapers_directory));
   config_get_path(conf, "boxarts_directory", settings->boxarts_directory, sizeof(settings->boxarts_directory));
   if (!strcmp(settings->core_assets_directory, "default"))
      *settings->core_assets_directory = '\0';
   if (!strcmp(settings->assets_directory, "default"))
      *settings->assets_directory = '\0';
   if (!strcmp(settings->dynamic_wallpapers_directory, "default"))
      *settings->dynamic_wallpapers_directory = '\0';
   if (!strcmp(settings->boxarts_directory, "default"))
      *settings->boxarts_directory = '\0';
#ifdef HAVE_MENU
   /* override content directory if specified */
   if (global->content_dir_override)
      strlcpy(settings->menu_content_directory, g_defaults.content_dir, PATH_MAX_LENGTH);
   else
   {
      config_get_path(conf, "rgui_browser_directory",
                      settings->menu_content_directory,
                      sizeof(settings->menu_content_directory));
      if (!strcmp(settings->menu_content_directory, "default"))
         *settings->menu_content_directory = '\0';
   }
   config_get_path(conf, "rgui_config_directory",
                   settings->menu_config_directory,
                   sizeof(settings->menu_config_directory));
   if (!strcmp(settings->menu_config_directory, "default"))
   {
      strlcpy(settings->menu_config_directory, path_default_dotslash(),
              sizeof(settings->menu_config_directory));
      strlcat(settings->menu_config_directory, "config",
              sizeof(settings->menu_config_directory));
      path_mkdir(settings->menu_config_directory);
   }
   CONFIG_GET_BOOL_BASE(conf, settings, menu_show_start_screen, "rgui_show_start_screen");
#endif
   CONFIG_GET_INT_BASE(conf, settings, libretro_log_level, "libretro_log_level");

   if (!global->has_set_verbosity)
      CONFIG_GET_BOOL_BASE(conf, global, verbosity, "log_verbosity");
   if (global->verbosity)
      frontend_driver_attach_console();
   else
      frontend_driver_detach_console();

   CONFIG_GET_BOOL_BASE(conf, global, perfcnt_enable, "perfcnt_enable");

   CONFIG_GET_INT_BASE(conf, settings, archive.mode, "archive_mode");

   config_get_path(conf, "recording_output_directory", global->record.output_dir, sizeof(global->record.output_dir));
   config_get_path(conf, "recording_config_directory", global->record.config_dir, sizeof(global->record.config_dir));

#ifdef HAVE_OVERLAY
   config_get_path(conf, "overlay_directory", global->overlay_dir, sizeof(global->overlay_dir));
   if (!strcmp(global->overlay_dir, "default") || !*global->overlay_dir)
      strlcpy( global->overlay_dir, g_defaults.overlay_dir, PATH_MAX_LENGTH);

   CONFIG_GET_PATH_BASE(conf, settings, input.overlay, "input_overlay");
   config_check_overlay_preset();
   CONFIG_GET_BOOL_BASE(conf, settings, input.overlay_enable, "input_overlay_enable");
   CONFIG_GET_FLOAT_BASE(conf, settings, input.overlay_opacity, "input_overlay_opacity");
   CONFIG_GET_FLOAT_BASE(conf, settings, input.overlay_scale, "input_overlay_scale");
   
   CONFIG_GET_INT_BASE(conf, settings, input.dpad_method, "input_dpad_method");
   CONFIG_GET_FLOAT_BASE(conf, settings, input.dpad_diagonal_sensitivity, "input_dpad_diagonal_sensitivity");
   CONFIG_GET_INT_BASE(conf, settings, input.abxy_method, "input_abxy_method");
   CONFIG_GET_FLOAT_BASE(conf, settings, input.abxy_diagonal_sensitivity, "input_abxy_diagonal_sensitivity");
   CONFIG_GET_FLOAT_BASE(conf, settings, input.touch_ellipse_magnify, "input_touch_ellipse_magnify");
   
   CONFIG_GET_BOOL_BASE(conf, settings, input.overlay_adjust_aspect, "input_overlay_adjust_aspect");
   CONFIG_GET_FLOAT_BASE(conf, settings, input.overlay_bisect_aspect_ratio, "input_overlay_bisect_aspect_ratio");
   CONFIG_GET_INT_BASE(conf, settings, input.overlay_aspect_ratio_index, "input_overlay_aspect_ratio_index");
   if (settings->input.overlay_aspect_ratio_index >= OVERLAY_ASPECT_RATIO_END)
         settings->input.overlay_aspect_ratio_index = OVERLAY_ASPECT_RATIO_END-1;
   CONFIG_GET_FLOAT_BASE(conf, settings, input.overlay_shift_y, "input_overlay_adjust_vertical");
   CONFIG_GET_FLOAT_BASE(conf, settings, input.overlay_shift_x, "input_overlay_adjust_horizontal");
   CONFIG_GET_BOOL_BASE(conf, settings, input.overlay_shift_y_lock_edges, "input_overlay_adjust_vertical_lock_edges");

   CONFIG_GET_INT_BASE(conf, settings, input.vibrate_time, "input_vibrate_time");

   config_get_path(conf, "osk_overlay_directory", global->osk_overlay_dir, sizeof(global->osk_overlay_dir));
   if (!strcmp(global->osk_overlay_dir, "default"))
      *global->osk_overlay_dir = '\0';

   config_get_path(conf, "input_osk_overlay", settings->osk.overlay, sizeof(settings->osk.overlay));
   CONFIG_GET_BOOL_BASE(conf, settings, osk.enable, "input_osk_overlay_enable");
#endif

   CONFIG_GET_BOOL_BASE(conf, settings, rewind_enable, "rewind_enable");
   CONFIG_GET_INT_BASE(conf, settings, rewind_buffer_size, "rewind_buffer_size");
   CONFIG_GET_INT_BASE(conf, settings, rewind_granularity, "rewind_granularity");

   CONFIG_GET_FLOAT_BASE(conf, settings, slowmotion_ratio, "slowmotion_ratio");
   if (settings->slowmotion_ratio < 1.0f)
      settings->slowmotion_ratio = 1.0f;

   CONFIG_GET_FLOAT_BASE(conf, settings, fastforward_ratio, "fastforward_ratio");

   /* Sanitize fastforward_ratio value - previously range was -1
    * and up (with 0 being skipped) */
   if (settings->fastforward_ratio <= 0.0f)
      settings->fastforward_ratio = 1.0f;

   CONFIG_GET_BOOL_BASE(conf, settings, core_throttle_enable, "core_throttle_enable");
   CONFIG_GET_BOOL_BASE(conf, settings, throttle_using_core_fps, "throttle_using_core_fps");
   CONFIG_GET_BOOL_BASE(conf, settings, pause_nonactive, "pause_nonactive");
   CONFIG_GET_INT_BASE(conf, settings, autosave_interval, "autosave_interval");

   CONFIG_GET_PATH_BASE(conf, settings, cheat_database, "cheat_database_path");
   CONFIG_GET_PATH_BASE(conf, settings, cheat_settings_path, "cheat_settings_path");

   CONFIG_GET_BOOL_BASE(conf, settings, block_sram_overwrite, "block_sram_overwrite");
   CONFIG_GET_BOOL_BASE(conf, settings, savestate_auto_index, "savestate_auto_index");
   CONFIG_GET_BOOL_BASE(conf, settings, savestate_auto_save, "savestate_auto_save");
   CONFIG_GET_BOOL_BASE(conf, settings, savestate_auto_load, "savestate_auto_load");

   CONFIG_GET_BOOL_BASE(conf, settings, network_cmd_enable, "network_cmd_enable");
   CONFIG_GET_INT_BASE(conf, settings, network_cmd_port, "network_cmd_port");
   CONFIG_GET_BOOL_BASE(conf, settings, stdin_cmd_enable, "stdin_cmd_enable");

   CONFIG_GET_BOOL_BASE(conf, settings, input.autodetect_enable, "input_autodetect_enable");
   CONFIG_GET_PATH_BASE(conf, settings, input.autoconfig_dir, "joypad_autoconfig_dir");

   if (!global->has_set_username)
      CONFIG_GET_PATH_BASE(conf, settings, username, "netplay_nickname");
   CONFIG_GET_INT_BASE(conf, settings, user_language, "user_language");
#ifdef HAVE_NETPLAY
   if (!global->has_set_netplay_mode)
      CONFIG_GET_BOOL_BASE(conf, global, netplay_is_client, "netplay_mode");
   if (!global->has_set_netplay_ip_address)
      CONFIG_GET_PATH_BASE(conf, global, netplay_server, "netplay_ip_address");
   if (!global->has_set_netplay_delay_frames)
      CONFIG_GET_INT_BASE(conf, global, netplay_sync_frames, "netplay_delay_frames");
   if (!global->has_set_netplay_ip_port)
      CONFIG_GET_INT_BASE(conf, global, netplay_port, "netplay_ip_port");
#endif

   CONFIG_GET_BOOL_BASE(conf, settings, config_save_on_exit, "config_save_on_exit");

   if (!global->has_set_save_path &&
         config_get_path(conf, "savefile_directory", tmp_str, sizeof(tmp_str)))
   {
      if (!strcmp(tmp_str, "default"))
         strlcpy(global->savefile_dir, g_defaults.sram_dir,
               sizeof(global->savefile_dir));
      else if (path_is_directory(tmp_str))
      {
         strlcpy(global->savefile_dir, tmp_str,
               sizeof(global->savefile_dir));
         strlcpy(global->savefile_name, tmp_str,
               sizeof(global->savefile_name));
         fill_pathname_dir(global->savefile_name, global->basename,
               ".srm", sizeof(global->savefile_name));
      }
      else
         RARCH_WARN("savefile_directory is not a directory, ignoring ...\n");
   }

   if (!global->has_set_state_path &&
         config_get_path(conf, "savestate_directory", tmp_str, sizeof(tmp_str)))
   {
      if (!strcmp(tmp_str, "default"))
         strlcpy(global->savestate_dir, g_defaults.savestate_dir,
               sizeof(global->savestate_dir));
      else if (path_is_directory(tmp_str))
      {
         strlcpy(global->savestate_dir, tmp_str,
               sizeof(global->savestate_dir));
         strlcpy(global->savestate_name, tmp_str,
               sizeof(global->savestate_name));
         fill_pathname_dir(global->savestate_name, global->basename,
               ".state", sizeof(global->savestate_name));
      }
      else
         RARCH_WARN("savestate_directory is not a directory, ignoring ...\n");
   }

   if (!config_get_path(conf, "system_directory",
            settings->system_directory, sizeof(settings->system_directory))
       || !strcmp(settings->system_directory, "default"))
   {
      strlcpy(settings->system_directory, path_default_dotslash(),
              sizeof(settings->system_directory));
      strlcat(settings->system_directory, "system",
              sizeof(settings->system_directory));
      RARCH_WARN("system_directory is not set in config. Assuming relative directory: \"%s\".\n",
            settings->system_directory);
      path_mkdir(settings->system_directory);
   }

   config_read_keybinds_conf(conf);

   CONFIG_GET_BOOL_BASE(conf, settings, auto_remaps_enable, "auto_remaps_enable");

   CONFIG_GET_BOOL_BASE(conf, settings, sort_savefiles_enable, "sort_savefiles_enable");
   CONFIG_GET_BOOL_BASE(conf, settings, sort_savestates_enable, "sort_savestates_enable");

   CONFIG_GET_BOOL_BASE(conf, settings, savestate_file_compression, "savestate_file_compression");
   CONFIG_GET_BOOL_BASE(conf, settings, sram_file_compression, "sram_file_compression");

   CONFIG_GET_INT_BASE(conf, settings, menu_ok_btn,          "menu_ok_btn");
   CONFIG_GET_INT_BASE(conf, settings, menu_cancel_btn,      "menu_cancel_btn");
   CONFIG_GET_INT_BASE(conf, settings, menu_search_btn,      "menu_search_btn");
   CONFIG_GET_INT_BASE(conf, settings, menu_info_btn,        "menu_info_btn");
   CONFIG_GET_INT_BASE(conf, settings, menu_default_btn,     "menu_default_btn");
   CONFIG_GET_INT_BASE(conf, settings, menu_cancel_btn,      "menu_cancel_btn");
   CONFIG_GET_INT_BASE(conf, settings, menu_scroll_down_btn, "menu_scroll_down_btn");
   CONFIG_GET_INT_BASE(conf, settings, menu_scroll_up_btn,   "menu_scroll_up_btn");

   scoped_conf[GLOBAL] = conf;
   return true;
}

static void parse_config_file(void)
{
   global_t *global = global_get_ptr();
   bool ret = config_load_file((*global->config_path)
         ? global->config_path : NULL, false);

   if (*global->config_path)
   {
      RARCH_LOG("Loading config from: %s.\n", global->config_path);
   }
   else
   {
      RARCH_LOG("Loading default config.\n");
      if (*global->config_path)
         RARCH_LOG("Found default config: %s.\n", global->config_path);
   }

   if (ret)
      return;

   RARCH_ERR("Couldn't find config at path: \"%s\"\n",
         global->config_path);
}

static void save_keybind_key(config_file_t *conf, const char *prefix,
      const char *base, const struct retro_keybind *bind)
{
   char key[64] = {0};
   char btn[64] = {0};

   snprintf(key, sizeof(key), "%s_%s", prefix, base);
   input_keymaps_translate_rk_to_str(bind->key, btn, sizeof(btn));
   
   /* Avoid saving null binds so that menu-essential defaults are restored on
    * next launch. Hotkeys can be null, but not menu toggle. */
   if ( bind->key == RETROK_UNKNOWN
        && (strcmp(prefix,"input") || !strcmp(base,"menu_toggle")) )
      strcpy(btn, "");
   config_set_string(conf, key, btn);
}

static void save_keybind_hat(config_file_t *conf, const char *key,
      const struct retro_keybind *bind)
{
   char config[16]  = {0};
   unsigned hat     = GET_HAT(bind->joykey);
   const char *dir  = NULL;

   switch (GET_HAT_DIR(bind->joykey))
   {
      case HAT_UP_MASK:
         dir = "up";
         break;

      case HAT_DOWN_MASK:
         dir = "down";
         break;

      case HAT_LEFT_MASK:
         dir = "left";
         break;

      case HAT_RIGHT_MASK:
         dir = "right";
         break;

      default:
         rarch_assert(0);
   }

   snprintf(config, sizeof(config), "h%u%s", hat, dir);
   config_set_string(conf, key, config);
}

static void save_keybind_joykey(config_file_t *conf, const char *prefix,
      const char *base, const struct retro_keybind *bind)
{
   char key[64] = {0};

   snprintf(key, sizeof(key), "%s_%s_btn", prefix, base);

   if (bind->joykey == NO_BTN)
      config_set_string(conf, key, "");
   else if (GET_HAT_DIR(bind->joykey))
      save_keybind_hat(conf, key, bind);
   else
      config_set_uint64(conf, key, bind->joykey);
}

static void save_keybind_axis(config_file_t *conf, const char *prefix,
      const char *base, const struct retro_keybind *bind)
{
   char key[64]    = {0};
   char config[16] = {0};
   unsigned axis   = 0;
   char dir        = '\0';

   snprintf(key, sizeof(key), "%s_%s_axis", prefix, base);

   if (bind->joyaxis == AXIS_NONE)
      config_set_string(conf, key, "");
   else if (AXIS_NEG_GET(bind->joyaxis) != AXIS_DIR_NONE)
   {
      dir = '-';
      axis = AXIS_NEG_GET(bind->joyaxis);
   }
   else if (AXIS_POS_GET(bind->joyaxis) != AXIS_DIR_NONE)
   {
      dir = '+';
      axis = AXIS_POS_GET(bind->joyaxis);
   }

   if (dir)
   {
      snprintf(config, sizeof(config), "%c%u", dir, axis);
      config_set_string(conf, key, config);
   }
}

/**
 * save_keybind:
 * @conf               : pointer to config file object
 * @prefix             : prefix name of keybind
 * @base               : base name   of keybind
 * @bind               : pointer to key binding object
 *
 * Save a key binding to the config file.
 */
static void save_keybind(config_file_t *conf, const char *prefix,
      const char *base, const struct retro_keybind *bind)
{
   if (!bind->valid)
      return;

   save_keybind_key(conf, prefix, base, bind);
   save_keybind_joykey(conf, prefix, base, bind);
   save_keybind_axis(conf, prefix, base, bind);
}



/**
 * save_keybinds_user:
 * @conf               : pointer to config file object
 * @user               : user number
 *
 * Save the current keybinds of a user (@user) to the config file (@conf).
 */
static void save_keybinds_user(config_file_t *conf, unsigned user)
{
   unsigned i = 0;
   settings_t *settings = config_get_ptr();

   for (i = 0; input_config_bind_map[i].valid; i++)
   {
      const char *prefix = input_config_get_prefix(user,
            input_config_bind_map[i].meta);

      if (prefix)
         save_keybind(conf, prefix, input_config_bind_map[i].base,
               &settings->input.binds[user][i]);
   }
}

/**
 * config_load:
 *
 * Loads a config file and reads all the values into memory.
 *
 */
void config_load(void)
{
   global_t   *global   = global_get_ptr();

   /* Flush out some states that could have been set by core environment variables */
   global->has_set_input_descriptors = false;

   if (!global->block_config_read)
   {
      config_set_defaults();
      parse_config_file();
   }
}

/**
 * config_save_file:
 * @path            : Path that shall be written to.
 *
 * Writes a config file to disk.
 *
 * Returns: true (1) on success, otherwise returns false (0).
 **/
bool config_save_file(const char *path)
{
   unsigned i           = 0;
   bool ret             = false;
   settings_t *settings = config_get_ptr();
   global_t   *global   = global_get_ptr();
   const video_viewport_t *custom_vp = (const video_viewport_t*)
      video_viewport_get_custom();
   config_file_t *conf;

   config_file_free(scoped_conf[GLOBAL]);
   scoped_conf[GLOBAL] = NULL;

   conf = config_file_new(NULL);
   if (!conf)
      return false;

   RARCH_LOG("Saving config at path: \"%s\"\n", path);

#ifdef HAVE_MENU
   if (!global->content_dir_override)
      config_set_path(conf, "rgui_browser_directory",
            *settings->menu_content_directory ?
            settings->menu_content_directory : "default");
   config_set_path(conf, "rgui_config_directory",
         *settings->menu_config_directory ?
         settings->menu_config_directory : "default");
#endif
   config_set_path(conf, "system_directory",
         *settings->system_directory ?
         settings->system_directory : "default");
   if (!global->core_dir_override)
      config_set_path(conf,  "libretro_directory", settings->libretro_directory);
   if (!global->info_dir_override)
      config_set_path(conf,  "libretro_info_path", settings->libretro_info_path);

   config_set_path(conf, "savefile_directory",
         *global->savefile_dir ? global->savefile_dir : "default");
   config_set_bool(conf, "sort_savefiles_enable",
         settings->sort_savefiles_enable);
   config_set_bool(conf, "sram_file_compression",
         settings->sram_file_compression);
   config_set_bool(conf, "block_sram_overwrite",
         settings->block_sram_overwrite);
   config_set_int(conf,  "autosave_interval", settings->autosave_interval);

   config_set_path(conf, "savestate_directory",
         *global->savestate_dir ? global->savestate_dir : "default");
   config_set_bool(conf, "sort_savestates_enable",
         settings->sort_savestates_enable);
   config_set_bool(conf, "savestate_file_compression",
         settings->savestate_file_compression);
   config_set_bool(conf, "savestate_auto_index",
         settings->savestate_auto_index);
   config_set_bool(conf, "savestate_auto_save",
         settings->savestate_auto_save);
   config_set_bool(conf, "savestate_auto_load",
         settings->savestate_auto_load);

   config_set_bool(conf, "load_dummy_on_core_shutdown",
                   settings->load_dummy_on_core_shutdown);
   config_set_bool(conf,  "ui_menubar_enable", settings->ui.menubar_enable);

   config_set_path(conf,  "recording_output_directory", global->record.output_dir);
   config_set_path(conf,  "recording_config_directory", global->record.config_dir);

   config_set_bool(conf,  "suspend_screensaver_enable", settings->ui.suspend_screensaver_enable);

   config_set_bool(conf, "rewind_enable", settings->rewind_enable);
   config_set_int(conf, "rewind_buffer_size", settings->rewind_buffer_size);
   config_set_int(conf,   "rewind_granularity", settings->rewind_granularity);

   config_set_string(conf,"video_driver", settings->video.driver);
   config_set_int(conf,   "video_monitor_index", settings->video.monitor_index);
   config_set_string(conf,"video_context_driver", settings->video.context_driver);
   config_set_bool(conf,  "video_shared_context", settings->video.shared_context);
   config_set_float(conf, "video_refresh_rate", settings->video.refresh_rate);
   config_set_bool(conf,  "fps_show", settings->fps_show);
   if (settings->video.vsync_scope == GLOBAL)
   {
      config_set_bool(conf,"video_vsync", settings->video.vsync);
      config_set_int(conf, "video_swap_interval", settings->video.swap_interval);
   }
   config_set_bool(conf, "video_fake_swap_interval", settings->video.fake_swap_interval);
   config_set_float(conf, "video_aspect_ratio", settings->video.aspect_ratio);
   config_set_bool(conf,  "video_aspect_ratio_auto", settings->video.aspect_ratio_auto);

   if (settings->video.aspect_ratio_idx_scope == GLOBAL)
   {
      config_set_int(conf, "custom_viewport_width",
            custom_vp->width);
      config_set_int(conf, "custom_viewport_height",
            custom_vp->height);
      config_set_int(conf, "custom_viewport_x",
            custom_vp->x);
      config_set_int(conf, "custom_viewport_y",
            custom_vp->y);
   }

#ifdef GEKKO
   config_set_int(conf,   "video_viwidth", settings->video.viwidth);
   config_set_bool(conf,  "video_vfilter", settings->video.vfilter);
#endif
   config_set_bool(conf, "video_smooth", settings->video.smooth);
   if (settings->video.threaded_scope == GLOBAL)
      config_set_bool(conf, "video_threaded", settings->video.threaded);
   config_set_bool(conf,  "video_force_srgb_disable",
         settings->video.force_srgb_disable);
   config_set_bool(conf,  "video_fullscreen", settings->video.fullscreen);
   config_set_bool(conf,  "video_windowed_fullscreen",
         settings->video.windowed_fullscreen);
   config_set_float(conf, "video_scale", settings->video.scale);
   config_set_bool(conf,  "video_crop_overscan", settings->video.crop_overscan);
   config_set_bool(conf,  "video_scale_integer", settings->video.scale_integer);
   config_set_int(conf,   "video_fullscreen_x", settings->video.fullscreen_x);
   config_set_int(conf,   "video_fullscreen_y", settings->video.fullscreen_y);
   config_set_bool(conf,  "video_gpu_record", settings->video.gpu_record);
   
#ifdef HAVE_GL_SYNC
   if (settings->video.hard_sync_scope == GLOBAL)
   {
      config_set_bool(conf, "video_hard_sync", settings->video.hard_sync);
      config_set_int(conf, "video_hard_sync_frames", settings->video.hard_sync_frames);
   }
#endif
   if (settings->preempt_frames_scope == GLOBAL)
      config_set_int(conf, "preempt_frames", settings->preempt_frames);
   if (settings->video.frame_delay_scope == GLOBAL)
      config_set_int(conf, "video_frame_delay", settings->video.frame_delay);
   config_set_bool(conf,  "video_black_frame_insertion",
         settings->video.black_frame_insertion);
   config_set_bool(conf,  "video_disable_composition",
         settings->video.disable_composition);
   config_set_bool(conf,  "pause_nonactive", settings->pause_nonactive);
   config_set_bool(conf, "video_gpu_screenshot", settings->video.gpu_screenshot);
   
   if (settings->video.rotation_scope == GLOBAL)
      config_set_int(conf, "video_rotation", settings->video.rotation);
   
   config_set_path(conf, "screenshot_directory",
         *settings->screenshot_directory ?
         settings->screenshot_directory : "default");
   
   if (settings->video.aspect_ratio_idx_scope == GLOBAL)
      config_set_int(conf, "aspect_ratio_index", settings->video.aspect_ratio_idx);

   config_set_path(conf, "video_shader_dir",
         *settings->video.shader_dir ?
         settings->video.shader_dir : "default");
   config_set_path(conf, "video_filter_dir",
         *settings->video.filter_dir ?
         settings->video.filter_dir : "default");
   if (settings->video.filter_shader_scope == GLOBAL)
   {
      config_set_path(conf, "video_filter", settings->video.softfilter_plugin);
      config_set_path(conf, "video_shader", settings->video.shader_path);
   }

   config_set_float(conf, "video_font_size", settings->video.font_size);
   config_set_bool(conf,  "video_font_enable", settings->video.font_enable);
   config_set_path(conf,  "video_font_path", settings->video.font_path);
   config_set_float(conf, "video_message_pos_x", settings->video.msg_pos_x);
   config_set_float(conf, "video_message_pos_y", settings->video.msg_pos_y);
   config_set_float(conf, "video_font_size", settings->video.font_size);

#ifdef HAVE_MENU
   config_set_bool(conf, "dpi_override_enable", settings->menu.dpi.override_enable);
   config_set_int (conf, "dpi_override_value", settings->menu.dpi.override_value);
   config_set_string(conf,"menu_driver", settings->menu.driver);
   config_set_bool(conf,"menu_pause_libretro", settings->menu.pause_libretro);
   config_set_bool(conf,"menu_mouse_enable", settings->menu.mouse.enable);
   config_set_bool(conf,"menu_pointer_enable", settings->menu.pointer.enable);
   config_set_bool(conf,"menu_timedate_enable", settings->menu.timedate_enable);
   config_set_bool(conf,"menu_core_enable", settings->menu.core_enable);
   config_set_bool(conf,"menu_dynamic_wallpaper_enable", settings->menu.dynamic_wallpaper_enable);
   config_set_bool(conf,"menu_boxart_enable", settings->menu.boxart_enable);
   config_set_bool(conf,"menu_thicken_bg_checkerboard", settings->menu.rgui_thick_bg_checkerboard);
   config_set_bool(conf,"menu_thicken_border_checkerboard", settings->menu.rgui_thick_bd_checkerboard);
   config_set_float(conf, "menu_ticker_speed", settings->menu.ticker_speed);
   config_set_path(conf, "menu_theme_dir", settings->menu.theme_dir);
   if (settings->menu.theme_scope == GLOBAL)
   {
      config_set_float(conf, "menu_wallpaper_opacity", settings->menu.wallpaper_opacity);
      config_set_path(conf, "menu_theme", settings->menu.theme);
#ifdef HAVE_RGUI
      config_set_int(conf, "rgui_particle_effect", settings->menu.rgui_particle_effect);
      config_set_float(conf, "rgui_particle_effect_speed_factor", settings->menu.rgui_particle_effect_speed_factor);
#endif
   }

   config_set_bool(conf, "rgui_show_start_screen",
         settings->menu_show_start_screen);
   config_set_bool(conf, "menu_navigation_wraparound_vertical_enable",
         settings->menu.navigation.wraparound.vertical_enable);
   config_set_bool(conf, "menu_navigation_browser_filter_supported_extensions_enable",
         settings->menu.navigation.browser.filter.supported_extensions_enable);
   config_set_bool(conf, "menu_show_advanced_settings",
         settings->menu.show_advanced_settings);
   config_set_bool(conf, "mame_titles",
         settings->menu.mame_titles);
#ifdef HAVE_OVERLAY
   config_set_bool(conf, "show_overlay_menu",
         settings->menu.show_overlay_menu);
#endif
   config_set_bool(conf, "show_frame_throttle_menu",
                   settings->menu.show_frame_throttle_menu);
   config_set_bool(conf, "show_netplay_menu", settings->menu.show_netplay_menu);
   config_set_bool(conf, "show_saving_menu", settings->menu.show_saving_menu);
   config_set_bool(conf, "show_core_menu", settings->menu.show_core_menu);
   config_set_bool(conf, "show_driver_menu", settings->menu.show_driver_menu);
   config_set_bool(conf, "show_ui_menu", settings->menu.show_ui_menu);
   config_set_bool(conf, "show_logging_menu", settings->menu.show_logging_menu);
   config_set_bool(conf, "show_hotkey_menu", settings->menu.show_hotkey_menu);
   config_set_bool(conf, "show_rewind_menu", settings->menu.show_rewind_menu);
   config_set_bool(conf, "show_cheat_options", settings->menu.show_cheat_options);
   config_set_bool(conf, "menu_swap_ok_cancel", settings->menu.swap_ok_cancel);
#ifndef EXTERNAL_LAUNCHER
   config_set_bool(conf, "show_core_updater", settings->menu.show_core_updater);
#endif
   config_set_bool(conf, "menu_show_core_info", settings->menu.show_core_info);
   config_set_bool(conf, "menu_show_system_info", settings->menu.show_system_info);
   config_set_bool(conf, "show_configuration_menu", settings->menu.show_configuration_menu);
   config_set_bool(conf, "show_user_menu", settings->menu.show_user_menu);
   config_set_bool(conf, "show_directory_menu", settings->menu.show_directory_menu);
   config_set_bool(conf, "show_privacy_menu", settings->menu.show_privacy_menu);
   config_set_bool(conf, "show_recording_menu", settings->menu.show_recording_menu);
   config_set_bool(conf, "show_core_updater_menu", settings->menu.show_core_updater_menu);
   config_set_bool(conf, "show_font_menu", settings->menu.show_font_menu);
#endif

   config_set_string(conf, "core_updater_buildbot_url", settings->network.buildbot_url);
   config_set_string(conf, "core_updater_buildbot_assets_url", settings->network.buildbot_assets_url);
   config_set_bool(conf, "core_updater_auto_extract_archive", settings->network.buildbot_auto_extract_archive);

   config_set_string(conf, "camera_device", settings->camera.device);
   config_set_bool(conf, "camera_allow", settings->camera.allow);

   config_set_string(conf, "audio_driver", settings->audio.driver);
   config_set_bool(conf,   "audio_enable", settings->audio.enable);
   if (settings->audio.sync_scope == GLOBAL)
      config_set_bool(conf,"audio_sync", settings->audio.sync);
   config_set_bool(conf,   "audio_rate_control", settings->audio.rate_control);
   config_set_float(conf,  "audio_rate_control_delta",
         settings->audio.rate_control_delta);
   if (settings->audio.max_timing_skew_scope == GLOBAL)
      config_set_float(conf, "audio_max_timing_skew",
                       settings->audio.max_timing_skew);
   if (settings->audio.volume_scope == GLOBAL)
      config_set_float(conf, "audio_volume", settings->audio.volume);
   config_set_bool(conf,  "audio_mute_enable", settings->audio.mute_enable);
   config_set_int(conf,   "audio_out_rate", settings->audio.out_rate);
   config_set_string(conf,"audio_device", settings->audio.device);
   config_set_int(conf,   "audio_latency", settings->audio.latency);
   config_set_int(conf,   "audio_block_frames", settings->audio.block_frames);
   if (settings->audio.dsp_scope == GLOBAL)
      config_set_string(conf, "audio_dsp_plugin", settings->audio.dsp_plugin);
   config_set_string(conf, "audio_resampler", settings->audio.resampler);
   config_set_path(conf, "audio_filter_dir",
         *settings->audio.filter_dir ?
         settings->audio.filter_dir : "default");

   config_set_bool(conf, "location_allow", settings->location.allow);

   if (!global->has_set_ups_pref)
      config_set_bool(conf, "ups_pref", global->ups_pref);
   if (!global->has_set_bps_pref)
      config_set_bool(conf, "bps_pref", global->bps_pref);
   if (!global->has_set_ips_pref)
      config_set_bool(conf, "ips_pref", global->ips_pref);

   config_set_path(conf, "extraction_directory",
         settings->extraction_directory);
   config_set_path(conf, "core_assets_directory",
         *settings->core_assets_directory ?
         settings->core_assets_directory : "default");
   config_set_path(conf, "assets_directory",
         *settings->assets_directory ?
         settings->assets_directory : "default");
   config_set_path(conf, "dynamic_wallpapers_directory",
         *settings->dynamic_wallpapers_directory ?
         settings->dynamic_wallpapers_directory : "default");
   config_set_path(conf, "boxarts_directory",
         *settings->boxarts_directory ?
         settings->boxarts_directory : "default");

   config_set_string(conf, "input_driver", settings->input.driver);
   config_set_string(conf, "input_joypad_driver",
         settings->input.joypad_driver);
   config_set_string(conf, "input_keyboard_layout",
         settings->input.keyboard_layout);

   for (i = 0; i < settings->input.max_users; i++)
   {
      char cfg[64] = {0};
      snprintf(cfg, sizeof(cfg), "input_player%u_joypad_index", i + 1);
      config_set_int(conf, cfg, settings->input.joypad_map[i]);

      if (settings->input.libretro_device_scope == GLOBAL)
      {
         snprintf(cfg, sizeof(cfg), "input_libretro_device_p%u", i + 1);
         config_set_int(conf, cfg, settings->input.libretro_device[i]);
      }
   }

   for (i = 0; i < settings->input.max_users; i++)
      save_keybinds_user(conf, i);

   config_set_int(conf, "menu_ok_btn",          settings->menu_ok_btn);
   config_set_int(conf, "menu_cancel_btn",      settings->menu_cancel_btn);
   config_set_int(conf, "menu_search_btn",      settings->menu_search_btn);
   config_set_int(conf, "menu_info_btn",        settings->menu_info_btn);
   config_set_int(conf, "menu_default_btn",     settings->menu_default_btn);
   config_set_int(conf, "menu_scroll_down_btn", settings->menu_scroll_down_btn);
   config_set_int(conf, "menu_scroll_up_btn",   settings->menu_scroll_up_btn);

   config_set_bool(conf, "auto_remaps_enable",
         settings->auto_remaps_enable);
   config_set_path(conf, "input_remapping_directory",
         settings->input_remapping_directory);
   config_set_bool(conf, "input_autodetect_enable",
         settings->input.autodetect_enable);
   config_set_path(conf, "joypad_autoconfig_dir",
         settings->input.autoconfig_dir);
   config_set_bool(conf, "autoconfig_descriptor_label_show",
         settings->input.autoconfig_descriptor_label_show);

   if (settings->input.max_users_scope == GLOBAL)
      config_set_int(conf, "input_max_users", settings->input.max_users);
   config_set_float(conf, "input_axis_threshold",
         settings->input.axis_threshold);
   config_set_bool(conf, "input_rumble_enable",
         settings->input.rumble_enable);
   config_set_bool(conf, "input_remap_binds_enable",
         settings->input.remap_binds_enable);
   if (settings->input.analog_dpad_scope == GLOBAL)
   {
      config_set_int(conf, "input_analog_dpad_mode",
            settings->input.analog_dpad_mode);
      config_set_float(conf, "input_analog_diagonal_sensitivity",
            settings->input.analog_diagonal_sensitivity);
      config_set_float(conf, "input_analog_dpad_deadzone",
            settings->input.analog_dpad_deadzone);
   }

#ifdef HAVE_OVERLAY
   config_set_path(conf, "overlay_directory",
         *global->overlay_dir ? global->overlay_dir : "default");

   if (settings->input.overlay_scope == GLOBAL)
   {
      config_set_path(conf, "input_overlay", settings->input.overlay);
      config_set_bool(conf, "input_overlay_enable", settings->input.overlay_enable);
      config_set_float(conf, "input_overlay_scale", settings->input.overlay_scale);
   }

   config_set_path(conf, "osk_overlay_directory",
         *global->osk_overlay_dir ? global->osk_overlay_dir : "default");
   config_set_path(conf, "input_osk_overlay", settings->osk.overlay);
   config_set_bool(conf, "input_osk_overlay_enable", settings->osk.enable);
   
   if (settings->input.overlay_opacity_scope == GLOBAL)
      config_set_float(conf, "input_overlay_opacity", settings->input.overlay_opacity);
   
   if (settings->input.dpad_abxy_config_scope == GLOBAL)
   {
      config_set_int(conf, "input_dpad_method",
                     settings->input.dpad_method);
      config_set_float(conf, "input_dpad_diagonal_sensitivity",
                       settings->input.dpad_diagonal_sensitivity);
      config_set_int(conf, "input_abxy_method",
                     settings->input.abxy_method);
      config_set_float(conf, "input_abxy_diagonal_sensitivity",
                       settings->input.abxy_diagonal_sensitivity);
   }

   config_set_float(conf, "input_touch_ellipse_magnify",
                    settings->input.touch_ellipse_magnify);
   
   if (settings->input.overlay_shift_xy_scope == GLOBAL)
   {
      config_set_float(conf, "input_overlay_adjust_vertical",
            settings->input.overlay_shift_y);
      config_set_bool(conf, "input_overlay_adjust_vertical_lock_edges",
            settings->input.overlay_shift_y_lock_edges);
      config_set_float(conf, "input_overlay_adjust_horizontal",
            settings->input.overlay_shift_x);
   }

   if (settings->input.overlay_aspect_scope == GLOBAL)
   {
      config_set_bool(conf, "input_overlay_adjust_aspect",
           settings->input.overlay_adjust_aspect);
      config_set_int(conf, "input_overlay_aspect_ratio_index",
            settings->input.overlay_aspect_ratio_index);
      config_set_float(conf, "input_overlay_bisect_aspect_ratio",
            settings->input.overlay_bisect_aspect_ratio);
   }

   config_set_float(conf, "input_vibrate_time", settings->input.vibrate_time);
#endif

   config_set_float(conf, "fastforward_ratio", settings->fastforward_ratio);
   
   if (settings->throttle_setting_scope == GLOBAL)
   {
      config_set_bool(conf, "core_throttle_enable", settings->core_throttle_enable);
      config_set_bool(conf, "throttle_using_core_fps", settings->throttle_using_core_fps);
   }
   
   config_set_float(conf, "slowmotion_ratio", settings->slowmotion_ratio);

#ifdef HAVE_NETPLAY
   config_set_bool(conf, "netplay_mode", global->netplay_is_client);
   config_set_string(conf, "netplay_ip_address", global->netplay_server);
   config_set_int(conf, "netplay_ip_port", global->netplay_port);
   config_set_int(conf, "netplay_delay_frames", global->netplay_sync_frames);
   config_set_bool(conf, "netplay_client_swap_input",
         settings->input.netplay_client_swap_input);
#endif
   config_set_string(conf, "netplay_nickname", settings->username);
   config_set_int(conf, "user_language", settings->user_language);

   config_set_int(conf, "libretro_log_level", settings->libretro_log_level);
   config_set_bool(conf, "log_verbosity", global->verbosity);
   config_set_bool(conf, "perfcnt_enable", global->perfcnt_enable);

   config_set_bool(conf, "core_set_supports_no_game_enable",
                   settings->core.set_supports_no_game_enable);

   config_set_int(conf, "archive_mode", settings->archive.mode);

   config_set_bool(conf, "ui_companion_start_on_boot", settings->ui.companion_start_on_boot);

   config_set_path(conf, "cheat_database_path", settings->cheat_database);

   config_set_bool(conf, "gamma_correction",
         global->console.screen.gamma_correction);
   config_set_bool(conf, "soft_filter_enable",
         global->console.softfilter_enable);
   config_set_bool(conf, "flicker_filter_enable",
         global->console.flickerfilter_enable);

   config_set_int(conf, "flicker_filter_index",
         global->console.screen.flicker_filter_index);
   config_set_int(conf, "soft_filter_index",
         global->console.screen.soft_filter_index);
   config_set_int(conf, "current_resolution_id",
         global->console.screen.resolutions.current.id);

   config_set_int(conf, "sound_mode", global->console.sound.mode);
   config_set_bool(conf,"custom_bgm_enable",
         global->console.sound.system_bgm_enable);

   ret = config_file_write(conf, path);
   config_file_free(conf);
   return ret;
}

settings_t *config_get_ptr(void)
{
   return g_config;
}

void config_free(void)
{
   if (!g_config)
      return;

   free(g_config);
   g_config = NULL;
}

settings_t *config_init(void)
{
   g_config = (settings_t*)calloc(1, sizeof(settings_t));

   if (!g_config)
      return NULL;

   return g_config;
}

static inline bool get_scoped_config_filename(char* buf, const unsigned scope)
{
   global_t *global = global_get_ptr();
   char tmp_buf[NAME_MAX_LENGTH];

   if (scope == THIS_CORE)
   {
      if (!*global->libretro_name)
         return false;

      strlcpy(buf, global->libretro_name, NAME_MAX_LENGTH);
   }
   else if (scope == THIS_CONTENT_DIR)
   {
      if (!*global->basename)
         return false;

      /* Basename is conveniently updated between saving and loading scoped cfgs */
      if (!path_parent_dir_name(buf, global->basename))
         strcpy(buf, "root");

      /* Check for name clash */
      if (!strcmp(buf, global->libretro_name))
         strlcat(buf, " (dir)", NAME_MAX_LENGTH);
   }
   else if (scope == THIS_CONTENT_ONLY)
   {
      if (!*global->basename)
         return false;

      strlcpy(buf, path_basename(global->basename), NAME_MAX_LENGTH);

      /* Check for name clash */
      if (!path_parent_dir_name(tmp_buf, global->basename))
         strcpy(tmp_buf, "root");
      if (!strcmp(buf, global->libretro_name) || !strcmp(buf, tmp_buf))
         strlcat(buf, " (rom)", NAME_MAX_LENGTH);
   }
   else
      return false;

   strlcat(buf, ".cfg", NAME_MAX_LENGTH);
   return true;
}

static void scoped_config_file_save(unsigned scope)
{
   char directory[PATH_MAX_LENGTH]  = {0};
   char buf[NAME_MAX_LENGTH]        = {0};
   char fullpath[PATH_MAX_LENGTH]   = {0};
   global_t *global                 = global_get_ptr();
   settings_t *settings             = config_get_ptr();
   config_file_t* conf;
   unsigned i;

   config_file_free(scoped_conf[scope]);
   scoped_conf[scope] = NULL;

   conf = config_file_new(NULL);
   if (!conf)
      return;
   
   /* Set scoped cfg path */
   if (!get_scoped_config_filename(buf, scope))
      return;
   
   fill_pathname_join(directory, settings->menu_config_directory,
                      global->libretro_name, PATH_MAX_LENGTH);
   fill_pathname_join(fullpath, directory, buf, PATH_MAX_LENGTH);

   RARCH_LOG("Saving scoped config at path: \"%s\"\n", fullpath);

   /* Populate config
    * Higher scopes are more specific and mask lower scopes */

   if (scope == THIS_CORE)
   {
      config_set_path(conf, "rgui_browser_directory",
                      settings->core_content_directory);
      config_set_bool(conf, "video_shared_context",
                      settings->video.shared_context);
      config_set_bool(conf, "load_dummy_on_core_shutdown",
                      settings->load_dummy_on_core_shutdown);
      config_set_bool(conf, "core_set_supports_no_game_enable",
                      settings->core.set_supports_no_game_enable);
      config_set_bool(conf, "rewind_enable", settings->rewind_enable);
      config_set_int(conf, "rewind_buffer_size", settings->rewind_buffer_size);
   }

   if (settings->audio.sync_scope == scope)
      config_set_bool(conf, "audio_sync", settings->audio.sync);
   else if (settings->audio.sync_scope < scope)
      config_remove_entry(conf, "audio_sync");
   
   if (settings->audio.volume_scope == scope)
      config_set_float(conf, "audio_volume", settings->audio.volume);
   else if (settings->audio.volume_scope < scope)
      config_remove_entry(conf, "audio_volume");

   if (settings->audio.max_timing_skew_scope == scope)
      config_set_float(conf, "audio_max_timing_skew",
                       settings->audio.max_timing_skew);
   else if (settings->audio.max_timing_skew_scope < scope)
      config_remove_entry(conf, "audio_max_timing_skew");

   if (settings->audio.dsp_scope == scope)
   {
      if (!*settings->audio.dsp_plugin)
         config_set_string(conf, "audio_dsp_plugin", EXPLICIT_NULL);
      else
         config_set_string(conf, "audio_dsp_plugin",
                           settings->audio.dsp_plugin);
   }
   else if (settings->audio.dsp_scope < scope)
      config_remove_entry(conf, "audio_dsp_plugin");

   if (settings->video.threaded_scope == scope)
      config_set_bool(conf, "video_threaded", settings->video.threaded);
   else if (settings->video.threaded_scope < scope)
      config_remove_entry(conf, "video_threaded");

   if (settings->video.vsync_scope == scope)
   {
      config_set_bool(conf, "video_vsync", settings->video.vsync);
      config_set_int(conf, "video_swap_interval",
                     settings->video.swap_interval);
   }
   else if (settings->video.vsync_scope < scope)
   {
      config_remove_entry(conf, "video_vsync");
      config_remove_entry(conf, "video_swap_interval");
   }

   if (settings->video.hard_sync_scope == scope)
   {
      config_set_bool(conf, "video_hard_sync", settings->video.hard_sync);
      config_set_int(conf, "video_hard_sync_frames",
                     settings->video.hard_sync_frames);
   }
   else if (settings->video.hard_sync_scope < scope)
   {
      config_remove_entry(conf, "video_hard_sync");
      config_remove_entry(conf, "video_hard_sync_frames");
   }

   if (settings->preempt_frames_scope == scope)
      config_set_int(conf, "preempt_frames", settings->preempt_frames);
   else if (settings->preempt_frames_scope < scope)
      config_remove_entry(conf, "preempt_frames");

   if (settings->video.aspect_ratio_idx_scope == scope)
   {
      config_set_int(conf, "aspect_ratio_index",
                     settings->video.aspect_ratio_idx);
      if (settings->video.aspect_ratio_idx == ASPECT_RATIO_CUSTOM)
      {
         const video_viewport_t *p_custom_vp
            = (const video_viewport_t*) video_viewport_get_custom();
         config_set_int(conf, "custom_viewport_width", p_custom_vp->width);
         config_set_int(conf, "custom_viewport_height", p_custom_vp->height);
         config_set_int(conf, "custom_viewport_x", p_custom_vp->x);
         config_set_int(conf, "custom_viewport_y", p_custom_vp->y);
      }
   }
   else if (settings->video.aspect_ratio_idx_scope < scope)
   {
      config_remove_entry(conf, "aspect_ratio_index");
      config_remove_entry(conf, "custom_viewport_width");
      config_remove_entry(conf, "custom_viewport_height");
      config_remove_entry(conf, "custom_viewport_x");
      config_remove_entry(conf, "custom_viewport_y");
   }

   if (settings->video.rotation_scope == scope)
      config_set_int(conf, "video_rotation", settings->video.rotation);
   else if (settings->video.rotation_scope < scope)
      config_remove_entry(conf, "video_rotation");

   if (settings->video.filter_shader_scope == scope)
   {
      if (!*settings->video.softfilter_plugin)
         config_set_path(conf, "video_filter", EXPLICIT_NULL);
      else
         config_set_path(conf, "video_filter",
                         settings->video.softfilter_plugin);

      if (!*settings->video.shader_path)
         config_set_path(conf, "video_shader", EXPLICIT_NULL);
      else
         config_set_path(conf, "video_shader", settings->video.shader_path);
   }
   else if (settings->video.filter_shader_scope < scope)
   {
      config_remove_entry(conf, "video_filter");
      config_remove_entry(conf, "video_shader");
   }

   if (settings->video.frame_delay_scope == scope)
      config_set_int(conf, "video_frame_delay", settings->video.frame_delay);
   else if (settings->video.frame_delay_scope < scope)
      config_remove_entry(conf, "video_frame_delay");

   if (settings->throttle_setting_scope == scope)
   {
      config_set_bool(conf, "core_throttle_enable",
                      settings->core_throttle_enable);
      config_set_bool(conf, "throttle_using_core_fps",
                      settings->throttle_using_core_fps);
   }
   else if (settings->throttle_setting_scope < scope)
   {
      config_remove_entry(conf, "core_throttle_enable");
      config_remove_entry(conf, "throttle_using_core_fps");
   }

#ifdef HAVE_OVERLAY
   if (settings->input.overlay_scope == scope)
   {
      config_set_path(conf, "input_overlay", settings->input.overlay);
      config_set_bool(conf, "input_overlay_enable",
                      settings->input.overlay_enable);
      config_set_float(conf, "input_overlay_scale",
                       settings->input.overlay_scale);
   }
   else if (settings->input.overlay_scope < scope)
   {
      config_remove_entry(conf, "input_overlay");
      config_remove_entry(conf, "input_overlay_enable");
      config_remove_entry(conf, "input_overlay_scale");
   }

   if (settings->input.dpad_abxy_config_scope == scope)
   {
      config_set_int(conf, "input_dpad_method", settings->input.dpad_method);
      config_set_float(conf, "input_dpad_diagonal_sensitivity",
                       settings->input.dpad_diagonal_sensitivity);
      config_set_int(conf, "input_abxy_method", settings->input.abxy_method);
      config_set_float(conf, "input_abxy_diagonal_sensitivity",
                       settings->input.abxy_diagonal_sensitivity);
   }
   else if (settings->input.dpad_abxy_config_scope < scope)
   {
      config_remove_entry(conf, "input_dpad_diagonal_sensitivity");
      config_remove_entry(conf, "input_abxy_diagonal_sensitivity");
      config_remove_entry(conf, "input_abxy_method");
      config_remove_entry(conf, "input_dpad_method");
   }

   if (settings->input.overlay_shift_xy_scope == scope)
   {
      config_set_float(conf, "input_overlay_adjust_vertical",
            settings->input.overlay_shift_y);
      config_set_bool(conf, "input_overlay_adjust_vertical_lock_edges",
            settings->input.overlay_shift_y_lock_edges);
      config_set_float(conf, "input_overlay_adjust_horizontal",
            settings->input.overlay_shift_x);
   }
   else if (settings->input.overlay_shift_xy_scope < scope)
   {
      config_remove_entry(conf, "input_overlay_adjust_vertical");
      config_remove_entry(conf, "input_overlay_adjust_vertical_lock_edges");
      config_remove_entry(conf, "input_overlay_adjust_horizontal");
   }

   if (settings->input.overlay_aspect_scope == scope)
   {
      config_set_bool(conf, "input_overlay_adjust_aspect",
           settings->input.overlay_adjust_aspect);
      config_set_int(conf, "input_overlay_aspect_ratio_index",
            settings->input.overlay_aspect_ratio_index);
      config_set_float(conf, "input_overlay_bisect_aspect_ratio",
            settings->input.overlay_bisect_aspect_ratio);
   }
   else if (settings->input.overlay_aspect_scope < scope)
   {
      config_remove_entry(conf, "input_overlay_adjust_aspect");
      config_remove_entry(conf, "input_overlay_aspect_ratio_index");
      config_remove_entry(conf, "input_overlay_bisect_aspect_ratio");
   }
   
   if (settings->input.overlay_opacity_scope == scope)
      config_set_float(conf, "input_overlay_opacity",
                       settings->input.overlay_opacity);
   else if (settings->input.overlay_opacity_scope < scope)
      config_remove_entry(conf, "input_overlay_opacity");
#endif /* HAVE_OVERLAY */

   if (settings->input.max_users_scope == scope)
      config_set_int(conf, "input_max_users", settings->input.max_users);
   else if (settings->input.max_users_scope < scope)
      config_remove_entry(conf, "input_max_users");

   if (settings->input.libretro_device_scope == scope)
   {
      for (i = 0; i < settings->input.max_users; i++)
      {
         snprintf(buf, sizeof(buf), "input_libretro_device_p%u", i + 1);
         config_set_int(conf, buf, settings->input.libretro_device[i]);
      }
   }
   else if (settings->input.libretro_device_scope < scope)
   {
      for (i = 0; i < MAX_USERS; i++)
      {
         snprintf(buf, sizeof(buf), "input_libretro_device_p%u", i + 1);
         config_remove_entry(conf, buf);
      }
   }

   if (settings->input.analog_dpad_scope == scope)
   {
      config_set_int(conf, "input_analog_dpad_mode",
            settings->input.analog_dpad_mode);
      config_set_float(conf, "input_analog_diagonal_sensitivity",
            settings->input.analog_diagonal_sensitivity);
      config_set_float(conf, "input_analog_dpad_deadzone",
            settings->input.analog_dpad_deadzone);
   }
   else if (settings->input.analog_dpad_scope < scope)
   {
      config_remove_entry(conf, "input_analog_dpad_mode");
      config_remove_entry(conf, "input_analog_diagonal_sensitivity");
      config_remove_entry(conf, "input_analog_dpad_deadzone");
   }
   
#ifdef HAVE_MENU
   if (settings->menu.theme_scope == scope)
   {
      if (!*settings->menu.theme)
         config_set_path(conf, "menu_theme", EXPLICIT_NULL);
      else
         config_set_path(conf, "menu_theme", settings->menu.theme);

      config_set_float(conf, "menu_wallpaper_opacity",
                       settings->menu.wallpaper_opacity);
#ifdef HAVE_RGUI
      config_set_int(conf, "rgui_particle_effect",
                     settings->menu.rgui_particle_effect);
      config_set_float(conf, "rgui_particle_effect_speed_factor",
                       settings->menu.rgui_particle_effect_speed_factor);
#endif
   }
   else if (settings->menu.theme_scope < scope)
   {
      config_remove_entry(conf, "menu_theme");
      config_remove_entry(conf, "menu_wallpaper_opacity");
#ifdef HAVE_RGUI
      config_remove_entry(conf, "rgui_particle_effect");
      config_remove_entry(conf, "rgui_particle_effect_speed_factor");
#endif
   }
#endif

   /* Create/update or delete config file */
   if (conf->entries)
   {
      if (!path_is_directory(directory))
         path_mkdir(directory);
      config_file_write(conf, fullpath);
   }
   else if (path_file_exists(fullpath))
      remove(fullpath);
   
   /* Cleanup */
   config_file_free(conf);
}

void scoped_config_files_save()
{
   scoped_config_file_save(THIS_CORE);
   scoped_config_file_save(THIS_CONTENT_DIR);
   scoped_config_file_save(THIS_CONTENT_ONLY);
}

void config_backup_restore_globals()
{
   settings_t *settings = config_get_ptr();
   global_t   *global   = global_get_ptr();
   config_file_t *conf  = scoped_conf[GLOBAL];
   char buf[64]         = {0};
   static bool prev_libretro;
   unsigned i;
   
   if (!settings || !conf)
      return;
   
   video_viewport_t* p_custom_vp
      = (video_viewport_t*) video_viewport_get_custom();
   
   if (settings->audio.sync_scope != GLOBAL)
   {  /* restore */
      settings->audio.sync_scope = GLOBAL;
      config_get_bool(conf, "audio_sync", &settings->audio.sync);
   }
   else
   {  /* back up */
      config_set_bool(conf, "audio_sync", settings->audio.sync);
   }
   
   if (settings->audio.volume_scope != GLOBAL)
   {  /* restore */
      settings->audio.volume_scope = GLOBAL;
      config_get_float(conf, "audio_volume", &settings->audio.volume);
      audio_driver_set_volume_gain(db_to_gain(settings->audio.volume));
   }
   else
   {  /* back up */
      config_set_float(conf, "audio_volume", settings->audio.volume);
   }
   
   if (settings->audio.max_timing_skew_scope != GLOBAL)
   {  /* restore */
      settings->audio.max_timing_skew_scope = GLOBAL;
      config_get_float(conf, "audio_max_timing_skew",
                       &settings->audio.max_timing_skew);
   }
   else
   {  /* back up */
      config_set_float(conf, "audio_max_timing_skew",
                       settings->audio.max_timing_skew);
   }
   
   if (settings->audio.dsp_scope != GLOBAL)
   {  /* restore */
      settings->audio.dsp_scope = GLOBAL;
      if (!config_get_path(conf, "audio_dsp_plugin", settings->audio.dsp_plugin,
                           sizeof(settings->audio.dsp_plugin)))
         *settings->audio.dsp_plugin = '\0';
   }
   else
   {  /* back up */
      config_set_string(conf, "audio_dsp_plugin", settings->audio.dsp_plugin);
   }
   
   if (settings->video.threaded_scope != GLOBAL)
   {  /* restore */
      settings->video.threaded_scope = GLOBAL;
      config_get_bool(conf, "video_threaded", &settings->video.threaded);
   }
   else
   {  /* back up */
      config_set_bool(conf, "video_threaded", settings->video.threaded);
   }
   
   if (settings->video.vsync_scope != GLOBAL)
   {  /* restore */
      settings->video.vsync_scope = GLOBAL;
      config_get_bool(conf, "video_vsync", &settings->video.vsync);
      config_get_uint(conf, "video_swap_interval",
                      &settings->video.swap_interval);
   }
   else
   {  /* back up */
      config_set_bool(conf, "video_vsync", settings->video.vsync);
      config_set_int(conf, "video_swap_interval",
                     settings->video.swap_interval);
   }

   if (settings->video.hard_sync_scope != GLOBAL)
   {  /* restore */
      settings->video.hard_sync_scope = GLOBAL;
      config_get_bool(conf, "video_hard_sync",
                      &settings->video.hard_sync);
      config_get_uint(conf, "video_hard_sync_frames",
                      &settings->video.hard_sync_frames);
   }
   else
   {  /* back up */
      config_set_bool(conf, "video_hard_sync", settings->video.hard_sync);
      config_set_int(conf, "video_hard_sync_frames",
                     settings->video.hard_sync_frames);
   }
   
#ifdef HAVE_OVERLAY   
   if (settings->input.overlay_scope != GLOBAL)
   {  /* restore */
      settings->input.overlay_scope = GLOBAL;
      if (!config_get_path(conf, "input_overlay", settings->input.overlay,
                           sizeof(settings->input.overlay)))
         *settings->input.overlay = '\0';
      config_get_bool(conf, "input_overlay_enable",
                      &settings->input.overlay_enable);
      config_get_float(conf, "input_overlay_scale",
                       &settings->input.overlay_scale);
   }
   else
   {  /* back up */
      config_set_path(conf, "input_overlay", settings->input.overlay);
      config_set_bool(conf, "input_overlay_enable",
                      settings->input.overlay_enable);
      config_set_float(conf, "input_overlay_scale",
                       settings->input.overlay_scale);
   }

   if (settings->input.dpad_abxy_config_scope != GLOBAL)
   {  /* restore */
      settings->input.dpad_abxy_config_scope = GLOBAL;
      config_get_uint(conf, "input_dpad_method", &settings->input.dpad_method);
      config_get_float(conf, "input_dpad_diagonal_sensitivity",
                       &settings->input.dpad_diagonal_sensitivity);
      config_get_uint(conf, "input_abxy_method", &settings->input.abxy_method);
      config_get_float(conf, "input_abxy_diagonal_sensitivity",
                       &settings->input.abxy_diagonal_sensitivity);
   }
   else
   {  /* back up */
      config_set_int(conf, "input_dpad_method", settings->input.dpad_method);
      config_set_float(conf, "input_dpad_diagonal_sensitivity",
                       settings->input.dpad_diagonal_sensitivity);
      config_set_int(conf, "input_abxy_method", settings->input.abxy_method);
      config_set_float(conf, "input_abxy_diagonal_sensitivity",
                       settings->input.abxy_diagonal_sensitivity);
   }

   if (settings->input.overlay_shift_xy_scope != GLOBAL)
   {  /* restore */
      settings->input.overlay_shift_xy_scope = GLOBAL;
      config_get_float(conf, "input_overlay_adjust_vertical",
                       &settings->input.overlay_shift_y);
      config_get_bool(conf, "input_overlay_adjust_vertical_lock_edges",
                      &settings->input.overlay_shift_y_lock_edges);
      config_get_float(conf, "input_overlay_adjust_horizontal",
                       &settings->input.overlay_shift_x);
   }
   else
   {  /* back up */
      config_set_float(conf, "input_overlay_adjust_vertical",
            settings->input.overlay_shift_y);
      config_set_bool(conf, "input_overlay_adjust_vertical_lock_edges",
            settings->input.overlay_shift_y_lock_edges);
      config_set_float(conf, "input_overlay_adjust_horizontal",
            settings->input.overlay_shift_x);
   }

   if (settings->input.overlay_aspect_scope != GLOBAL)
   {  /* restore */
      settings->input.overlay_aspect_scope = GLOBAL;
      config_get_bool(conf, "input_overlay_adjust_aspect",
                      &settings->input.overlay_adjust_aspect);
      config_get_float(conf, "input_overlay_bisect_aspect_ratio",
                       &settings->input.overlay_bisect_aspect_ratio);
      config_get_uint(conf, "input_overlay_aspect_ratio_index",
                      &settings->input.overlay_aspect_ratio_index);
   }
   else
   {  /* back up */
      config_set_bool(conf, "input_overlay_adjust_aspect",
           settings->input.overlay_adjust_aspect);
      config_set_int(conf, "input_overlay_aspect_ratio_index",
            settings->input.overlay_aspect_ratio_index);
      config_set_float(conf, "input_overlay_bisect_aspect_ratio",
            settings->input.overlay_bisect_aspect_ratio);
   }
   
   if (settings->input.overlay_opacity_scope != GLOBAL)
   {  /* restore */
      settings->input.overlay_opacity_scope = GLOBAL;
      config_get_float(conf, "input_overlay_opacity",
                       &settings->input.overlay_opacity);
   }
   else
   {  /* back up */
      config_set_float(conf, "input_overlay_opacity",
                       settings->input.overlay_opacity);
   }
#endif /* HAVE_OVERLAY */
      
   if (settings->throttle_setting_scope != GLOBAL)
   {  /* restore */
      settings->throttle_setting_scope = GLOBAL;
      config_get_bool(conf, "core_throttle_enable",
                      &settings->core_throttle_enable);
      config_get_bool(conf, "throttle_using_core_fps",
                      &settings->throttle_using_core_fps);
   }
   else
   {  /* back up */
      config_set_bool(conf, "core_throttle_enable",
                      settings->core_throttle_enable);
      config_set_bool(conf, "throttle_using_core_fps",
                      settings->throttle_using_core_fps);
   }
   
   if (settings->video.aspect_ratio_idx_scope != GLOBAL)
   {  /* restore */
      settings->video.aspect_ratio_idx_scope = GLOBAL;
      config_get_uint(conf, "aspect_ratio_index",
                      &settings->video.aspect_ratio_idx);
      config_get_uint(conf, "custom_viewport_width",  &p_custom_vp->width);
      config_get_uint(conf, "custom_viewport_height", &p_custom_vp->height);
      config_get_int(conf, "custom_viewport_x", &p_custom_vp->x);
      config_get_int(conf, "custom_viewport_y", &p_custom_vp->y);
   }
   else
   {  /* back up */
      config_set_int(conf, "aspect_ratio_index",
                     settings->video.aspect_ratio_idx);
      config_set_int(conf, "custom_viewport_width", p_custom_vp->width);
      config_set_int(conf, "custom_viewport_height", p_custom_vp->height);
      config_set_int(conf, "custom_viewport_x", p_custom_vp->x);
      config_set_int(conf, "custom_viewport_y", p_custom_vp->y);
   }

   if (settings->video.rotation_scope != GLOBAL)
   {  /* restore */
      settings->video.rotation_scope = GLOBAL;
      config_get_uint(conf, "video_rotation", &settings->video.rotation);
   }
   else
   {  /* back up */
      config_set_int(conf, "video_rotation", settings->video.rotation);
   }
   
   if (settings->video.frame_delay_scope != GLOBAL)
   {  /* restore */
      settings->video.frame_delay_scope = GLOBAL;
      config_get_uint(conf, "video_frame_delay", &settings->video.frame_delay);
   }
   else
   {  /* back up */
      config_set_int(conf, "video_frame_delay", settings->video.frame_delay);
   }
   
   if (settings->input.max_users_scope != GLOBAL)
   {  /* restore */
      settings->input.max_users_scope = GLOBAL;
      config_get_uint(conf, "input_max_users", &settings->input.max_users);
   }
   else
   {  /* back up */
      config_set_int(conf, "input_max_users", settings->input.max_users);
   }
   
   if (settings->input.libretro_device_scope != GLOBAL)
   {  /* restore */
      settings->input.libretro_device_scope = GLOBAL;
      for (i = 0; i < MAX_USERS; i++)
      {
         snprintf(buf, sizeof(buf), "input_libretro_device_p%u", i + 1);
         config_get_uint(conf, buf, &settings->input.libretro_device[i]);
      }
   }
   else
   {  /* back up */
      for (i = 0; i < MAX_USERS; i++)
      {
         snprintf(buf, sizeof(buf), "input_libretro_device_p%u", i + 1);
         config_set_int(conf, buf, settings->input.libretro_device[i]);
      }
   }

   if (settings->input.analog_dpad_scope != GLOBAL)
   {  /* restore */
      settings->input.analog_dpad_scope = GLOBAL;
      config_get_uint(conf, "input_analog_dpad_mode",
            &settings->input.analog_dpad_mode);
      config_get_float(conf, "input_analog_diagonal_sensitivity",
            &settings->input.analog_diagonal_sensitivity);
      config_get_float(conf, "input_analog_dpad_deadzone",
            &settings->input.analog_dpad_deadzone);
   }
   else
   {  /* back up */
      config_set_int(conf, "input_analog_dpad_mode",
            settings->input.analog_dpad_mode);
      config_set_float(conf, "input_analog_diagonal_sensitivity",
            settings->input.analog_diagonal_sensitivity);
      config_set_float(conf, "input_analog_dpad_deadzone",
            settings->input.analog_dpad_deadzone);
   }
   
   if (settings->video.filter_shader_scope != GLOBAL)
   {  /* restore */
      settings->video.filter_shader_scope = GLOBAL;
      if (!config_get_path(conf, "video_filter",
                           settings->video.softfilter_plugin,
                           sizeof(settings->video.softfilter_plugin)))
         *settings->video.softfilter_plugin = '\0';
      if (!config_get_path(conf, "video_shader",
                           settings->video.shader_path,
                           sizeof(settings->video.shader_path)))
         *settings->video.shader_path = '\0';
   }
   else
   {  /* back up */
      config_set_path(conf, "video_filter", settings->video.softfilter_plugin);
      config_set_path(conf, "video_shader", settings->video.shader_path);
   }
   
   if (settings->preempt_frames_scope != GLOBAL)
   {  /* restore */
      settings->preempt_frames_scope = GLOBAL;
      config_get_uint(conf, "preempt_frames", &settings->preempt_frames);
   }
   else
   {  /* back up */
      config_set_int(conf, "preempt_frames", settings->preempt_frames);
   }
   
#ifdef HAVE_MENU
   if (settings->menu.theme_scope != GLOBAL)
   {  /* restore */
      settings->menu.theme_scope = GLOBAL;
      if (!config_get_path(conf, "menu_theme", settings->menu.theme,
                           sizeof(settings->menu.theme)))
         *settings->menu.theme = '\0';
      config_get_float(conf, "menu_wallpaper_opacity",
                       &settings->menu.wallpaper_opacity);
#ifdef HAVE_RGUI
      config_get_uint(conf, "rgui_particle_effect",
                      &settings->menu.rgui_particle_effect);
      config_get_float(conf, "rgui_particle_effect_speed_factor",
                       &settings->menu.rgui_particle_effect_speed_factor);
#endif
   }
   else
   {  /* back up */
      config_set_path(conf, "menu_theme", settings->menu.theme);
      config_set_float(conf, "menu_wallpaper_opacity",
                       settings->menu.wallpaper_opacity);
#ifdef HAVE_RGUI
      config_set_int(conf, "rgui_particle_effect",
                     settings->menu.rgui_particle_effect);
      config_set_float(conf, "rgui_particle_effect_speed_factor",
                       settings->menu.rgui_particle_effect_speed_factor);
#endif
   }
   global->menu.theme_update_flag = true;
#endif
   
   /* Core specific settings */
   if (prev_libretro && !*settings->libretro)
   {  /* restore */
      config_get_bool(conf, "video_shared_context",
                      &settings->video.shared_context);
      config_get_bool(conf, "load_dummy_on_core_shutdown",
                      &settings->load_dummy_on_core_shutdown);
      config_get_bool(conf, "core_set_supports_no_game_enable",
                      &settings->core.set_supports_no_game_enable);
      config_get_bool(conf, "rewind_enable", &settings->rewind_enable);
      config_get_uint(conf, "rewind_buffer_size",
                      &settings->rewind_buffer_size);
      *settings->core_content_directory = '\0';
      global->libretro_dummy = true;
      prev_libretro = false;
   }
   else if (!prev_libretro && *settings->libretro)
   {  /* back up */
      config_set_bool(conf, "video_shared_context",
                      settings->video.shared_context);
      config_set_bool(conf, "load_dummy_on_core_shutdown",
                      settings->load_dummy_on_core_shutdown);
      config_set_bool(conf, "core_set_supports_no_game_enable",
                      settings->core.set_supports_no_game_enable);
      config_set_bool(conf, "rewind_enable", settings->rewind_enable);
      config_set_int(conf, "rewind_buffer_size", settings->rewind_buffer_size);
   }
   
   /* Force THIS_CORE or narrower scope for certain settings */
   if (*settings->libretro)
   {
      settings->input.libretro_device_scope = THIS_CORE;
      settings->video.filter_shader_scope = THIS_CORE;
      settings->preempt_frames_scope = THIS_CORE;
      settings->video.frame_delay_scope = THIS_CORE;
   }
   
   prev_libretro = *settings->libretro;
}

static void scoped_config_file_load(unsigned scope)
{
   char directory[PATH_MAX_LENGTH]  = {0};
   char buf[PATH_MAX_LENGTH]        = {0};
   char fullpath[PATH_MAX_LENGTH]   = {0};
   global_t *global                 = global_get_ptr();
   settings_t *settings             = config_get_ptr();
   config_file_t* conf              = scoped_conf[scope];
   unsigned i;
   
   if (!global || !settings)
      return;
   
   /* Set scoped cfg path */
   if (!get_scoped_config_filename(buf, scope))
      return;

   fill_pathname_join(directory, settings->menu_config_directory,
                      global->libretro_name, PATH_MAX_LENGTH);
   fill_pathname_join(fullpath, directory, buf, PATH_MAX_LENGTH);
   
   if (conf)
      config_file_free(conf);
   conf = config_file_new(fullpath);
   if (!conf)
   {
      scoped_conf[scope] = NULL;
      return;
   }

   RARCH_LOG("Loading scoped config from: %s.\n", fullpath);

   /* Override values if found in scoped config, and update scope in those cases */
   if (config_get_bool(conf, "audio_sync", &settings->audio.sync))
      settings->audio.sync_scope = scope;
   if (config_get_float(conf, "audio_volume", &settings->audio.volume))
   {
      settings->audio.volume_scope = scope;
      audio_driver_set_volume_gain(db_to_gain(settings->audio.volume));
   }
   if (config_get_float(conf, "audio_max_timing_skew", &settings->audio.max_timing_skew))
      settings->audio.max_timing_skew_scope = scope;
   if (config_get_path(conf, "audio_dsp_plugin", settings->audio.dsp_plugin,
                       sizeof(settings->audio.dsp_plugin)))
   {
      if (!strcmp(settings->audio.dsp_plugin, EXPLICIT_NULL))
         *settings->audio.dsp_plugin = '\0';
      settings->audio.dsp_scope = scope;
   }
   if (config_get_bool(conf, "video_vsync", &settings->video.vsync))
   {
      settings->video.vsync_scope = scope;
      config_get_uint(conf, "video_swap_interval",
                      &settings->video.swap_interval);
      settings->video.swap_interval = max(settings->video.swap_interval, 1);
      settings->video.swap_interval = min(settings->video.swap_interval, 4);
   }
   if (config_get_bool(conf, "video_hard_sync", &settings->video.hard_sync))
   {
      settings->video.hard_sync_scope = scope;
      config_get_uint(conf, "video_hard_sync_frames",
                      &settings->video.hard_sync_frames);
   }
   if (config_get_bool(conf, "video_threaded", &settings->video.threaded))
      settings->video.threaded_scope = scope;

#ifdef HAVE_OVERLAY
   if (config_get_path(conf, "input_overlay", settings->input.overlay, sizeof(settings->input.overlay)))
   {
      if (!strcmp(settings->input.overlay, EXPLICIT_NULL))
         *settings->input.overlay = '\0';
      config_check_overlay_preset();
      config_get_bool(conf, "input_overlay_enable", &settings->input.overlay_enable);
      config_get_float(conf, "input_overlay_scale", &settings->input.overlay_scale);
      settings->input.overlay_scope = scope;
   }

   if (config_get_float(conf, "input_dpad_diagonal_sensitivity", &settings->input.dpad_diagonal_sensitivity))
   {
      settings->input.dpad_abxy_config_scope = scope;
      config_get_uint(conf, "input_dpad_method", &settings->input.dpad_method);
      config_get_uint(conf, "input_abxy_method", &settings->input.abxy_method);
      config_get_float(conf, "input_abxy_diagonal_sensitivity",
                       &settings->input.abxy_diagonal_sensitivity);
   }

   if (config_get_float(conf, "input_overlay_adjust_vertical", &settings->input.overlay_shift_y))
   {
      settings->input.overlay_shift_xy_scope = scope;
      config_get_bool(conf, "input_overlay_adjust_vertical_lock_edges",
            &settings->input.overlay_shift_y_lock_edges);
      config_get_float(conf, "input_overlay_adjust_horizontal",
            &settings->input.overlay_shift_x);
   }

   if (config_get_bool(conf, "input_overlay_adjust_aspect", &settings->input.overlay_adjust_aspect))
   {
      settings->input.overlay_aspect_scope = scope;
      config_get_float(conf, "input_overlay_bisect_aspect_ratio",
            &settings->input.overlay_bisect_aspect_ratio);
      config_get_uint(conf, "input_overlay_aspect_ratio_index",
            &settings->input.overlay_aspect_ratio_index);
      if (settings->input.overlay_aspect_ratio_index >= OVERLAY_ASPECT_RATIO_END)
         settings->input.overlay_aspect_ratio_index = OVERLAY_ASPECT_RATIO_END-1;
   }

   if (config_get_float(conf, "input_overlay_opacity", &settings->input.overlay_opacity))
      settings->input.overlay_opacity_scope = scope;
#endif /* HAVE_OVERLAY */

   if (config_get_bool(conf, "core_throttle_enable", &settings->core_throttle_enable))
   {
      settings->throttle_setting_scope = scope;
      config_get_bool(conf, "throttle_using_core_fps",
                      &settings->throttle_using_core_fps);
   }
   if (config_get_uint(conf, "aspect_ratio_index", &settings->video.aspect_ratio_idx))
   {
      settings->video.aspect_ratio_idx_scope = scope;
      video_viewport_t *p_custom_vp
         = (video_viewport_t*) video_viewport_get_custom();
      config_get_uint(conf, "custom_viewport_width",  &p_custom_vp->width);
      config_get_uint(conf, "custom_viewport_height", &p_custom_vp->height);
      config_get_int(conf, "custom_viewport_x", &p_custom_vp->x);
      config_get_int(conf, "custom_viewport_y", &p_custom_vp->y);
   }
   if (config_get_uint(conf, "video_rotation", &settings->video.rotation))
      settings->video.rotation_scope = scope;
   if (config_get_uint(conf, "video_frame_delay", &settings->video.frame_delay))
      settings->video.frame_delay_scope = scope;
   if (config_get_uint(conf, "input_max_users", &settings->input.max_users))
      settings->input.max_users_scope = scope;
   for (i = 0; i < settings->input.max_users; i++)
   {
      snprintf(buf, sizeof(buf), "input_libretro_device_p%u", i + 1);
      if (!config_get_uint(conf, buf, &settings->input.libretro_device[i]))
         break;
   }
   if (i > 0)
      settings->input.libretro_device_scope = scope;

   if (config_get_uint(conf, "input_analog_dpad_mode",
         &settings->input.analog_dpad_mode))
   {
      settings->input.analog_dpad_scope = scope;
      config_get_float(conf, "input_analog_diagonal_sensitivity",
            &settings->input.analog_diagonal_sensitivity);
      config_get_float(conf, "input_analog_dpad_deadzone",
            &settings->input.analog_dpad_deadzone);

      input_joypad_update_analog_dpad_params();
   }

   if (config_get_path(conf, "video_filter", settings->video.softfilter_plugin,
                       sizeof(settings->video.softfilter_plugin)))
   {
      if (!strcmp(settings->video.softfilter_plugin, EXPLICIT_NULL))
         *settings->video.softfilter_plugin = '\0';
      settings->video.filter_shader_scope = scope;
   }
   if (config_get_path(conf, "video_shader", settings->video.shader_path,
                       sizeof(settings->video.shader_path)))
   {
      if (!strcmp(settings->video.shader_path, EXPLICIT_NULL))
         *settings->video.shader_path = '\0';
      settings->video.filter_shader_scope = scope;
   }
   
   if (config_get_uint(conf, "preempt_frames", &settings->preempt_frames))
      settings->preempt_frames_scope = scope;
   
#ifdef HAVE_MENU
   if (config_get_path(conf, "menu_theme", settings->menu.theme,
                       sizeof(settings->menu.theme)))
   {
      if (!strcmp(settings->menu.theme, EXPLICIT_NULL))
         *settings->menu.theme = '\0';
      config_get_float(conf, "menu_wallpaper_opacity",
                       &settings->menu.wallpaper_opacity);
#ifdef HAVE_RGUI
      config_get_uint(conf, "rgui_particle_effect",
                      &settings->menu.rgui_particle_effect);
      config_get_float(conf, "rgui_particle_effect_speed_factor",
                       &settings->menu.rgui_particle_effect_speed_factor);
#endif
      settings->menu.theme_scope = scope;
      global->menu.theme_update_flag = true;
   }
#endif

   /* Core specific settings */
   if (scope == THIS_CORE)
   {
      if (config_get_path(conf, "rgui_browser_directory", buf, PATH_MAX_LENGTH))
         strlcpy(settings->core_content_directory, buf, PATH_MAX_LENGTH);
      config_get_bool(conf, "video_shared_context",
                      &settings->video.shared_context);
      config_get_bool(conf, "load_dummy_on_core_shutdown",
                      &settings->load_dummy_on_core_shutdown);
      config_get_bool(conf, "core_set_supports_no_game_enable",
                      &settings->core.set_supports_no_game_enable);
      config_get_bool(conf, "rewind_enable", &settings->rewind_enable);
      config_get_uint(conf, "rewind_buffer_size",
                      &settings->rewind_buffer_size);
   }

   /* Leave config in memory until scoped_config_file_save is called. */
   scoped_conf[scope] = conf;
}

void scoped_config_files_load_auto()
{
   /* Back up or unmask global settings */
   config_backup_restore_globals();
   
   scoped_config_file_load(THIS_CORE);
   scoped_config_file_load(THIS_CONTENT_DIR);
   scoped_config_file_load(THIS_CONTENT_ONLY);
}

void core_config_file_load_auto()
{
   config_backup_restore_globals();
   scoped_config_file_load(THIS_CORE);
}
