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

#include "core_options.h"
#include "general.h"
#include <string.h>
#include <file/config_file.h>
#include <file/dir_list.h>
#include "dir_list_special.h"
#include <compat/posix_string.h>
#include <compat/strl.h>
#include <retro_miscellaneous.h>
#include <string/stdstring.h>
#include "menu/menu.h"
#include "menu/menu_hash.h"

bool core_options_touched = false;
unsigned core_options_scope = THIS_CORE;

struct core_option
{
   char *desc;
   char *key;
   char *category_key;
   char *info;
   struct string_list *vals;
   struct string_list *labels;
   size_t index;
   size_t default_index;
   bool hide;
};

struct core_option_manager
{
   config_file_t *conf;
   char conf_path[PATH_MAX_LENGTH];

   struct core_option *opts; /* options followed by categories */
   unsigned *index_map;      /* maps menu index to @opts index */
   size_t num_opts;          /* @opts size without categories */
   size_t size;              /* @opts size including categories */

   const char *category_key;
   const char *category_desc;

   bool updated;
};

static retro_core_options_update_display_callback_t core_option_update_display_cb;

/**
 * core_option_free:
 * @opt_mgr        : options manager handle
 *
 * Frees core option manager handle.
 **/
void core_option_free(core_option_manager_t *opt_mgr)
{
   size_t i;

   if (!opt_mgr)
      return;

   for (i = 0; i < opt_mgr->size; i++)
   {
      free(opt_mgr->opts[i].desc);
      free(opt_mgr->opts[i].key);
      free(opt_mgr->opts[i].category_key);
      free(opt_mgr->opts[i].info);
      string_list_free(opt_mgr->opts[i].vals);
      string_list_free(opt_mgr->opts[i].labels);
   }

   config_file_free(opt_mgr->conf);
   free(opt_mgr->opts);
   free(opt_mgr->index_map);
   free(opt_mgr);

   core_option_update_display_cb = NULL;
}

void core_option_get(core_option_manager_t *opt_mgr, struct retro_variable *var)
{
   size_t i;

   opt_mgr->updated = false;

   for (i = 0; i < opt_mgr->num_opts; i++)
   {
      if (!strcmp(opt_mgr->opts[i].key, var->key))
      {
         var->value = core_option_val(opt_mgr, i);
         return;
      }
   }

   var->value = NULL;
}

static void core_option_add_info(struct core_option *option, const char *info)
{
   option->info = malloc(strlen(info)+1);
   menu_driver_wrap_text(option->info, info, 48);
}

/* From RA v1.8.5 core_option_manager_parse_option.
 * Parses @values for a v2 or v1 core option.
 */
static bool parse_option_vals(
      struct core_option *option, config_file_t *conf,
      const struct retro_core_option_value *values, const char *default_val)
{
   union string_list_elem_attr attr;
   size_t num_vals    = 0;
   char *config_val   = NULL;
   size_t i;

   /* Get number of values */
   for (;;)
   {
      if (string_is_empty(values[num_vals].value))
         break;
      num_vals++;
   }

   if (num_vals < 1)
      return false;

   /* Initialize string lists */
   attr.i             = 0;
   option->vals       = string_list_new();
   option->labels     = string_list_new();

   if (!option->vals || !option->labels)
      return false;

   /* Initialize default value */
   option->index         = 0;
   option->default_index = 0;

   /* Extract value/label pairs */
   for (i = 0; i < num_vals; i++)
   {
      /* We know that 'value' is valid */
      string_list_append(option->vals, values[i].value, attr);

      /* Value 'label' may be NULL */
      if (!string_is_empty(values[i].label))
         string_list_append(option->labels, values[i].label, attr);
      else
         string_list_append(option->labels, values[i].value, attr);

      /* Check whether this value is the default setting */
      if (!string_is_empty(default_val))
      {
         if (!strcmp(default_val, values[i].value))
         {
            option->index         = i;
            option->default_index = i;
         }
      }
   }

   /* Set current config value */
   if (config_get_string(conf, option->key, &config_val))
   {
      for (i = 0; i < option->vals->size; i++)
      {
         if (!strcmp(option->vals->elems[i].data, config_val))
         {
            option->index = i;
            break;
         }
      }

      free(config_val);
   }

   return true;
}

