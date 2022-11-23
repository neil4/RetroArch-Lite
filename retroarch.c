/*  RetroArch - A frontend for libretro.
 *  Copyright (C) 2010-2014 - Hans-Kristian Arntzen
 *  Copyright (C) 2011-2015 - Daniel De Matteis
 *  Copyright (C) 2012-2015 - Michael Lelli
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

#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <ctype.h>
#include <errno.h>
#include <boolean.h>

#ifdef _WIN32
#ifdef _XBOX
#include <xtl.h>
#else
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#endif
#endif

#include <compat/strl.h>
#include <compat/getopt.h>
#include <compat/posix_string.h>
#include <file/file_path.h>

#include <rhash.h>

#include "libretro_version_1.h"
#include "dynamic.h"
#include "configuration.h"
#include "general.h"
#include "retroarch.h"
#include "runloop_data.h"
#include "performance.h"
#include "cheats.h"
#include "input/input_remapping.h"

#include "git_version.h"
#include "intl/intl.h"

#include "menu/menu.h"
#include "menu/menu_hash.h"
#include "menu/menu_entries.h"
#include "menu/menu_setting.h"
#include "menu/menu_shader.h"
#include "menu/menu_input.h"

/* Descriptive names for options without short variant. Please keep the name in
   sync with the option name. Order does not matter. */
enum {
   RA_OPT_MENU,
   RA_OPT_PORT,
   RA_OPT_NICK,
   RA_OPT_COMMAND,
   RA_OPT_APPENDCONFIG,
   RA_OPT_BPS,
   RA_OPT_IPS,
   RA_OPT_NO_PATCH,
   RA_OPT_RECORDCONFIG,
   RA_OPT_SUBSYSTEM,
   RA_OPT_SIZE,
   RA_OPT_FEATURES,
   RA_OPT_VERSION,
   RA_OPT_EOF_EXIT,
   RA_OPT_LOG_FILE,
   RA_OPT_MAX_FRAMES
};

#include "config.features.h"

#define _PSUPP(var, name, desc) printf("  %s:\n\t\t%s: %s\n", name, desc, _##var##_supp ? "yes" : "no")
static void print_features(void)
{
   puts("");
   puts("Features:");
   _PSUPP(command, "Command", "Command interface support");
   _PSUPP(network_command, "Network Command", "Network Command interface support");
   _PSUPP(sdl, "SDL", "SDL input/audio/video drivers");
   _PSUPP(sdl2, "SDL2", "SDL2 input/audio/video drivers");
   _PSUPP(x11, "X11", "X11 input/video drivers");
   _PSUPP(wayland, "wayland", "Wayland input/video drivers");
   _PSUPP(thread, "Threads", "Threading support");
   _PSUPP(opengl, "OpenGL", "OpenGL driver");
   _PSUPP(opengles, "OpenGL ES", "OpenGL ES driver");
   _PSUPP(xvideo, "XVideo", "Video driver");
   _PSUPP(udev, "UDEV", "UDEV/EVDEV input driver support");
   _PSUPP(egl, "EGL",   "video context driver");
   _PSUPP(kms, "KMS",   "video context driver");
   _PSUPP(vg, "OpenVG", "video context driver");
   _PSUPP(coreaudio, "CoreAudio", "Audio driver");
   _PSUPP(alsa, "ALSA", "Audio driver");
   _PSUPP(oss, "OSS", "Audio driver");
   _PSUPP(jack, "Jack", "Audio driver");
   _PSUPP(rsound, "RSound", "Audio driver");
   _PSUPP(roar, "RoarAudio", "Audio driver");
   _PSUPP(pulse, "PulseAudio", "Audio driver");
   _PSUPP(dsound, "DirectSound", "Audio driver");
   _PSUPP(xaudio, "XAudio2", "Audio driver");
   _PSUPP(al, "OpenAL", "Audio driver");
   _PSUPP(sl, "OpenSL", "Audio driver");
   _PSUPP(7zip, "7zip", "7zip support");
   _PSUPP(zlib, "zlib", ".zip extraction");
   _PSUPP(dylib, "External", "External filter and plugin support");
   _PSUPP(cg, "Cg", "Fragment/vertex shader driver");
   _PSUPP(glsl, "GLSL", "Fragment/vertex shader driver");
   _PSUPP(glsl, "HLSL", "Fragment/vertex shader driver");
   _PSUPP(libxml2, "libxml2", "libxml2 XML parsing");
   _PSUPP(sdl_image, "SDL_image", "SDL_image image loading");
   _PSUPP(rpng, "rpng", "PNG image loading/encoding");
   _PSUPP(fbo, "FBO", "OpenGL render-to-texture (multi-pass shaders)");
   _PSUPP(dynamic, "Dynamic", "Dynamic run-time loading of libretro library");
   _PSUPP(ffmpeg, "FFmpeg", "On-the-fly recording of gameplay with libavcodec");
   _PSUPP(freetype, "FreeType", "TTF font rendering driver");
   _PSUPP(coretext, "CoreText", "TTF font rendering driver (for OSX and/or iOS)");
   _PSUPP(netplay, "Netplay", "Peer-to-peer netplay");
   _PSUPP(python, "Python", "Script support in shaders");
   _PSUPP(libusb, "Libusb", "Libusb support");
   _PSUPP(cocoa, "Cocoa", "Cocoa UI companion support (for OSX and/or iOS)");
   _PSUPP(qt, "QT", "QT UI companion support");
   _PSUPP(avfoundation, "AVFoundation", "Camera driver");
   _PSUPP(v4l2, "Video4Linux2", "Camera driver");
}
#undef _PSUPP

static void print_version(void)
{
   char str[PATH_MAX_LENGTH];

#ifdef HAVE_GIT_VERSION
   printf(RETRO_FRONTEND ": Frontend for libretro -- v" PACKAGE_VERSION " -- %s --\n", rarch_git_version);
#else
   puts(RETRO_FRONTEND ": Frontend for libretro -- v" PACKAGE_VERSION " --");
#endif
   str[0] = '\0';
   rarch_info_get_capabilities(RARCH_CAPABILITIES_COMPILER, str, sizeof(str));
   fprintf(stdout, "%s", str);
   fprintf(stdout, "Built: %s\n", __DATE__);
}

