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

#include <stdio.h>
#include <errno.h>
#include <unistd.h>
#include <string.h>
#include <sys/resource.h>

#include <rhash.h>

#include "platform_android.h"

#include "../frontend.h"
#include "../../general.h"
#include "../../retroarch.h"
#include <retro_inline.h>

struct android_app *g_android;
static pthread_key_t thread_key;

static retro_time_t onstop_usec;

static INLINE void android_app_write_cmd(void *data, int8_t cmd)
{
   struct android_app *android_app = (struct android_app*)data;

   if (!android_app)
      return;

   if (write(android_app->msgwrite, &cmd, sizeof(cmd)) != sizeof(cmd))
      RARCH_ERR("Failure writing android_app cmd: %s\n", strerror(errno));
}

static void android_app_set_input(void *data, AInputQueue* inputQueue)
{
   struct android_app *android_app = (struct android_app*)data;

   if (!android_app)
      return;

   slock_lock(android_app->mutex);
   android_app->pendingInputQueue = inputQueue;
   android_app_write_cmd(android_app, APP_CMD_INPUT_CHANGED);

   while (android_app->inputQueue != android_app->pendingInputQueue)
      scond_wait(android_app->cond, android_app->mutex);

   slock_unlock(android_app->mutex);
}

static void android_app_set_window(void *data, ANativeWindow* window)
{
   struct android_app *android_app = (struct android_app*)data;

   if (!android_app)
      return;

   slock_lock(android_app->mutex);
   if (android_app->pendingWindow)
      android_app_write_cmd(android_app, APP_CMD_TERM_WINDOW);

   android_app->pendingWindow = window;

   if (window)
      android_app_write_cmd(android_app, APP_CMD_INIT_WINDOW);

   while (android_app->window != android_app->pendingWindow)
      scond_wait(android_app->cond, android_app->mutex);

   slock_unlock(android_app->mutex);
}

static void android_app_set_activity_state(void *data, int8_t cmd)
{
   struct android_app *android_app = (struct android_app*)data;

   if (!android_app)
      return;

   slock_lock(android_app->mutex);
   android_app_write_cmd(android_app, cmd);
   while (android_app->activityState != cmd
         && android_app->activityState != APP_CMD_DEAD)
      scond_wait(android_app->cond, android_app->mutex);
   slock_unlock(android_app->mutex);

   if (android_app->activityState == APP_CMD_DEAD)
      RARCH_LOG("RetroArch native thread is dead.\n");
}

/*
 * Note: not called on normal exit
 */
static void onDestroy(ANativeActivity* activity)
{
   global_t *global  = global_get_ptr();
   bool save_configs = (rarch_get_time_usec() - onstop_usec > 10000000);

   RARCH_LOG("Destroy: %p\n", activity);

   /* TODO: Want to save only when OS kills app, not when user force closes */
   if (global && save_configs)
   {
      global->system.shutdown = true;
      rarch_update_configs();
   }

   exit(0);
}

static void onStart(ANativeActivity* activity)
{
   RARCH_LOG("Start: %p\n", activity);
   android_app_set_activity_state((struct android_app*)
         activity->instance, APP_CMD_START);
}

static void onResume(ANativeActivity* activity)
{
   RARCH_LOG("Resume: %p\n", activity);
   android_app_set_activity_state((struct android_app*)
         activity->instance, APP_CMD_RESUME);
}

static void onPause(ANativeActivity* activity)
{
   RARCH_LOG("Pause: %p\n", activity);
   android_app_set_activity_state((struct android_app*)
         activity->instance, APP_CMD_PAUSE);
}

static void onStop(ANativeActivity* activity)
{
   RARCH_LOG("Stop: %p\n", activity);
   android_app_set_activity_state((struct android_app*)
         activity->instance, APP_CMD_STOP);
   onstop_usec = rarch_get_time_usec();
}

