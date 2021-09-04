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
extern unsigned core_options_scope;

typedef struct core_option_manager core_option_manager_t;

/**
 * core_options_init:
 * @data            : Pointer to option definition array handle (v1 or v2) or variable array handle (legacy)
 * @version         : Version of option definition type. 0 for legacy.
 *
 * Creates and initializes a core manager handle.
 *
 **/
void core_options_init(void *data, unsigned version);

/**
 * core_option_set_visible:
 * @opt                   : options manager handle
 * @key                   : String id of core_option
 * @visible               : False if option should be hidden in menu
 */
void core_option_set_visible(core_option_manager_t *opt,
                             const char* key, bool visible);

/**
 * core_option_update_category_visibilities:
 * @opt_mgr                                : options manager handle
 *
 * Sets category menu visibilities based on visible options.
 */
void core_option_update_category_visibilities(core_option_manager_t *opt_mgr);

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
 * Returns: Total number of options and categories.
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

const char *core_option_key(core_option_manager_t *opt_mgr, size_t idx);

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
 * core_option_is_category:
 * @opt_mgr               : options manager handle
 * @idx                   : option index or menu entry type
 *
 * Returns: True if @idx indexes a category
 */
bool core_option_is_category(core_option_manager_t *opt_mgr, size_t idx);

/**
 * core_option_set_category:
 * @opt_mgr                : options manager handle
 * @cat_key                : category key to set as current
 * @cat_desc               : category description (menu title) to set as current
 *
 * Sets current category and description to @cat_key and @cat_desc
 */
void core_option_set_category(core_option_manager_t *opt_mgr,
                              const char *cat_key, const char *cat_desc);

/**
 * core_option_category_desc:
 * @opt_mgr                 : options manager handle
 *
 * Returns: Pointer to current category description
 */
const char* core_option_category_desc(core_option_manager_t *opt);

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
 * core_option_get_conf_path:
 * @path                    : pointer to PATH_MAX_LENGTH buffer
 * @scope                   : THIS_CORE, THIS_CONTENT_DIR, or THIS_CONTENT_ONLY
 *
 * Sets @path to options file path for the @scope given
 */
void core_option_get_conf_path(char *path, enum setting_scope scope);

/**
 * core_option_set_update_cb:
 * @opt_mgr                 : pointer to core option manager object.
 * @cb                      : callback function
 *
 * Sets callback to force core to update displayed options.
 **/
void core_option_set_update_cb(core_option_manager_t *opt_mgr,
      retro_core_options_update_display_callback_t cb);

#ifdef __cplusplus
}
#endif

#endif

