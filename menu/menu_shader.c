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

#include <string.h>

#include <compat/strl.h>
#include <file/file_path.h>
#include <string/stdstring.h>

#include "menu.h"
#include "menu_hash.h"
#include "menu_shader.h"
#include "menu_setting.h"
#include "../configuration.h"
#include "../runloop.h"
/**
 * menu_shader_manager_init:
 *
 * Initializes shader manager.
 **/
void menu_shader_manager_init(menu_handle_t *menu)
{
#ifdef HAVE_SHADER_MANAGER
   uint32_t ext_hash;
   char preset_path[PATH_MAX_LENGTH];
   const char *ext             = NULL;
   struct video_shader *shader = NULL;
   config_file_t *conf         = NULL;
   settings_t *settings        = config_get_ptr();

   if (!menu)
      return;

   shader = (struct video_shader*)menu->shader;

   strlcpy(menu->default_glslp, "temporary.glslp", sizeof(menu->default_glslp));
   strlcpy(menu->default_cgp, "temporary.cgp", sizeof(menu->default_cgp));

   ext = path_get_extension(settings->video.shader_path);
   ext_hash = menu_hash_calculate(ext);

   switch (ext_hash)
   {
      case MENU_VALUE_GLSLP:
      case MENU_VALUE_CGP:
         conf = config_file_new(settings->video.shader_path);
         if (conf)
         {
            if (video_shader_read_conf(conf, shader))
            {
               video_shader_resolve_relative(shader, settings->video.shader_path);
               video_shader_resolve_parameters(conf, shader);
            }
            config_file_free(conf);
         }
         break;
      case MENU_VALUE_GLSL:
      case MENU_VALUE_CG:
         strlcpy(shader->pass[0].source.path, settings->video.shader_path,
               sizeof(shader->pass[0].source.path));
         shader->passes = 1;
         break;
      default:
         {
            const char *shader_dir = *settings->video.shader_dir ?
               settings->video.shader_dir : settings->system_directory;

            fill_pathname_join(preset_path, shader_dir, "temporary.glslp", sizeof(preset_path));
            conf = config_file_new(preset_path);

            if (!conf)
            {
               fill_pathname_join(preset_path, shader_dir, "temporary.cgp", sizeof(preset_path));
               conf = config_file_new(preset_path);
            }

            if (conf)
            {
               if (video_shader_read_conf(conf, shader))
               {
                  video_shader_resolve_relative(shader, preset_path);
                  video_shader_resolve_parameters(conf, shader);
               }
               config_file_free(conf);
            }
         }
         break;
   }
#endif
}

/**
 * menu_shader_manager_set_preset:
 * @shader                   : Shader handle.   
 * @type                     : Type of shader.
 * @preset_path              : Preset path to load from.
 *
 * Sets shader preset.
 **/
void menu_shader_manager_set_preset(struct video_shader *shader,
      unsigned type, const char *preset_path)
{
#ifdef HAVE_SHADER_MANAGER
   config_file_t *conf         = NULL;
   settings_t *settings        = config_get_ptr();

   if (!video_driver_set_shader((enum rarch_shader_type)type, preset_path))
      return;

   /* Makes sure that we use Menu Preset shader on driver reinit.
    * Only do this when the cgp actually works to avoid potential errors. */
   strlcpy(settings->video.shader_path, preset_path ? preset_path : "",
         sizeof(settings->video.shader_path));

   if (!preset_path)
      return;
   if (!shader)
      return;

   /* Load stored Preset into menu on success. 
    * Used when a preset is directly loaded.
    * No point in updating when the Preset was 
    * created from the menu itself. */
   conf = config_file_new(preset_path);

   if (!conf)
      return;

   RARCH_LOG("Setting Menu shader: %s.\n", preset_path ? preset_path : "N/A (stock)");

   if (video_shader_read_conf(conf, shader))
   {
      video_shader_resolve_relative(shader, preset_path);
      video_shader_resolve_parameters(conf, shader);
   }
   config_file_free(conf);

   menu_entries_set_refresh();

   event_command(EVENT_CMD_SHADER_DIR_INIT);
   scoped_settings_touched = true;
   settings_touched = true;
#endif
}

static void menu_shader_manager_update_preset_params(void)
{
   menu_handle_t *menu = menu_driver_get_ptr();
   struct video_shader *shader = video_shader_driver_get_current_shader();
   uint8_t i;

   if (!shader || !menu || !menu->shader)
      return;

   for (i = 0; i < shader->num_parameters; i++)
   {
      if (!strcmp(menu->shader->parameters[i].id, shader->parameters[i].id))
         menu->shader->parameters[i] = shader->parameters[i];
   }
}

/**
 * menu_shader_manager_save_preset:
 * @basename                 : basename of preset
 * @apply                    : immediately set preset after saving
 *
 * Save a shader preset to disk.
 **/
