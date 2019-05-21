/*  RetroArch - A frontend for libretro.
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

#include <retro_miscellaneous.h>
#include <net/net_http.h>
#include <queues/message_queue.h>
#include <string/string_list.h>
#include <compat/strl.h>
#include <file/file_path.h>
#include <rhash.h>

#include "../file_ops.h"
#include "../general.h"
#include "../runloop_data.h"
#include "../menu/menu.h"
#include "../menu/menu_hash.h"
#include "tasks.h"

#define CB_CORE_UPDATER_DOWNLOAD 0x7412da7dU
#define CB_CORE_UPDATER_LIST     0x32fd4f01U
#define CB_CORE_INFO_DOWNLOAD    0x92551e94U

extern char download_filename[NAME_MAX_LENGTH];

int cb_core_updater_list(void *data_, size_t len);

#ifdef HAVE_ZLIB
static int zlib_extract_core_callback(const char *name, const char *valid_exts,
      const uint8_t *cdata, unsigned cmode, uint32_t csize, uint32_t size,
      uint32_t crc32, void *userdata)
{
   char path[PATH_MAX_LENGTH] = {0};

   /* Make directory */
   fill_pathname_join(path, (const char*)userdata, name, sizeof(path));
   path_basedir(path);

   if (!path_mkdir(path))
   {
      RARCH_ERR("Failed to create directory: %s.\n", path);
      return 0;
   }

   /* Ignore directories. */
   if (name[strlen(name) - 1] == '/' || name[strlen(name) - 1] == '\\')
      return 1;

   fill_pathname_join(path, (const char*)userdata, name, sizeof(path));

   RARCH_LOG("path is: %s, CRC32: 0x%x\n", path, crc32);

   if (!zlib_perform_mode(path, valid_exts,
            cdata, cmode, csize, size, crc32, userdata))
   {
      if (cmode == 0)
      {
         RARCH_ERR("Failed to write file: %s.\n", path);
         return 0;
      }
      goto error;
   }

   return 1;

error:
   RARCH_ERR("Failed to deflate to: %s.\n", path);
   return 0;
}
#endif

static int cb_core_updater_download(void *data, size_t len)
{
   char output_path[PATH_MAX_LENGTH] = {0};
   char buf[PATH_MAX_LENGTH]         = {0};
   settings_t              *settings = config_get_ptr();
   global_t                  *global = global_get_ptr();
   char* substr;

   if (!data)
      return -1;

   fill_pathname_join(output_path, settings->libretro_directory,
         download_filename, sizeof(output_path));

   if (!write_file(output_path, data, len))
      return -1;
   
   snprintf(buf, sizeof(buf), "Download complete: %s.",
         download_filename);

   rarch_main_msg_queue_push(buf, 1, 90, true);

#ifdef HAVE_ZLIB
   if (settings->network.buildbot_auto_extract_archive
       && !strcasecmp(path_get_extension(output_path),"zip"))
   {
      if (!zlib_parse_file(output_path, NULL, zlib_extract_core_callback,
               (void*)settings->libretro_directory))
         RARCH_LOG("Could not process ZIP file.\n");
      remove(output_path);
   }
#endif
   /* refresh installed core list */
   core_info_list_free(global->core_info);
   global->core_info = core_info_list_new(INSTALLED_CORES);
   
   strlcpy(buf, download_filename, NAME_MAX_LENGTH);
   substr = strstr(buf,"_libretro");
   if (substr)
      *substr = '\0';
   core_info_download_queue(buf);
   
   return 0;
}

static int cb_core_info_download(void *data, size_t len)
{
   const char *file_ext              = NULL;
   char output_path[PATH_MAX_LENGTH] = {0};
   char buf[PATH_MAX_LENGTH]         = {0};
   settings_t *settings              = config_get_ptr();
   global_t *global                  = global_get_ptr();

   if (!data)
      return -1;
   
   fill_pathname_join(output_path, settings->libretro_info_path,
         download_filename, sizeof(output_path));

   if (!write_file(output_path, data, len))
      return -1;
   
   snprintf(buf, sizeof(buf), "Download complete: %s.",
         download_filename);

   rarch_main_msg_queue_push(buf, 1, 90, true);

#ifdef HAVE_ZLIB
   file_ext = path_get_extension(download_filename);

   if (settings->network.buildbot_auto_extract_archive
       && !strcasecmp(file_ext,"zip"))
   {
      if (!zlib_parse_file(output_path, NULL, zlib_extract_core_callback,
               (void*)settings->libretro_info_path))
         RARCH_LOG("Could not process ZIP file.\n");
      remove(output_path);
   }
#endif
   
   /* Refresh installed core info */
   core_info_list_free(global->core_info);
   global->core_info = core_info_list_new(INSTALLED_CORES);

   /* Refresh core updater (or core list) menu */
   event_command(EVENT_CMD_MENU_ENTRIES_REFRESH);

   return 0;
}


