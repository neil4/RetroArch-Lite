/*  RetroArch - A frontend for libretro.
 *  Copyright (C) 2010-2014 - Hans-Kristian Arntzen
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

#ifndef __RARCH_SETTINGS_H__
#define __RARCH_SETTINGS_H__

#include <stdint.h>
#include <retro_miscellaneous.h>
#include "driver.h"

#ifndef MAX_USERS
#define MAX_USERS 16
#endif

#define EXPLICIT_NULL "nul"

#ifdef __cplusplus
extern "C" {
#endif

/* All config related settings go here. */

/* Higher scopes are more specific and mask lower scopes */
enum setting_scope
{
   GLOBAL = 0,
   THIS_CORE,
   THIS_CONTENT_DIR,
   THIS_CONTENT_ONLY,
   NUM_SETTING_SCOPES
};

struct enum_lut
{
   const char* name;
   float value;
};

extern struct enum_lut scope_lut[NUM_SETTING_SCOPES];

extern bool settings_touched;
extern bool scoped_settings_touched;

typedef struct settings
{
   struct
   {
      char driver[32];
      char context_driver[32];
      float scale;
      bool fullscreen;
      bool windowed_fullscreen;
      unsigned monitor_index;
      unsigned fullscreen_x;
      unsigned fullscreen_y;
      bool vsync;
      unsigned vsync_scope;
      bool hard_sync;
      unsigned hard_sync_scope;
      bool black_frame_insertion;
      unsigned swap_interval;
      bool fake_swap_interval;
      unsigned hard_sync_frames;
      unsigned frame_delay;
      unsigned frame_delay_scope;
#ifdef GEKKO
      unsigned viwidth;
      bool vfilter;
#endif
      bool smooth;
      bool crop_overscan;
      float aspect_ratio;
      bool scale_integer;
      unsigned aspect_ratio_idx;
      unsigned aspect_ratio_idx_scope;
      unsigned rotation;
      unsigned rotation_scope;

      char shader_path[PATH_MAX_LENGTH];

      char softfilter_plugin[PATH_MAX_LENGTH];
      unsigned filter_shader_scope;
      float refresh_rate;
      bool threaded;
      unsigned threaded_scope;

      char filter_dir[PATH_MAX_LENGTH];
      char shader_dir[PATH_MAX_LENGTH];

      char font_path[PATH_MAX_LENGTH];
      float font_size;
      bool font_enable;
      float msg_pos_x;
      float msg_pos_y;
      float msg_color_r;
      float msg_color_g;
      float msg_color_b;

      bool disable_composition;

      bool post_filter_record;
      bool gpu_record;
      bool gpu_screenshot;

      bool allow_rotate;
      bool shared_context;
      unsigned shared_context_scope;
      bool force_srgb_disable;
   } video;

   struct
   {
      char driver[32];
   } record;

   struct
   {
      bool menubar_enable;
      bool suspend_screensaver_enable;
      bool companion_start_on_boot;
   } ui;

   struct
   {
      char driver[32];
      char theme_dir[PATH_MAX_LENGTH];
      char theme[PATH_MAX_LENGTH];
      unsigned rgui_particle_effect;
      unsigned theme_scope;
      unsigned timedate_mode;
      float rgui_particle_effect_speed_factor;
      float wallpaper_opacity;
      float ticker_speed;
      bool pause_libretro;
      bool core_enable;
      bool dynamic_wallpaper_enable;
      bool boxart_enable;
      bool rgui_thick_bg_checkerboard;
      bool rgui_thick_bd_checkerboard;

      struct
      {
         bool enable;
      } mouse;

      struct
      {
         bool enable;
      } pointer;

      struct
      {
         struct
         {
            bool vertical_enable;
         } wraparound;
         struct
         {
            struct
            {
               bool supported_extensions_enable;
            } filter;
         } browser;
      } navigation;

      struct
      {
         bool     override_enable;
         unsigned override_value;
      } dpi;

      bool show_advanced_settings;
      
      bool mame_titles;
#ifdef HAVE_OVERLAY
      bool show_overlay_menu;
      bool show_osk_menu;
      bool show_overlay_mouse_menu;
      bool show_overlay_lightgun_menu;
#endif
      bool show_frame_throttle_menu;
      bool show_netplay_menu;
      bool show_saving_menu;
      bool show_hotkey_menu;
      bool show_rewind_menu;
      bool show_core_menu;
      bool show_core_history_menu;
      bool show_cheat_options;
      bool show_driver_menu;
      bool show_ui_menu;
      bool show_logging_menu;
      bool show_configuration_menu;
      bool show_user_menu;
      bool show_directory_menu;
      bool show_recording_menu;
      bool show_core_updater_menu;
      bool show_font_menu;
      bool show_core_info;
      bool show_system_info;
      bool show_core_updater;
      bool swap_ok_cancel;
   } menu;

   struct
   {
      char driver[32];
      bool enable;
      bool mute_enable;
      bool sync;
      unsigned sync_scope;
      unsigned out_rate;
      unsigned block_frames;
      unsigned latency;

      float volume; /* dB scale. */
      unsigned volume_scope;

      bool rate_control;
      float rate_control_delta;
      float max_timing_skew;
      unsigned max_timing_skew_scope;

      char dsp_plugin[PATH_MAX_LENGTH];
      char filter_dir[PATH_MAX_LENGTH];
      unsigned dsp_scope;

      char resampler[32];
   } audio;

   struct input_struct
   {
      char driver[32];
      char joypad_driver[32];
      char keyboard_layout[64];

      unsigned remap_ids[MAX_USERS][RARCH_BIND_LIST_END];
      struct retro_keybind binds[MAX_USERS][RARCH_BIND_LIST_END];
      struct retro_keybind autoconf_binds[MAX_USERS][RARCH_BIND_LIST_END];

      unsigned turbo_id[MAX_USERS];
      unsigned turbo_remap_id[MAX_USERS];

      unsigned max_users;
      unsigned max_users_scope;

      /* Set by autoconfiguration in joypad_autoconfig_dir.
       * Does not override main binds. */
      bool autoconfigured[MAX_USERS];

      unsigned libretro_device[MAX_USERS];

      bool rumble_enable;
      bool remap_binds_enable;
      bool turbo_binds_enable;
      bool auto_keyboard_focus;
      unsigned turbo_period;
      unsigned turbo_settings_scope;
      float axis_threshold;
      unsigned axis_threshold_scope;
      unsigned analog_dpad_mode;
      unsigned analog_diagonal_sensitivity;
      unsigned analog_dpad_deadzone;
      unsigned analog_dpad_scope;

      unsigned joypad_map[MAX_USERS];
      char device_names[MAX_USERS][64];
      bool autodetect_enable;
      bool netplay_client_swap_input;

      char overlay[PATH_MAX_LENGTH];
      unsigned overlay_scope;
      float overlay_opacity;
      float overlay_scale;
      bool overlay_adjust_aspect;
      uint32_t overlay_aspect_ratio_index;
      float overlay_bisect_aspect_ratio;
      unsigned overlay_aspect_scope;
      float overlay_shift_y;
      float overlay_shift_x;
      bool overlay_shift_y_lock_edges;
      unsigned overlay_shift_xy_scope;
      unsigned overlay_dpad_diag_sens;  /* diagonal-to-normal ratio (percent) */
      unsigned overlay_abxy_diag_sens;  /* diagonal-to-normal ratio (percent) */
      unsigned overlay_dpad_method;
      unsigned overlay_abxy_method;
      unsigned overlay_dpad_abxy_analog_config_scope;
      float touch_ellipse_magnify;   /* hack for inaccurate touchscreens */
      unsigned overlay_analog_recenter_zone;

      char osk_overlay[PATH_MAX_LENGTH];
      unsigned osk_scope;
      float osk_opacity;
      unsigned osk_opacity_scope;

      float overlay_mouse_speed;
      bool overlay_mouse_tap_and_drag;
      unsigned overlay_mouse_tap_and_drag_ms;
      unsigned overlay_mouse_tap_and_drag_scope;
      bool overlay_mouse_hold_to_drag;
      unsigned overlay_mouse_hold_ms;
      unsigned overlay_mouse_hold_to_drag_scope;
      float overlay_mouse_swipe_thres;
      unsigned overlay_mouse_swipe_thres_scope;
      unsigned lightgun_trigger_delay;
      unsigned lightgun_trigger_delay_scope;
      unsigned lightgun_two_touch_input;
      unsigned lightgun_two_touch_input_scope;
      bool lightgun_allow_oob;
      unsigned lightgun_allow_oob_scope;

      int overlay_vibrate_time;

      unsigned menu_toggle_btn_combo;
#ifdef ANDROID
      bool back_btn_toggles_menu;
#endif

      char autoconfig_dir[PATH_MAX_LENGTH];
      bool autoconfig_descriptor_label_show;

      char remapping_path[PATH_MAX_LENGTH];
   } input;

   struct
   {
      unsigned mode;
   } archive;

   struct
   {
      char buildbot_url[PATH_MAX_LENGTH];
      char buildbot_assets_url[PATH_MAX_LENGTH];
      bool buildbot_auto_extract_archive;
   } network;

   struct
   {
      bool start_without_content;
      bool option_categories;
      bool history_write;
      bool history_show_always;
      unsigned history_size;
      unsigned history_scope;
   } core;

   int state_slot;

   unsigned libretro_log_level;

   char libretro[PATH_MAX_LENGTH];
   char libretro_directory[PATH_MAX_LENGTH];
   char libretro_info_path[PATH_MAX_LENGTH];
   char cheat_database[PATH_MAX_LENGTH];
   char cheat_settings_path[PATH_MAX_LENGTH];
   char input_remapping_directory[PATH_MAX_LENGTH];

   char screenshot_directory[PATH_MAX_LENGTH];
   char system_directory[PATH_MAX_LENGTH];

   char extraction_directory[PATH_MAX_LENGTH];

   bool rewind_enable;
   unsigned rewind_buffer_size; /* MB */
   unsigned rewind_granularity;

   unsigned preempt_frames;
   unsigned preempt_frames_scope;
   bool preempt_fast_savestates;

   float slowmotion_ratio;
   float fastforward_ratio;
   bool core_throttle_enable;
   unsigned throttle_setting_scope;
   bool throttle_using_core_fps;

   bool pause_nonactive;
   unsigned autosave_interval;

   bool block_sram_overwrite;
   bool savestate_auto_index;
   bool savestate_auto_save;
   bool savestate_auto_load;

   bool network_cmd_enable;
   unsigned network_cmd_port;
   bool stdin_cmd_enable;
   bool netplay_periodic_resync;
   bool netplay_show_crc_checks;
   bool netplay_show_rollback;

   char core_assets_directory[PATH_MAX_LENGTH];
   char assets_directory[PATH_MAX_LENGTH];
   char dynamic_wallpapers_directory[PATH_MAX_LENGTH];
   char boxarts_directory[PATH_MAX_LENGTH];
   char menu_config_directory[PATH_MAX_LENGTH];
   char menu_content_directory[PATH_MAX_LENGTH];
   char core_content_directory[PATH_MAX_LENGTH];
   bool menu_show_start_screen;
   bool fps_show;
   bool load_dummy_on_core_shutdown;

   bool sort_savefiles_enable;
   bool sort_savestates_enable;

   bool savestate_file_compression;
   bool sram_file_compression;

   unsigned menu_ok_btn;
   unsigned menu_cancel_btn;
   unsigned menu_search_btn;
   unsigned menu_default_btn;
   unsigned menu_info_btn;

   char username[32];

   bool config_save_on_exit;
} settings_t;