void menu_shader_manager_save_preset(
      const char *basename, bool apply)
{
#ifdef HAVE_SHADER_MANAGER
   char *buffer                = string_alloc(PATH_MAX_LENGTH);
   char *config_directory      = string_alloc(PATH_MAX_LENGTH);
   char *preset_path           = string_alloc(PATH_MAX_LENGTH);
   unsigned d, type            = RARCH_SHADER_NONE;
   config_file_t *conf         = NULL;
   bool ret                    = false;
   global_t *global            = global_get_ptr();
   settings_t *settings        = config_get_ptr();
   menu_handle_t *menu         = menu_driver_get_ptr();

   if (!menu)
   {
      RARCH_ERR("Cannot save shader preset, menu handle is not initialized.\n");
      goto end;
   }

   type = menu_shader_manager_get_type(menu->shader);

   if (type == RARCH_SHADER_NONE)
      goto end;

   menu_shader_manager_update_preset_params();

   *config_directory = '\0';

   if (basename)
   {
      strlcpy(buffer, basename, PATH_MAX_LENGTH);

      /* Append extension automatically as appropriate. */
      if (!strstr(basename, ".cgp") && !strstr(basename, ".glslp"))
      {
         switch (type)
         {
            case RARCH_SHADER_GLSL:
               strlcat(buffer, ".glslp", PATH_MAX_LENGTH);
               break;
            case RARCH_SHADER_CG:
               strlcat(buffer, ".cgp", PATH_MAX_LENGTH);
               break;
         }
      }
   }
   else
   {
      const char *conf_path = (type == RARCH_SHADER_GLSL) ?
         menu->default_glslp : menu->default_cgp;
      strlcpy(buffer, conf_path, PATH_MAX_LENGTH);
   }

   if (*global->config_path)
      fill_pathname_basedir(config_directory,
            global->config_path, PATH_MAX_LENGTH);

   const char *dirs[] = {
      settings->video.shader_dir,
      settings->menu_config_directory,
      config_directory,
   };

   if (!(conf = (config_file_t*)config_file_new(NULL)))
      return;
   video_shader_write_conf(conf, menu->shader);

   for (d = 0; d < ARRAY_SIZE(dirs); d++)
   {
      if (!*dirs[d])
         continue;

      fill_pathname_join(preset_path, dirs[d], buffer, PATH_MAX_LENGTH);
      if (config_file_write(conf, preset_path))
      {
         RARCH_LOG("Saved shader preset to %s.\n", preset_path);
         if (apply)
            menu_shader_manager_set_preset(NULL, type, preset_path);
         ret = true;
         break;
      }
      else
         RARCH_LOG("Failed writing shader preset to %s.\n", preset_path);
   }

   config_file_free(conf);
   if (!ret)
      RARCH_ERR("Failed to save shader preset. Make sure config directory and/or shader dir are writable.\n");

end:
   free(buffer);
   free(config_directory);
   free(preset_path);
#endif
}

/**
 * menu_shader_manager_get_type:
 * @shader                   : shader handle     
 *
 * Gets type of shader.
 *
 * Returns: type of shader. 
 **/
unsigned menu_shader_manager_get_type(const struct video_shader *shader)
{
#ifndef HAVE_SHADER_MANAGER
   return RARCH_SHADER_NONE;
#else
   /* All shader types must be the same, or we cannot use it. */
   unsigned i = 0, type = 0;

   if (!shader)
      return RARCH_SHADER_NONE;

   for (i = 0; i < shader->passes; i++)
   {
      enum rarch_shader_type pass_type = 
         video_shader_parse_type(shader->pass[i].source.path,
            RARCH_SHADER_NONE);

      switch (pass_type)
      {
         case RARCH_SHADER_CG:
         case RARCH_SHADER_GLSL:
            if (type == RARCH_SHADER_NONE)
               type = pass_type;
            else if (type != pass_type)
               return RARCH_SHADER_NONE;
            break;
         default:
            return RARCH_SHADER_NONE;
      }
   }

   return type;
#endif
}

/**
 * menu_shader_manager_apply_changes:
 *
 * Apply shader state changes.
 **/
void menu_shader_manager_apply_changes(void)
{
#ifdef HAVE_SHADER_MANAGER
   menu_handle_t *menu     = menu_driver_get_ptr();
   settings_t    *settings = config_get_ptr();
   char msg[64];

   unsigned shader_type = menu_shader_manager_get_type(menu->shader);

   if (menu->shader->passes 
         && shader_type != RARCH_SHADER_NONE)
   {
      if (settings->video.shader_path[0] != '\0')
      {
         menu_shader_manager_update_preset_params();
         menu_shader_manager_save_preset
            (path_basename(settings->video.shader_path), true);
      }
      else
         menu_shader_manager_save_preset(NULL, true);

      snprintf(msg, 64, "Saved %s",
               path_basename(settings->video.shader_path));
      rarch_main_msg_queue_push(msg, 2, 180, true);
      return;
   }

   /* Fall-back */
#if defined(HAVE_CG) || defined(HAVE_HLSL) || defined(HAVE_GLSL)
   shader_type = video_shader_parse_type("", DEFAULT_SHADER_TYPE);
#endif

   if (shader_type == RARCH_SHADER_NONE)
   {
#if defined(HAVE_GLSL)
      shader_type = RARCH_SHADER_GLSL;
#elif defined(HAVE_CG) || defined(HAVE_HLSL)
      shader_type = RARCH_SHADER_CG;
#endif
   }
   menu_shader_manager_set_preset(NULL, shader_type, NULL);
#endif
}

void menu_shader_free(menu_handle_t *menu)
{
#ifdef HAVE_SHADER_MANAGER
   if (menu->shader)
      free(menu->shader);
   menu->shader = NULL;
#endif
}
