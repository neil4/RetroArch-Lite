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

#ifndef _PLATFORM_ANDROID_H
#define _PLATFORM_ANDROID_H

#include <jni.h>
#include <poll.h>
#include <sched.h>

#include <android/looper.h>
#include <android/native_activity.h>
#include <android/window.h>
#include <android/sensor.h>

#include <rthreads/rthreads.h>

struct android_app
{
   ANativeActivity* activity;
   ALooper* looper;
   AInputQueue* inputQueue;
   AInputQueue* pendingInputQueue;
   ANativeWindow* window;
   ANativeWindow* pendingWindow;
   slock_t *mutex;
   scond_t *cond;
   int activityState;
   int msgread;
   int msgwrite;
   int running;
   unsigned accelerometer_event_rate;
   const ASensor* accelerometerSensor;
   uint64_t sensor_state_mask;
   sthread_t *thread;
   char current_ime[NAME_MAX_LENGTH];
   char app_data_dir[NAME_MAX_LENGTH];
   jmethodID getIntent;
   jmethodID onRetroArchExit;
   jmethodID hapticFeedback;
   jmethodID getVolumePaths;
   jmethodID getStringExtra;
   jmethodID clearPendingIntent;
   jmethodID hasPendingIntent;
   jmethodID getPendingIntentConfigPath;
   jmethodID getPendingIntentLibretroPath;
   jmethodID getPendingIntentFullPath;
   jmethodID getPendingIntentIME;
};

enum
{
   LOOPER_ID_MAIN = 1,
   LOOPER_ID_INPUT,
   LOOPER_ID_USER,
   LOOPER_ID_INPUT_MSG,
};

enum
{
   APP_CMD_INPUT_CHANGED,
   /**
    * Command from main thread: a new ANativeWindow is ready for use.  Upon
    * receiving this command, android_app->window will contain the new window
    * surface.
    */
   APP_CMD_INIT_WINDOW,

   /**
    * Command from main thread: the existing ANativeWindow needs to be
    * terminated.  Upon receiving this command, android_app->window still
    * contains the existing window; after calling android_app_exec_cmd
    * it will be set to NULL.
    */
   APP_CMD_TERM_WINDOW,

   /**
    * Command from main thread: the current ANativeWindow has been resized.
    * Please redraw with its new size.
    */
   APP_CMD_WINDOW_RESIZED,

   /**
    * Command from main thread: the system needs that the current ANativeWindow
    * be redrawn.  You should redraw the window before handing this to
    * android_app_exec_cmd() in order to avoid transient drawing glitches.
    */
   APP_CMD_WINDOW_REDRAW_NEEDED,

   /**
    * Command from main thread: the content area of the window has changed,
    * such as from the soft input window being shown or hidden.  You can
    * find the new content rect in android_app::contentRect.
    */
   APP_CMD_CONTENT_RECT_CHANGED,

   /**
    * Command from main thread: the app's activity window has gained
    * input focus.
    */
   APP_CMD_GAINED_FOCUS,

   /**
    * Command from main thread: the app's activity window has lost
    * input focus.
    */
   APP_CMD_LOST_FOCUS,

   /**
    * Command from main thread: the current device configuration has changed.
    */
   APP_CMD_CONFIG_CHANGED,

   /**
    * Command from main thread: the system is running low on memory.
    * Try to reduce your memory use.
    */
   APP_CMD_LOW_MEMORY,

   /**
    * Command from main thread: the app's activity has been started.
    */
   APP_CMD_START,

   /**
    * Command from main thread: the app's activity has been resumed.
    */
   APP_CMD_RESUME,

   /**
    * Command from main thread: the app should generate a new saved state
    * for itself, to restore from later if needed.  
    */
   APP_CMD_SAVE_STATE,

   /**
    * Command from main thread: the app's activity has been paused.
    */
   APP_CMD_PAUSE,

   /**
    * Command from main thread: the app's activity has been stopped.
    */
   APP_CMD_STOP,

