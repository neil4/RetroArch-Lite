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

#ifndef __CONFIG_DEF_H
#define __CONFIG_DEF_H

#include <boolean.h>
#include <libretro.h>
#include "configuration.h"
#include "gfx/video_viewport.h"

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

enum 
{
   VIDEO_GL = 0,
   VIDEO_XVIDEO,
   VIDEO_SDL,
   VIDEO_SDL2,
   VIDEO_EXT,
   VIDEO_WII,
   VIDEO_XENON360,
   VIDEO_XDK_D3D,
   VIDEO_PSP1,
   VIDEO_VITA,
   VIDEO_CTR,
   VIDEO_D3D9,
   VIDEO_VG,
   VIDEO_NULL,
   VIDEO_OMAP,
   VIDEO_EXYNOS,
   VIDEO_SUNXI,
   VIDEO_DISPMANX,

   AUDIO_RSOUND,
   AUDIO_OSS,
   AUDIO_ALSA,
   AUDIO_ALSATHREAD,
   AUDIO_ROAR,
   AUDIO_AL,
   AUDIO_SL,
   AUDIO_JACK,
   AUDIO_SDL,
   AUDIO_SDL2,
   AUDIO_XAUDIO,
   AUDIO_PULSE,
   AUDIO_EXT,
   AUDIO_DSOUND,
   AUDIO_COREAUDIO,
   AUDIO_PS3,
   AUDIO_XENON360,
   AUDIO_WII,
   AUDIO_RWEBAUDIO,
   AUDIO_PSP1,
   AUDIO_CTR,
   AUDIO_NULL,

   AUDIO_RESAMPLER_CC,
   AUDIO_RESAMPLER_SINC,
   AUDIO_RESAMPLER_NEAREST,

   INPUT_ANDROID,
   INPUT_SDL,
   INPUT_SDL2,
   INPUT_X,
   INPUT_WAYLAND,
   INPUT_DINPUT,
   INPUT_PS3,
   INPUT_PSP,
   INPUT_CTR,
   INPUT_XENON360,
   INPUT_WII,
   INPUT_XINPUT,
   INPUT_UDEV,
   INPUT_LINUXRAW,
   INPUT_COCOA,
   INPUT_QNX,
   INPUT_RWEBINPUT,
   INPUT_NULL,

   JOYPAD_PS3,
   JOYPAD_XINPUT,
   JOYPAD_GX,
   JOYPAD_XDK,
   JOYPAD_PSP,
   JOYPAD_CTR,
   JOYPAD_DINPUT,
   JOYPAD_UDEV,
   JOYPAD_LINUXRAW,
   JOYPAD_ANDROID,
   JOYPAD_SDL,
   JOYPAD_HID,
   JOYPAD_QNX,
   JOYPAD_NULL,

   OSK_PS3,
   OSK_NULL,

   MENU_RGUI,
   MENU_GLUI,
   MENU_XMB,

   RECORD_FFMPEG,
   RECORD_NULL,
};

#if defined(HAVE_OPENGL) || defined(HAVE_OPENGLES) || defined(__CELLOS_LV2__)
#define VIDEO_DEFAULT_DRIVER VIDEO_GL
#elif defined(GEKKO)
#define VIDEO_DEFAULT_DRIVER VIDEO_WII
#elif defined(XENON)
#define VIDEO_DEFAULT_DRIVER VIDEO_XENON360
#elif (defined(_XBOX1) || defined(_XBOX360)) && (defined(HAVE_D3D8) || defined(HAVE_D3D9))
#define VIDEO_DEFAULT_DRIVER VIDEO_XDK_D3D
#elif defined(HAVE_D3D9)
#define VIDEO_DEFAULT_DRIVER VIDEO_D3D9
#elif defined(HAVE_VG)
#define VIDEO_DEFAULT_DRIVER VIDEO_VG
#elif defined(SN_TARGET_PSP2)
#define VIDEO_DEFAULT_DRIVER VIDEO_VITA
#elif defined(PSP)
#define VIDEO_DEFAULT_DRIVER VIDEO_PSP1
#elif defined(_3DS)
#define VIDEO_DEFAULT_DRIVER VIDEO_CTR
#elif defined(HAVE_XVIDEO)
#define VIDEO_DEFAULT_DRIVER VIDEO_XVIDEO
#elif defined(HAVE_SDL)
#define VIDEO_DEFAULT_DRIVER VIDEO_SDL
#elif defined(HAVE_SDL2)
#define VIDEO_DEFAULT_DRIVER VIDEO_SDL2
#elif defined(HAVE_DYLIB) && !defined(ANDROID)
#define VIDEO_DEFAULT_DRIVER VIDEO_EXT
#else
#define VIDEO_DEFAULT_DRIVER VIDEO_NULL
#endif