static bool parse_v2_option(
      core_option_manager_t *opt_mgr, size_t idx,
      const struct retro_core_option_v2_definition *option_def,
      const bool use_categories)
{
   struct core_option *option = (struct core_option*)&opt_mgr->opts[idx];
   const char *info;

   option->key             = strdup(option_def->key);

   if (use_categories && !string_is_empty(option_def->category_key))
      option->category_key = strdup(option_def->category_key);

   if (use_categories && !string_is_empty(option_def->desc_categorized))
      option->desc         = strdup(option_def->desc_categorized);
   else
      option->desc         = strdup(option_def->desc);

   if (use_categories && !string_is_empty(option_def->info_categorized))
      info                 = option_def->info_categorized;
   else
      info                 = option_def->info;

   if (!string_is_empty(info))
      core_option_add_info(option, info);

   return parse_option_vals(option, opt_mgr->conf,
         option_def->values, option_def->default_value);
}

static void parse_v2_category(
      core_option_manager_t *opt_mgr, size_t idx,
      const struct retro_core_option_v2_category *category_def)
{
   struct core_option *option = (struct core_option*)&opt_mgr->opts[idx];
   const char *info;

   option->key  = strdup(category_def->key);
   option->desc = strdup(category_def->desc);

   if (!string_is_empty(category_def->info))
      info      = category_def->info;
   else
      info      = NULL;

   if (!string_is_empty(info))
      core_option_add_info(option, info);
}

static bool parse_v1_option(
      core_option_manager_t *opt_mgr, size_t idx,
      const struct retro_core_option_definition *option_def)
{
   struct core_option *option = (struct core_option*)&opt_mgr->opts[idx];

   if (!string_is_empty(option_def->key))
      option->key  = strdup(option_def->key);

   if (!string_is_empty(option_def->desc))
      option->desc = strdup(option_def->desc);

   if (!string_is_empty(option->desc) && !string_is_empty(option_def->info))
      core_option_add_info(option, option_def->info);

   return parse_option_vals(option, opt_mgr->conf,
         option_def->values, option_def->default_value);
}

static bool parse_legacy_variable(core_option_manager_t *opt_mgr, size_t idx,
      const struct retro_variable *var)
{
   size_t i;
   const char *val_start      = NULL;
   char *value                = NULL;
   char *desc_end             = NULL;
   char *config_val           = NULL;
   struct core_option *option = (struct core_option*)&opt_mgr->opts[idx];

   if (!option)
      return false;

   option->key = strdup(var->key);
   value       = strdup(var->value);
   desc_end    = strstr(value, "; ");

   if (!desc_end)
   {
      free(value);
      return false;
   }

   *desc_end    = '\0';
   option->desc = strdup(value);

   val_start    = desc_end + 2;
   option->vals = string_split(val_start, "|");

   if (!option->vals)
   {
      free(value);
      return false;
   }

   option->default_index = 0;

   if (config_get_string(opt_mgr->conf, option->key, &config_val))
   {
      for (i = 0; i < option->vals->size; i++)
      {
         if (!strcmp(option->vals->elems[i].data, config_val))
         {
            option->index = i;
            break;
         }
      }

      free(config_val);
   }

   free(value);

   return true;
}

/**
 * core_option_v2_new:
 * @conf_path        : Filesystem path to write core option config file to.
 * @options_v2       : Pointer to core options v2 struct
 *
 * Creates and initializes a core manager handle.
 *
 * Returns: handle to new core manager handle, otherwise NULL.
 **/