static void onConfigurationChanged(ANativeActivity *activity)
{
   struct android_app* android_app = (struct android_app*)
      activity->instance;

   if (!android_app)
      return;

   RARCH_LOG("ConfigurationChanged: %p\n", activity);
   android_app_write_cmd(android_app, APP_CMD_CONFIG_CHANGED);
}

static void onWindowFocusChanged(ANativeActivity* activity, int focused)
{
   RARCH_LOG("WindowFocusChanged: %p -- %d\n", activity, focused);
   android_app_write_cmd((struct android_app*)activity->instance,
         focused ? APP_CMD_GAINED_FOCUS : APP_CMD_LOST_FOCUS);
}

static void onNativeWindowCreated(ANativeActivity* activity,
      ANativeWindow* window)
{
   RARCH_LOG("NativeWindowCreated: %p -- %p\n", activity, window);
   android_app_set_window((struct android_app*)activity->instance, window);
}

static void onNativeWindowDestroyed(ANativeActivity* activity,
      ANativeWindow* window)
{
   RARCH_LOG("NativeWindowDestroyed: %p -- %p\n", activity, window);
   android_app_set_window((struct android_app*)activity->instance, NULL);
}

static void onInputQueueCreated(ANativeActivity* activity, AInputQueue* queue)
{
   RARCH_LOG("InputQueueCreated: %p -- %p\n", activity, queue);
   android_app_set_input((struct android_app*)activity->instance, queue);
}

static void onInputQueueDestroyed(ANativeActivity* activity,
      AInputQueue* queue)
{
   RARCH_LOG("InputQueueDestroyed: %p -- %p\n", activity, queue);
   android_app_set_input((struct android_app*)activity->instance, NULL);
}

JNIEnv *jni_thread_getenv(void)
{
   JNIEnv *env;
   struct android_app* android_app = (struct android_app*)g_android;
   int status = (*android_app->activity->vm)->
      AttachCurrentThread(android_app->activity->vm, &env, 0);

   if (status < 0)
   {
      RARCH_ERR("jni_thread_getenv: Failed to attach current thread.\n");
      return NULL;
   }
   pthread_setspecific(thread_key, (void*)env);

   return env;
}

static void jni_thread_destruct(void *value)
{
   JNIEnv *env = (JNIEnv*)value;
   struct android_app *android_app = (struct android_app*)g_android;
   RARCH_LOG("jni_thread_destruct()\n");

   if (!env)
      return;

   if (android_app)
      (*android_app->activity->vm)->
         DetachCurrentThread(android_app->activity->vm);
   pthread_setspecific(thread_key, NULL);
}

static void android_app_entry(void *data)
{
   char *argv[1];
   int argc = 0;

   rarch_main(argc, argv, data);
}

/*
 * Native activity interaction (called from main thread)
 **/

void ANativeActivity_onCreate(ANativeActivity* activity,
      void* savedState, size_t savedStateSize)
{
   int msgpipe[2];
   struct android_app* android_app;

   (void)savedState;
   (void)savedStateSize;

   RARCH_LOG("Creating Native Activity: %p\n", activity);
   activity->callbacks->onDestroy = onDestroy;
   activity->callbacks->onStart = onStart;
   activity->callbacks->onResume = onResume;
   activity->callbacks->onSaveInstanceState = NULL;
   activity->callbacks->onPause = onPause;
   activity->callbacks->onStop = onStop;
   activity->callbacks->onConfigurationChanged = onConfigurationChanged;
   activity->callbacks->onLowMemory = NULL;
   activity->callbacks->onWindowFocusChanged = onWindowFocusChanged;
   activity->callbacks->onNativeWindowCreated = onNativeWindowCreated;
   activity->callbacks->onNativeWindowDestroyed = onNativeWindowDestroyed;
   activity->callbacks->onInputQueueCreated = onInputQueueCreated;
   activity->callbacks->onInputQueueDestroyed = onInputQueueDestroyed;

   /* These are set only for the native activity,
    * and are reset when it ends. */
   ANativeActivity_setWindowFlags(activity, AWINDOW_FLAG_KEEP_SCREEN_ON
         | AWINDOW_FLAG_FULLSCREEN, 0);

   if (pthread_key_create(&thread_key, jni_thread_destruct))
      RARCH_ERR("Error initializing pthread_key\n");

   android_app = (struct android_app*)calloc(1, sizeof(*android_app));
   if (!android_app)
   {
      RARCH_ERR("Failed to initialize android_app\n");
      return;
   }

   memset(android_app, 0, sizeof(struct android_app));
   
   android_app->activity = activity;
   android_app->mutex    = slock_new();
   android_app->cond     = scond_new();

   if (pipe(msgpipe))
   {
      RARCH_ERR("could not create pipe: %s.\n", strerror(errno));
      activity->instance = NULL;
   }
   android_app->msgread  = msgpipe[0];
   android_app->msgwrite = msgpipe[1];

   android_app->thread   = sthread_create(android_app_entry, android_app);

   /* Wait for thread to start. */
   slock_lock(android_app->mutex);
   while (!android_app->running)
      scond_wait(android_app->cond, android_app->mutex);
   slock_unlock(android_app->mutex);

   activity->instance = android_app;
}