/**
 * print_help:
 *
 * Prints help message explaining RetroArch's commandline switches.
 **/
static void print_help(const char *arg0)
{
   puts("===================================================================");
   print_version();
   puts("===================================================================");

   printf("Usage: %s [OPTIONS]... [FILE]\n", arg0);

   puts("  -h, --help            Show this help message.");
   puts("  -v, --verbose         Verbose logging.");
   puts("      --log-file=FILE   Log " RETRO_FRONTEND " messages to FILE.");
   puts("      --version         Show " RETRO_FRONTEND " version.");
   puts("      --features        Prints available features compiled into " RETRO_FRONTEND ".");
   puts("      --menu            Do not require content or libretro core to be loaded,\n"
        "                        starts directly in menu. If no arguments are passed to\n"
        "                        " RETRO_FRONTEND ", it is equivalent to using --menu as only argument.");
   puts("  -s, --save=PATH       Path for save files (*.srm).");
   puts("  -S, --savestate=PATH  Path for the save state files (*.state).");
   puts("  -f, --fullscreen      Start " RETRO_FRONTEND " in fullscreen regardless of config settings.");
   puts("  -c, --config=FILE     Path for config file." RARCH_DEFAULT_CONF_PATH_STR);
   puts("      --appendconfig=FILE\n"
        "                        Extra config files are loaded in, and take priority over\n"
        "                        config selected in -c (or default). Multiple configs are\n"
        "                        delimited by '|'.");
#ifdef HAVE_DYNAMIC
   puts("  -L, --libretro=FILE   Path to libretro implementation. Overrides any config setting.");
#endif
   puts("      --subsystem=NAME  Use a subsystem of the libretro core. Multiple content\n"
        "                        files are loaded as multiple arguments. If a content\n"
        "                        file is skipped, use a blank (\"\") command line argument.\n"
        "                        Content must be loaded in an order which depends on the\n"
        "                        particular subsystem used. See verbose log output to learn\n"
        "                        how a particular subsystem wants content to be loaded.\n");

   printf("  -N, --nodevice=PORT\n"
          "                        Disconnects controller device connected to PORT (1 to %d).\n", MAX_USERS);
   printf("  -A, --dualanalog=PORT\n"
          "                        Connect a DualAnalog controller to PORT (1 to %d).\n", MAX_USERS);
   printf("  -d, --device=PORT:ID\n"
          "                        Connect a generic device into PORT of the device (1 to %d).\n", MAX_USERS);
   puts("                        Format is PORT:ID, where ID is a number corresponding to the particular device.");

   puts("  -M, --sram-mode=MODE  SRAM handling mode. MODE can be 'noload-nosave',\n"
        "                        'noload-save', 'load-nosave' or 'load-save'.\n"
        "                        Note: 'noload-save' implies that save files *WILL BE OVERWRITTEN*.");

#ifdef HAVE_NETPLAY
   puts("  -H, --host            Host netplay as user 1.");
   puts("  -C, --connect=HOST    Connect to netplay server as user 2.");
   puts("      --port=PORT       Port used to netplay. Default is 55435.");
   puts("  -F, --frames=NUMBER   Sync frames when using netplay.");
#endif
   puts("      --nick=NICK       Picks a username (for use with netplay). Not mandatory.");
#if defined(HAVE_NETWORK_CMD) && defined(HAVE_NETPLAY)
   puts("      --command         Sends a command over UDP to an already running " RETRO_FRONTEND " process.");
   puts("      Available commands are listed if command is invalid.");
#endif

   puts("  -r, --record=FILE     Path to record video file.\n        Using .mkv extension is recommended.");
   puts("      --recordconfig    Path to settings used during recording.");
   puts("      --size=WIDTHxHEIGHT\n"
        "                        Overrides output video size when recording.");
   puts("  -U, --ups=FILE        Specifies path for UPS patch that will be applied to content.");
   puts("      --bps=FILE        Specifies path for BPS patch that will be applied to content.");
   puts("      --ips=FILE        Specifies path for IPS patch that will be applied to content.");
   puts("      --no-patch        Disables all forms of content patching.");
   puts("  -D, --detach          Detach " RETRO_FRONTEND " from the running console. Not relevant for all platforms.");
   puts("      --max-frames=NUMBER\n"
        "                        Runs for the specified number of frames, then exits.\n");
}

static void set_basename(const char *path)
{
   char *dst          = NULL;
   global_t *global   = global_get_ptr();

   strlcpy(global->fullpath, path, sizeof(global->fullpath));
   strlcpy(global->basename, path, sizeof(global->basename));

#ifdef HAVE_COMPRESSION
   /* Removing extension is a bit tricky for compressed files.
    * Basename means:
    * /file/to/path/game.extension should be:
    * /file/to/path/game
    *
    * Two things to consider here are: /file/to/path/ is expected
    * to be a directory and "game" is a single file. This is used for
    * states and srm default paths.
    *
    * For compressed files we have:
    *
    * /file/to/path/comp.7z#game.extension and
    * /file/to/path/comp.7z#folder/game.extension
    *
    * The choice I take here is:
    * /file/to/path/game as basename. We might end up in a writable
    * directory then and the name of srm and states are meaningful.
    *
    */
   path_basedir(global->basename);
   fill_pathname_dir(global->basename, path, "", sizeof(global->basename));
#endif

   if ((dst = strrchr(global->basename, '.')))
      *dst = '\0';
}

static void set_special_paths(char **argv, unsigned num_content)
{
   unsigned i;
   union string_list_elem_attr attr;
   global_t   *global   = global_get_ptr();
   settings_t *settings = config_get_ptr();

   /* First content file is the significant one. */
   set_basename(argv[0]);

   global->subsystem_fullpaths = string_list_new();
   rarch_assert(global->subsystem_fullpaths);

   attr.i = 0;

   for (i = 0; i < num_content; i++)
      string_list_append(global->subsystem_fullpaths, argv[i], attr);

   /* We defer SRAM path updates until we can resolve it.
    * It is more complicated for special content types. */

   if (!global->has_set_state_path)
      fill_pathname_noext(global->savestate_name, global->basename,
            ".state", sizeof(global->savestate_name));

   if (path_is_directory(global->savestate_name))
   {
      fill_pathname_dir(global->savestate_name, global->basename,
            ".state", sizeof(global->savestate_name));
      RARCH_LOG("Redirecting save state to \"%s\".\n",
            global->savestate_name);
   }

   /* If this is already set,
    * do not overwrite it as this was initialized before in
    * a menu or otherwise. */
   if (!*settings->system_directory)
      fill_pathname_basedir(settings->system_directory, argv[0],
            sizeof(settings->system_directory));
}