#if defined(__CELLOS_LV2__)
#define AUDIO_DEFAULT_DRIVER AUDIO_PS3
#elif defined(XENON)
#define AUDIO_DEFAULT_DRIVER AUDIO_XENON360
#elif defined(GEKKO)
#define AUDIO_DEFAULT_DRIVER AUDIO_WII
#elif defined(PSP)
#define AUDIO_DEFAULT_DRIVER AUDIO_PSP1
#elif defined(_3DS)
#define AUDIO_DEFAULT_DRIVER AUDIO_CTR
#elif defined(HAVE_ALSA) && defined(HAVE_VIDEOCORE)
#define AUDIO_DEFAULT_DRIVER AUDIO_ALSATHREAD
#elif defined(HAVE_ALSA)
#define AUDIO_DEFAULT_DRIVER AUDIO_ALSA
#elif defined(HAVE_PULSE)
#define AUDIO_DEFAULT_DRIVER AUDIO_PULSE
#elif defined(HAVE_OSS)
#define AUDIO_DEFAULT_DRIVER AUDIO_OSS
#elif defined(HAVE_JACK)
#define AUDIO_DEFAULT_DRIVER AUDIO_JACK
#elif defined(HAVE_COREAUDIO)
#define AUDIO_DEFAULT_DRIVER AUDIO_COREAUDIO
#elif defined(HAVE_AL)
#define AUDIO_DEFAULT_DRIVER AUDIO_AL
#elif defined(HAVE_SL)
#define AUDIO_DEFAULT_DRIVER AUDIO_SL
#elif defined(HAVE_XAUDIO)
#define AUDIO_DEFAULT_DRIVER AUDIO_XAUDIO
#elif defined(EMSCRIPTEN)
#define AUDIO_DEFAULT_DRIVER AUDIO_RWEBAUDIO
#elif defined(HAVE_SDL)
#define AUDIO_DEFAULT_DRIVER AUDIO_SDL
#elif defined(HAVE_SDL2)
#define AUDIO_DEFAULT_DRIVER AUDIO_SDL2
#elif defined(HAVE_DSOUND)
#define AUDIO_DEFAULT_DRIVER AUDIO_DSOUND
#elif defined(HAVE_RSOUND)
#define AUDIO_DEFAULT_DRIVER AUDIO_RSOUND
#elif defined(HAVE_ROAR)
#define AUDIO_DEFAULT_DRIVER AUDIO_ROAR
#elif defined(HAVE_DYLIB) && !defined(ANDROID)
#define AUDIO_DEFAULT_DRIVER AUDIO_EXT
#else
#define AUDIO_DEFAULT_DRIVER AUDIO_NULL
#endif

#ifdef PSP
#define AUDIO_DEFAULT_RESAMPLER_DRIVER  AUDIO_RESAMPLER_CC
#else
#define AUDIO_DEFAULT_RESAMPLER_DRIVER  AUDIO_RESAMPLER_SINC
#endif

#if defined(HAVE_FFMPEG)
#define RECORD_DEFAULT_DRIVER RECORD_FFMPEG
#else
#define RECORD_DEFAULT_DRIVER RECORD_NULL
#endif

#if defined(XENON)
#define INPUT_DEFAULT_DRIVER INPUT_XENON360
#elif defined(_XBOX360) || defined(_XBOX) || defined(HAVE_XINPUT2) || defined(HAVE_XINPUT_XBOX1)
#define INPUT_DEFAULT_DRIVER INPUT_XINPUT
#elif defined(ANDROID)
#define INPUT_DEFAULT_DRIVER INPUT_ANDROID
#elif defined(EMSCRIPTEN)
#define INPUT_DEFAULT_DRIVER INPUT_RWEBINPUT
#elif defined(_WIN32)
#define INPUT_DEFAULT_DRIVER INPUT_DINPUT
#elif defined(__CELLOS_LV2__)
#define INPUT_DEFAULT_DRIVER INPUT_PS3
#elif (defined(SN_TARGET_PSP2) || defined(PSP))
#define INPUT_DEFAULT_DRIVER INPUT_PSP
#elif defined(_3DS)
#define INPUT_DEFAULT_DRIVER INPUT_CTR
#elif defined(GEKKO)
#define INPUT_DEFAULT_DRIVER INPUT_WII
#elif defined(HAVE_UDEV)
#define INPUT_DEFAULT_DRIVER INPUT_UDEV
#elif defined(__linux__) && !defined(ANDROID)
#define INPUT_DEFAULT_DRIVER INPUT_LINUXRAW
#elif defined(HAVE_X11)
#define INPUT_DEFAULT_DRIVER INPUT_X
#elif defined(HAVE_WAYLAND)
#define INPUT_DEFAULT_DRIVER INPUT_WAYLAND
#elif defined(HAVE_COCOA) || defined(HAVE_COCOATOUCH)
#define INPUT_DEFAULT_DRIVER INPUT_COCOA
#elif defined(__QNX__)
#define INPUT_DEFAULT_DRIVER INPUT_QNX
#elif defined(HAVE_SDL)
#define INPUT_DEFAULT_DRIVER INPUT_SDL
#elif defined(HAVE_SDL2)
#define INPUT_DEFAULT_DRIVER INPUT_SDL2
#else
#define INPUT_DEFAULT_DRIVER INPUT_NULL
#endif

#if defined(__CELLOS_LV2__)
#define JOYPAD_DEFAULT_DRIVER JOYPAD_PS3
#elif defined(HAVE_XINPUT)
#define JOYPAD_DEFAULT_DRIVER JOYPAD_XINPUT
#elif defined(GEKKO)
#define JOYPAD_DEFAULT_DRIVER JOYPAD_GX
#elif defined(_XBOX)
#define JOYPAD_DEFAULT_DRIVER JOYPAD_XDK
#elif defined(PSP)
#define JOYPAD_DEFAULT_DRIVER JOYPAD_PSP
#elif defined(_3DS)
#define JOYPAD_DEFAULT_DRIVER JOYPAD_CTR
#elif defined(HAVE_DINPUT)
#define JOYPAD_DEFAULT_DRIVER JOYPAD_DINPUT
#elif defined(HAVE_UDEV)
#define JOYPAD_DEFAULT_DRIVER JOYPAD_UDEV
#elif defined(__linux) && !defined(ANDROID)
#define JOYPAD_DEFAULT_DRIVER JOYPAD_LINUXRAW
#elif defined(ANDROID)
#define JOYPAD_DEFAULT_DRIVER JOYPAD_ANDROID
#elif defined(HAVE_SDL) || defined(HAVE_SDL2)
#define JOYPAD_DEFAULT_DRIVER JOYPAD_SDL
#elif defined(HAVE_HID)
#define JOYPAD_DEFAULT_DRIVER JOYPAD_HID
#elif defined(__QNX__)
#define JOYPAD_DEFAULT_DRIVER JOYPAD_QNX
#else
#define JOYPAD_DEFAULT_DRIVER JOYPAD_NULL
#endif

