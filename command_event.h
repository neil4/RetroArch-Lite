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

#ifndef COMMAND_EVENT_H__
#define COMMAND_EVENT_H__

#include <stdint.h>
#include <boolean.h>
#ifdef __cplusplus
extern "C" {
#endif
   
bool in_core_deinit();

enum event_command
{
   EVENT_CMD_NONE = 0,
   /* Resets RetroArch. */
   EVENT_CMD_RESET,
   /* Loads content file. */
   EVENT_CMD_LOAD_CONTENT,
   EVENT_CMD_LOAD_CONTENT_PERSIST,
   /* Loads core. */
   EVENT_CMD_LOAD_CORE_DEINIT,
   EVENT_CMD_LOAD_CORE,
   EVENT_CMD_LOAD_CORE_PERSIST,
   EVENT_CMD_UNLOAD_CORE,
   EVENT_CMD_LOAD_STATE,
   EVENT_CMD_SAVE_STATE,
   /* Takes screenshot. */
   EVENT_CMD_TAKE_SCREENSHOT,
   /* Initializes dummy core. */
   EVENT_CMD_PREPARE_DUMMY,
   /* Quits RetroArch. */
   EVENT_CMD_QUIT,
   /* Reinitialize all drivers. */
   EVENT_CMD_REINIT,
   /* Deinitialize rewind. */
   EVENT_CMD_REWIND_DEINIT,
   /* Initializes rewind. */
   EVENT_CMD_REWIND_INIT,
   /* Toggles rewind. */
   EVENT_CMD_REWIND_TOGGLE,
   /* Deinitializes autosave. */
   EVENT_CMD_AUTOSAVE_DEINIT,
   /* Initializes autosave. */
   EVENT_CMD_AUTOSAVE_INIT,
   EVENT_CMD_AUTOSAVE_STATE,
   /* Stops audio. */
   EVENT_CMD_AUDIO_STOP,
   /* Starts audio. */
   EVENT_CMD_AUDIO_START,
   /* Mutes audio. */
   EVENT_CMD_AUDIO_MUTE_TOGGLE,
   /* Loads overlay. */
   EVENT_CMD_OVERLAY_LOAD,
   /* Disables and caches overlay */
   EVENT_CMD_OVERLAY_UNLOAD,
   /* Swaps current overlay with cached overlay. */
   EVENT_CMD_OVERLAY_SWAP_CACHED,
   /* Deletes cached overlay. */
   EVENT_CMD_OVERLAY_FREE_CACHED,
   /* Sets current scale factor for overlay. */
   EVENT_CMD_OVERLAY_SET_SCALE_FACTOR,
   EVENT_CMD_OVERLAY_UPDATE_ASPECT_AND_SHIFT,
   EVENT_CMD_OVERLAY_UPDATE_EIGHTWAY_DIAG_SENS,
   /* Sets current opacity for overlay. */
   EVENT_CMD_OVERLAY_SET_ALPHA,
   /* Cycle to next overlay. */
   EVENT_CMD_OVERLAY_NEXT,
   /* Initializes audio filter. */
   EVENT_CMD_DSP_FILTER_INIT,
   /* Deinitializes audio filter. */
   EVENT_CMD_DSP_FILTER_DEINIT,
   /* Deinitializes GPU recoring. */
   EVENT_CMD_GPU_RECORD_DEINIT,
   /* Initializes recording system. */
   EVENT_CMD_RECORD_INIT,
   /* Deinitializes recording system. */
   EVENT_CMD_RECORD_DEINIT,
   /* Deinitializes core information. */
   EVENT_CMD_CORE_INFO_DEINIT,
   /* Initializes core information. */
   EVENT_CMD_CORE_INFO_INIT,
   /* Deinitializes core. */
   EVENT_CMD_CORE_DEINIT,
   /* Initializes core. */
   EVENT_CMD_CORE_INIT,
   /* Set audio blocking state. */
   EVENT_CMD_AUDIO_SET_BLOCKING_STATE,
   /* Set audio nonblocking state. */
   EVENT_CMD_AUDIO_SET_NONBLOCKING_STATE,
   /* Apply video state changes. */
   EVENT_CMD_VIDEO_APPLY_STATE_CHANGES,
   /* Set video blocking state. */
   EVENT_CMD_VIDEO_SET_BLOCKING_STATE,
   /* Set video nonblocking state. */
   EVENT_CMD_VIDEO_SET_NONBLOCKING_STATE,
   /* Sets current aspect ratio index. */
   EVENT_CMD_VIDEO_SET_ASPECT_RATIO,
   EVENT_CMD_RESET_CONTEXT,
   /* Restarts RetroArch. */
   EVENT_CMD_RESTART_RETROARCH,
   /* Force-quit RetroArch. */
   EVENT_CMD_QUIT_RETROARCH,
   /* Resume RetroArch when in menu. */
   EVENT_CMD_RESUME,
   /* Toggles pause. */
   EVENT_CMD_PAUSE_TOGGLE,
   /* Pauses RetroArch. */
   EVENT_CMD_UNPAUSE,
   /* Unpauses retroArch. */
   EVENT_CMD_PAUSE,
   EVENT_CMD_PAUSE_CHECKS,
   EVENT_CMD_MENU_PAUSE_LIBRETRO,
   /* Toggles menu on/off. */
   EVENT_CMD_MENU_TOGGLE,
   EVENT_CMD_MENU_ENTRIES_REFRESH,
   /* Applies shader changes. */
   EVENT_CMD_SHADERS_APPLY_CHANGES,
   /* Initializes shader directory. */
   EVENT_CMD_SHADER_DIR_INIT,
   /* Deinitializes shader directory. */
   EVENT_CMD_SHADER_DIR_DEINIT,
   /* Initializes controllers. */
   EVENT_CMD_CONTROLLERS_INIT,
   EVENT_CMD_SAVEFILES,
   /* Initializes savefiles. */
   EVENT_CMD_SAVEFILES_INIT,
   /* Deinitializes savefiles. */
   EVENT_CMD_SAVEFILES_DEINIT,
   /* Initializes message queue. */
   EVENT_CMD_MSG_QUEUE_INIT,
   /* Deinitializes message queue. */
   EVENT_CMD_MSG_QUEUE_DEINIT,
   /* Initializes cheats. */
   EVENT_CMD_CHEATS_INIT,
   /* Deinitializes cheats. */
   EVENT_CMD_CHEATS_DEINIT,
   /* Deinitializes network system. */
   EVENT_CMD_NETWORK_DEINIT,
   /* Initializes network system. */
   EVENT_CMD_NETWORK_INIT,
   /* Initializes netplay system. */
   EVENT_CMD_NETPLAY_INIT,
   /* Deinitializes netplay system. */
   EVENT_CMD_NETPLAY_DEINIT,
   EVENT_CMD_NETPLAY_TOGGLE,
   /* Flip netplay players. */
   EVENT_CMD_NETPLAY_FLIP_PLAYERS,
   /* Deinits/Reinits Preemptive Frames as needed. */
   EVENT_CMD_PREEMPT_UPDATE,
   /* Force Preemptive Frames to refill its state buffer. */
   EVENT_CMD_PREEMPT_RESET_BUFFER,
   /* Initializes command interface. */
   EVENT_CMD_COMMAND_INIT,
   /* Deinitialize command interface. */
   EVENT_CMD_COMMAND_DEINIT,
   /* Deinitializes drivers. */
   EVENT_CMD_DRIVERS_DEINIT,
   /* Initializes drivers. */
   EVENT_CMD_DRIVERS_INIT,
   /* Reinitializes audio driver. */
   EVENT_CMD_AUDIO_REINIT,
   /* Resizes windowed scale. Will reinitialize video driver. */
   EVENT_CMD_RESIZE_WINDOWED_SCALE,
   /* Deinitializes temporary content. */
   EVENT_CMD_TEMPORARY_CONTENT_DEINIT,
   EVENT_CMD_SUBSYSTEM_FULLPATHS_DEINIT,
   EVENT_CMD_LOG_FILE_DEINIT,
   /* Toggles disk eject. */
   EVENT_CMD_DISK_EJECT_TOGGLE,
   /* Cycle to next disk. */
   EVENT_CMD_DISK_NEXT,
   /* Cycle to previous disk. */
   EVENT_CMD_DISK_PREV,
   /* Stops rumbling. */
   EVENT_CMD_RUMBLE_STOP,
   /* Toggles mouse grab. */
   EVENT_CMD_GRAB_MOUSE_TOGGLE,
   /* Toggles fullscreen mode. */
   EVENT_CMD_FULLSCREEN_TOGGLE,
   EVENT_CMD_PERFCNT_REPORT_FRONTEND_LOG,
   EVENT_CMD_ADVANCED_SETTINGS_TOGGLE,
   EVENT_CMD_KEYBOARD_FOCUS_TOGGLE,
   EVENT_CMD_INPUT_UPDATE_ANALOG_DPAD_PARAMS,
   EVENT_CMD_DATA_RUNLOOP_FREE,
};

typedef struct event_cmd_state
{
   bool fullscreen_toggle;
   bool overlay_next_pressed;
   bool grab_mouse_pressed;
   bool menu_pressed;
   bool quit_key_pressed;
   bool screenshot_pressed;
   bool mute_pressed;
   bool osk_pressed;
   bool advanced_toggle_pressed;
   bool reset_pressed;
   bool disk_prev_pressed;
   bool disk_next_pressed;
   bool disk_eject_pressed;
   bool save_state_pressed;
   bool load_state_pressed;
   bool slowmotion_pressed;
   bool shader_next_pressed;
   bool shader_prev_pressed;
   bool fastforward_pressed;
   bool hold_pressed;
   bool old_hold_pressed;
   bool state_slot_increase;
   bool state_slot_decrease;
   bool pause_pressed;
   bool frameadvance_pressed;
   bool rewind_pressed;
   bool netplay_flip_pressed;
   bool cheat_index_plus_pressed;
   bool cheat_index_minus_pressed;
   bool cheat_toggle_pressed;
   bool kbd_focus_toggle_pressed;
} event_cmd_state_t;

/**
 * event_disk_control_append_image:
 * @path                 : Path to disk image. 
 *
 * Appends disk image to disk image list.
 **/
void event_disk_control_append_image(const char *path);

/**
 * event_command:
 * @cmd                  : Command index.
 *
 * Performs RetroArch command with index @cmd.
 *
 * Returns: true (1) on success, otherwise false (0).
 **/
bool event_command(enum event_command action);

#ifdef __cplusplus
}
#endif

#endif