static core_option_manager_t *core_option_v2_new(const char *conf_path,
       const struct retro_core_options_v2 *options_v2)
{
   settings_t *settings       = config_get_ptr();
   const bool use_categories  = settings->core.option_categories;

   const struct retro_core_option_v2_category *v2_cats
         = options_v2->categories;
   const struct retro_core_option_v2_category *v2_cat;

   const struct retro_core_option_v2_definition *v2_defs
         = options_v2->definitions;
   const struct retro_core_option_v2_definition *v2_def;

   core_option_manager_t *opt_mgr;
   size_t num_opts = 0;
   size_t num_cats = 0;
   size_t i;

   opt_mgr = (core_option_manager_t*) calloc(1, sizeof(*opt_mgr));
   if (!opt_mgr)
      return NULL;

   if (*conf_path)
      opt_mgr->conf = config_file_new(conf_path);
   if (!opt_mgr->conf)
      opt_mgr->conf = config_file_new(NULL);

   strlcpy(opt_mgr->conf_path, conf_path, sizeof(opt_mgr->conf_path));

   if (!opt_mgr->conf)
      goto error;

   for (v2_def = v2_defs;
        v2_def->key && v2_def->desc && v2_def->values[0].value;
        v2_def++)
      num_opts++;

   if (use_categories)
      for (v2_cat = v2_cats; v2_cat->key && v2_cat->desc; v2_cat++)
         num_cats++;

   opt_mgr->num_opts  = num_opts;
   opt_mgr->size      = num_opts + num_cats;

   opt_mgr->opts      = calloc(opt_mgr->size, sizeof(*opt_mgr->opts));
   opt_mgr->index_map = calloc(opt_mgr->size, sizeof(unsigned));

   if (!opt_mgr->opts || !opt_mgr->index_map)
      goto error;

   for (i = 0, v2_def = v2_defs; i < num_opts; v2_def++, i++)
      if (!parse_v2_option(opt_mgr, i, v2_def, use_categories))
         goto error;

   /* Append categories to end of option list */
   if (use_categories)
      for (i = 0, v2_cat = v2_cats; i < num_cats; v2_cat++, i++)
         parse_v2_category(opt_mgr, i + num_opts, v2_cat);

   return opt_mgr;

error:
   core_option_free(opt_mgr);
   return NULL;
}

/**
 * core_option_v1_new:
 * @conf_path        : Filesystem path to write core option config file to.
 * @v1_defs          : Pointer to option definition v1 array handle
 *
 * Creates and initializes a core manager handle.
 *
 * Returns: handle to new core manager handle, otherwise NULL.
 **/
static core_option_manager_t *core_option_v1_new(const char *conf_path,
      const struct retro_core_option_definition *v1_defs)
{
   const struct retro_core_option_definition *v1_def;

   core_option_manager_t *opt_mgr;
   size_t num_opts = 0;
   size_t i;

   opt_mgr = (core_option_manager_t*) calloc(1, sizeof(*opt_mgr));
   if (!opt_mgr)
      return NULL;

   if (*conf_path)
      opt_mgr->conf = config_file_new(conf_path);
   if (!opt_mgr->conf)
      opt_mgr->conf = config_file_new(NULL);

   strlcpy(opt_mgr->conf_path, conf_path, sizeof(opt_mgr->conf_path));

   if (!opt_mgr->conf)
      goto error;

   for (v1_def = v1_defs;
        v1_def->key && v1_def->desc && v1_def->values[0].value;
        v1_def++)
      num_opts++;

   opt_mgr->num_opts  = num_opts;
   opt_mgr->size      = num_opts;
   opt_mgr->opts      = calloc(num_opts, sizeof(*opt_mgr->opts));
   opt_mgr->index_map = calloc(opt_mgr->size, sizeof(unsigned));

   if (!opt_mgr->opts || !opt_mgr->index_map)
      goto error;

   for (v1_def = v1_defs, i = 0; i < num_opts; v1_def++, i++)
      if (!parse_v1_option(opt_mgr, i, v1_def))
         goto error;

   return opt_mgr;

error:
   core_option_free(opt_mgr);
   return NULL;
}

/**
 * core_option_legacy_new:
 * @conf_path            : Filesystem path to write core option config file to.
 * @vars                 : Pointer to variable array handle
 *
 * Creates and initializes a core manager handle.
 *
 * Returns: handle to new core manager handle, otherwise NULL.
 **/