int system_property_get(const char *name, char *value)
{
   FILE *pipe;
   int length = 0;
   char buffer[PROP_VALUE_MAX];
   char cmd[NAME_MAX_LENGTH];
   char *curpos;

   snprintf(cmd, sizeof(cmd), "getprop %s", name);

   pipe = popen(cmd, "r");

   if (!pipe)
   {
      RARCH_ERR("Could not create pipe.\n");
      return 0;
   }

   curpos = value;
   
   while (!feof(pipe))
   {
      if (fgets(buffer, sizeof(buffer), pipe) != NULL)
      {
         int curlen = strlen(buffer);

         memcpy(curpos, buffer, curlen);

         curpos    += curlen;
         length    += curlen;
      }
   }

   *curpos = '\0';

   pclose(pipe);

   return length;
}

static void frontend_android_get_name(char *s, size_t len)
{
   system_property_get("ro.product.model", s);
}

static void frontend_android_get_version(int32_t *major, int32_t *minor, int32_t *rel)
{
   char os_version_str[PROP_VALUE_MAX] = {0};
   system_property_get("ro.build.version.release", os_version_str);

   *major  = 0;
   *minor  = 0;
   *rel    = 0;

   /* Parse out the OS version numbers from the system properties. */
   if (os_version_str[0])
   {
      /* Try to parse out the version numbers from the string. */
      int num_read = sscanf(os_version_str, "%d.%d.%d", major, minor, rel);

      if (num_read > 0)
      {
         if (num_read < 2)
            *minor = 0;
         if (num_read < 3)
            *rel = 0;
         return;
      }
   }
}

static void frontend_android_get_os(char *s, size_t len, int *major, int *minor)
{
   int rel;

   frontend_android_get_version(major, minor, &rel);

   strlcpy(s, "Android", len);
}

static void frontend_android_get_version_sdk(int32_t *sdk)
{
  char os_version_str[PROP_VALUE_MAX] = {0};
  system_property_get("ro.build.version.sdk", os_version_str);

  *sdk = 0;
  if (os_version_str[0])
  {
    int num_read = sscanf(os_version_str, "%d", sdk);
    (void) num_read;
  }
}

static bool device_is_game_console(const char *name)
{
   if (
         !strcmp(name, "OUYA Console") ||
         !strcmp(name, "GAMEMID_BT") ||
         !strcmp(name, "S7800") ||
         !strcmp(name, "SHIELD")
         )
      return true;

   return false;
}