void set_paths_redirect()
{
   global_t                *global   = global_get_ptr();
   settings_t              *settings = config_get_ptr();
   uint32_t global_library_name_hash = ((global && global->system.info.library_name &&
            (global->system.info.library_name[0] != '\0'))
         ? djb2_calculate(global->system.info.library_name) : 0);
   char *basename = *global->fullpath ?
         global->basename : global->libretro_name;
   
   /* Default to subdirectories 'save' and 'state' */
   if (*global->savefile_dir)
      strlcpy(global->core_savefile_dir, global->savefile_dir,
              sizeof(global->savefile_dir));
   else
   {
      strlcpy(global->core_savefile_dir, path_default_dotslash(),
              sizeof(global->core_savefile_dir));
      strlcat(global->core_savefile_dir, "save",
              sizeof(global->core_savefile_dir));
   }
   
   if (*global->savestate_dir)
      strlcpy(global->core_savestate_dir, global->savestate_dir,
              sizeof(global->savestate_dir));
   else
   {
      strlcpy(global->core_savestate_dir, path_default_dotslash(),
              sizeof(global->core_savestate_dir));
      strlcat(global->core_savestate_dir, "state",
              sizeof(global->core_savestate_dir));
   }
   
   /* Default input_remapping_directory to menu_config_directory if empty.
    * Subdirectories are created later. */
   if (settings->input_remapping_directory[0] == '\0')
      strlcpy(settings->input_remapping_directory,
              settings->menu_config_directory,
              sizeof(settings->menu_config_directory));

   if (global_library_name_hash != MENU_VALUE_NO_CORE)
   {
      /* per-core saves: append the libretro_name to the save location */
      if (settings->sort_savefiles_enable)
      {
         fill_pathname_dir(
               global->core_savefile_dir,
               global->libretro_name,
               "",
               sizeof(global->core_savefile_dir));

         /* if path doesn't exist, try to create it.
          * If everything fails revert to the original path. */
         if(!path_is_directory(global->core_savefile_dir))
            if(!path_mkdir(global->core_savefile_dir))
               strlcpy(global->core_savefile_dir,
                       global->savefile_dir,
                       sizeof(global->savefile_dir));
      }

      /* per-core states: append the libretro_name to the save location */
      if (settings->sort_savestates_enable)
      {
         fill_pathname_dir(
               global->core_savestate_dir,
               global->libretro_name,
               "",
               sizeof(global->core_savestate_dir));

         /* If path doesn't exist, try to create it.
          * If everything fails, revert to the original path. */
         if(!path_is_directory(global->core_savestate_dir))
            if(!path_mkdir(global->core_savestate_dir))
               strlcpy(global->core_savestate_dir,
                       global->savestate_dir,
                       sizeof(global->savestate_dir));
      }
   }

   if(path_is_directory(global->core_savefile_dir))
      strlcpy(global->savefile_name,
              global->core_savefile_dir, sizeof(global->core_savefile_dir));

   if(path_is_directory(global->core_savestate_dir))
      strlcpy(global->savestate_name,
              global->core_savestate_dir, sizeof(global->core_savestate_dir));

   if (path_is_directory(global->savefile_name))
   {
      fill_pathname_dir(global->savefile_name, basename,
            ".srm", sizeof(global->savefile_name));
      RARCH_LOG("Redirecting save file to \"%s\".\n", global->savefile_name);
   }

   if (path_is_directory(global->savestate_name))
   {
      fill_pathname_dir(global->savestate_name, basename,
            ".state", sizeof(global->savestate_name));
      RARCH_LOG("Redirecting save state to \"%s\".\n", global->savestate_name);
   }

   if (path_is_directory(global->cheatfile_name))
   {
      fill_pathname_dir(global->cheatfile_name, basename,
            ".state", sizeof(global->cheatfile_name));
      RARCH_LOG("Redirecting cheat file to \"%s\".\n", global->cheatfile_name);
   }
}

void rarch_set_paths(const char *path)
{
   settings_t *settings = config_get_ptr();
   global_t   *global   = global_get_ptr();

   set_basename(path);

   if (!global->has_set_save_path)
      fill_pathname_noext(global->savefile_name, global->basename,
            ".srm", sizeof(global->savefile_name));
   if (!global->has_set_state_path)
      fill_pathname_noext(global->savestate_name, global->basename,
            ".state", sizeof(global->savestate_name));
   fill_pathname_noext(global->cheatfile_name, global->basename,
         ".cht", sizeof(global->cheatfile_name));

   set_paths_redirect();

   /* If this is already set, do not overwrite it
    * as this was initialized before in a menu or otherwise. */
   if (*settings->system_directory)
      return;

   fill_pathname_basedir(settings->system_directory, path,
         sizeof(settings->system_directory));
}

/**
 * parse_input:
 * @argc                 : Count of (commandline) arguments.
 * @argv                 : (Commandline) arguments.
 *
 * Parses (commandline) arguments passed to RetroArch.
 *
 **/