#if defined(__CELLOS_LV2__)
#define OSK_DEFAULT_DRIVER OSK_PS3
#else
#define OSK_DEFAULT_DRIVER OSK_NULL
#endif

#define MENU_DEFAULT_DRIVER MENU_RGUI

#if defined(XENON) || defined(_XBOX360) || defined(__CELLOS_LV2__)
#define DEFAULT_ASPECT_RATIO 1.7778f
#elif defined(_XBOX1) || defined(GEKKO) || defined(ANDROID) || defined(__QNX__)
#define DEFAULT_ASPECT_RATIO 1.3333f
#else
#define DEFAULT_ASPECT_RATIO -1.0f
#endif

#ifdef RARCH_MOBILE
static const bool pointer_enable = true;
#else
static const bool pointer_enable = false;
#endif

static const unsigned int def_user_language = 0;

/* VIDEO */

#if defined(_XBOX360)
#define DEFAULT_GAMMA 1
#else
#define DEFAULT_GAMMA 0
#endif

/* Windowed
 * Real x resolution = aspect * base_size * x scale
 * Real y resolution = base_size * y scale
 */
static const float scale = 3.0;

/* Fullscreen */

/* To start in Fullscreen, or not. */
static const bool fullscreen = false;

/* To use windowed mode or not when going fullscreen. */
static const bool windowed_fullscreen = true; 

/* Which monitor to prefer. 0 is any monitor, 1 and up selects
 * specific monitors, 1 being the first monitor. */
static const unsigned monitor_index = 0;

/* Fullscreen resolution. A value of 0 uses the desktop
 * resolution. */
static const unsigned fullscreen_x = 0;
static const unsigned fullscreen_y = 0;

#if defined(RARCH_CONSOLE) || defined(__APPLE__)
static const bool load_dummy_on_core_shutdown = false;
#else
static const bool load_dummy_on_core_shutdown = true;
#endif

/* Forcibly disable composition.
 * Only valid on Windows Vista/7/8 for now. */
static const bool disable_composition = false;

/* Video VSYNC (recommended) */
static const bool vsync = true;

/* Attempts to hard-synchronize CPU and GPU.
 * Can reduce latency at cost of performance. */
static const bool hard_sync = true;

/* Configures how many frames the GPU can run ahead of CPU.
 * 0: Syncs to GPU immediately.
 * 1: Syncs to previous frame.
 * 2: Etc ...
 */
static const unsigned hard_sync_frames = 1;

/* Sets how many milliseconds to delay after VSync before running the core.
 * Can reduce latency at cost of higher risk of stuttering.
 */
static const unsigned frame_delay = 0;

/* Inserts a black frame inbetween frames.
 * Useful for 120 Hz monitors who want to play 60 Hz material with eliminated 
 * ghosting. video_refresh_rate should still be configured as if it 
 * is a 60 Hz monitor (divide refresh rate by 2).
 */
static bool black_frame_insertion = false;

/* Uses a custom swap interval for VSync.
 * Set this to effectively divide the monitor refresh rate.
 */
static unsigned swap_interval = 1;

/* Use duplicate frames for swap intervals higher than 1. */
#ifdef ANDROID
static bool fake_swap_interval = true;
#else
static bool fake_swap_interval = false;
#endif

/* Threaded video. Will possibly increase performance significantly 
 * at the cost of worse synchronization and latency.
 */
static const bool video_threaded = false;

/* Set to true if HW render cores should get their private context. */
static const bool video_shared_context = true;

/* Sets GC/Wii screen width. */
static const unsigned video_viwidth = 640;

/* Removes 480i flicker, smooths picture a little. */
static const bool video_vfilter = true;

/* Smooths picture. */
static const bool video_smooth = true;

/* Only scale in integer steps.
 * The base size depends on system-reported geometry and aspect ratio.
 * If video_force_aspect is not set, X/Y will be integer scaled independently.
 */
static const bool scale_integer = false;

/* Controls aspect ratio handling. */

/* Automatic */
static const float aspect_ratio = DEFAULT_ASPECT_RATIO;

/* 1:1 PAR */
static const bool aspect_ratio_auto = false;

#if defined(__CELLOS_LV2) || defined(_XBOX360)
static unsigned aspect_ratio_idx = ASPECT_RATIO_16_9;
#elif defined(PSP)
static unsigned aspect_ratio_idx = ASPECT_RATIO_CORE;
#elif defined(RARCH_CONSOLE)
static unsigned aspect_ratio_idx = ASPECT_RATIO_CORE;
#else
static unsigned aspect_ratio_idx = ASPECT_RATIO_CORE;
#endif

/* Set false to request same-binary savestates
 * instead of same-instance savesates */
static const bool preempt_fast_savestates = true;

/* Save configuration file on exit. */
static bool config_save_on_exit = true;

static const bool default_overlay_enable = false;

static bool default_block_config_read = true;