static void frontend_android_get_environment_settings(int *argc,
      char *argv[], void *data, void *params_data)
{
   int32_t major, minor, rel;
   char device_model[PROP_VALUE_MAX] = {0};
   char device_id[PROP_VALUE_MAX]    = {0};
   char path[PATH_MAX_LENGTH];
   struct rarch_main_wrap      *args = NULL;
   JNIEnv                       *env = NULL;
   jobject                       obj = NULL;
   jstring                      jstr = NULL;
   struct android_app   *android_app = (struct android_app*)data;
   global_t                  *global = global_get_ptr();

   if (!android_app)
      return;

   env = jni_thread_getenv();
   if (!env)
      return;

   args = (struct rarch_main_wrap*)params_data;

   if (args)
   {
      args->touched    = true;
      args->no_content = false;
      args->verbose    = false;
      args->sram_path  = NULL;
      args->state_path = NULL;
   }
   
   frontend_android_get_version(&major, &minor, &rel);

   RARCH_LOG("Android OS version (major : %d, minor : %d, rel : %d)\n", major, minor, rel);

   CALL_OBJ_METHOD(env, obj, android_app->activity->clazz,
         android_app->getIntent);
   RARCH_LOG("Checking arguments passed from intent ...\n");

   /* Config file. */
   CALL_OBJ_METHOD_PARAM(env, jstr, obj, android_app->getStringExtra,
         (*env)->NewStringUTF(env, "CONFIGFILE"));

   if (android_app->getStringExtra && jstr)
   {
      static char config_path[PATH_MAX_LENGTH] = {0};
      const char *argv = NULL;

      argv = (*env)->GetStringUTFChars(env, jstr, 0);

      if (argv && *argv)
         strlcpy(config_path, argv, sizeof(config_path));
      (*env)->ReleaseStringUTFChars(env, jstr, argv);

      RARCH_LOG("Config file: [%s].\n", config_path);
      if (args && *config_path)
         args->config_path = config_path;
   }

   /* Current IME. */
   CALL_OBJ_METHOD_PARAM(env, jstr, obj, android_app->getStringExtra,
         (*env)->NewStringUTF(env, "IME"));

   if (android_app->getStringExtra && jstr)
   {
      const char *argv = (*env)->GetStringUTFChars(env, jstr, 0);

      strlcpy(android_app->current_ime, argv,
            sizeof(android_app->current_ime));
      (*env)->ReleaseStringUTFChars(env, jstr, argv);

      RARCH_LOG("Current IME: [%s].\n", android_app->current_ime);
   }

   CALL_OBJ_METHOD_PARAM(env, jstr, obj, android_app->getStringExtra,
         (*env)->NewStringUTF(env, "USED"));

   if (android_app->getStringExtra && jstr)
   {
      const char *argv = (*env)->GetStringUTFChars(env, jstr, 0);
      bool used = (!strcmp(argv, "false")) ? false : true;

      (*env)->ReleaseStringUTFChars(env, jstr, argv);

      RARCH_LOG("USED: [%s].\n", used ? "true" : "false");
   }

   /* LIBRETRO. */
   CALL_OBJ_METHOD_PARAM(env, jstr, obj, android_app->getStringExtra,
         (*env)->NewStringUTF(env, "LIBRETRO"));

   if (android_app->getStringExtra && jstr)
   {
      static char core_path[PATH_MAX_LENGTH];
      const char *argv = NULL;

      *core_path = '\0';
      argv = (*env)->GetStringUTFChars(env, jstr, 0);
      if (argv && *argv)
         strlcpy(core_path, argv, sizeof(core_path));
      (*env)->ReleaseStringUTFChars(env, jstr, argv);

      RARCH_LOG("Libretro path: [%s].\n", core_path);
      if (args && *core_path)
      {
         args->libretro_path = core_path;

         strlcpy(g_defaults.core_dir, core_path, PATH_MAX_LENGTH);
         path_basedir(g_defaults.core_dir);
         global->core_dir_override = true;
         
         strlcpy(g_defaults.core_info_dir,
                 g_defaults.core_dir, PATH_MAX_LENGTH);
         path_parent_dir(g_defaults.core_info_dir);
         strlcat(g_defaults.core_info_dir, "info", PATH_MAX_LENGTH);
         global->info_dir_override = true;
      }
   }

   /* Content. */
   CALL_OBJ_METHOD_PARAM(env, jstr, obj, android_app->getStringExtra,
         (*env)->NewStringUTF(env, "ROM"));

   if (android_app->getStringExtra && jstr)
   {
      static char rom_path[PATH_MAX_LENGTH];
      const char *argv = NULL;

      *rom_path = '\0';
      argv = (*env)->GetStringUTFChars(env, jstr, 0);

      if (argv && *argv)
         strlcpy(rom_path, argv, sizeof(rom_path));
      (*env)->ReleaseStringUTFChars(env, jstr, argv);

      if (*rom_path)
      {
         RARCH_LOG("Auto-start game %s.\n", rom_path);
         if (args)
            args->content_path = rom_path;
         strlcpy(global->fullpath, rom_path, PATH_MAX_LENGTH);
         global->max_scope = NUM_SETTING_SCOPES-1;
         
         strlcpy(g_defaults.content_dir, rom_path, PATH_MAX_LENGTH);
         path_basedir(g_defaults.content_dir);
         global->content_dir_override = true;
      }
      else
         global->max_scope = THIS_CORE;
   }
   else
   {
      args->no_content = true;
      global->max_scope = THIS_CORE;
   }

   /* Internal Paths. */
   CALL_OBJ_METHOD_PARAM(env, jstr, obj, android_app->getStringExtra,
         (*env)->NewStringUTF(env, "DATADIR"));

   if (android_app->getStringExtra && jstr)
   {
      const char *argv = NULL;

      *path = '\0';
      argv = (*env)->GetStringUTFChars(env, jstr, 0);

      if (argv && *argv)
         strlcpy(path, argv, sizeof(path));
      (*env)->ReleaseStringUTFChars(env, jstr, argv);

      if (*path)
      {
         RARCH_LOG("Data path: [%s].\n", path);
         strlcpy(g_android->app_data_dir, path,
               sizeof(g_android->app_data_dir));

         if (args && *path)
         {
            fill_pathname_join(g_defaults.assets_dir, path,
                  "assets", sizeof(g_defaults.assets_dir));
            fill_pathname_join(g_defaults.extraction_dir, path,
                  "tmp", sizeof(g_defaults.extraction_dir));
            fill_pathname_join(g_defaults.shader_dir, path,
                  "shaders_glsl", sizeof(g_defaults.shader_dir));
            fill_pathname_join(g_defaults.overlay_dir, path,
                  "overlays", sizeof(g_defaults.overlay_dir));
            fill_pathname_join(g_defaults.osk_overlay_dir, path,
                  "overlays/keyboards", sizeof(g_defaults.osk_overlay_dir));
            fill_pathname_join(g_defaults.autoconfig_dir, path,
                  "autoconfig", sizeof(g_defaults.autoconfig_dir));
            fill_pathname_join(g_defaults.audio_filter_dir, path,
                  "audio_filters", sizeof(g_defaults.audio_filter_dir));
            fill_pathname_join(g_defaults.video_filter_dir, path,
                  "video_filters", sizeof(g_defaults.video_filter_dir));
            fill_pathname_join(g_defaults.menu_theme_dir, path,
                  "themes_rgui", sizeof(g_defaults.menu_theme_dir));
         }
      }
   }

   /* External Paths. */
   CALL_OBJ_METHOD_PARAM(env, jstr, obj, android_app->getStringExtra,
         (*env)->NewStringUTF(env, "EXTDIR"));

   if (android_app->getStringExtra && jstr)
   {
      const char *argv = NULL;

      *path = '\0';
      argv = (*env)->GetStringUTFChars(env, jstr, 0);

      if (argv && *argv)
         strlcpy(path, argv, sizeof(path));
      (*env)->ReleaseStringUTFChars(env, jstr, argv);

      if (*path)
      {
         RARCH_LOG("Ext Storage path: [%s].\n", path);
         if (args && *path)
         {
            fill_pathname_join(g_defaults.savestate_dir, path,
                  "state", sizeof(g_defaults.savestate_dir));
            fill_pathname_join(g_defaults.sram_dir, path,
                  "save", sizeof(g_defaults.sram_dir));
            fill_pathname_join(g_defaults.system_dir, path,
                  "system", sizeof(g_defaults.system_dir));
            fill_pathname_join(g_defaults.menu_config_dir, path,
                  "config", sizeof(g_defaults.menu_config_dir));
            fill_pathname_join(g_defaults.cheats_dir, path,
                  "cheats", sizeof(g_defaults.cheats_dir));
         }
      }
   }

   frontend_android_get_name(device_model, sizeof(device_model));
   system_property_get("ro.product.id", device_id);

   g_defaults.settings.video_threaded_enable = false;

   if (!strcmp(device_model, "GAMEMID_BT"))
      g_defaults.settings.out_latency = 160;
   else if (!strcmp(device_model, "SHIELD"))
      g_defaults.settings.video_refresh_rate = 60.0;
   else if (!strcmp(device_model, "JSS15J"))
      g_defaults.settings.video_refresh_rate = 59.65;

   global->force_fullscreen = true;
}

