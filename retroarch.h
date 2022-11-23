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

#ifndef __RETROARCH_H
#define __RETROARCH_H

#include <boolean.h>
#include <retro_miscellaneous.h>

#include "core_info.h"
#include "command_event.h"

#ifdef __cplusplus
extern "C" {
#endif

#define RETRO_ENVIRONMENT_RETROARCH_START_BLOCK 0x800000

#define RETRO_ENVIRONMENT_GET_CLEAR_ALL_THREAD_WAITS_CB (3 | RETRO_ENVIRONMENT_RETROARCH_START_BLOCK)
                                           /* retro_environment_t * --
                                            * Provides the callback to the frontend method which will cancel
                                            * all currently waiting threads.  Used when coordination is needed
                                            * between the core and the frontend to gracefully stop all threads.
                                            */

enum action_state
{
   RARCH_ACTION_STATE_NONE = 0,
   RARCH_ACTION_STATE_LOAD_CONTENT,
   RARCH_ACTION_STATE_MENU_RUNNING,
   RARCH_ACTION_STATE_MENU_RUNNING_FINISHED,
   RARCH_ACTION_STATE_QUIT,
   RARCH_ACTION_STATE_FORCE_QUIT,
};

enum rarch_capabilities
{
   RARCH_CAPABILITIES_NONE = 0,
   RARCH_CAPABILITIES_CPU,
   RARCH_CAPABILITIES_COMPILER,
};

struct rarch_main_wrap
{
   const char *content_path;
   const char *sram_path;
   const char *state_path;
   const char *config_path;
   const char *libretro_path;
   bool verbose;
   bool no_content;

   bool touched;
};

void rarch_main_alloc(void);

void rarch_main_new(void);

void rarch_main_free(void);

void rarch_main_set_state(unsigned action);

/**
 * rarch_main_init:
 * @argc                 : Count of (commandline) arguments.
 * @argv                 : (Commandline) arguments. 
 *
 * Initializes RetroArch.
 *
 * Returns: 0 on success, otherwise 1 if there was an error.
 **/
int rarch_main_init(int argc, char *argv[]);

/**
 * rarch_main_init_wrap:
 * @args                 : Input arguments.
 * @argc                 : Count of arguments.
 * @argv                 : Arguments.
 *
 * Generates an @argc and @argv pair based on @args
 * of type rarch_main_wrap.
 **/
void rarch_main_init_wrap(const struct rarch_main_wrap *args,
      int *argc, char **argv);

/**
 * rarch_main_deinit:
 *
 * Deinitializes RetroArch.
 **/
void rarch_main_deinit(void);

/**
 * rarch_update_configs
 *
 * Called on load/unload core to save core options, remaps, and settings.
 * Global settings saved only if shutting down.
 */
void rarch_update_configs();

/**
 * rarch_defer_core:
 * @core_info            : Core info list handle.
 * @dir                  : Directory. Gets joined with @path.
 * @path                 : Path. Gets joined with @dir.
 * @menu_label           : Label identifier of menu setting.
 * @s                    : Deferred core path. Will be filled in
 *                         by function.
 * @len                  : Size of @s.
 *
 * Gets deferred core.
 *
 * Returns: 0 if there are multiple deferred cores and a 
 * selection needs to be made from a list, otherwise
 * returns -1 and fills in @s with path to core.
 **/
int rarch_defer_core(core_info_list_t *data,
      const char *dir, const char *path, const char *menu_label,
      char *s, size_t len);

void rarch_fill_pathnames(void);

/* 
 * rarch_verify_api_version:
 *
 * Compare libretro core API version against API version
 * used by RetroArch.
 *
 * TODO - when libretro v2 gets added, allow for switching
 * between libretro version backend dynamically.
 **/
void rarch_verify_api_version(void);

/**
 * rarch_init_system_av_info:
 *
 * Initialize system A/V information by calling the libretro core's
 * get_system_av_info function.
 **/
void rarch_init_system_av_info(void);

void rarch_set_paths(const char *path);

/**
 * set_paths_redirect:
 * 
 * Set default remap, save, and state directories.
 * Create core-specific savefile and savestate subdirectories.
 */
void set_paths_redirect();

int rarch_info_get_capabilities(enum rarch_capabilities type, char *s, size_t len);

bool rarch_clear_all_thread_waits(unsigned clear_threads, void* data);

#ifdef __cplusplus
}
#endif

#endif