static core_option_manager_t *core_option_legacy_new(const char *conf_path,
      const struct retro_variable *vars)
{
   const struct retro_variable *var;

   core_option_manager_t *opt_mgr;
   size_t num_opts = 0;
   size_t i;

   opt_mgr = (core_option_manager_t*) calloc(1, sizeof(*opt_mgr));
   if (!opt_mgr)
      return NULL;

   if (*conf_path)
      opt_mgr->conf = config_file_new(conf_path);
   if (!opt_mgr->conf)
      opt_mgr->conf = config_file_new(NULL);

   strlcpy(opt_mgr->conf_path, conf_path, sizeof(opt_mgr->conf_path));

   if (!opt_mgr->conf)
      goto error;

   for (var = vars; var->key && var->value; var++)
      num_opts++;

   opt_mgr->num_opts  = num_opts;
   opt_mgr->size      = num_opts;
   opt_mgr->opts      = calloc(opt_mgr->size, sizeof(*opt_mgr->opts));
   opt_mgr->index_map = calloc(opt_mgr->size, sizeof(unsigned));

   if (!opt_mgr->opts || !opt_mgr->index_map)
      goto error;

   for (var = vars, i = 0; i < num_opts; i++, var++)
      if (!parse_legacy_variable(opt_mgr, i, var))
         goto error;

   return opt_mgr;

error:
   core_option_free(opt_mgr);
   return NULL;
}

/**
 * core_options_init:
 * @data            : Pointer to option definition array handle (v1 or v2) or variable array handle (legacy)
 * @version         : Version of option definition type. 0 for legacy.
 *
 * Creates and initializes a core manager handle.
 *
 **/
void core_options_init(void *data, unsigned version)
{
   global_t *global = global_get_ptr();
   char path[PATH_MAX_LENGTH];

   if (global->system.core_options)
   {
      core_option_free(global->system.core_options);
      global->system.core_options = NULL;
   }

   core_option_get_conf_path(path, THIS_CONTENT_ONLY);
   if (path_file_exists(path))
   {
      core_options_scope = THIS_CONTENT_ONLY;
      goto finish;
   }

   core_option_get_conf_path(path, THIS_CONTENT_DIR);
   if (path_file_exists(path))
   {
      core_options_scope = THIS_CONTENT_DIR;
      goto finish;
   }

   core_option_get_conf_path(path, THIS_CORE);
   core_options_scope = THIS_CORE;

finish:
   switch(version)
   {
      case 2:
         global->system.core_options = core_option_v2_new(path, data);
         break;

      case 1:
         global->system.core_options = core_option_v1_new(path, data);
         break;

      case 0:
         global->system.core_options = core_option_legacy_new(path, data);
         break;

      default:
         return;
   }

   path_basedir(path);
   path_mkdir(path);

   core_options_touched = false;
}

/**
 * core_option_set_visible:
 * @opt_mgr               : options manager handle
 * @key                   : String id of core_option
 * @visible               : False if option should be hidden in menu
 */
void core_option_set_visible(core_option_manager_t *opt_mgr,
                             const char* key, bool visible)
{
   unsigned i;

   if (!opt_mgr || !key)
      return;

   for (i = 0; i < opt_mgr->num_opts; i++)
   {
      if (!strcmp(opt_mgr->opts[i].key, key))
      {
         opt_mgr->opts[i].hide = !visible;
         menu_entries_set_refresh();
         return;
      }
   }
}

/**
 * core_option_update_category_visibilities:
 * @opt_mgr                                : options manager handle
 *
 * Sets category menu visibilities based on visible options.
 */
void core_option_update_category_visibilities(core_option_manager_t *opt_mgr)
{
   struct core_option *cat, *opt;
   unsigned i, j;

   if (!opt_mgr)
      return;

   /* Hide each category until an option is found using it.
    * Categories start at @opt_mgr->num_opts. */
   for (i = opt_mgr->num_opts; i < opt_mgr->size; i++)
   {
      cat = &opt_mgr->opts[i];
      cat->hide = true;

      for (j = 0; j < opt_mgr->num_opts; j++)
      {
         opt = &opt_mgr->opts[j];

         if (opt->hide == true || opt->category_key == NULL)
            continue;

         if (!strcmp(opt->category_key, cat->key))
         {
            cat->hide = false;
            break;
         }
      }
   }
}