static bool show_advanced_settings    = false;
static bool mame_titles = true;
static float wallpaper_opacity = 1.0f;
static float menu_ticker_speed = 2.0f;
#ifdef HAVE_OVERLAY
#ifdef ANDROID
static bool show_overlay_menu = true;
#else
static bool show_overlay_menu = false;
#endif
static float overlay_opacity = 0.5f;
static const unsigned overlay_dpad_diag_sens = 80;
static const unsigned overlay_abxy_diag_sens = 50;
static const float overlay_bisect_aspect_ratio = OVERLAY_MAX_BISECT;
static const bool overlay_shift_y_lock_edges = true;
static const bool overlay_mouse_hold_to_drag = true;
static const float overlay_mouse_speed = 1.0f;
static const float overlay_mouse_swipe_thres = 1.0f;
static const unsigned overlay_mouse_hold_ms = 200;
static const bool overlay_mouse_tap_and_drag = false;
static const unsigned overlay_mouse_tap_and_drag_ms = 200;
static const unsigned overlay_analog_recenter_zone = 0;
#endif
static bool show_frame_throttle_menu = true;
#ifdef HAVE_NETPLAY
static bool show_netplay_menu = true;
#endif
static bool show_saving_menu = false;
static bool show_core_menu = true;
static bool show_core_history_menu = true;
static bool show_driver_menu = false;
static bool show_ui_menu = false;
static bool show_logging_menu = false;
static bool show_cheat_options = false;
static bool menu_show_core_info = true;
static bool menu_show_system_info = true;
static bool show_configuration_menu = false;
static bool show_user_menu = false;
#ifdef EXTERNAL_LAUNCHER
static bool show_directory_menu = false;
static bool show_core_updater = false;
#else
static bool show_directory_menu = true;
static bool show_core_updater = true;
#endif
static bool show_recording_menu = false;
static bool show_core_updater_menu = false;
static bool show_font_menu = false;
#ifdef ANDROID
static bool show_hotkey_menu = false;
#else
static bool show_hotkey_menu = true;
#endif
static bool show_rewind_menu = false;

static unsigned core_history_size = 30;
#ifdef EXTERNAL_LAUNCHER
static bool core_history_show_always = true;
#else
static bool core_history_show_always = false;
#endif

static bool default_sort_savefiles_enable = true;
static bool default_sort_savestates_enable = true;

static unsigned default_menu_btn_ok      = RETRO_DEVICE_ID_JOYPAD_A;
static unsigned default_menu_btn_cancel  = RETRO_DEVICE_ID_JOYPAD_B;
static unsigned default_menu_btn_search  = RETRO_DEVICE_ID_JOYPAD_X;
static unsigned default_menu_btn_default = RETRO_DEVICE_ID_JOYPAD_START;
static unsigned default_menu_btn_info    = RETRO_DEVICE_ID_JOYPAD_SELECT;

/* Crop overscanned frames. */
static const bool crop_overscan = true;

/* Font size for on-screen messages. */
static const float font_size = 32;

/* Offset for where messages will be placed on-screen. 
 * Values are in range [0.0, 1.0]. */
static const float message_pos_offset_x = 0.05;
#ifdef RARCH_CONSOLE
static const float message_pos_offset_y = 0.90;
#else
static const float message_pos_offset_y = 0.05;
#endif

/* Color of the message.
 * RGB hex value. */
static const uint32_t message_color = 0xffff00;

/* Record post-filtered (CPU filter) video,
 * rather than raw game output. */
static const bool post_filter_record = false;

/* Screenshots post-shaded GPU output if available. */
static const bool gpu_screenshot = true;

/* Record post-shaded GPU output instead of raw game footage if available. */
static const bool gpu_record = false;

/* OSD-messages. */
static const bool font_enable = true;

/* The accurate refresh rate of your monitor (Hz).
 * This is used to calculate audio input rate with the formula:
 * audio_input_rate = game_input_rate * display_refresh_rate / 
 * game_refresh_rate.
 *
 * If the implementation does not report any values,
 * NTSC defaults will be assumed for compatibility.
 * This value should stay close to 60Hz to avoid large pitch changes.
 * If your monitor does not run at 60Hz, or something close to it, 
 * disable VSync, and leave this at its default. */
#if defined(RARCH_CONSOLE)
static const float refresh_rate = 60/1.001; 
#else
static const float refresh_rate = 60.0;
#endif

/* Allow games to set rotation. If false, rotation requests are 
 * honored, but ignored.
 * Used for setups where one manually rotates the monitor. */
static const bool allow_rotate = true;

/* AUDIO */

/* Will enable audio or not. */
static const bool audio_enable = true;

/* Output samplerate. */
static const unsigned out_rate = 48000;

/* Desired audio latency in milliseconds. Might not be honored 
 * if driver can't provide given latency. */
static const int out_latency = 64;

/* Will sync audio. (recommended) */
static const bool audio_sync = true;

/* Audio rate control. */
#if defined(GEKKO) || !defined(RARCH_CONSOLE)
static const bool rate_control = true;
#else
static const bool rate_control = false;
#endif

/* Rate control delta. Defines how much rate_control 
 * is allowed to adjust input rate. */
static const float rate_control_delta = 0.005;

/* Maximum timing skew. Defines how much adjust_system_rates
 * is allowed to adjust input rate. */
static const float max_timing_skew = 0.05;

/* Default audio volume in dB. (0.0 dB == unity gain). */
static const float audio_volume = 0.0;

/* MISC */

/* Enables displaying the current frames per second. */
static const bool fps_show = false;

/* Enables use of rewind. This will incur some memory footprint 
 * depending on the save state buffer. */
static const bool rewind_enable = false;

/* The buffer size for the rewind buffer. Very core dependant. */
static const unsigned rewind_buffer_size = 20; /* 20MiB */

/* How many frames to rewind at a time. */
static const unsigned rewind_granularity = 1;

/* Pause gameplay when gameplay loses focus. */
static const bool pause_nonactive = false;

/* Saves non-volatile SRAM at a regular interval.
 * It is measured in seconds. A value of 0 disables autosave. */