static void parse_input(int argc, char *argv[])
{
   runloop_t *runloop = rarch_main_get_ptr();
   global_t  *global  = global_get_ptr();

   global->libretro_no_content           = false;
   global->libretro_dummy                = false;
   global->has_set_save_path             = false;
   global->has_set_state_path            = false;
   global->has_set_libretro              = false;
   global->has_set_libretro_directory    = false;
   global->has_set_verbosity             = false;

   global->has_set_netplay_mode          = false;
   global->has_set_username              = false;
   global->has_set_netplay_ip_address    = false;
   global->has_set_netplay_delay_frames  = false;
   global->has_set_netplay_ip_port       = false;

   global->has_set_ups_pref              = false;
   global->has_set_bps_pref              = false;
   global->has_set_ips_pref              = false;

   global->ups_pref                      = false;
   global->bps_pref                      = false;
   global->ips_pref                      = false;
   *global->ups_name                     = '\0';
   *global->bps_name                     = '\0';
   *global->ips_name                     = '\0';
   *global->subsystem                    = '\0';

   if (argc < 2)
   {
      global->libretro_dummy             = true;
      return;
   }

   /* Make sure we can call parse_input several times ... */
   optind = 0;

   int val = 0;

   const struct option opts[] = {
#ifdef HAVE_DYNAMIC
      { "libretro",     1, NULL, 'L' },
#endif
      { "menu",         0, &val, RA_OPT_MENU },
      { "help",         0, NULL, 'h' },
      { "save",         1, NULL, 's' },
      { "fullscreen",   0, NULL, 'f' },
      { "record",       1, NULL, 'r' },
      { "recordconfig", 1, &val, RA_OPT_RECORDCONFIG },
      { "size",         1, &val, RA_OPT_SIZE },
      { "verbose",      0, NULL, 'v' },
      { "config",       1, NULL, 'c' },
      { "appendconfig", 1, &val, RA_OPT_APPENDCONFIG },
      { "nodevice",     1, NULL, 'N' },
      { "dualanalog",   1, NULL, 'A' },
      { "device",       1, NULL, 'd' },
      { "savestate",    1, NULL, 'S' },
      { "sram-mode",    1, NULL, 'M' },
#ifdef HAVE_NETPLAY
      { "host",         0, NULL, 'H' },
      { "connect",      1, NULL, 'C' },
      { "frames",       1, NULL, 'F' },
      { "port",         1, &val, RA_OPT_PORT },
#endif
      { "nick",         1, &val, RA_OPT_NICK },
#if defined(HAVE_NETWORK_CMD) && defined(HAVE_NETPLAY)
      { "command",      1, &val, RA_OPT_COMMAND },
#endif
      { "ups",          1, NULL, 'U' },
      { "bps",          1, &val, RA_OPT_BPS },
      { "ips",          1, &val, RA_OPT_IPS },
      { "no-patch",     0, &val, RA_OPT_NO_PATCH },
      { "detach",       0, NULL, 'D' },
      { "features",     0, &val, RA_OPT_FEATURES },
      { "subsystem",    1, &val, RA_OPT_SUBSYSTEM },
      { "max-frames",   1, &val, RA_OPT_MAX_FRAMES },
      { "eof-exit",     0, &val, RA_OPT_EOF_EXIT },
      { "version",      0, &val, RA_OPT_VERSION },
#ifdef HAVE_FILE_LOGGER
      { "log-file",     1, &val, RA_OPT_LOG_FILE },
#endif
      { NULL, 0, NULL, 0 }
   };

#define FFMPEG_RECORD_ARG "r:"

#ifdef HAVE_DYNAMIC
#define DYNAMIC_ARG "L:"
#else
#define DYNAMIC_ARG
#endif

#ifdef HAVE_NETPLAY
#define NETPLAY_ARG "HC:F:"
#else
#define NETPLAY_ARG
#endif


   const char *optstring = "hs:fvS:A:c:U:DN:d:" NETPLAY_ARG DYNAMIC_ARG FFMPEG_RECORD_ARG;
   settings_t *settings = config_get_ptr();

   for (;;)
   {
      int port;
      val   = 0;
      int c = getopt_long(argc, argv, optstring, opts, NULL);

      if (c == -1)
         break;

      switch (c)
      {
         case 'h':
            print_help(argv[0]);
            exit(0);

         case 'd':
         {
            unsigned id = 0;
            struct string_list *list = string_split(optarg, ":");

            port = 0;

            if (list && list->size == 2)
            {
               port = strtol(list->elems[0].data, NULL, 0);
               id = strtoul(list->elems[1].data, NULL, 0);
            }
            string_list_free(list);

            if (port < 1 || port > MAX_USERS)
            {
               RARCH_ERR("Connect device to a valid port.\n");
               print_help(argv[0]);
               rarch_fail(1, "parse_input()");
            }
            settings->input.libretro_device[port - 1] = id;
            global->has_set_libretro_device[port - 1] = true;
            break;
         }

         case 'A':
            port = strtol(optarg, NULL, 0);
            if (port < 1 || port > MAX_USERS)
            {
               RARCH_ERR("Connect dualanalog to a valid port.\n");
               print_help(argv[0]);
               rarch_fail(1, "parse_input()");
            }
            settings->input.libretro_device[port - 1] = RETRO_DEVICE_ANALOG;
            global->has_set_libretro_device[port - 1] = true;
            break;

         case 's':
            strlcpy(global->savefile_name, optarg,
                  sizeof(global->savefile_name));
            global->has_set_save_path = true;
            break;

         case 'f':
            global->force_fullscreen = true;
            break;

         case 'S':
            strlcpy(global->savestate_name, optarg,
                  sizeof(global->savestate_name));
            global->has_set_state_path = true;
            break;

         case 'v':
            global->verbosity = true;
            global->has_set_verbosity = true;
            frontend_driver_attach_console();
            break;

         case 'N':
            port = strtol(optarg, NULL, 0);
            if (port < 1 || port > MAX_USERS)
            {
               RARCH_ERR("Disconnect device from a valid port.\n");
               print_help(argv[0]);
               rarch_fail(1, "parse_input()");
            }
            settings->input.libretro_device[port - 1] = RETRO_DEVICE_NONE;
            global->has_set_libretro_device[port - 1] = true;
            break;

         case 'c':
            strlcpy(global->config_path, optarg,
                  sizeof(global->config_path));
            break;

         case 'r':
            strlcpy(global->record.path, optarg,
                  sizeof(global->record.path));
            global->record.enable = true;
            break;

#ifdef HAVE_DYNAMIC
         case 'L':
            if (path_is_directory(optarg))
            {
               *settings->libretro = '\0';
               *settings->core_content_directory = '\0';
               strlcpy(settings->libretro_directory, optarg,
                     sizeof(settings->libretro_directory));
               global->has_set_libretro = true;
               global->has_set_libretro_directory = true;
               RARCH_WARN("Using old --libretro behavior. Setting libretro_directory to \"%s\" instead.\n", optarg);
            }
            else
            {
               strlcpy(settings->libretro, optarg,
                     sizeof(settings->libretro));
               global->has_set_libretro = true;
            }
            break;
#endif

         case 'M':
            if (!strcmp(optarg, "noload-nosave"))
            {
               global->sram_load_disable = true;
               global->sram_save_disable = true;
            }
            else if (!strcmp(optarg, "noload-save"))
               global->sram_load_disable = true;
            else if (!strcmp(optarg, "load-nosave"))
               global->sram_save_disable = true;
            else if (strcmp(optarg, "load-save") != 0)
            {
               RARCH_ERR("Invalid argument in --sram-mode.\n");
               print_help(argv[0]);
               rarch_fail(1, "parse_input()");
            }
            break;

#ifdef HAVE_NETPLAY
         case 'H':
            global->has_set_netplay_ip_address = true;
            global->netplay_enable = true;
            *global->netplay_server = '\0';
            break;

         case 'C':
            global->has_set_netplay_ip_address = true;
            global->netplay_enable = true;
            strlcpy(global->netplay_server, optarg,
                  sizeof(global->netplay_server));
            break;

         case 'F':
            global->netplay_sync_frames = strtol(optarg, NULL, 0);
            global->has_set_netplay_delay_frames = true;
            break;
#endif

         case 'U':
            strlcpy(global->ups_name, optarg,
                  sizeof(global->ups_name));
            global->ups_pref = true;
            global->has_set_ups_pref = true;
            break;

         case 'D':
#if defined(_WIN32) && !defined(_XBOX)
            FreeConsole();
#endif
            break;

         case 0: /* options without short variant */
            switch (val)
            {
               case RA_OPT_MENU:
                  global->libretro_dummy = true;
                  break;

#ifdef HAVE_NETPLAY
               case RA_OPT_PORT:
                  global->has_set_netplay_ip_port = true;
                  global->netplay_port = strtoul(optarg, NULL, 0);
                  break;
#endif
               case RA_OPT_NICK:
                  global->has_set_username = true;
                  strlcpy(settings->username, optarg,
                        sizeof(settings->username));
                  break;

#if defined(HAVE_NETWORK_CMD) && defined(HAVE_NETPLAY)
               case RA_OPT_COMMAND:
                  if (network_cmd_send(optarg))
                     exit(0);
                  else
                     rarch_fail(1, "network_cmd_send()");
                  break;
#endif

               case RA_OPT_APPENDCONFIG:
                  strlcpy(global->append_config_path, optarg,
                        sizeof(global->append_config_path));
                  break;

               case RA_OPT_BPS:
                  strlcpy(global->bps_name, optarg,
                        sizeof(global->bps_name));
                  global->bps_pref = true;
                  global->has_set_bps_pref = true;
                  break;

               case RA_OPT_IPS:
                  strlcpy(global->ips_name, optarg,
                        sizeof(global->ips_name));
                  global->ips_pref = true;
                  global->has_set_ips_pref = true;
                  break;

               case RA_OPT_NO_PATCH:
                  global->block_patch = true;
                  break;

               case RA_OPT_SIZE:
               {
                  if (sscanf(optarg, "%ux%u", &global->record.width,
                           &global->record.height) != 2)
                  {
                     RARCH_ERR("Wrong format for --size.\n");
                     print_help(argv[0]);
                     rarch_fail(1, "parse_input()");
                  }
                  break;
               }

               case RA_OPT_RECORDCONFIG:
                  strlcpy(global->record.config, optarg,
                        sizeof(global->record.config));
                  break;

               case RA_OPT_MAX_FRAMES:
                  runloop->frames.video.max = strtoul(optarg, NULL, 10);
                  break;

               case RA_OPT_SUBSYSTEM:
                  strlcpy(global->subsystem, optarg, sizeof(global->subsystem));
                  break;

               case RA_OPT_FEATURES:
                  print_features();
                  exit(0);

               case RA_OPT_EOF_EXIT:
                  break;

               case RA_OPT_VERSION:
                  print_version();
                  exit(0);

#ifdef HAVE_FILE_LOGGER
               case RA_OPT_LOG_FILE:
                  global->log_file = fopen(optarg, "wb");
                  break;
#endif
               default:
                  break;
            }
            break;

         case '?':
            print_help(argv[0]);
            rarch_fail(1, "parse_input()");

         default:
            RARCH_ERR("Error parsing arguments.\n");
            rarch_fail(1, "parse_input()");
      }
   }

   if (global->libretro_dummy)
   {
      if (optind < argc)
      {
         RARCH_ERR("--menu was used, but content file was passed as well.\n");
         rarch_fail(1, "parse_input()");
      }
   }
   else if (!*global->subsystem && optind < argc)
      rarch_set_paths(argv[optind]);
   else if (*global->subsystem && optind < argc)
      set_special_paths(argv + optind, argc - optind);
   else
      set_basename(global->fullpath);

   if (!*global->fullpath)
      global->libretro_no_content = true;

   /* Copy SRM/state dirs used, so they can be reused on reentrancy. */
   if (global->has_set_save_path &&
         path_is_directory(global->savefile_name))
      strlcpy(global->savefile_dir, global->savefile_name,
            sizeof(global->savefile_dir));

   if (global->has_set_state_path &&
         path_is_directory(global->savestate_name))
      strlcpy(global->savestate_dir, global->savestate_name,
            sizeof(global->savestate_dir));
}

