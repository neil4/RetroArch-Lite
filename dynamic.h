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

#ifndef __DYNAMIC_H
#define __DYNAMIC_H

#include <boolean.h>
#include <libretro.h>

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <dynamic/dylib.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * libretro_get_environment_info:
 * @func                         : Function pointer for get_environment_info.
 * @load_no_content              : If true, core should be able to auto-start
 *                                 without any content loaded.
 *
 * Sets environment callback in order to get statically known 
 * information from it.
 *
 * Fetched via environment callbacks instead of
 * retro_get_system_info(), as this info is part of extensions.
 *
 * Should only be called once right after core load to 
 * avoid overwriting the "real" environ callback.
 *
 * For statically linked cores, pass retro_set_environment as argument.
 */
void libretro_get_environment_info(void (*)(retro_environment_t),
      bool *load_no_content);

#ifdef HAVE_DYNAMIC
/**
 * libretro_get_system_info:
 * @path                         : Path to libretro library.
 * @info                         : System info information.
 * @load_no_content              : If true, core should be able to auto-start
 *                                 without any content loaded.
 *
 * Gets system info from an arbitrary lib.
 * The struct returned must be freed as strings are allocated dynamically.
 *
 * Returns: true (1) if successful, otherwise false (0).
 **/
bool libretro_get_system_info(const char *path,
      struct retro_system_info *info, bool *load_no_content);

/**
 * libretro_free_system_info:
 * @info                         : Pointer to system info information.
 *
 * Frees system information.
 **/
void libretro_free_system_info(struct retro_system_info *info);
#endif

/**
 * libretro_get_current_core_pathname:
 * @name                         : Sanitized name of libretro core.
 * @size                         : Size of @name
 *
 * Transforms a library id to a name suitable as a pathname.
 **/
void libretro_get_current_core_pathname(char *name, size_t size);

const struct retro_subsystem_info *libretro_find_subsystem_info(
      const struct retro_subsystem_info *info,
      unsigned num_info, const char *ident);

/**
 * libretro_find_controller_description:
 * @info                         : Pointer to controller info handle.
 * @id                           : Identifier of controller to search
 *                                 for.
 *
 * Search for a controller of type @id in @info.
 *
 * Returns: controller description of found controller on success,
 * otherwise NULL.
 **/
const struct retro_controller_description *
   libretro_find_controller_description(
         const struct retro_controller_info *info, unsigned id);

/**
 * rarch_environment_cb:
 * @cmd                          : Identifier of command.
 * @data                         : Pointer to data.
 *
 * Environment callback function implementation.
 *
 * Returns: true (1) if environment callback command could
 * be performed, otherwise false (0).
 **/
bool rarch_environment_cb(unsigned cmd, void *data);

extern void (*pretro_init)(void);

extern void (*pretro_deinit)(void);

extern unsigned (*pretro_api_version)(void);

extern void (*pretro_get_system_info)(struct retro_system_info*);

extern void (*pretro_get_system_av_info)(struct retro_system_av_info*);

extern void (*pretro_set_environment)(retro_environment_t);

extern void (*pretro_set_video_refresh)(retro_video_refresh_t);

extern void (*pretro_set_audio_sample)(retro_audio_sample_t);

extern void (*pretro_set_audio_sample_batch)(retro_audio_sample_batch_t);

extern void (*pretro_set_input_poll)(retro_input_poll_t);

extern void (*pretro_set_input_state)(retro_input_state_t);

extern void (*pretro_set_controller_port_device)(unsigned, unsigned);

extern void (*pretro_reset)(void); 

extern void (*pretro_run)(void);

extern size_t (*pretro_serialize_size)(void);

extern bool (*pretro_serialize)(void*, size_t);

extern bool (*pretro_unserialize)(const void*, size_t);

extern void (*pretro_cheat_reset)(void);

extern void (*pretro_cheat_set)(unsigned, bool, const char*);

extern bool (*pretro_load_game)(const struct retro_game_info*);

extern bool (*pretro_load_game_special)(unsigned,
      const struct retro_game_info*, size_t);

extern void (*pretro_unload_game)(void);

extern unsigned (*pretro_get_region)(void);

extern void *(*pretro_get_memory_data)(unsigned);

extern size_t (*pretro_get_memory_size)(unsigned);

/**
 * init_libretro_sym:
 * @dummy                        : Load dummy symbols if true
 *
 * Initializes libretro symbols and
 * setups environment callback functions.
 **/
void init_libretro_sym(bool dummy);

/**
 * uninit_libretro_sym:
 *
 * Frees libretro core.
 *
 * Frees all core options,
 * associated state, and
 * unbind all libretro callback symbols.
 **/
void uninit_libretro_sym(void);

bool libretro_get_shared_context(void);

void core_set_controller_port_device(unsigned port, unsigned device);

#ifdef __cplusplus
}
#endif

#endif

