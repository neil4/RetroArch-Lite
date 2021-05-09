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

#ifndef CORE_OPTIONS_H__
#define CORE_OPTIONS_H__

#include <boolean.h>
#include "libretro.h"
#include "configuration.h"
#include <stddef.h>
#include <string/string_list.h>

#ifdef __cplusplus
extern "C" {
#endif
   
extern bool options_touched;
extern bool have_core_opt_file;
extern bool have_game_opt_file;

typedef struct core_option_manager core_option_manager_t;

/**
 * core_options_init:
 * @option_defs     : Pointer to option definition array handle (version 1)
 * @vars            : Pointer to variable array handle (legacy)
 *
 * Creates and initializes a core manager handle. @vars is only used if
 * @option_defs is NULL.
 *
 **/
void core_options_init(const struct retro_core_option_definition *option_defs,
      const struct retro_variable *vars);

/**
 * core_option_set_visible:
 * @opt                   : options manager handle
 * @key                   : String id of core_option
 * @visible               : False if option should be hidden in menu
 */
void core_option_set_visible(core_option_manager_t *opt,
                             const char* key, bool visible);

/**
 * core_option_updated:
 * @opt              : options manager handle
 *
 * Has a core option been updated?
 *
 * Returns: true (1) if a core option has been updated,
 * otherwise false (0).
 **/
bool core_option_updated(core_option_manager_t *opt);

/**
 * core_option_flush:
 * @opt              : options manager handle
 *
 * Writes core option key-pair values to file.
 *
 * Returns: true (1) if core option values could be
 * successfully saved to disk, otherwise false (0).
 **/
bool core_option_flush(core_option_manager_t *opt);

/**
 * core_option_free:
 * @opt              : options manager handle
 *
 * Frees core option manager handle.
 **/
void core_option_free(core_option_manager_t *opt);

void core_option_get(core_option_manager_t *opt, struct retro_variable *var);

/**
 * core_option_size:
 * @opt              : options manager handle
 *
 * Gets total number of options.
 *
 * Returns: Total number of options.
 **/
size_t core_option_size(core_option_manager_t *opt);

/**
 * core_option_set_menu_offset:
 * @opt                       : options manager handle
 * @idx                       : index of option in @opt
 * @menu_offset               : offset from first option in menu display list
 *
 * Maps @idx to a menu entry. Required to index core option by menu entry type.
 */
void core_option_set_menu_offset(core_option_manager_t *opt,
                                 unsigned idx, unsigned menu_offset);

/**
 * core_option_get_desc:
 * @opt                : options manager handle
 * @idx                : option index or menu entry type
 *
 * Gets description for an option.
 *
 * Returns: Description for an option.
 **/
const char *core_option_get_desc(core_option_manager_t *opt, size_t idx);

/**
 * core_option_get_val:
 * @opt               : options manager handle
 * @idx               : option index or menu entry type
 *
 * Gets value for an option.
 *
 * Returns: Value for an option.
 **/
const char *core_option_get_val(core_option_manager_t *opt, size_t idx);

/**
 * core_option_get_label:
 * @opt              : options manager handle
 * @idx              : option index or menu entry type
 *
 * Gets label for an option value.
 *
 * Returns: Label for an option value.
 **/
const char *core_option_get_label(core_option_manager_t *opt, size_t idx);

/**
 * core_option_is_hidden:
 * @opt                 : options manager handle
 * @idx                 : index identifier of the option
 *
 * Returns: True if option should be hidden in menu, false if not
 */
bool core_option_is_hidden(core_option_manager_t *opt, size_t idx);

/**
 * core_option_get_info:
 * @opt                : options manager handle
 * @s                  : output message
 * @len                : size of @s
 * @idx                : option index or menu entry type
 *
 * Gets info message text describing an option.
 */
void core_option_get_info(core_option_manager_t *opt,
                          char *s, size_t len, size_t idx);

/**
 * core_option_get_vals:
 * @opt                : pointer to core option manager object.
 * @idx                : option index or menu entry type
 *
 * Gets list of core option values from core option specified by @idx.
 *
 * Returns: string list of core option values if successful, otherwise
 * NULL.
 **/
struct string_list *core_option_get_vals(
      core_option_manager_t *opt, size_t idx);

void core_option_set_val(core_option_manager_t *opt,
      size_t idx, size_t val_idx);

/**
 * core_option_update_vals_from_file:
 * @opt                             : pointer to core option manager object.
 * @path                            : .opt file path
 *
 * Updates @opt values from the contents of @path.
 * Does not add or remove @opt entries.
 */
void core_option_update_vals_from_file(core_option_manager_t *opt,
                                       const char *path);

/**
 * core_option_next:
 * @opt            : pointer to core option manager object.
 * @idx            : option index or menu entry type
 *
 * Get next value for core option specified by @idx.
 * Options wrap around.
 **/
void core_option_next(core_option_manager_t *opt, size_t idx);

/**
 * core_option_prev:
 * @opt                   : pointer to core option manager object.
 * @idx                   : option index or menu entry type
 *
 * Get previous value for core option specified by @idx.
 * Options wrap around.
 **/
void core_option_prev(core_option_manager_t *opt, size_t idx);

/**
 * core_option_set_default:
 * @opt                   : pointer to core option manager object.
 * @idx                   : option index or menu entry type
 *
 * Reset core option specified by @idx and sets default value for option.
 **/
void core_option_set_default(core_option_manager_t *opt, size_t idx);

/**
 * core_options_set_defaults:
 * @opt                     : pointer to core option manager object.
 *
 * Resets all core options to their default values.
 **/
void core_options_set_defaults(core_option_manager_t *opt);

/**
 * core_options_conf_reload:
 * @opt                    : pointer to core option manager object.
 * 
 * Reloads @opt->conf from @opt->conf_path so that its entries will be saved
 * on flush. Does not change in-use option values.
 */
void core_options_conf_reload(core_option_manager_t *opt);

/**
 * core_option_conf_path:
 * @opt                 : pointer to core option manager object.
 *
 * Returns pointer to core options file path.
 */
char* core_option_conf_path(core_option_manager_t *opt);

/**
 * core_option_get_core_conf_path:
 * @param path                   : pointer to PATH_MAX_LENGTH length string
 * 
 * Sets @path to default core options file path
 */
void core_option_get_core_conf_path(char *path);

/**
 * core_option_get_game_conf_path:
 * @param path                   : pointer to PATH_MAX_LENGTH length string
 *
 * Sets @path to ROM options file path
 */
void core_option_get_game_conf_path(char *path);

#ifdef __cplusplus
}
#endif

#endif