void core_info_download_queue(const char* libretro_name)
{
#ifdef HAVE_NETWORKING
   char info_path[PATH_MAX_LENGTH] = {0};
   settings_t *settings            = config_get_ptr();

   if (!libretro_name)
   {
      fill_pathname_join(info_path, settings->network.buildbot_assets_url,
                         "frontend/info.zip", PATH_MAX_LENGTH);
      strlcpy(download_filename, "info.zip", NAME_MAX_LENGTH);
   }
   else
   {
      fill_pathname_join(info_path, settings->network.buildbot_assets_url,
                         "frontend/info/", PATH_MAX_LENGTH);
      strlcat(info_path, libretro_name, PATH_MAX_LENGTH);
      strlcat(info_path, "_libretro.info", PATH_MAX_LENGTH);
      
      strlcpy(download_filename, libretro_name, NAME_MAX_LENGTH);
      strlcat(download_filename, "_libretro.info", NAME_MAX_LENGTH);
   }

   rarch_main_data_msg_queue_push(DATA_TYPE_HTTP, info_path,
         "cb_core_info_download", 0, 1, false);
#endif
}

static int rarch_main_data_http_con_iterate_transfer(http_handle_t *http)
{
   if (!net_http_connection_iterate(http->connection.handle))
      return -1;
   return 0;
}

static int rarch_main_data_http_conn_iterate_transfer_parse(http_handle_t *http)
{
   int rv = -1;
   
   if (net_http_connection_done(http->connection.handle))
   {
      if (http->connection.handle && http->connection.cb)
         rv = http->connection.cb(http, 0);
   }
   
   net_http_connection_free(http->connection.handle);

   http->connection.handle = NULL;

   return rv;
}

/**
 * rarch_main_data_http_cancel_transfer:
 *
 * Cancels HTTP transfer
 **/
static void rarch_main_data_http_cancel_transfer(void *data, const char* msg)
{
   http_handle_t *http = (http_handle_t*)data;
   
   menu_entries_unset_nonblocking_refresh();

   net_http_delete(http->handle);
   http->handle = NULL;
   http->status = HTTP_STATUS_POLL;
   
   if (msg)
      rarch_main_msg_queue_push(msg, 1, 180, false);
}

static bool rarch_main_data_http_iterate_transfer_parse(http_handle_t *http)
{
   bool rv = true;
   size_t len = 0;
   char msg[32];
   char *data = (char*)net_http_data(http->handle, &len, false);

   strlcpy(download_filename, http->connection.filename, NAME_MAX_LENGTH);
   if (!http->cb || http->cb(data, len) < 0)
   {
      rv = false;
      /* Notify user unless this is a missing info file, which is common */
      if (strcmp("info", path_get_extension(download_filename)))
      {
         if (http->handle)
            snprintf(msg, sizeof(msg),
                  "Transfer Failed\nStatus %i", net_http_status(http->handle));
         else
            rarch_main_data_http_cancel_transfer(http, "Transfer Failed");
         rarch_main_data_http_cancel_transfer(http, msg);
      }
   }
   
   net_http_delete(http->handle);
   http->handle = NULL;
   return rv;
}

static int cb_http_conn_default(void *data_, size_t len)
{
   http_handle_t *http = (http_handle_t*)data_;

   if (!http)
      return -1;

   http->handle = net_http_new(http->connection.handle);

   if (!http->handle)
   {
      RARCH_ERR("Could not create new HTTP session handle.\n");
      rarch_main_data_http_cancel_transfer(data_, "Connection Failed");
      return -1;
   }

   http->cb     = NULL;

   if (http->connection.elem1[0] != '\0')
   {
      uint32_t label_hash = djb2_calculate(http->connection.elem1);

      switch (label_hash)
      {
         case CB_CORE_UPDATER_DOWNLOAD:
            http->cb = &cb_core_updater_download;
            break;
         case CB_CORE_UPDATER_LIST:
            http->cb = &cb_core_updater_list;
            break;
         case CB_CORE_INFO_DOWNLOAD:
            http->cb = &cb_core_info_download;
            break;
      }
   }

   return 0;
}