/**
 * core_option_index:
 * @opt_mgr         : options manager handle
 * @type            : option index or menu entry type
 *
 * Returns: Index of core option
 */
static INLINE unsigned core_option_index(core_option_manager_t *opt_mgr,
                                         const unsigned type)
{
   if (type >= MENU_SETTINGS_CORE_OPTION_START)
      return opt_mgr->index_map[type - MENU_SETTINGS_CORE_OPTION_START];
   return type;
}

/**
 * core_option_updated:
 * @opt_mgr           : options manager handle
 *
 * Has a core option been updated since the last
 * call to RETRO_ENVIRONMENT_GET_VARIABLE?
 *
 * Returns: true (1) if a core option has been updated,
 * otherwise false (0).
 **/
bool core_option_updated(core_option_manager_t *opt_mgr)
{
   if (!opt_mgr)
      return false;
   return opt_mgr->updated;
}

static void core_options_delete_unscoped(void)
{
   char path[PATH_MAX_LENGTH];

   if (core_options_scope < THIS_CONTENT_ONLY)
   {
      core_option_get_conf_path(path, THIS_CONTENT_ONLY);
      remove(path);
   }

   if (core_options_scope < THIS_CONTENT_DIR)
   {
      core_option_get_conf_path(path, THIS_CONTENT_DIR);
      remove(path);
   }
}

/**
 * core_option_flush:
 * @opt_mgr         : options manager handle
 *
 * Writes core option key-pair values to file. Also deletes option
 * files as necessary if core_options_scope was changed.
 *
 * Returns: true (1) if core option values could be
 * successfully saved to disk, otherwise false (0).
 **/
bool core_option_flush(core_option_manager_t *opt_mgr)
{
   bool ret;
   size_t i;

   if (!opt_mgr)
      return false;

   /* Match conf to current scope. */
   core_option_get_conf_path(opt_mgr->conf_path, core_options_scope);
   core_options_conf_reload(opt_mgr);

   for (i = 0; i < opt_mgr->num_opts; i++)
   {
      struct core_option *option = (struct core_option*)&opt_mgr->opts[i];

      if (option)
         config_set_string(
               opt_mgr->conf, option->key, core_option_val(opt_mgr, i));
   }

   /* Write file, including unused options if scope includes other content */
   opt_mgr->conf->write_unused_entries
         = (core_options_scope < THIS_CONTENT_ONLY);
   ret = config_file_write(opt_mgr->conf, opt_mgr->conf_path);

   if (ret)
   {
      core_options_delete_unscoped();
      core_options_touched = false;
   }

   return ret;
}

/**
 * core_option_size:
 * @opt_mgr        : options manager handle
 *
 * Returns: Total number of options and categories.
 **/
size_t core_option_size(core_option_manager_t *opt_mgr)
{
   if (!opt_mgr)
      return 0;
   return opt_mgr->size;
}

/**
 * core_option_set_menu_offset:
 * @opt_mgr                   : options manager handle
 * @idx                       : index of option in @opt_mgr
 * @menu_offset               : offset from first option in menu display list
 *
 * Maps @idx to a menu entry. Required to index core option by menu entry type.
 */
void core_option_set_menu_offset(core_option_manager_t *opt_mgr,
                                 unsigned idx, unsigned menu_offset)
{
   if (!opt_mgr || idx >= opt_mgr->size || menu_offset >= opt_mgr->size)
      return;
   opt_mgr->index_map[menu_offset] = idx;
}

/**
 * core_option_desc:
 * @opt_mgr        : options manager handle
 * @idx            : option index or menu entry type
 *
 * Gets description for an option.
 *
 * Returns: Description for an option.
 **/