static void frontend_android_deinit(void *data)
{
   JNIEnv                     *env = NULL;
   struct android_app *android_app = (struct android_app*)data;

   if (!android_app)
      return;

   RARCH_LOG("Deinitializing RetroArch ...\n");
   android_app->activityState = APP_CMD_DEAD;

   env = jni_thread_getenv();

   if (env && android_app->onRetroArchExit)
      CALL_VOID_METHOD(env, android_app->activity->clazz,
            android_app->onRetroArchExit);

   if (android_app->inputQueue)
   {
      RARCH_LOG("Detaching Android input queue looper ...\n");
      AInputQueue_detachLooper(android_app->inputQueue);
   }
}

static void frontend_android_shutdown(bool unused)
{
   (void)unused;
   /* Cleaner approaches don't work sadly. */
   exit(0);
}

bool android_run_events(void *data);

static void frontend_android_init(void *data)
{
   JNIEnv                     *env = NULL;
   ALooper                 *looper = NULL;
   jclass                    class = NULL;
   jobject                     obj = NULL;
   struct android_app* android_app = (struct android_app*)data;
   jmethodID        vol_control_id;

   if (!android_app)
      return;

   looper = (ALooper*)ALooper_prepare(ALOOPER_PREPARE_ALLOW_NON_CALLBACKS);
   ALooper_addFd(looper, android_app->msgread, LOOPER_ID_MAIN,
         ALOOPER_EVENT_INPUT, NULL, NULL);
   android_app->looper = looper;

   slock_lock(android_app->mutex);
   android_app->running = 1;
   scond_broadcast(android_app->cond);
   slock_unlock(android_app->mutex);

   memset(&g_android, 0, sizeof(g_android));
   g_android = (struct android_app*)android_app;

   RARCH_LOG("Waiting for Android Native Window to be initialized ...\n");

   while (!android_app->window)
   {
      if (!android_run_events(android_app))
      {
         frontend_android_deinit(android_app);
         frontend_android_shutdown(android_app);
         return;
      }
   }

   RARCH_LOG("Android Native Window initialized.\n");

   env = jni_thread_getenv();
   if (!env)
      return;

   GET_OBJECT_CLASS(env, class, android_app->activity->clazz);
   GET_METHOD_ID(env, android_app->getIntent, class,
         "getIntent", "()Landroid/content/Intent;");
   GET_METHOD_ID(env, android_app->onRetroArchExit, class,
         "onRetroArchExit", "()V");
   GET_METHOD_ID(env, android_app->hapticFeedback, class,
         "hapticFeedback", "(I)V");
   GET_METHOD_ID(env, vol_control_id, class,
         "setVolumeControlStream", "(I)V" );
   GET_METHOD_ID(env, android_app->getVolumePaths, class,
         "getVolumePaths", "(C)Ljava/lang/String;");
   CALL_OBJ_METHOD(env, obj, android_app->activity->clazz,
         android_app->getIntent);

   GET_OBJECT_CLASS(env, class, obj);
   GET_METHOD_ID(env, android_app->getStringExtra, class,
         "getStringExtra", "(Ljava/lang/String;)Ljava/lang/String;");

   /* set volume control to media */
   CALL_VOID_METHOD_PARAM(env, android_app->activity->clazz,
         vol_control_id, (jint)3);
}