static const unsigned autosave_interval = 0;

/* When being client over netplay, use keybinds for 
 * user 1 rather than user 2. */
static const bool netplay_client_swap_input = true;

/* On save state load, block SRAM from being overwritten.
 * This could potentially lead to buggy games. */
static const bool block_sram_overwrite = false;

/* When saving savestates, state index is automatically 
 * incremented before saving.
 * When the content is loaded, state index will be set 
 * to the highest existing value. */
static const bool savestate_auto_index = false;

/* Automatically saves a savestate at the end of RetroArch's lifetime.
 * The path is $SRAM_PATH.auto.
 * RetroArch will automatically load any savestate with this path on 
 * startup if savestate_auto_load is set. */
static const bool savestate_auto_save = false;
static const bool savestate_auto_load = false;

/* Slowmotion ratio. */
static const float slowmotion_ratio = 3.0;

/* Maximum fast forward ratio. */
static const float fastforward_ratio = 1.0;

/* Normal core throttling. */
static const bool throttle_using_core_fps = true;

/* Enable stdin/network command interface. */
static const bool network_cmd_enable = false;
static const uint16_t network_cmd_port = 55355;
static const bool stdin_cmd_enable = false;

/* Show Menu start-up screen on boot. */
#ifdef EXTERNAL_LAUNCHER
static const bool menu_show_start_screen = false;
#else
static const bool menu_show_start_screen = true;
#endif

#ifdef RARCH_MOBILE
static const bool menu_mouse_support = false;
#else
static const bool menu_mouse_support = true;
#endif

static const bool menu_dpi_override_enable = false;

static const unsigned menu_dpi_override_value = 72;

/* Log level for libretro cores (GET_LOG_INTERFACE). */
static const unsigned libretro_log_level = 3;

#ifndef RARCH_DEFAULT_PORT
#define RARCH_DEFAULT_PORT 55435
#endif

/* KEYBINDS, JOYPAD */

/* Analog to D-Pad params; percentages */
static const unsigned analog_dpad_deadzone        = 33;
static const unsigned analog_diagonal_sensitivity = 100;

/* Axis threshold (between 0.0 and 1.0)
 * How far an axis must be tilted to result in a button press. */
static const float axis_threshold = 0.5f;

/* Turbo input rate (~10Hz default) */
static const unsigned turbo_period = 6;
static const bool show_turbo_id = true;

/* Enable input auto-detection. Will attempt to autoconfigure
 * gamepads, plug-and-play style. */
static const bool input_autodetect_enable = true;

static const bool lightgun_allow_oob = true;
#ifdef HAVE_OVERLAY
static const unsigned lightgun_trigger_delay = 1;
#endif

#if defined(ANDROID)
#if defined(ANDROID_ARM)
static char buildbot_server_url[] = "http://buildbot.libretro.com/nightly/android/latest/armeabi-v7a/";
#elif defined(ANDROID_AARCH64)
static char buildbot_server_url[] = "http://buildbot.libretro.com/nightly/android/latest/arm64-v8a/";
#elif defined(ANDROID_X86)
static char buildbot_server_url[] = "http://buildbot.libretro.com/nightly/android/latest/x86/";
#elif defined(ANDROID_X64)
static char buildbot_server_url[] = "http://buildbot.libretro.com/nightly/android/latest/x86_64/"
#else
static char buildbot_server_url[] = "";
#endif
#elif defined(IOS)
static char buildbot_server_url[] = "http://buildbot.libretro.com/nightly/ios/latest/";
#elif defined(OSX)
#if defined(__x86_64__)
static char buildbot_server_url[] = "http://buildbot.libretro.com/nightly/osx-x86_64/latest/";
#elif defined(__i386__) || defined(__i486__) || defined(__i686__)
static char buildbot_server_url[] = "http://buildbot.libretro.com/nightly/osx-i386/latest/";
#else
static char buildbot_server_url[] = "http://buildbot.libretro.com/nightly/osx-ppc/latest/";
#endif
#elif defined(_WIN32) && !defined(_XBOX)
#if defined(__x86_64__)
static char buildbot_server_url[] = "http://buildbot.libretro.com/nightly/windows/x86_64/latest/";
#elif defined(__i386__) || defined(__i486__) || defined(__i686__)
static char buildbot_server_url[] = "http://buildbot.libretro.com/nightly/windows/x86/latest/";
#endif
#elif defined(__linux__)
#if defined(__x86_64__)
static char buildbot_server_url[] = "http://buildbot.libretro.com/nightly/linux/x86_64/latest/";
#else
static char buildbot_server_url[] = "";
#endif
#else
static char buildbot_server_url[] = "";
#endif

static char buildbot_assets_server_url[] = "http://buildbot.libretro.com/assets/";

#ifndef IS_SALAMANDER
#include "intl/intl.h"