const char *core_option_desc(core_option_manager_t *opt_mgr, size_t idx)
{
   if (!opt_mgr)
      return NULL;

   idx = core_option_index(opt_mgr, idx);
   return opt_mgr->opts[idx].desc;
}

const char *core_option_key(core_option_manager_t *opt_mgr, size_t idx)
{
   if (!opt_mgr)
      return NULL;

   idx = core_option_index(opt_mgr, idx);
   return opt_mgr->opts[idx].key;
}

/**
 * core_option_val:
 * @opt_mgr       : options manager handle
 * @idx           : option index or menu entry type
 *
 * Gets value for an option.
 *
 * Returns: Value for an option.
 **/
const char *core_option_val(core_option_manager_t *opt_mgr, size_t idx)
{
   struct core_option *option;
   if (!opt_mgr)
      return NULL;

   idx = core_option_index(opt_mgr, idx);
   option = (struct core_option*)&opt_mgr->opts[idx];

   if (!option)
      return NULL;
   return option->vals->elems[option->index].data;
}

/**
 * core_option_label:
 * @opt_mgr         : options manager handle
 * @idx             : option index or menu entry type
 *
 * Gets label for an option value.
 *
 * Returns: Label for an option value.
 **/
const char *core_option_label(core_option_manager_t *opt_mgr, size_t idx)
{
   struct core_option *option;

   if (!opt_mgr)
      return "";

   idx = core_option_index(opt_mgr, idx);
   option = (struct core_option*)&opt_mgr->opts[idx];

   if (!option)
      return NULL;
   if (option->labels)
      return option->labels->elems[option->index].data;
   else
      return option->vals->elems[option->index].data;
}

/**
 * core_option_is_hidden:
 * @opt_mgr             : options manager handle
 * @idx                 : index identifier of the option
 *
 * Returns: True if option should be hidden in menu, false if not
 */
bool core_option_is_hidden(core_option_manager_t *opt_mgr, size_t idx)
{
   struct core_option *option;

   if (!opt_mgr || idx >= opt_mgr->size)
      return true;

   option = &opt_mgr->opts[idx];

   /* Explicit */
   if (option->hide)
      return true;

   /* Hidden by category */
   if (opt_mgr->category_key != NULL && option->category_key != NULL)
      return (strcmp(opt_mgr->category_key, option->category_key) != 0);
   else
      return (opt_mgr->category_key != NULL || option->category_key != NULL);
}

/**
 * core_option_is_category:
 * @opt_mgr               : options manager handle
 * @idx                   : option index or menu entry type
 *
 * Returns: True if @idx indexes a category
 */
bool core_option_is_category(core_option_manager_t *opt_mgr, size_t idx)
{
   if (!opt_mgr)
      return false;

   idx = core_option_index(opt_mgr, idx);
   return (idx >= opt_mgr->num_opts && idx < opt_mgr->size);
}

/**
 * core_option_set_category:
 * @opt_mgr                : options manager handle
 * @cat_key                : category key to set as current
 * @cat_desc               : category description (menu title) to set as current
 *
 * Sets current category and description to @cat_key and @cat_desc
 */
void core_option_set_category(core_option_manager_t *opt_mgr,
                              const char *cat_key, const char *cat_desc)
{
   if (!opt_mgr)
      return;

   opt_mgr->category_key  = cat_key;
   opt_mgr->category_desc = cat_desc;
}

/**
 * core_option_category_desc:
 * @opt_mgr                 : options manager handle
 *
 * Returns: Pointer to current category description
 */
const char* core_option_category_desc(core_option_manager_t *opt_mgr)
{
   if (!opt_mgr)
      return menu_hash_to_str(MENU_LABEL_CORE_OPTIONS);
   return opt_mgr->category_desc;
}

/**
 * core_option_get_info:
 * @opt_mgr            : options manager handle
 * @s                  : output message
 * @len                : size of @s
 * @idx                : option index or menu entry type
 *
 * Gets info message text describing an option.
 */