static int frontend_android_get_rating(void)
{
   char device_model[PROP_VALUE_MAX] = {0};
   frontend_android_get_name(device_model, sizeof(device_model));

   RARCH_LOG("ro.product.model: (%s).\n", device_model);

   if (!strcmp(device_model, "GT-I9505"))
      return 12;
   else if (!strcmp(device_model, "SHIELD"))
      return 13;
   return -1;
}

#define ANDROID_ARCH_ARMV7    0x26257a91U
#define ANDROID_ARCH_ARM      0x406a3516U
#define ANDROID_ARCH_MIPS     0x7c9aa25eU
#define ANDROID_ARCH_X86      0x0b88b8cbU

static enum frontend_architecture frontend_android_get_architecture(void)
{
   uint32_t abi_hash;
   char abi[PROP_VALUE_MAX] = {0};
   system_property_get("ro.product.cpu.abi", abi);

   abi_hash = djb2_calculate(abi);

   switch (abi_hash)
   {
      case ANDROID_ARCH_ARMV7:
         return FRONTEND_ARCH_ARM;
      case ANDROID_ARCH_ARM:
         return FRONTEND_ARCH_ARM;
      case ANDROID_ARCH_MIPS:
         return FRONTEND_ARCH_MIPS;
      case ANDROID_ARCH_X86:
         return FRONTEND_ARCH_X86;
   }