static void rarch_init_savefile_paths(void)
{
   global_t *global = global_get_ptr();

   event_command(EVENT_CMD_SAVEFILES_DEINIT);

   global->savefiles = string_list_new();
   rarch_assert(global->savefiles);

   if (*global->subsystem)
   {
      /* For subsystems, we know exactly which RAM types are supported. */

      unsigned i, j;
      const struct retro_subsystem_info *info =
         libretro_find_subsystem_info(
               global->system.special, global->system.num_special,
               global->subsystem);

      /* We'll handle this error gracefully later. */
      unsigned num_content = min(info ? info->num_roms : 0,
            global->subsystem_fullpaths ?
            global->subsystem_fullpaths->size : 0);

      bool use_sram_dir = path_is_directory(global->savefile_dir);

      for (i = 0; i < num_content; i++)
      {
         for (j = 0; j < info->roms[i].num_memory; j++)
         {
            union string_list_elem_attr attr;
            char path[PATH_MAX_LENGTH];
            char ext[32] = {0};
            const struct retro_subsystem_memory_info *mem =
               (const struct retro_subsystem_memory_info*)
               &info->roms[i].memory[j];

            snprintf(ext, sizeof(ext), ".%s", mem->extension);

            if (use_sram_dir)
            {
               /* Redirect content fullpath to save directory. */
               strlcpy(path, global->savefile_dir, sizeof(path));
               fill_pathname_dir(path,
                     global->subsystem_fullpaths->elems[i].data, ext,
                     sizeof(path));
            }
            else
            {
               fill_pathname(path, global->subsystem_fullpaths->elems[i].data,
                     ext, sizeof(path));
            }

            attr.i = mem->type;
            string_list_append(global->savefiles, path, attr);
         }
      }

      /* Let other relevant paths be inferred from the main SRAM location. */
      if (!global->has_set_save_path)
         fill_pathname_noext(global->savefile_name, global->basename, ".srm",
               sizeof(global->savefile_name));
      if (path_is_directory(global->savefile_name))
      {
         fill_pathname_dir(global->savefile_name, global->basename, ".srm",
               sizeof(global->savefile_name));
         RARCH_LOG("Redirecting save file to \"%s\".\n",
               global->savefile_name);
      }
   }
   else
   {
      union string_list_elem_attr attr;
      char savefile_name_rtc[PATH_MAX_LENGTH];

      attr.i = RETRO_MEMORY_SAVE_RAM;
      string_list_append(global->savefiles, global->savefile_name, attr);

      /* Infer .rtc save path from save ram path. */
      attr.i = RETRO_MEMORY_RTC;
      fill_pathname(savefile_name_rtc,
            global->savefile_name, ".rtc", sizeof(savefile_name_rtc));
      string_list_append(global->savefiles, savefile_name_rtc, attr);
   }
}