void core_option_get_info(core_option_manager_t *opt_mgr,
                          char *s, size_t len, size_t idx)
{
   char *info;
   if (!opt_mgr)
      return;

   idx = core_option_index(opt_mgr, idx);
   info = opt_mgr->opts[idx].info;

   if (!info || !*info)
      strlcpy(s, "-- No info on this item is available. --\n", len);
   else
      strlcpy(s, info, len);
}

void core_option_set_val(core_option_manager_t *opt_mgr,
      size_t idx, size_t val_idx)
{
   if (!opt_mgr)
      return;

   idx = core_option_index(opt_mgr, idx);
   opt_mgr->opts[idx].index = val_idx % opt_mgr->opts[idx].vals->size;

   if (core_option_update_display_cb)
      core_option_update_display_cb();

   opt_mgr->updated         = true;  /* need sync with core */
   core_options_touched     = true;  /* need flush to disk */
}

/* Returns the string_list index of the @option value matching @val */
static size_t core_option_val_index(struct core_option *option,
                                    const char *val)
{
   struct string_list *vals;
   size_t i;

   if (!option)
      return 0;

   vals = option->vals;

   for (i = 0; i < vals->size; i++)
   {
      if (strcasecmp(vals->elems[i].data, val) == 0)
         return i;
   }

   return option->index;
}

/**
 * core_option_update_vals_from_file:
 * @opt_mgr                         : pointer to core option manager object.
 * @path                            : options file path
 *
 * Updates option values from the contents of @path.
 * Does not add or remove @opt_mgr entries.
 */
void core_option_update_vals_from_file(core_option_manager_t *opt_mgr,
                                       const char *path)
{
   config_file_t *conf;
   char *conf_val;
   size_t i;

   if (!opt_mgr)
      return;

   conf = config_file_new(path);
   if (!conf)
      return;

   for (i = 0; i < opt_mgr->num_opts; i++)
   {
      struct core_option *option = &opt_mgr->opts[i];

      if (config_get_string(conf, option->key, &conf_val))
      {
         option->index = core_option_val_index(option, conf_val);
         free(conf_val);
      }
   }

   if (core_option_update_display_cb)
      core_option_update_display_cb();

   opt_mgr->updated     = true;  /* need sync with core */
   core_options_touched = true;  /* need flush to disk */
   config_file_free(conf);
}

/**
 * core_option_next:
 * @opt_mgr        : pointer to core option manager object.
 * @idx            : option index or menu entry type
 *
 * Get next value for core option specified by @idx.
 * Options wrap around.
 **/
void core_option_next(core_option_manager_t *opt_mgr, size_t idx)
{
   struct core_option *option;
   if (!opt_mgr)
      return;

   idx = core_option_index(opt_mgr, idx);
   option = (struct core_option*)&opt_mgr->opts[idx];

   option->index = (option->index + 1) % option->vals->size;

   if (core_option_update_display_cb)
      core_option_update_display_cb();

   opt_mgr->updated     = true;  /* need sync with core */
   core_options_touched = true;  /* need flush to disk */
}

/**
 * core_option_prev:
 * @opt_mgr        : pointer to core option manager object.
 * @idx            : option index or menu entry type
 * Options wrap around.
 *
 * Get previous value for core option specified by @idx.
 * Options wrap around.
 **/
void core_option_prev(core_option_manager_t *opt_mgr, size_t idx)
{
   struct core_option *option;
   if (!opt_mgr)
      return;

   idx = core_option_index(opt_mgr, idx);
   option = (struct core_option*)&opt_mgr->opts[idx];

   option->index = (option->index + option->vals->size - 1) %
         option->vals->size;

   if (core_option_update_display_cb)
      core_option_update_display_cb();

   opt_mgr->updated     = true;  /* need sync with core */
   core_options_touched = true;  /* need flush to disk */
}

/**
 * core_option_first:
 * @opt_mgr         : pointer to core option manager object.
 * @idx             : option index or menu entry type
 *
 * Get value at index 0 for core option specified by @idx.
 **/