   return FRONTEND_ARCH_NONE;
}

static int frontend_android_parse_drive_list(void *data)
{
   file_list_t *list = (file_list_t*)data;
   JNIEnv *env       = NULL;
   jstring jstr      = NULL;
   const char *cstr  = NULL;
   char *paths       = NULL;

   env = jni_thread_getenv();
   if (!env)
      return 0;

   /* Get storage volume list, delimited by '|' */
   CALL_OBJ_METHOD_PARAM(env, jstr, g_android->activity->clazz,
         g_android->getVolumePaths, '|');
   paths = strdup(cstr = (*env)->GetStringUTFChars(env, jstr, 0));
   (*env)->ReleaseStringUTFChars(env, jstr, cstr);

   /* Push list to menu if not empty */
   if (*paths)
   {
      char *tok;
      char *save = NULL;

      for (tok = strtok_r(paths, "|", &save); tok;
            tok = strtok_r(NULL, "|", &save))
         menu_list_push(list, tok, "", MENU_FILE_DIRECTORY, 0, 0);
   }
   else  /* Fall back to hardcoded list */
   {
      menu_list_push(list, "/storage/emulated/0",
            "", MENU_FILE_DIRECTORY, 0, 0);
      menu_list_push(list, "/storage",
            "", MENU_FILE_DIRECTORY, 0, 0);
   }

   if (*g_android->app_data_dir)
      menu_list_push(list, g_android->app_data_dir,
            "", MENU_FILE_DIRECTORY, 0, 0);

   free(paths);
   return 0;
}

const frontend_ctx_driver_t frontend_ctx_android = {
   frontend_android_get_environment_settings,
   frontend_android_init,
   frontend_android_deinit,
   NULL,                         /* exitspawn */
   NULL,                         /* process_args */
   NULL,                         /* exec */
   NULL,                         /* set_fork */
   frontend_android_shutdown,
   frontend_android_get_name,
   frontend_android_get_os,
   frontend_android_get_rating,
   NULL,                         /* load_content */
   frontend_android_get_architecture,
   NULL,                         /* get_powerstate */
   frontend_android_parse_drive_list,
   NULL,                         /* attach_console */
   NULL,                         /* detach_console */
   "android",
};