void rarch_fill_pathnames(void)
{
   global_t   *global   = global_get_ptr();

   rarch_init_savefile_paths();

   if (!*global->basename)
      return;

   if (!*global->ups_name)
      fill_pathname_noext(global->ups_name, global->basename, ".ups",
            sizeof(global->ups_name));
   if (!*global->bps_name)
      fill_pathname_noext(global->bps_name, global->basename, ".bps",
            sizeof(global->bps_name));
   if (!*global->ips_name)
      fill_pathname_noext(global->ips_name, global->basename, ".ips",
            sizeof(global->ips_name));
}

static bool init_state(void)
{
   driver_t *driver     = driver_get_ptr();
   if (!driver)
      return false;

   driver->video_active = true;
   driver->audio_active = true;

   return true;
}

static void main_clear_state_drivers(void)
{
   global_t *global = global_get_ptr();
   bool inited      = false;
   if (!global)
      return;
   inited           = global->main_is_init;
   if (!inited)
      return;

   event_command(EVENT_CMD_DRIVERS_DEINIT);
   event_command(EVENT_CMD_DRIVERS_INIT);
}

static void main_init_state_config(void)
{
   unsigned i;
   settings_t *settings = config_get_ptr();

   if (!settings)
      return;

   for (i = 0; i < MAX_USERS; i++)
      settings->input.libretro_device[i] = RETRO_DEVICE_JOYPAD;
}

void rarch_main_alloc(void)
{
   settings_t *settings = config_get_ptr();

   if (settings)
      config_free();
   settings = config_init();

   if (!settings)
      return;
   
   rarch_main_clear_state();
   rarch_main_data_clear_state();
}

/**
 * rarch_main_new:
 *
 * Will teardown drivers and clears all 
 * internal state of RetroArch.
 * If @inited is true, will initialize all
 * drivers again after teardown.
 **/
void rarch_main_new(void)
{
   main_clear_state_drivers();
   init_state();
   main_init_state_config();

   event_command(EVENT_CMD_MSG_QUEUE_INIT);
}

void rarch_main_free(void)
{
   event_command(EVENT_CMD_MSG_QUEUE_DEINIT);
   event_command(EVENT_CMD_DRIVERS_DEINIT);
   event_command(EVENT_CMD_LOG_FILE_DEINIT);

   rarch_main_state_free();
   rarch_main_global_free();
   config_free();
}

#ifdef HAVE_ZLIB
#define DEFAULT_EXT "zip"
#else
#define DEFAULT_EXT ""
#endif

static void init_system_info(void)
{
   global_t *global               = global_get_ptr();
   struct retro_system_info *info = (struct retro_system_info*)
      global ? &global->system.info : NULL;

   pretro_get_system_info(info);

   if (!info->library_name)
      info->library_name = "Unknown";
   if (!info->library_version)
      info->library_version = "v0";

#ifdef RARCH_CONSOLE
   snprintf(global->title_buf, sizeof(global->title_buf), "%s %s",
         info->library_name, info->library_version);
#else
   snprintf(global->title_buf, sizeof(global->title_buf),
         RETRO_FRONTEND " : %s %s",
         info->library_name, info->library_version);
#endif
   strlcpy(global->system.valid_extensions, info->valid_extensions ?
         info->valid_extensions : DEFAULT_EXT,
         sizeof(global->system.valid_extensions));
   global->system.block_extract = info->block_extract;
}

/* 
 * rarch_verify_api_version:
 *
 * Compare libretro core API version against API version
 * used by RetroArch.
 *
 * TODO - when libretro v2 gets added, allow for switching
 * between libretro version backend dynamically.
 **/
void rarch_verify_api_version(void)
{

   RARCH_LOG("Version of libretro API: %u\n", pretro_api_version());
   RARCH_LOG("Compiled against API: %u\n", RETRO_API_VERSION);

   if (pretro_api_version() != RETRO_API_VERSION)
      RARCH_WARN(RETRO_LOG_LIBRETRO_ABI_BREAK);
}

#define FAIL_CPU(simd_type) do { \
   RARCH_ERR(simd_type " code is compiled in, but CPU does not support this feature. Cannot continue.\n"); \
   rarch_fail(1, "validate_cpu_features()"); \
} while(0)