   /**
    * Command from main thread: the app's activity is being destroyed,
    * and waiting for the app thread to clean up and exit before proceeding.
    */
   APP_CMD_DESTROY,

   /**
    * Set by thread when it will no longer reply to commands.
    */
   APP_CMD_DEAD,
};

#define JNI_EXCEPTION(env) \
   if ((*env)->ExceptionOccurred(env)) \
   { \
      (*env)->ExceptionDescribe(env); \
      (*env)->ExceptionClear(env); \
   }

#define FIND_CLASS(env, var, classname) \
   var = (*env)->FindClass(env, classname); \
   JNI_EXCEPTION(env)

#define GET_OBJECT_CLASS(env, var, clazz_obj) \
   var = (*env)->GetObjectClass(env, clazz_obj); \
   JNI_EXCEPTION(env)

#define GET_FIELD_ID(env, var, clazz, fieldName, fieldDescriptor) \
   var = (*env)->GetFieldID(env, clazz, fieldName, fieldDescriptor); \
   JNI_EXCEPTION(env)

#define GET_METHOD_ID(env, var, clazz, methodName, fieldDescriptor) \
   var = (*env)->GetMethodID(env, clazz, methodName, fieldDescriptor); \
   JNI_EXCEPTION(env)

#define GET_STATIC_METHOD_ID(env, var, clazz, methodName, fieldDescriptor) \
   var = (*env)->GetStaticMethodID(env, clazz, methodName, fieldDescriptor); \
   JNI_EXCEPTION(env)

#define CALL_OBJ_METHOD(env, var, clazz_obj, methodId) \
   var = (*env)->CallObjectMethod(env, clazz_obj, methodId); \
   JNI_EXCEPTION(env)

#define CALL_OBJ_STATIC_METHOD(env, var, clazz, methodId) \
   var = (*env)->CallStaticObjectMethod(env, clazz, methodId); \
   JNI_EXCEPTION(env)

#define CALL_LONG_STATIC_METHOD(env, var, clazz, methodId) \
   var = (*env)->CallStaticLongMethod(env, clazz, methodId); \
   JNI_EXCEPTION(env)

#define CALL_OBJ_STATIC_METHOD_PARAM(env, var, clazz, methodId, ...) \
   var = (*env)->CallStaticObjectMethod(env, clazz, methodId, __VA_ARGS__); \
   JNI_EXCEPTION(env)

#define CALL_OBJ_METHOD_PARAM(env, var, clazz_obj, methodId, ...) \
   var = (*env)->CallObjectMethod(env, clazz_obj, methodId, __VA_ARGS__); \
   JNI_EXCEPTION(env)

#define CALL_VOID_METHOD(env, clazz_obj, methodId) \
   (*env)->CallVoidMethod(env, clazz_obj, methodId); \
   JNI_EXCEPTION(env)

#define CALL_VOID_METHOD_PARAM(env, clazz_obj, methodId, ...) \
   (*env)->CallVoidMethod(env, clazz_obj, methodId, __VA_ARGS__); \
   JNI_EXCEPTION(env)

#define CALL_BOOLEAN_METHOD(env, var, clazz_obj, methodId) \
   var = (*env)->CallBooleanMethod(env, clazz_obj, methodId); \
   JNI_EXCEPTION(env)

#define CALL_BOOLEAN_METHOD_PARAM(env, var, clazz_obj, methodId, ...) \
   var = (*env)->CallBooleanMethod(env, clazz_obj, methodId, __VA_ARGS__); \
   JNI_EXCEPTION(env)

#define CALL_DOUBLE_METHOD(env, var, clazz_obj, methodId) \
   var = (*env)->CallDoubleMethod(env, clazz_obj, methodId); \
   JNI_EXCEPTION(env)

#define CALL_INT_METHOD(env, var, clazz_obj, methodId) \
   var = (*env)->CallIntMethod(env, clazz_obj, methodId); \
   JNI_EXCEPTION(env)

extern JNIEnv *jni_thread_getenv(void);

extern struct android_app *g_android;

#endif /* _PLATFORM_ANDROID_H */