/* User 1 */
static const struct retro_keybind retro_keybinds_1[] = {
    /*     | RetroPad button            | desc                           | keyboard key  | js btn |     js axis   | */
   { true, RETRO_DEVICE_ID_JOYPAD_B,      RETRO_LBL_JOYPAD_B,              RETROK_z,       NO_BTN, 0, AXIS_NONE },
   { true, RETRO_DEVICE_ID_JOYPAD_Y,      RETRO_LBL_JOYPAD_Y,              RETROK_a,       NO_BTN, 0, AXIS_NONE },
   { true, RETRO_DEVICE_ID_JOYPAD_SELECT, RETRO_LBL_JOYPAD_SELECT,         RETROK_RSHIFT,  NO_BTN, 0, AXIS_NONE },
   { true, RETRO_DEVICE_ID_JOYPAD_START,  RETRO_LBL_JOYPAD_START,          RETROK_RETURN,  NO_BTN, 0, AXIS_NONE },
   { true, RETRO_DEVICE_ID_JOYPAD_UP,     RETRO_LBL_JOYPAD_UP,             RETROK_UP,      NO_BTN, 0, AXIS_NONE },
   { true, RETRO_DEVICE_ID_JOYPAD_DOWN,   RETRO_LBL_JOYPAD_DOWN,           RETROK_DOWN,    NO_BTN, 0, AXIS_NONE },
   { true, RETRO_DEVICE_ID_JOYPAD_LEFT,   RETRO_LBL_JOYPAD_LEFT,           RETROK_LEFT,    NO_BTN, 0, AXIS_NONE },
   { true, RETRO_DEVICE_ID_JOYPAD_RIGHT,  RETRO_LBL_JOYPAD_RIGHT,          RETROK_RIGHT,   NO_BTN, 0, AXIS_NONE },
   { true, RETRO_DEVICE_ID_JOYPAD_A,      RETRO_LBL_JOYPAD_A,              RETROK_x,       NO_BTN, 0, AXIS_NONE },
   { true, RETRO_DEVICE_ID_JOYPAD_X,      RETRO_LBL_JOYPAD_X,              RETROK_s,       NO_BTN, 0, AXIS_NONE },
   { true, RETRO_DEVICE_ID_JOYPAD_L,      RETRO_LBL_JOYPAD_L,              RETROK_d,       NO_BTN, 0, AXIS_NONE },
   { true, RETRO_DEVICE_ID_JOYPAD_R,      RETRO_LBL_JOYPAD_R,              RETROK_c,       NO_BTN, 0, AXIS_NONE },
   { true, RETRO_DEVICE_ID_JOYPAD_L2,     RETRO_LBL_JOYPAD_L2,             RETROK_UNKNOWN, NO_BTN, 0, AXIS_NONE },
   { true, RETRO_DEVICE_ID_JOYPAD_R2,     RETRO_LBL_JOYPAD_R2,             RETROK_UNKNOWN, NO_BTN, 0, AXIS_NONE },
   { true, RETRO_DEVICE_ID_JOYPAD_L3,     RETRO_LBL_JOYPAD_L3,             RETROK_UNKNOWN, NO_BTN, 0, AXIS_NONE },
   { true, RETRO_DEVICE_ID_JOYPAD_R3,     RETRO_LBL_JOYPAD_R3,             RETROK_UNKNOWN, NO_BTN, 0, AXIS_NONE },

   { true, RARCH_ANALOG_LEFT_X_PLUS,      RETRO_LBL_ANALOG_LEFT_X_PLUS,    RETROK_UNKNOWN, NO_BTN, 0, AXIS_NONE },
   { true, RARCH_ANALOG_LEFT_X_MINUS,     RETRO_LBL_ANALOG_LEFT_X_MINUS,   RETROK_UNKNOWN, NO_BTN, 0, AXIS_NONE },
   { true, RARCH_ANALOG_LEFT_Y_PLUS,      RETRO_LBL_ANALOG_LEFT_Y_PLUS,    RETROK_UNKNOWN, NO_BTN, 0, AXIS_NONE },
   { true, RARCH_ANALOG_LEFT_Y_MINUS,     RETRO_LBL_ANALOG_LEFT_Y_MINUS,   RETROK_UNKNOWN, NO_BTN, 0, AXIS_NONE },
   { true, RARCH_ANALOG_RIGHT_X_PLUS,     RETRO_LBL_ANALOG_RIGHT_X_PLUS,   RETROK_UNKNOWN, NO_BTN, 0, AXIS_NONE },
   { true, RARCH_ANALOG_RIGHT_X_MINUS,    RETRO_LBL_ANALOG_RIGHT_X_MINUS,  RETROK_UNKNOWN, NO_BTN, 0, AXIS_NONE },
   { true, RARCH_ANALOG_RIGHT_Y_PLUS,     RETRO_LBL_ANALOG_RIGHT_Y_PLUS,   RETROK_UNKNOWN, NO_BTN, 0, AXIS_NONE },
   { true, RARCH_ANALOG_RIGHT_Y_MINUS,    RETRO_LBL_ANALOG_RIGHT_Y_MINUS,  RETROK_UNKNOWN, NO_BTN, 0, AXIS_NONE },

   { true, RARCH_LIGHTGUN_TRIGGER,        RETRO_LBL_LIGHTGUN_TRIGGER,      RETROK_UNKNOWN, NO_BTN, 0, AXIS_NONE },
   { true, RARCH_LIGHTGUN_START,          RETRO_LBL_LIGHTGUN_START,        RETROK_UNKNOWN, NO_BTN, 0, AXIS_NONE },
   { true, RARCH_LIGHTGUN_SELECT,         RETRO_LBL_LIGHTGUN_SELECT,       RETROK_UNKNOWN, NO_BTN, 0, AXIS_NONE },
   { true, RARCH_LIGHTGUN_AUX_A,          RETRO_LBL_LIGHTGUN_AUX_A,        RETROK_UNKNOWN, NO_BTN, 0, AXIS_NONE },
   { true, RARCH_LIGHTGUN_AUX_B,          RETRO_LBL_LIGHTGUN_AUX_B,        RETROK_UNKNOWN, NO_BTN, 0, AXIS_NONE },
   { true, RARCH_LIGHTGUN_AUX_C,          RETRO_LBL_LIGHTGUN_AUX_C,        RETROK_UNKNOWN, NO_BTN, 0, AXIS_NONE },
   { true, RARCH_LIGHTGUN_RELOAD,         RETRO_LBL_LIGHTGUN_RELOAD,       RETROK_UNKNOWN, NO_BTN, 0, AXIS_NONE },

   { true, RARCH_TOGGLE_KEYBOARD_FOCUS,   RETRO_LBL_TOGGLE_KEYBOARD_FOCUS, RETROK_SCROLLOCK, NO_BTN, 0, AXIS_NONE },
   { true, RARCH_ENABLE_HOTKEY,           RETRO_LBL_ENABLE_HOTKEY,         RETROK_UNKNOWN, NO_BTN, 0, AXIS_NONE },
   { true, RARCH_FAST_FORWARD_KEY,        RETRO_LBL_FAST_FORWARD_KEY,      RETROK_UNKNOWN, NO_BTN, 0, AXIS_NONE },
   { true, RARCH_FAST_FORWARD_HOLD_KEY,   RETRO_LBL_FAST_FORWARD_HOLD_KEY, RETROK_SPACE,   NO_BTN, 0, AXIS_NONE },
   { true, RARCH_LOAD_STATE_KEY,          RETRO_LBL_LOAD_STATE_KEY,        RETROK_F4,      NO_BTN, 0, AXIS_NONE },
   { true, RARCH_SAVE_STATE_KEY,          RETRO_LBL_SAVE_STATE_KEY,        RETROK_F2,      NO_BTN, 0, AXIS_NONE },
   { true, RARCH_FULLSCREEN_TOGGLE_KEY,   RETRO_LBL_FULLSCREEN_TOGGLE_KEY, RETROK_f,       NO_BTN, 0, AXIS_NONE },
   { true, RARCH_QUIT_KEY,                RETRO_LBL_QUIT_KEY,              RETROK_q,       NO_BTN, 0, AXIS_NONE },
   { true, RARCH_STATE_SLOT_PLUS,         RETRO_LBL_STATE_SLOT_PLUS,       RETROK_F7,      NO_BTN, 0, AXIS_NONE },
   { true, RARCH_STATE_SLOT_MINUS,        RETRO_LBL_STATE_SLOT_MINUS,      RETROK_F6,      NO_BTN, 0, AXIS_NONE },
   { true, RARCH_REWIND,                  RETRO_LBL_REWIND,                RETROK_UNKNOWN, NO_BTN, 0, AXIS_NONE },
   { true, RARCH_PAUSE_TOGGLE,            RETRO_LBL_PAUSE_TOGGLE,          RETROK_p,       NO_BTN, 0, AXIS_NONE },
   { true, RARCH_FRAMEADVANCE,            RETRO_LBL_FRAMEADVANCE,          RETROK_k,       NO_BTN, 0, AXIS_NONE },
   { true, RARCH_RESET,                   RETRO_LBL_RESET,                 RETROK_UNKNOWN, NO_BTN, 0, AXIS_NONE },
   { true, RARCH_SHADER_NEXT,             RETRO_LBL_SHADER_NEXT,           RETROK_UNKNOWN, NO_BTN, 0, AXIS_NONE },
   { true, RARCH_SHADER_PREV,             RETRO_LBL_SHADER_PREV,           RETROK_UNKNOWN, NO_BTN, 0, AXIS_NONE },
   { true, RARCH_CHEAT_INDEX_PLUS,        RETRO_LBL_CHEAT_INDEX_PLUS,      RETROK_UNKNOWN, NO_BTN, 0, AXIS_NONE },
   { true, RARCH_CHEAT_INDEX_MINUS,       RETRO_LBL_CHEAT_INDEX_MINUS,     RETROK_UNKNOWN, NO_BTN, 0, AXIS_NONE },
   { true, RARCH_CHEAT_TOGGLE,            RETRO_LBL_CHEAT_TOGGLE,          RETROK_UNKNOWN, NO_BTN, 0, AXIS_NONE },
   { true, RARCH_SCREENSHOT,              RETRO_LBL_SCREENSHOT,            RETROK_UNKNOWN, NO_BTN, 0, AXIS_NONE },
   { true, RARCH_MUTE,                    RETRO_LBL_MUTE,                  RETROK_UNKNOWN, NO_BTN, 0, AXIS_NONE },
   { true, RARCH_OSK,                     RETRO_LBL_OSK,                   RETROK_UNKNOWN, NO_BTN, 0, AXIS_NONE },
   { true, RARCH_NETPLAY_FLIP,            RETRO_LBL_NETPLAY_FLIP,          RETROK_UNKNOWN, NO_BTN, 0, AXIS_NONE },
   { true, RARCH_SLOWMOTION,              RETRO_LBL_SLOWMOTION,            RETROK_UNKNOWN, NO_BTN, 0, AXIS_NONE },
   { true, RARCH_ADVANCED_TOGGLE,         RETRO_LBL_ADVANCED_TOGGLE,       RETROK_TAB,     NO_BTN, 0, AXIS_NONE },
   { true, RARCH_OVERLAY_NEXT,            RETRO_LBL_OVERLAY_NEXT,          RETROK_UNKNOWN, NO_BTN, 0, AXIS_NONE },
   { true, RARCH_DISK_EJECT_TOGGLE,       RETRO_LBL_DISK_EJECT_TOGGLE,     RETROK_UNKNOWN, NO_BTN, 0, AXIS_NONE },
   { true, RARCH_DISK_NEXT,               RETRO_LBL_DISK_NEXT,             RETROK_UNKNOWN, NO_BTN, 0, AXIS_NONE },
   { true, RARCH_DISK_PREV,               RETRO_LBL_DISK_PREV,             RETROK_UNKNOWN, NO_BTN, 0, AXIS_NONE },
   { true, RARCH_GRAB_MOUSE_TOGGLE,       RETRO_LBL_GRAB_MOUSE_TOGGLE,     RETROK_F11,     NO_BTN, 0, AXIS_NONE },
   { true, RARCH_MENU_TOGGLE,             RETRO_LBL_MENU_TOGGLE,           RETROK_ESCAPE,  NO_BTN, 0, AXIS_NONE },
};