/* validate_cpu_features:
 *
 * Validates CPU features for given processor architecture.
 *
 * Make sure we haven't compiled for something we cannot run.
 * Ideally, code would get swapped out depending on CPU support, 
 * but this will do for now.
 */
static void validate_cpu_features(void)
{
   uint64_t cpu = rarch_get_cpu_features();
   (void)cpu;

#ifdef __SSE__
   if (!(cpu & RETRO_SIMD_SSE))
      FAIL_CPU("SSE");
#endif
#ifdef __SSE2__
   if (!(cpu & RETRO_SIMD_SSE2))
      FAIL_CPU("SSE2");
#endif
#ifdef __AVX__
   if (!(cpu & RETRO_SIMD_AVX))
      FAIL_CPU("AVX");
#endif
}

/**
 * rarch_init_system_av_info:
 *
 * Initialize system A/V information by calling the libretro core's
 * get_system_av_info function.
 **/
void rarch_init_system_av_info(void)
{
   runloop_t *runloop = rarch_main_get_ptr();
   struct retro_system_av_info *av_info = 
      video_viewport_get_system_av_info();

   pretro_get_system_av_info(av_info);
   runloop->frames.limit.last_time = rarch_get_time_usec();
}

/**
 * rarch_main_init:
 * @argc                 : Count of (commandline) arguments.
 * @argv                 : (Commandline) arguments. 
 *
 * Initializes RetroArch.
 *
 * Returns: 0 on success, otherwise 1 if there was an error.
 **/
int rarch_main_init(int argc, char *argv[])
{
   int sjlj_ret;
   global_t *global = global_get_ptr();

   init_state();

   if ((sjlj_ret = setjmp(global->error_sjlj_context)) > 0)
   {
      RARCH_ERR("Fatal error received in: \"%s\"\n", global->error_string);
      return sjlj_ret;
   }
   global->error_in_init = true;
   global->log_file = stderr;
   parse_input(argc, argv);

   if (global->verbosity)
   {
      char str[PATH_MAX_LENGTH];

      RARCH_LOG_OUTPUT("=== Build =======================================");
      str[0] = '\0';
      rarch_info_get_capabilities(RARCH_CAPABILITIES_CPU, str, sizeof(str));
      fprintf(stderr, "%s", str);
      fprintf(stderr, "Built: %s\n", __DATE__);
      RARCH_LOG_OUTPUT("Version: %s\n", PACKAGE_VERSION);
#ifdef HAVE_GIT_VERSION
      RARCH_LOG_OUTPUT("Git: %s\n", rarch_git_version);
#endif
      RARCH_LOG_OUTPUT("=================================================\n");
   }

   validate_cpu_features();
   config_load();

   init_libretro_sym(global->libretro_dummy);
   init_system_info();

   init_drivers_pre();

   if (!event_command(EVENT_CMD_CORE_INIT))
   {
      init_libretro_sym(global->libretro_dummy = true);
      event_command(EVENT_CMD_CORE_INIT);
   }

   event_command(EVENT_CMD_DRIVERS_INIT);
   event_command(EVENT_CMD_COMMAND_INIT);
   event_command(EVENT_CMD_REWIND_INIT);
   event_command(EVENT_CMD_CONTROLLERS_INIT);
   event_command(EVENT_CMD_RECORD_INIT);
   event_command(EVENT_CMD_CHEATS_INIT);

   event_command(EVENT_CMD_SAVEFILES_INIT);
#if defined(GEKKO) && defined(HW_RVL)
   {
      settings_t *settings = config_get_ptr();

      if (settings)
      {
         event_command(EVENT_CMD_VIDEO_SET_ASPECT_RATIO);
         video_driver_set_aspect_ratio(settings->video.aspect_ratio_idx);
      }
   }
#endif

   global->error_in_init = false;
   global->main_is_init  = true;
   return 0;
}

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
      int *argc, char **argv)
{
#ifdef HAVE_FILE_LOGGER
   int i;
#endif

   *argc = 0;
   argv[(*argc)++] = strdup("retroarch");

   if (!args->no_content)
   {
      if (args->content_path)
      {
         RARCH_LOG("Using content: %s.\n", args->content_path);
         argv[(*argc)++] = strdup(args->content_path);
      }
      else
      {
         RARCH_LOG("No content, starting dummy core.\n");
         argv[(*argc)++] = strdup("--menu");
      }
   }

   if (args->sram_path)
   {
      argv[(*argc)++] = strdup("-s");
      argv[(*argc)++] = strdup(args->sram_path);
   }

   if (args->state_path)
   {
      argv[(*argc)++] = strdup("-S");
      argv[(*argc)++] = strdup(args->state_path);
   }

   if (args->config_path)
   {
      argv[(*argc)++] = strdup("-c");
      argv[(*argc)++] = strdup(args->config_path);
   }

#ifdef HAVE_DYNAMIC
   if (args->libretro_path)
   {
      argv[(*argc)++] = strdup("-L");
      argv[(*argc)++] = strdup(args->libretro_path);
      rarch_environment_cb(RETRO_ENVIRONMENT_SET_LIBRETRO_PATH,
                           (void*)args->libretro_path);
   }
#endif

   if (args->verbose)
      argv[(*argc)++] = strdup("-v");

#ifdef HAVE_FILE_LOGGER
   for (i = 0; i < *argc; i++)
      RARCH_LOG("arg #%d: %s\n", i, argv[i]);
#endif
}