/**
 * rarch_main_data_http_iterate_poll:
 *
 * Polls HTTP message queue to see if any new URLs 
 * are pending.
 *
 * If handle is freed, will set up a new http handle. 
 * The transfer will be started on the next frame.
 *
 * Returns: 0 when an URL has been pulled and we will
 * begin transferring on the next frame. Returns -1 if
 * no HTTP URL has been pulled. Do nothing in that case.
 **/
static int rarch_main_data_http_iterate_poll(http_handle_t *http)
{
   char elem0[PATH_MAX_LENGTH]  = {0};
   struct string_list *str_list = NULL;
   const char *url              = msg_queue_pull(http->msg_queue);

   if (!url)
      return -1;

   /* Can only deal with one HTTP transfer at a time for now */
   if (http->handle)
      return -1; 

   str_list                     = string_split(url, "|");

   if (!str_list)
      return -1;

   if (str_list->size > 0)
      strlcpy(elem0, str_list->elems[0].data, sizeof(elem0));

   http->connection.handle = net_http_connection_new(elem0);

   if (!http->connection.handle)
      return -1;

   http->connection.cb     = &cb_http_conn_default;

   if (str_list->size > 1)
      strlcpy(http->connection.elem1,
            str_list->elems[1].data,
            sizeof(http->connection.elem1));
   
   if (str_list->size > 2)
      strlcpy(http->connection.filename,
              str_list->elems[2].data, NAME_MAX_LENGTH);

   string_list_free(str_list);
   
   return 0;
}

/**
 * rarch_main_data_http_iterate_transfer:
 *
 * Resumes HTTP transfer update.
 *
 * Returns: 0 when finished, -1 when we should continue
 * with the transfer on the next frame.
 **/
static int rarch_main_data_http_iterate_transfer(void *data)
{
   http_handle_t *http  = (http_handle_t*)data;
   settings_t *settings = config_get_ptr();
   size_t pos = 0, tot = 0;
   int percent = 0;
   static bool in_progress;
   static size_t stall_frames;
   char tmp[NAME_MAX_LENGTH];
   
   /* Allow canceling stalled downloads */
   if (menu_driver_alive() && stall_frames > 60
       && settings && input_driver_key_pressed(settings->menu_cancel_btn))
   {
      snprintf(tmp, sizeof(tmp),
               "Download Canceled: %s", http->connection.filename);
      rarch_main_data_http_cancel_transfer(http, tmp);
      return -1;
   }
   
   if (!net_http_update(http->handle, &pos, &tot))
   {
      if(tot != 0)
         percent=(unsigned long long)pos*100/(unsigned long long)tot;
      else
         percent=0;
         
      if (tot > 0)
      {
         snprintf(tmp, sizeof(tmp), "Download progress: %d%%", percent);
         data_runloop_osd_msg(tmp, sizeof(tmp)); 
         in_progress = true;
         return 1;
      }
      else
      {
         if (in_progress)
            rarch_main_data_http_cancel_transfer(http, "Download Interrupted");
         else if (stall_frames++ > 60)
         {
            snprintf(tmp, sizeof(tmp), "Waiting for Connection...");
            data_runloop_osd_msg(tmp, sizeof(tmp));
         }
         in_progress = false;
         return -1;
      }
   }

   stall_frames = 0;
   in_progress = false;
   return 0;
}

void rarch_main_data_http_iterate(void *data)
{
   data_runloop_t *runloop = (data_runloop_t*)data;
   http_handle_t *http = runloop ? &runloop->http : NULL;
   if (!http)
      return;

   switch (http->status)
   {
      case HTTP_STATUS_CONNECTION_TRANSFER_PARSE:
         if (!rarch_main_data_http_conn_iterate_transfer_parse(http))
            http->status = HTTP_STATUS_TRANSFER;
         break;
      case HTTP_STATUS_CONNECTION_TRANSFER:
         if (!rarch_main_data_http_con_iterate_transfer(http))
            http->status = HTTP_STATUS_CONNECTION_TRANSFER_PARSE;
         break;
      case HTTP_STATUS_TRANSFER_PARSE:
         rarch_main_data_http_iterate_transfer_parse(http);
         http->status = HTTP_STATUS_POLL;
         break;
      case HTTP_STATUS_TRANSFER:
         if (!rarch_main_data_http_iterate_transfer(http))
            http->status = HTTP_STATUS_TRANSFER_PARSE;
         break;
      case HTTP_STATUS_POLL:
      default:
         if (rarch_main_data_http_iterate_poll(http) == 0)
            http->status = HTTP_STATUS_CONNECTION_TRANSFER;
         break;
   }
}