void core_option_first(core_option_manager_t *opt_mgr, size_t idx)
{
   struct core_option *option;
   if (!opt_mgr)
      return;

   idx = core_option_index(opt_mgr, idx);
   option = (struct core_option*)&opt_mgr->opts[idx];

   option->index = 0;

   if (core_option_update_display_cb)
      core_option_update_display_cb();

   opt_mgr->updated     = true;  /* need sync with core */
   core_options_touched = true;  /* need flush to disk */
}

/**
 * core_option_last:
 * @opt_mgr        : pointer to core option manager object.
 * @idx            : option index or menu entry type
 *
 * Get value at the last index for the core option specified by @idx.
 **/
void core_option_last(core_option_manager_t *opt_mgr, size_t idx)
{
   struct core_option *option;
   if (!opt_mgr)
      return;

   idx = core_option_index(opt_mgr, idx);
   option = (struct core_option*)&opt_mgr->opts[idx];

   option->index = option->vals->size - 1;

   if (core_option_update_display_cb)
      core_option_update_display_cb();

   opt_mgr->updated     = true;  /* need sync with core */
   core_options_touched = true;  /* need flush to disk */
}

/**
 * core_option_set_default:
 * @opt_mgr               : pointer to core option manager object.
 * @idx                   : option index or menu entry type
 *
 * Reset core option specified by @idx and sets default value for option.
 **/
void core_option_set_default(core_option_manager_t *opt_mgr, size_t idx)
{
   if (!opt_mgr)
      return;

   idx                      = core_option_index(opt_mgr, idx);
   opt_mgr->opts[idx].index = opt_mgr->opts[idx].default_index;

   if (core_option_update_display_cb)
      core_option_update_display_cb();

   opt_mgr->updated         = true;  /* need sync with core */
   core_options_touched     = true;  /* need flush to disk */
}

/**
 * core_options_set_defaults:
 * @opt_mgr                 : pointer to core option manager object.
 *
 * Resets all core options to their default values.
 **/
void core_options_set_defaults(core_option_manager_t *opt_mgr)
{
   size_t i;

   if (!opt_mgr)
      return;

   for (i = 0; i < opt_mgr->num_opts; i++)
      opt_mgr->opts[i].index = opt_mgr->opts[i].default_index;

   if (core_option_update_display_cb)
      core_option_update_display_cb();

   opt_mgr->updated     = true;  /* need sync with core */
   core_options_touched = true;  /* need flush to disk */
}

/**
 * core_options_conf_reload:
 * @opt_mgr                : pointer to core option manager object.
 *
 * Reloads @opt_mgr->conf from @opt_mgr->conf_path so that its entries will be
 * saved on flush. Does not change in-use option values.
 */
void core_options_conf_reload(core_option_manager_t *opt_mgr)
{
   if (!opt_mgr)
      return;

   config_file_free(opt_mgr->conf);
   if (path_file_exists(opt_mgr->conf_path))
      opt_mgr->conf = config_file_new(opt_mgr->conf_path);
   else
      opt_mgr->conf = config_file_new(NULL);
}

/**
 * core_option_get_conf_path:
 * @path                    : pointer to PATH_MAX_LENGTH string
 * @scope                   : THIS_CORE, THIS_CONTENT_DIR, or THIS_CONTENT_ONLY
 *
 * Sets @path to options file path for the @scope given
 */
void core_option_get_conf_path(char *path, enum setting_scope scope)
{
   settings_t *settings  = config_get_ptr();
   global_t   *global    = global_get_ptr();
   char filename[NAME_MAX_LENGTH];

   if (!get_scoped_config_filename(filename, scope, "opt"))
   {
      *path = '\0';
      return;
   }

   fill_pathname_join(path, settings->menu_config_directory,
         global->libretro_name, PATH_MAX_LENGTH);
   fill_pathname_slash(path, PATH_MAX_LENGTH);
   strlcat(path, filename, PATH_MAX_LENGTH);
}

/**
 * core_option_set_update_cb:
 * @cb                      : callback function
 *
 * Sets callback to force core to update displayed options.
 **/
void core_option_set_update_cb(
      retro_core_options_update_display_callback_t cb)
{
   core_option_update_display_cb = cb;
}