void rarch_main_set_state(unsigned cmd)
{
   driver_t *driver     = driver_get_ptr();
   global_t *global     = global_get_ptr();
   settings_t *settings = config_get_ptr();

   switch (cmd)
   {
      case RARCH_ACTION_STATE_MENU_RUNNING:
         {
            menu_handle_t *menu = menu_driver_get_ptr();
            if (!menu)
               return;

            menu_driver_toggle(true);

            /* Menu should always run with vsync on. */
            event_command(EVENT_CMD_VIDEO_SET_BLOCKING_STATE);
            /* Undo any fast-fowarding. */
            if (driver->nonblock_state)
            {
               driver->nonblock_state = false;
               rarch_main_msg_queue_push("", 0, 1, true);
            }
            /* Stop all rumbling before entering the menu. */
            event_command(EVENT_CMD_RUMBLE_STOP);

            if (settings->menu.pause_libretro)
               event_command(EVENT_CMD_AUDIO_STOP);

            /* Override keyboard callback to redirect to menu instead.
             * We'll use this later for something ...
             * FIXME: This should probably be moved to menu_common somehow. */
            if (global)
            {
               global->frontend_key_event = global->system.key_event;
               global->system.key_event   = menu_input_key_event;
               global->system.frame_time_last = 0;
            }

            input_driver_keyboard_mapping_set_block(false);
            menu_entries_set_refresh();
            menu_driver_set_alive();

            /* Prevent stray input from going to menu */
            driver->flushing_input = true;
         }
         break;
      case RARCH_ACTION_STATE_LOAD_CONTENT:
         /* If content loading fails, we go back to menu. */
         if (!menu_load_content())
            rarch_main_set_state(RARCH_ACTION_STATE_MENU_RUNNING);
         if (driver->frontend_ctx && driver->frontend_ctx->content_loaded)
            driver->frontend_ctx->content_loaded();
         break;
      case RARCH_ACTION_STATE_MENU_RUNNING_FINISHED:
         menu_setting_apply_deferred();

         menu_driver_toggle(false);
         menu_driver_unset_alive();

         driver_set_nonblock_state(driver->nonblock_state);

         if (settings && settings->menu.pause_libretro)
            event_command(EVENT_CMD_AUDIO_START);

         /* Prevent stray input from going to libretro core */
         driver->flushing_input = true;

         if (global)
         {
            /* Restore libretro keyboard callback. */
            global->system.key_event = global->frontend_key_event;

            if (global->keyboard_focus)
               input_driver_keyboard_mapping_set_block(true);
         }
         video_driver_set_texture_enable(false, false);
         break;
      case RARCH_ACTION_STATE_QUIT:
         if (global)
            global->system.shutdown = true;
         rarch_main_set_state(RARCH_ACTION_STATE_MENU_RUNNING_FINISHED);
         break;
      case RARCH_ACTION_STATE_FORCE_QUIT:
         if (global)
            global->lifecycle_state = 0;
         rarch_main_set_state(RARCH_ACTION_STATE_QUIT);
         break;
      case RARCH_ACTION_STATE_NONE:
      default:
         break;
   }
}

/**
 * rarch_main_deinit:
 *
 * Deinitializes RetroArch.
 **/
void rarch_main_deinit(void)
{
   global_t *global = global_get_ptr();

   event_command(EVENT_CMD_NETPLAY_DEINIT);
   event_command(EVENT_CMD_COMMAND_DEINIT);

   if (global->use_sram)
      event_command(EVENT_CMD_AUTOSAVE_DEINIT);

   event_command(EVENT_CMD_RECORD_DEINIT);
   event_command(EVENT_CMD_SAVEFILES);

   event_command(EVENT_CMD_REWIND_DEINIT);
   event_command(EVENT_CMD_CHEATS_DEINIT);

   event_command(EVENT_CMD_AUTOSAVE_STATE);

   event_command(EVENT_CMD_CORE_DEINIT);

   event_command(EVENT_CMD_OVERLAY_FREE_CACHED);

   event_command(EVENT_CMD_TEMPORARY_CONTENT_DEINIT);
   event_command(EVENT_CMD_SUBSYSTEM_FULLPATHS_DEINIT);
   event_command(EVENT_CMD_SAVEFILES_DEINIT);

   global->main_is_init = false;
}

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
int rarch_defer_core(core_info_list_t *core_info, const char *dir,
      const char *path, const char *menu_label,
      char *s, size_t len)
{
   char new_core_path[PATH_MAX_LENGTH];
   const core_info_t *info             = NULL;
   size_t supported                    = 0;
   settings_t *settings                = config_get_ptr();
   global_t   *global                  = global_get_ptr();
   uint32_t menu_label_hash            = djb2_calculate(menu_label);

   new_core_path[0] = '\0';

   fill_pathname_join(s, dir, path, len);

#ifdef HAVE_COMPRESSION
   if (path_is_compressed_file(dir))
   {
      /* In case of a compressed archive, we have to join with a hash */
      /* We are going to write at the position of dir: */
      rarch_assert(strlen(dir) < strlen(s));
      s[strlen(dir)] = '#';
   }
#endif

   if (core_info)
      core_info_list_get_supported_cores(core_info, s, &info,
            &supported);

   if (menu_label_hash == MENU_LABEL_LOAD_CONTENT)
   {
      info = (const core_info_t*)global->core_info_current;

      if (info)
      {
         strlcpy(new_core_path, info->path, sizeof(new_core_path));
         supported = 1;
      }
   }
   else
      strlcpy(new_core_path, info->path, sizeof(new_core_path));

   /* There are multiple deferred cores and a
    * selection needs to be made from a list, return 0. */
   if (supported != 1)
      return 0;

   strlcpy(global->fullpath, s, sizeof(global->fullpath));

   if (path_file_exists(new_core_path))
      strlcpy(settings->libretro, new_core_path,
            sizeof(settings->libretro));
   return -1;
}

void rarch_update_configs()
{
   settings_t *settings = config_get_ptr();
   global_t   *global   = global_get_ptr();
   
   if (!settings || !global)
      return;

   if (settings->config_save_on_exit)
   {
      if (scoped_settings_touched)
         config_save_scoped_files();
      if (global->system.core_options)
      {
         if (core_options_touched)
            core_option_flush(global->system.core_options);
         core_option_free(global->system.core_options);
         global->system.core_options = NULL;
      }
      if (input_remapping_touched)
         input_remapping_save();
   }

   /* Get new core's libretro_name */
   path_libretro_name(global->libretro_name, settings->libretro);

   /* Restore globals when a core is unloaded */
   if (!*global->libretro_name)
      config_unmask_globals();

   /* Save main config if shutting down */
   if (global->system.shutdown && settings->config_save_on_exit
       && settings_touched && *global->config_path)
      main_config_file_save(global->config_path);
}

bool rarch_clear_all_thread_waits(unsigned clear_threads, void* data)
{
   if (clear_threads > 0)
      event_command(EVENT_CMD_AUDIO_START);
   else
      event_command(EVENT_CMD_AUDIO_STOP);

   return true;
}