/* Users 2 to MAX_USERS */
static const struct retro_keybind retro_keybinds_rest[] = {
    /*     | RetroPad button            | desc                           | keyboard key  | js btn |     js axis   | */
   { true, RETRO_DEVICE_ID_JOYPAD_B,      RETRO_LBL_JOYPAD_B,              RETROK_UNKNOWN, NO_BTN, 0, AXIS_NONE },
   { true, RETRO_DEVICE_ID_JOYPAD_Y,      RETRO_LBL_JOYPAD_Y,              RETROK_UNKNOWN, NO_BTN, 0, AXIS_NONE },
   { true, RETRO_DEVICE_ID_JOYPAD_SELECT, RETRO_LBL_JOYPAD_SELECT,         RETROK_UNKNOWN, NO_BTN, 0, AXIS_NONE },
   { true, RETRO_DEVICE_ID_JOYPAD_START,  RETRO_LBL_JOYPAD_START,          RETROK_UNKNOWN, NO_BTN, 0, AXIS_NONE },
   { true, RETRO_DEVICE_ID_JOYPAD_UP,     RETRO_LBL_JOYPAD_UP,             RETROK_UNKNOWN, NO_BTN, 0, AXIS_NONE },
   { true, RETRO_DEVICE_ID_JOYPAD_DOWN,   RETRO_LBL_JOYPAD_DOWN,           RETROK_UNKNOWN, NO_BTN, 0, AXIS_NONE },
   { true, RETRO_DEVICE_ID_JOYPAD_LEFT,   RETRO_LBL_JOYPAD_LEFT,           RETROK_UNKNOWN, NO_BTN, 0, AXIS_NONE },
   { true, RETRO_DEVICE_ID_JOYPAD_RIGHT,  RETRO_LBL_JOYPAD_RIGHT,          RETROK_UNKNOWN, NO_BTN, 0, AXIS_NONE },
   { true, RETRO_DEVICE_ID_JOYPAD_A,      RETRO_LBL_JOYPAD_A,              RETROK_UNKNOWN, NO_BTN, 0, AXIS_NONE },
   { true, RETRO_DEVICE_ID_JOYPAD_X,      RETRO_LBL_JOYPAD_X,              RETROK_UNKNOWN, NO_BTN, 0, AXIS_NONE },
   { true, RETRO_DEVICE_ID_JOYPAD_L,      RETRO_LBL_JOYPAD_L,              RETROK_UNKNOWN, NO_BTN, 0, AXIS_NONE },
   { true, RETRO_DEVICE_ID_JOYPAD_R,      RETRO_LBL_JOYPAD_R,              RETROK_UNKNOWN, NO_BTN, 0, AXIS_NONE },
   { true, RETRO_DEVICE_ID_JOYPAD_L2,     RETRO_LBL_JOYPAD_L2,             RETROK_UNKNOWN, NO_BTN, 0, AXIS_NONE },
   { true, RETRO_DEVICE_ID_JOYPAD_R2,     RETRO_LBL_JOYPAD_R2,             RETROK_UNKNOWN, NO_BTN, 0, AXIS_NONE },
   { true, RETRO_DEVICE_ID_JOYPAD_L3,     RETRO_LBL_JOYPAD_L3,             RETROK_UNKNOWN, NO_BTN, 0, AXIS_NONE },
   { true, RETRO_DEVICE_ID_JOYPAD_R3,     RETRO_LBL_JOYPAD_R3,             RETROK_UNKNOWN, NO_BTN, 0, AXIS_NONE },

   { true, RARCH_ANALOG_LEFT_X_PLUS,      RETRO_LBL_ANALOG_LEFT_X_PLUS,    RETROK_UNKNOWN, NO_BTN, 0, AXIS_NONE },
   { true, RARCH_ANALOG_LEFT_X_MINUS,     RETRO_LBL_ANALOG_LEFT_X_MINUS,   RETROK_UNKNOWN, NO_BTN, 0, AXIS_NONE },
   { true, RARCH_ANALOG_LEFT_Y_PLUS,      RETRO_LBL_ANALOG_LEFT_Y_PLUS,    RETROK_UNKNOWN, NO_BTN, 0, AXIS_NONE },
   { true, RARCH_ANALOG_LEFT_Y_MINUS,     RETRO_LBL_ANALOG_LEFT_Y_MINUS,   RETROK_UNKNOWN, NO_BTN, 0, AXIS_NONE },
   { true, RARCH_ANALOG_RIGHT_X_PLUS,     RETRO_LBL_ANALOG_RIGHT_X_PLUS,   RETROK_UNKNOWN, NO_BTN, 0, AXIS_NONE },
   { true, RARCH_ANALOG_RIGHT_X_MINUS,    RETRO_LBL_ANALOG_RIGHT_X_MINUS,  RETROK_UNKNOWN, NO_BTN, 0, AXIS_NONE },
   { true, RARCH_ANALOG_RIGHT_Y_PLUS,     RETRO_LBL_ANALOG_RIGHT_Y_PLUS,   RETROK_UNKNOWN, NO_BTN, 0, AXIS_NONE },
   { true, RARCH_ANALOG_RIGHT_Y_MINUS,    RETRO_LBL_ANALOG_RIGHT_Y_MINUS,  RETROK_UNKNOWN, NO_BTN, 0, AXIS_NONE },
};

#endif

#endif