enum config_var_type
{
   CV_BOOL,
   CV_INT,
   CV_UINT,
   CV_FLOAT,
   CV_PATH
};

struct setting_desc
{
   const char* key;
   enum config_var_type type;
   void* ptr;
   unsigned* scope_ptr;
};

/**
 * config_get_default_osk:
 *
 * Gets default OSK driver.
 *
 * Returns: Default OSK driver.
 **/
const char *config_get_default_osk(void);

/**
 * config_get_default_video:
 *
 * Gets default video driver.
 *
 * Returns: Default video driver.
 **/
const char *config_get_default_video(void);

/**
 * config_get_default_audio:
 *
 * Gets default audio driver.
 *
 * Returns: Default audio driver.
 **/
const char *config_get_default_audio(void);

/**
 * config_get_default_audio_resampler:
 *
 * Gets default audio resampler driver.
 *
 * Returns: Default audio resampler driver.
 **/
const char *config_get_default_audio_resampler(void);

/**
 * config_get_default_input:
 *
 * Gets default input driver.
 *
 * Returns: Default input driver.
 **/
const char *config_get_default_input(void);

/**
 * config_get_default_joypad:
 *
 * Gets default input joypad driver.
 *
 * Returns: Default input joypad driver.
 **/
const char *config_get_default_joypad(void);

