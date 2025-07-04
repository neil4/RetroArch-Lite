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

#include "dynamic.h"
#include <compat/strl.h>
#include <compat/posix_string.h>
#include "retroarch_logger.h"
#include "performance.h"
#include "preempt.h"
#include <file/file_path.h>
#include <string.h>
#include <ctype.h>
#include <file/vfs_implementation.h>

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <boolean.h>
#include "libretro_private.h"
#include "dynamic_dummy.h"
#include "retroarch.h"
#include "runloop.h"
#include "configuration.h"
#include "general.h"
#include "netplay.h"

#include "input/input_sensor.h"

#ifdef NEED_DYNAMIC
#ifdef _WIN32
#include <windows.h>
#else
#include <dlfcn.h>
#endif
#endif

#ifdef HAVE_DYNAMIC
#undef SYM
#define SYM(x) do { \
   function_t func = dylib_proc(lib_handle, #x); \
   memcpy(&p##x, &func, sizeof(func)); \
   if (p##x == NULL) { RARCH_ERR("Failed to load symbol: \"%s\"\n", #x); rarch_fail(1, "init_libretro_sym()"); } \
} while (0)

static dylib_t lib_handle;
#else
#define SYM(x) p##x = x
#endif

#define SYM_DUMMY(x) p##x = libretro_dummy_##x

static bool core_set_shared_context = false;

void (*pretro_init)(void);
void (*pretro_deinit)(void);

unsigned (*pretro_api_version)(void);

void (*pretro_get_system_info)(struct retro_system_info*);
void (*pretro_get_system_av_info)(struct retro_system_av_info*);

void (*pretro_set_environment)(retro_environment_t);
void (*pretro_set_video_refresh)(retro_video_refresh_t);
void (*pretro_set_audio_sample)(retro_audio_sample_t);
void (*pretro_set_audio_sample_batch)(retro_audio_sample_batch_t);
void (*pretro_set_input_poll)(retro_input_poll_t);
void (*pretro_set_input_state)(retro_input_state_t);

void (*pretro_set_controller_port_device)(unsigned, unsigned);

void (*pretro_reset)(void);
void (*pretro_run)(void);

size_t (*pretro_serialize_size)(void);
bool (*pretro_serialize)(void*, size_t);
bool (*pretro_unserialize)(const void*, size_t);

void (*pretro_cheat_reset)(void);
void (*pretro_cheat_set)(unsigned, bool, const char*);

bool (*pretro_load_game)(const struct retro_game_info*);
bool (*pretro_load_game_special)(unsigned,
      const struct retro_game_info*, size_t);

void (*pretro_unload_game)(void);

unsigned (*pretro_get_region)(void);

void *(*pretro_get_memory_data)(unsigned);
size_t (*pretro_get_memory_size)(unsigned);

static bool ignore_environment_cb;

static void dummy_set_led(int led, int value) {}

#ifdef HAVE_DYNAMIC
static bool *load_no_content_hook;

static bool environ_cb_get_system_info(unsigned cmd, void *data)
{
   switch (cmd)
   {
      case RETRO_ENVIRONMENT_SET_SUPPORT_NO_GAME:
         *load_no_content_hook = *(const bool*)data;
         break;

      default:
         return false;
   }

   return true;
}

bool libretro_get_shared_context(void)
{
   return core_set_shared_context;
}

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
void libretro_get_environment_info(void (*func)(retro_environment_t),
      bool *load_no_content)
{
   load_no_content_hook = load_no_content;

   /* load_no_content gets set in this callback. */
   func(environ_cb_get_system_info);

   /* It's possible that we just set get_system_info callback to the currently running core.
    * Make sure we reset it to the actual environment callback.
    * Ignore any environment callbacks here in case we're running on the non-current core. */
   ignore_environment_cb = true;
   func(rarch_environment_cb);
   ignore_environment_cb = false;
}

static dylib_t libretro_get_system_info_lib(const char *path,
      struct retro_system_info *info, bool *load_no_content)
{
   dylib_t lib = dylib_load(path);
   if (!lib)
      return NULL;

   void (*proc)(struct retro_system_info*) =
      (void (*)(struct retro_system_info*))dylib_proc(lib,
            "retro_get_system_info");

   if (!proc)
   {
      dylib_close(lib);
      return NULL;
   }

   proc(info);

   if (load_no_content)
   {
      *load_no_content = false;
      void (*set_environ)(retro_environment_t) =
         (void (*)(retro_environment_t))dylib_proc(lib,
               "retro_set_environment");

      if (!set_environ)
         return lib;

      libretro_get_environment_info(set_environ, load_no_content);
   }

   return lib;
}

/**
 * libretro_get_system_info:
 * @path                         : Path to libretro library.
 * @info                         : Pointer to system info information.
 * @load_no_content              : If true, core should be able to auto-start
 *                                 without any content loaded.
 *
 * Gets system info from an arbitrary lib.
 * The struct returned must be freed as strings are allocated dynamically.
 *
 * Returns: true (1) if successful, otherwise false (0).
 **/
bool libretro_get_system_info(const char *path,
      struct retro_system_info *info, bool *load_no_content)
{
   struct retro_system_info dummy_info = {0};
   dylib_t lib = libretro_get_system_info_lib(path,
         &dummy_info, load_no_content);
   if (!lib)
      return false;

   memcpy(info, &dummy_info, sizeof(*info));
   if (dummy_info.library_name)
      info->library_name = strdup(dummy_info.library_name);
   if (dummy_info.library_version)
      info->library_version = strdup(dummy_info.library_version);
   if (dummy_info.valid_extensions)
      info->valid_extensions = strdup(dummy_info.valid_extensions);
   dylib_close(lib);
   return true;
}

/**
 * libretro_free_system_info:
 * @info                         : Pointer to system info information.
 *
 * Frees system information.
 **/
void libretro_free_system_info(struct retro_system_info *info)
{
   if (!info)
      return;

   free((void*)info->library_name);
   free((void*)info->library_version);
   free((void*)info->valid_extensions);
   memset(info, 0, sizeof(*info));
}
#endif

const struct retro_subsystem_info *libretro_find_subsystem_info(
      const struct retro_subsystem_info *info, unsigned num_info,
      const char *ident)
{
   unsigned i;
   for (i = 0; i < num_info; i++)
   {
      if (!strcmp(info[i].ident, ident))
         return &info[i];
      else if (!strcmp(info[i].desc, ident))
         return &info[i];
   }

   return NULL;
}

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
      const struct retro_controller_info *info, unsigned id)
{
   unsigned i;

   for (i = 0; i < info->num_types; i++)
   {
      if (info->types[i].id != id)
         continue;

      return &info->types[i];
   }

   return NULL;
}

void core_set_controller_port_device(unsigned port, unsigned device)
{
   pretro_set_controller_port_device(port,
         device == RETRO_DEVICE_KEYBOARD_DEFAULT
         ? RETRO_DEVICE_NONE : device);
}

/**
 * load_symbols:
 * @dummy                        : Load dummy symbols if true
 *
 * Setup libretro callback symbols.
 **/
static void load_symbols(bool is_dummy)
{
   if (is_dummy)
   {
      SYM_DUMMY(retro_init);
      SYM_DUMMY(retro_deinit);

      SYM_DUMMY(retro_api_version);
      SYM_DUMMY(retro_get_system_info);
      SYM_DUMMY(retro_get_system_av_info);

      SYM_DUMMY(retro_set_environment);
      SYM_DUMMY(retro_set_video_refresh);
      SYM_DUMMY(retro_set_audio_sample);
      SYM_DUMMY(retro_set_audio_sample_batch);
      SYM_DUMMY(retro_set_input_poll);
      SYM_DUMMY(retro_set_input_state);

      SYM_DUMMY(retro_set_controller_port_device);

      SYM_DUMMY(retro_reset);
      SYM_DUMMY(retro_run);

      SYM_DUMMY(retro_serialize_size);
      SYM_DUMMY(retro_serialize);
      SYM_DUMMY(retro_unserialize);

      SYM_DUMMY(retro_cheat_reset);
      SYM_DUMMY(retro_cheat_set);

      SYM_DUMMY(retro_load_game);
      SYM_DUMMY(retro_load_game_special);

      SYM_DUMMY(retro_unload_game);
      SYM_DUMMY(retro_get_region);
      SYM_DUMMY(retro_get_memory_data);
      SYM_DUMMY(retro_get_memory_size);
   }
   else
   {
#ifdef HAVE_DYNAMIC
      settings_t *settings = config_get_ptr();

      /* Need to use absolute path for this setting. */
      path_resolve_realpath(settings->libretro,
            sizeof(settings->libretro));

      RARCH_LOG("Loading dynamic libretro from: \"%s\"\n",
            settings->libretro);
      lib_handle = dylib_load(settings->libretro);
      if (!lib_handle)
      {
         RARCH_ERR("Failed to open dynamic library: \"%s\"\n",
               settings->libretro);
         rarch_fail(1, "load_dynamic()");
      }
#endif

      SYM(retro_init);
      SYM(retro_deinit);

      SYM(retro_api_version);
      SYM(retro_get_system_info);
      SYM(retro_get_system_av_info);

      SYM(retro_set_environment);
      SYM(retro_set_video_refresh);
      SYM(retro_set_audio_sample);
      SYM(retro_set_audio_sample_batch);
      SYM(retro_set_input_poll);
      SYM(retro_set_input_state);

      SYM(retro_set_controller_port_device);

      SYM(retro_reset);
      SYM(retro_run);

      SYM(retro_serialize_size);
      SYM(retro_serialize);
      SYM(retro_unserialize);

      SYM(retro_cheat_reset);
      SYM(retro_cheat_set);

      SYM(retro_load_game);
      SYM(retro_load_game_special);

      SYM(retro_unload_game);
      SYM(retro_get_region);
      SYM(retro_get_memory_data);
      SYM(retro_get_memory_size);
   }
}

/**
 * libretro_get_current_core_pathname:
 * @name                         : Sanitized name of libretro core.
 * @size                         : Size of @name
 *
 * Transforms a library id to a name suitable as a pathname.
 **/
void libretro_get_current_core_pathname(char *name, size_t size)
{
   size_t i;
   const char                *id = NULL;
   struct retro_system_info info = {0};

   if (size == 0)
      return;

   pretro_get_system_info(&info);
   id = info.library_name ? info.library_name : "Unknown";

   if (!id || strlen(id) >= size)
   {
      name[0] = '\0';
      return;
   }

   name[strlen(id)] = '\0';

   for (i = 0; id[i] != '\0'; i++)
   {
      char c = id[i];

      if (isspace(c) || isblank(c))
         name[i] = '_';
      else
         name[i] = tolower(c);
   }
}

/**
 * init_libretro_sym:
 * @dummy                        : Load dummy symbols if true
 *
 * Initializes libretro symbols and
 * setups environment callback functions.
 **/
void init_libretro_sym(bool dummy)
{
   /* Guarantee that we can do "dirty" casting.
    * Every OS that this program supports should pass this. */
   rarch_assert(sizeof(void*) == sizeof(void (*)(void)));

   if (!dummy)
   {
#ifdef HAVE_DYNAMIC
      settings_t *settings = config_get_ptr();
      function_t sym       = dylib_proc(NULL, "retro_init");

      if (sym)
      {
         /* Try to verify that -lretro was not linked in from other modules
          * since loading it dynamically and with -l will fail hard. */
         RARCH_ERR("Serious problem. RetroArch wants to load libretro dyamically, but it is already linked.\n");
         RARCH_ERR("This could happen if other modules RetroArch depends on link against libretro directly.\n");
         RARCH_ERR("Proceeding could cause a crash. Aborting ...\n");
         rarch_fail(1, "init_libretro_sym()");
      }

      if (!*settings->libretro)
      {
         RARCH_ERR("RetroArch is built for dynamic libretro, but libretro_path is not set. Cannot continue.\n");
         rarch_fail(1, "init_libretro_sym()");
      }
#endif
   }

   load_symbols(dummy);
}

/**
 * uninit_libretro_sym:
 *
 * Frees libretro core.
 *
 * Frees all core options,
 * associated state, and
 * unbind all libretro callback symbols.
 **/
void uninit_libretro_sym(void)
{
   global_t *global = global_get_ptr();

#ifdef HAVE_DYNAMIC
   if (lib_handle)
      dylib_close(lib_handle);
   lib_handle = NULL;
#endif

   if (global->system.core_options)
   {
      core_option_free(global->system.core_options);
      global->system.core_options = NULL;
   }

   /* No longer valid. */
   free(global->system.special);
   free(global->system.ports);
   memset(&global->system, 0, sizeof(global->system));
   core_set_shared_context = false;
   global->frontend_key_event = NULL;

   /* Performance counters no longer valid. */
   retro_perf_clear();
}

static void rarch_log_libretro(enum retro_log_level level,
      const char *fmt, ...)
{
   va_list vp;
   settings_t *settings = config_get_ptr();

   if ((unsigned)level < settings->libretro_log_level)
      return;

   va_start(vp, fmt);

   switch (level)
   {
      case RETRO_LOG_DEBUG:
         RARCH_LOG_V("libretro DEBUG", fmt, vp);
         break;

      case RETRO_LOG_INFO:
         RARCH_LOG_V("libretro INFO", fmt, vp);
         break;

      case RETRO_LOG_WARN:
         RARCH_LOG_V("libretro WARN", fmt, vp);
         break;

      case RETRO_LOG_ERROR:
         RARCH_LOG_V("libretro ERROR", fmt, vp);
         break;

      default:
         break;
   }

   va_end(vp);
}

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
bool rarch_environment_cb(unsigned cmd, void *data)
{
   unsigned p;
   driver_t *driver     = driver_get_ptr();
   settings_t *settings = config_get_ptr();
   global_t *global     = global_get_ptr();

   if (ignore_environment_cb)
      return false;

   switch (cmd)
   {
      case RETRO_ENVIRONMENT_GET_OVERSCAN:
         *(bool*)data = !settings->video.crop_overscan;
         RARCH_LOG("Environ GET_OVERSCAN: %u\n",
               (unsigned)!settings->video.crop_overscan);
         break;

      case RETRO_ENVIRONMENT_GET_CAN_DUPE:
         *(bool*)data = true;
         RARCH_LOG("Environ GET_CAN_DUPE: true\n");
         break;

      case RETRO_ENVIRONMENT_GET_VARIABLE:
      {
         struct retro_variable *var = (struct retro_variable*)data;
         RARCH_LOG("Environ GET_VARIABLE %s:\n", var->key);

         if (global->system.core_options)
            core_option_get(global->system.core_options, var);
         else
            var->value = NULL;

         RARCH_LOG("\t%s\n", var->value ? var->value : "N/A");
         break;
      }

      case RETRO_ENVIRONMENT_GET_VARIABLE_UPDATE:
         *(bool*)data = global->system.core_options ?
            core_option_updated(global->system.core_options) : false;
         break;

      /* SET_VARIABLES: Legacy path */
      case RETRO_ENVIRONMENT_SET_VARIABLES:
         RARCH_LOG("Environ SET_VARIABLES.\n");
         core_options_init(data, 0);
         break;

      case RETRO_ENVIRONMENT_SET_CORE_OPTIONS:
         RARCH_LOG("Environ SET_CORE_OPTIONS.\n");
         core_options_init(data, 1);
         break;

      case RETRO_ENVIRONMENT_SET_CORE_OPTIONS_INTL:
      {  /* TODO: don't ignore core_options_intl->local */
         RARCH_LOG("Environ SET_CORE_OPTIONS_INTL.\n");
         const struct retro_core_options_intl *core_options_intl
               = ((const struct retro_core_options_intl *)data);
         core_options_init(core_options_intl->us, 1);
         break;
      }

      case RETRO_ENVIRONMENT_SET_CORE_OPTIONS_V2:
         RARCH_LOG("Environ SET_CORE_OPTIONS_V2.\n");
         core_options_init(data, 2);
         return settings->core.option_categories;

      case RETRO_ENVIRONMENT_SET_CORE_OPTIONS_V2_INTL:
      {  /* TODO: don't ignore core_options_intl->local */
         RARCH_LOG("Environ SET_CORE_OPTIONS_V2_INTL.\n");
         const struct retro_core_options_v2_intl *core_options_intl
               = ((const struct retro_core_options_v2_intl *)data);
         core_options_init(core_options_intl->us, 2);
         return settings->core.option_categories;
      }

      case RETRO_ENVIRONMENT_SET_CORE_OPTIONS_DISPLAY:
      {
         RARCH_LOG("Environ SET_CORE_OPTIONS_DISPLAY.\n");
         const struct retro_core_option_display *core_options_display =
               (const struct retro_core_option_display *)data;

         if (global->system.core_options && core_options_display)
            core_option_set_visible(global->system.core_options,
                                    core_options_display->key,
                                    core_options_display->visible);
         break;
      }

      case RETRO_ENVIRONMENT_SET_CORE_OPTIONS_UPDATE_DISPLAY_CALLBACK:
      {
         RARCH_LOG("Environ SET_CORE_OPTIONS_UPDATE_DISPLAY_CALLBACK.\n");
         const struct retro_core_options_update_display_callback *cb =
               (const struct retro_core_options_update_display_callback*)data;

         if (cb && cb->callback)
            core_option_set_update_cb(cb->callback);
         else
            core_option_set_update_cb(NULL);

         break;
      }

      case RETRO_ENVIRONMENT_SET_MESSAGE:
      {
         const struct retro_message *msg = (const struct retro_message*)data;
         RARCH_LOG("Environ SET_MESSAGE: %s\n", msg->msg);
         rarch_main_msg_queue_push(msg->msg, 1, msg->frames, true);
         break;
      }

      case RETRO_ENVIRONMENT_SET_MESSAGE_EXT:
      {
         const struct retro_message_ext *msg
               = (const struct retro_message_ext*)data;

         /* Log message, if required */
         if (msg->target != RETRO_MESSAGE_TARGET_OSD)
         {
            switch (msg->level)
            {
               case RETRO_LOG_WARN:
                  RARCH_WARN("Environ SET_MESSAGE_EXT: %s\n", msg->msg);
                  break;
               case RETRO_LOG_ERROR:
                  RARCH_ERR("Environ SET_MESSAGE_EXT: %s\n", msg->msg);
                  break;
               default:
                  RARCH_LOG("Environ SET_MESSAGE_EXT: %s\n", msg->msg);
                  break;
            }
         }

         /* Display message via OSD, if required */
         if (msg->target != RETRO_MESSAGE_TARGET_LOG)
         {
            double fps = video_viewport_get_system_av_info()->timing.fps;
            unsigned frames = round(((fps > 0.0 ? fps : 60.0)
                  * msg->duration * 0.001));
            rarch_main_msg_queue_push(msg->msg, msg->priority, frames, false);
         }

         break;
      }

      case RETRO_ENVIRONMENT_SET_ROTATION:
      {
         unsigned rotation = *(const unsigned*)data;
         RARCH_LOG("Environ SET_ROTATION: %u (%u deg)\n",
               rotation, rotation * 90);
         if (!settings->video.allow_rotate)
            break;

         global->system.rotation = rotation;

         if (!video_driver_set_rotation(rotation))
            return false;
         break;
      }

      case RETRO_ENVIRONMENT_SHUTDOWN:
         RARCH_LOG("Environ SHUTDOWN.\n");
         global->system.shutdown = true;
         global->core_shutdown_initiated = true;
         break;

      case RETRO_ENVIRONMENT_SET_PERFORMANCE_LEVEL:
         global->system.performance_level = *(const unsigned*)data;
         RARCH_LOG("Environ PERFORMANCE_LEVEL: %u.\n",
               global->system.performance_level);
         break;

      case RETRO_ENVIRONMENT_GET_SYSTEM_DIRECTORY:
         *(const char**)data = *settings->system_directory ?
            settings->system_directory : NULL;
         RARCH_LOG("Environ SYSTEM_DIRECTORY: \"%s\".\n",
               settings->system_directory);
         break;

      case RETRO_ENVIRONMENT_GET_SAVE_DIRECTORY:
         *(const char**)data = global->core_savefile_dir[0] ?
            global->core_savefile_dir : NULL;
         RARCH_LOG("Environ SAVE_DIRECTORY: \"%s\".\n",
               global->core_savefile_dir);
         break;

      case RETRO_ENVIRONMENT_GET_USERNAME:
         *(const char**)data = *settings->username ?
            settings->username : NULL;
         RARCH_LOG("Environ GET_USERNAME: \"%s\".\n",
               settings->username);
         break;

      case RETRO_ENVIRONMENT_GET_LANGUAGE:
         *(unsigned *)data = RETRO_LANGUAGE_ENGLISH;
         RARCH_LOG("Environ GET_LANGUAGE: \"English\".\n");
         break;

      case RETRO_ENVIRONMENT_SET_PIXEL_FORMAT:
      {
         enum retro_pixel_format pix_fmt = 
            *(const enum retro_pixel_format*)data;

         switch (pix_fmt)
         {
            case RETRO_PIXEL_FORMAT_0RGB1555:
               RARCH_LOG("Environ SET_PIXEL_FORMAT: 0RGB1555.\n");
               break;

            case RETRO_PIXEL_FORMAT_RGB565:
               RARCH_LOG("Environ SET_PIXEL_FORMAT: RGB565.\n");
               break;
            case RETRO_PIXEL_FORMAT_XRGB8888:
               RARCH_LOG("Environ SET_PIXEL_FORMAT: XRGB8888.\n");
               break;
            default:
               return false;
         }

         video_driver_set_pixel_format(pix_fmt);
         break;
      }

      case RETRO_ENVIRONMENT_SET_INPUT_DESCRIPTORS:
      {
         unsigned retro_id, retro_port;
         const struct retro_input_descriptor *desc = NULL;
         bool any_set = false;

         memset(global->system.input_desc_btn, 0,
               sizeof(global->system.input_desc_btn));

         desc = (const struct retro_input_descriptor*)data;

         for (; desc->description; desc++)
         {
            retro_port = desc->port;
            retro_id   = desc->id;

            if (desc->port >= MAX_USERS)
               continue;

            /* Ignore all others for now. */
            if (desc->device != RETRO_DEVICE_JOYPAD  &&
                  desc->device != RETRO_DEVICE_ANALOG)
               continue;

            if (desc->id >= RARCH_FIRST_CUSTOM_BIND)
               continue;

            if (desc->device == RETRO_DEVICE_ANALOG)
            {
               switch (retro_id)
               {
                  case RETRO_DEVICE_ID_ANALOG_X:
                     switch (desc->index)
                     {
                        case RETRO_DEVICE_INDEX_ANALOG_LEFT:
                           global->system.input_desc_btn[retro_port][RARCH_ANALOG_LEFT_X_PLUS] = desc->description;
                           global->system.input_desc_btn[retro_port][RARCH_ANALOG_LEFT_X_MINUS] = desc->description;
                           break;
                        case RETRO_DEVICE_INDEX_ANALOG_RIGHT:
                           global->system.input_desc_btn[retro_port][RARCH_ANALOG_RIGHT_X_PLUS] = desc->description;
                           global->system.input_desc_btn[retro_port][RARCH_ANALOG_RIGHT_X_MINUS] = desc->description;
                           break;
                     }
                     break;
                  case RETRO_DEVICE_ID_ANALOG_Y:
                     switch (desc->index)
                     {
                        case RETRO_DEVICE_INDEX_ANALOG_LEFT:
                           global->system.input_desc_btn[retro_port][RARCH_ANALOG_LEFT_Y_PLUS] = desc->description;
                           global->system.input_desc_btn[retro_port][RARCH_ANALOG_LEFT_Y_MINUS] = desc->description;
                           break;
                        case RETRO_DEVICE_INDEX_ANALOG_RIGHT:
                           global->system.input_desc_btn[retro_port][RARCH_ANALOG_RIGHT_Y_PLUS] = desc->description;
                           global->system.input_desc_btn[retro_port][RARCH_ANALOG_RIGHT_Y_MINUS] = desc->description;
                           break;
                     }
                     break;
               }
            }
            else
               global->system.input_desc_btn[retro_port][retro_id] = desc->description;
         }

         static const char *libretro_btn_desc[] = {
            "B (bottom)", "Y (left)", "Select", "Start",
            "D-Pad Up", "D-Pad Down", "D-Pad Left", "D-Pad Right",
            "A (right)", "X (up)",
            "L", "R", "L2", "R2", "L3", "R3",
         };

         RARCH_LOG("Environ SET_INPUT_DESCRIPTORS:\n");
         for (p = 0; p < settings->input.max_users; p++)
         {
            for (retro_id = 0; retro_id < RARCH_FIRST_CUSTOM_BIND; retro_id++)
            {
               const char *description = global->system.input_desc_btn[p][retro_id];

               if (!description)
                  continue;
               any_set = true;

               RARCH_LOG("\tRetroPad, User %u, Button \"%s\" => \"%s\"\n",
                     p + 1, libretro_btn_desc[retro_id], description);
            }
         }

         if (any_set)
            global->has_set_input_descriptors = true;
         else
            global->has_set_input_descriptors = false;

         break;
      }

      case RETRO_ENVIRONMENT_SET_KEYBOARD_CALLBACK:
      {
         const struct retro_keyboard_callback *info = 
            (const struct retro_keyboard_callback*)data;

         RARCH_LOG("Environ SET_KEYBOARD_CALLBACK.\n");
         global->system.key_event   = info->callback;
         global->frontend_key_event = global->system.key_event;
         break;
      }

      case RETRO_ENVIRONMENT_SET_DISK_CONTROL_INTERFACE:
         RARCH_LOG("Environ SET_DISK_CONTROL_INTERFACE.\n");
         if (data)
            global->system.disk_control =
                  *(const struct retro_disk_control_callback*)data;
         else
            memset(&global->system.disk_control, 0,
                  sizeof(struct retro_disk_control_callback));
         break;

      case RETRO_ENVIRONMENT_SET_HW_RENDER:
      case RETRO_ENVIRONMENT_SET_HW_RENDER | RETRO_ENVIRONMENT_EXPERIMENTAL:
      {
         struct retro_hw_render_callback *hw_render = video_driver_callback();
         struct retro_hw_render_callback *cb = 
            (struct retro_hw_render_callback*)data;

         RARCH_LOG("Environ SET_HW_RENDER.\n");

         switch (cb->context_type)
         {
            case RETRO_HW_CONTEXT_NONE:
               RARCH_LOG("Requesting no HW context.\n");
               break;

#if (defined(HAVE_OPENGLES2) || defined(HAVE_OPENGLES3))
            case RETRO_HW_CONTEXT_OPENGLES2:
            case RETRO_HW_CONTEXT_OPENGLES3:
               RARCH_LOG("Requesting OpenGLES%u context.\n",
                     cb->context_type == RETRO_HW_CONTEXT_OPENGLES2 ? 2 : 3);
               break;

#if defined(HAVE_OPENGLES3)
            case RETRO_HW_CONTEXT_OPENGLES_VERSION:
               RARCH_LOG("Requesting OpenGLES%u.%u context.\n",
                     cb->version_major, cb->version_minor);
               break;
#endif

            case RETRO_HW_CONTEXT_OPENGL:
            case RETRO_HW_CONTEXT_OPENGL_CORE:
               RARCH_ERR("Requesting OpenGL context, but RetroArch is compiled against OpenGLES. Cannot use HW context.\n");
               return false;
#elif defined(HAVE_OPENGL)
            case RETRO_HW_CONTEXT_OPENGLES2:
            case RETRO_HW_CONTEXT_OPENGLES3:
               RARCH_ERR("Requesting OpenGLES%u context, but RetroArch is compiled against OpenGL. Cannot use HW context.\n",
                     cb->context_type == RETRO_HW_CONTEXT_OPENGLES2 ? 2 : 3);
               return false;

            case RETRO_HW_CONTEXT_OPENGLES_VERSION:
               RARCH_ERR("Requesting OpenGLES%u.%u context, but RetroArch is compiled against OpenGL. Cannot use HW context.\n",
                     cb->version_major, cb->version_minor);
               return false;

            case RETRO_HW_CONTEXT_OPENGL:
               RARCH_LOG("Requesting OpenGL context.\n");
               break;

            case RETRO_HW_CONTEXT_OPENGL_CORE:
               RARCH_LOG("Requesting core OpenGL context (%u.%u).\n",
                     cb->version_major, cb->version_minor);
               core_set_shared_context = true;
               break;
#endif

            default:
               RARCH_LOG("Requesting unknown context.\n");
               return false;
         }
         cb->get_current_framebuffer = video_driver_get_current_framebuffer;
         cb->get_proc_address        = video_driver_get_proc_address;

         if (cmd & RETRO_ENVIRONMENT_EXPERIMENTAL) /* Old ABI. Don't copy garbage. */
            memcpy(hw_render,
                  cb, offsetof(struct retro_hw_render_callback, stencil));
         else
            memcpy(hw_render, cb, sizeof(*cb));
         break;
      }

      case RETRO_ENVIRONMENT_SET_SUPPORT_NO_GAME:
      {
         bool state = *(const bool*)data;
         RARCH_LOG("Environ SET_SUPPORT_NO_GAME: %s.\n", state ? "yes" : "no");
         global->system.no_content = state;
         break;
      }

      case RETRO_ENVIRONMENT_GET_LIBRETRO_PATH:
      {
         const char **path = (const char**)data;
#ifdef HAVE_DYNAMIC
         *path = settings->libretro;
#else
         *path = NULL;
#endif
         break;
      }

      /* FIXME - PS3 audio driver needs to be fixed so that threaded 
       * audio works correctly (audio is already on a thread for PS3 
       * audio driver so that's probably the problem) */
#if defined(HAVE_THREADS) && !defined(__CELLOS_LV2__)
      case RETRO_ENVIRONMENT_SET_AUDIO_CALLBACK:
      {
         RARCH_LOG("Environ SET_AUDIO_CALLBACK.\n");
         const struct retro_audio_callback *info = 
            (const struct retro_audio_callback*)data;

         if (driver->recording_data) /* A/V sync is a must. */
            return false;

#ifdef HAVE_NETPLAY
         if (global->netplay_enable)
            return false;
#endif

         audio_driver_set_callback(info);
         break;
      }
#endif

      case RETRO_ENVIRONMENT_SET_FRAME_TIME_CALLBACK:
      {
         const struct retro_frame_time_callback *info = 
            (const struct retro_frame_time_callback*)data;

         RARCH_LOG("Environ SET_FRAME_TIME_CALLBACK.\n");

#ifdef HAVE_NETPLAY
         /* retro_run() will be called in very strange and 
          * mysterious ways, have to disable it. */
         if (global->netplay_enable)
            return false;
#endif

         global->system.frame_time = *info;
         break;
      }

      case RETRO_ENVIRONMENT_GET_RUMBLE_INTERFACE:
      {
         struct retro_rumble_interface *iface = 
            (struct retro_rumble_interface*)data;

         RARCH_LOG("Environ GET_RUMBLE_INTERFACE.\n");
         iface->set_rumble_state = input_driver_set_rumble_state;
         break;
      }

      case RETRO_ENVIRONMENT_GET_INPUT_DEVICE_CAPABILITIES:
      {
         uint64_t *mask = (uint64_t*)data;

         RARCH_LOG("Environ GET_INPUT_DEVICE_CAPABILITIES.\n");
         if (driver->input &&
               driver->input->get_capabilities && driver->input_data)
            *mask = input_driver_get_capabilities();
         else
            return false;
         break;
      }

      case RETRO_ENVIRONMENT_GET_SENSOR_INTERFACE:
      {
         struct retro_sensor_interface *iface = 
            (struct retro_sensor_interface*)data;

         RARCH_LOG("Environ GET_SENSOR_INTERFACE.\n");
         iface->set_sensor_state = input_sensor_set_state;
         iface->get_sensor_input = input_sensor_get_input;
         break;
      }

      case RETRO_ENVIRONMENT_GET_LOG_INTERFACE:
      {
         struct retro_log_callback *cb = (struct retro_log_callback*)data;

         RARCH_LOG("Environ GET_LOG_INTERFACE.\n");
         cb->log = rarch_log_libretro;
         break;
      }

      case RETRO_ENVIRONMENT_GET_PERF_INTERFACE:
      {
         struct retro_perf_callback *cb = (struct retro_perf_callback*)data;

         RARCH_LOG("Environ GET_PERF_INTERFACE.\n");
         cb->get_time_usec    = rarch_get_time_usec;
         cb->get_cpu_features = rarch_get_cpu_features;
         cb->get_perf_counter = rarch_get_perf_counter;
         cb->perf_register    = retro_perf_register; /* libretro specific path. */
         cb->perf_start       = rarch_perf_start;
         cb->perf_stop        = rarch_perf_stop;
         cb->perf_log         = retro_perf_log; /* libretro specific path. */
         break;
      }

      case RETRO_ENVIRONMENT_GET_CORE_ASSETS_DIRECTORY:
      {
         const char **dir = (const char**)data;

         *dir = *settings->core_assets_directory ?
            settings->core_assets_directory : NULL;
         RARCH_LOG("Environ CORE_ASSETS_DIRECTORY: \"%s\".\n",
               settings->core_assets_directory);
         break;
      }

      case RETRO_ENVIRONMENT_SET_SYSTEM_AV_INFO:
      {
         RARCH_LOG("Environ SET_SYSTEM_AV_INFO.\n");
         return driver_update_system_av_info(
               (const struct retro_system_av_info*)data);
      }

      case RETRO_ENVIRONMENT_SET_SUBSYSTEM_INFO:
      {
         unsigned i, j;
         const struct retro_subsystem_info *info = 
            (const struct retro_subsystem_info*)data;

         RARCH_LOG("Environ SET_SUBSYSTEM_INFO.\n");

         for (i = 0; info[i].ident; i++)
         {
            RARCH_LOG("Special game type: %s\n", info[i].desc);
            RARCH_LOG("  Ident: %s\n", info[i].ident);
            RARCH_LOG("  ID: %u\n", info[i].id);
            RARCH_LOG("  Content:\n");
            for (j = 0; j < info[i].num_roms; j++)
            {
               RARCH_LOG("    %s (%s)\n",
                     info[i].roms[j].desc, info[i].roms[j].required ?
                     "required" : "optional");
            }
         }

         free(global->system.special);
         global->system.special = (struct retro_subsystem_info*)
            calloc(i, sizeof(*global->system.special));

         if (!global->system.special)
            return false;

         memcpy(global->system.special, info,
               i * sizeof(*global->system.special));
         global->system.num_special = i;
         break;
      }

      case RETRO_ENVIRONMENT_SET_CONTROLLER_INFO:
      {
         unsigned i, j;
         const struct retro_controller_info *info = 
            (const struct retro_controller_info*)data;

         RARCH_LOG("Environ SET_CONTROLLER_INFO.\n");

         for (i = 0; info[i].types; i++)
         {
            RARCH_LOG("Controller port: %u\n", i + 1);
            for (j = 0; j < info[i].num_types; j++)
               RARCH_LOG("   %s (ID: %u)\n", info[i].types[j].desc,
                     info[i].types[j].id);
         }

         free(global->system.ports);
         global->system.ports = (struct retro_controller_info*)
            calloc(i, sizeof(*global->system.ports));
         if (!global->system.ports)
            return false;

         memcpy(global->system.ports, info,
               i * sizeof(*global->system.ports));
         global->system.num_ports = i;
         break;
      }

      case RETRO_ENVIRONMENT_SET_GEOMETRY:
      {
         struct retro_system_av_info *av_info = video_viewport_get_system_av_info();
         const struct retro_game_geometry *in_geom = 
            (const struct retro_game_geometry*)data;
         struct retro_game_geometry *geom = av_info ? 
            (struct retro_game_geometry*)&av_info->geometry : NULL;

         RARCH_LOG("Environ SET_GEOMETRY.\n");

         /* Can potentially be called every frame,
          * don't do anything unless required. */
         if (geom->base_width != in_geom->base_width ||
               geom->base_height != in_geom->base_height ||
               geom->aspect_ratio != in_geom->aspect_ratio)
         {
            geom->base_width   = in_geom->base_width;
            geom->base_height  = in_geom->base_height;
            geom->aspect_ratio = in_geom->aspect_ratio;
            RARCH_LOG("SET_GEOMETRY: %ux%u, aspect: %.3f.\n",
                  geom->base_width, geom->base_height, geom->aspect_ratio);

            /* Forces recomputation of aspect ratios if 
             * using core-dependent aspect ratios. */
            event_command(EVENT_CMD_VIDEO_SET_ASPECT_RATIO);

            /* TODO: Figure out what to do, if anything, with recording. */
         }
         break;
      }
      
      case RETRO_ENVIRONMENT_SET_HW_SHARED_CONTEXT:
      {
         core_set_shared_context = true;
         break;
      }

      case RETRO_ENVIRONMENT_GET_LED_INTERFACE:
      {
         struct retro_led_interface *iface =
            (struct retro_led_interface *)data;

         if (iface)
            iface->set_led_state = dummy_set_led;

         RARCH_LOG("Environ GET_LED_INTERFACE.\n");
         return false;
      }
      
      case RETRO_ENVIRONMENT_GET_AUDIO_VIDEO_ENABLE:
      {
         int result = 0;
         if (driver->audio_active && !driver->audio_suspended)
            result |= 2;
         if (driver->video_active)
            result |= 1;
         if (preempt_in_preframe(driver->preempt_data))
            result |= 4;
         if (data != NULL)
         {
            int* result_p = (int*)data;
            *result_p = result;
         }
         break;
      }

      case RETRO_ENVIRONMENT_GET_SAVESTATE_CONTEXT:
      {
         global->savestate_context = RETRO_SAVESTATE_CONTEXT_NORMAL;

         if (preempt_in_preframe(driver->preempt_data))
         {
            global->savestate_context = settings->preempt_fast_savestates
                  ? RETRO_SAVESTATE_CONTEXT_RUNAHEAD_SAME_INSTANCE
                  : RETRO_SAVESTATE_CONTEXT_RUNAHEAD_SAME_BINARY;
         }
#ifdef HAVE_NETPLAY
         else if (netplay_use_rollback_states(driver->netplay_data))
            global->savestate_context = RETRO_SAVESTATE_CONTEXT_ROLLBACK_NETPLAY;
#endif
         if (data != NULL)
         {
            int* result_p = (int*)data;
            *result_p = global->savestate_context;
         }

         break;
      }

      case RETRO_ENVIRONMENT_GET_VFS_INTERFACE:
      {
         const uint32_t supported_vfs_version = 3;
         static struct retro_vfs_interface vfs_iface =
         {
            /* VFS API v1 */
            retro_vfs_file_get_path_impl,
            retro_vfs_file_open_impl,
            retro_vfs_file_close_impl,
            retro_vfs_file_size_impl,
            retro_vfs_file_tell_impl,
            retro_vfs_file_seek_impl,
            retro_vfs_file_read_impl,
            retro_vfs_file_write_impl,
            retro_vfs_file_flush_impl,
            retro_vfs_file_remove_impl,
            retro_vfs_file_rename_impl,
            /* VFS API v2 */
            retro_vfs_file_truncate_impl,
            /* VFS API v3 */
            retro_vfs_stat_impl,
            retro_vfs_mkdir_impl,
            retro_vfs_opendir_impl,
            retro_vfs_readdir_impl,
            retro_vfs_dirent_get_name_impl,
            retro_vfs_dirent_is_dir_impl,
            retro_vfs_closedir_impl
         };

         struct retro_vfs_interface_info *vfs_iface_info = (struct retro_vfs_interface_info *) data;
         if (vfs_iface_info->required_interface_version <= supported_vfs_version)
         {
            RARCH_LOG("Environ GET_VFS_INTERFACE. Core requested version >= V%d, providing V%d.\n",
                  vfs_iface_info->required_interface_version, supported_vfs_version);
            vfs_iface_info->required_interface_version = supported_vfs_version;
            vfs_iface_info->iface                      = &vfs_iface;
         }
         else
         {
            RARCH_WARN("Environ GET_VFS_INTERFACE. Core requested version V%d which is higher than what we support (V%d).\n",
                  vfs_iface_info->required_interface_version, supported_vfs_version);
            return false;
         }
         break;
      }
      
      case RETRO_ENVIRONMENT_GET_INPUT_BITMASKS:
         /* return true */
         break;

      case RETRO_ENVIRONMENT_GET_CORE_OPTIONS_VERSION:
         /* Current API version is 2 */
         *(unsigned *)data = 2;
         break;

      case RETRO_ENVIRONMENT_GET_TARGET_REFRESH_RATE:
         *(float *)data = settings->video.refresh_rate;
         break;
         
      case RETRO_ENVIRONMENT_GET_FASTFORWARDING:
         *(bool *)data = driver->nonblock_state;
         break;

      /* Private extensions for internal use, not part of libretro API. */
      case RETRO_ENVIRONMENT_SET_LIBRETRO_PATH:
         RARCH_LOG("Environ (Private) SET_LIBRETRO_PATH.\n");

         if (path_file_exists((const char*)data))
         {
            strlcpy(settings->libretro, (const char*)data,
                  sizeof(settings->libretro));
            path_libretro_name(global->libretro_name, settings->libretro);
         }
         else
            return false;
         break;

      case RETRO_ENVIRONMENT_GET_CLEAR_ALL_THREAD_WAITS_CB:
         *(retro_environment_t *)data = rarch_clear_all_thread_waits;
         break;

      case RETRO_ENVIRONMENT_EXEC:
      case RETRO_ENVIRONMENT_EXEC_ESCAPE:

         if (data)
            strlcpy(global->fullpath, (const char*)data,
                  sizeof(global->fullpath));
         else
            *global->fullpath = '\0';

#if defined(RARCH_CONSOLE)
         if (driver->frontend_ctx && driver->frontend_ctx->set_fork)
            driver->frontend_ctx->set_fork(true, true);
#elif defined(HAVE_DYNAMIC)
         rarch_main_set_state(RARCH_ACTION_STATE_LOAD_CONTENT);
#endif

         if (cmd == RETRO_ENVIRONMENT_EXEC_ESCAPE)
         {
            RARCH_LOG("Environ (Private) EXEC_ESCAPE.\n");
            global->exec = true;
         }
         else
            RARCH_LOG("Environ (Private) EXEC.\n");

         break;

      default:
         RARCH_LOG("Environ UNSUPPORTED (#%u).\n", cmd);
         return false;
   }

   return true;
}