/**
 * config_get_default_menu:
 *
 * Gets default menu driver.
 *
 * Returns: Default menu driver.
 **/
const char *config_get_default_menu(void);

const char *config_get_default_record(void);

/**
 * config_load:
 *
 * Loads a config file and reads all the values into memory.
 *
 */
void config_load(void);

/**
 * config_save_scoped_files()
 * 
 * Saves settings scoped to the content, content directory, and core
 */
void config_save_scoped_files();

/**
 * config_load_scoped_files
 * 
 * Called while loading content. Loads scoped config files.
 */
void config_load_scoped_files();

/**
 * config_load_core_file
 * 
 * Called when just loading a core. Loads core's config file
 */
void config_load_core_file();

/**
 * get_scoped_config_filename:
 * @out                      : NAME_MAX_LENGTH buffer
 * @scope                    : THIS_CORE, THIS_CONTENT_DIR, or THIS_CONTENT_ONLY
 * @ext                      : file extension to append
 *
 * Returns true if successful
 */
bool get_scoped_config_filename(char* out, const unsigned scope,
      const char* ext);

/**
 * config_unmask_globals
 * 
 * Called between ROM loads. Restores scoped settings to global values, and
 * updates backups for already-global values.
 */
void config_unmask_globals();

/**
 * main_config_file_save:
 * @path                : Path that shall be written to.
 *
 * Writes a global config file to disk.
 *
 * Returns: true (1) on success, otherwise returns false (0).
 **/
bool main_config_file_save(const char *path);

settings_t *config_init(void);

void config_free(void);

settings_t *config_get_ptr(void);

#ifdef __cplusplus
}
#endif

#endif
