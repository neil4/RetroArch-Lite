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
#include "runloop_data.h"
#include "general.h"

#ifdef HAVE_OVERLAY
#include "input/input_overlay.h"
#endif

#ifdef HAVE_THREADS
#include <rthreads/rthreads.h>
#endif

#include "menu/menu.h"
#include "menu/menu_entries.h"
#include "menu/menu_input.h"

#include <string/stdstring.h>

static struct data_runloop *g_data_runloop;

data_runloop_t *rarch_main_data_get_ptr(void)
{
   return g_data_runloop;
}

void rarch_main_data_deinit(void)
{
   data_runloop_t *runloop = rarch_main_data_get_ptr();

   if (!runloop)
      return;

   runloop->inited = false;
}

void rarch_main_data_free(void)
{
   data_runloop_t *runloop = rarch_main_data_get_ptr();

   if (runloop)
      free(runloop);
   runloop = NULL;
}

static void data_runloop_iterate(data_runloop_t *runloop)
{
   rarch_main_data_nbio_iterate(runloop);
#ifdef HAVE_RPNG
   rarch_main_data_nbio_image_iterate(runloop);
#endif
#ifdef HAVE_OVERLAY
   rarch_main_data_overlay_iterate(runloop);
#endif
#ifdef HAVE_NETWORKING
   rarch_main_data_http_iterate(runloop);
#endif
}


bool rarch_main_data_active(data_runloop_t *runloop)
{
   bool                  image_active, nbio_active, http_active,
                         http_conn_active, overlay_active;
   bool                  active = false;

   driver_t             *driver = driver_get_ptr();
   nbio_handle_t          *nbio = runloop ? &runloop->nbio : NULL;
#ifdef HAVE_RPNG
   nbio_image_handle_t   *image = nbio ? &nbio->image : NULL;
#endif
#ifdef HAVE_NETWORKING
   http_handle_t          *http = runloop ? &runloop->http : NULL;
   struct http_connection_t *http_conn = http ? http->connection.handle : NULL;
#endif

#ifdef HAVE_OVERLAY
   overlay_active               = driver && driver->overlay && 
      (driver->overlay->state != OVERLAY_STATUS_ALIVE 
       && driver->overlay->state != OVERLAY_STATUS_NONE);
   active                       = active || overlay_active;
#endif
#ifdef HAVE_RPNG
   image_active                 = image && image->handle != NULL;
   active                       = active || image_active;
#endif
   nbio_active                  = nbio->handle != NULL;
   active                       = active || nbio_active;
#ifdef HAVE_NETWORKING
   http_active                  = http && http->handle != NULL;
   active                       = active || http_active;
   http_conn_active             = http_conn != NULL;
   active                       = active || http_conn_active;
#endif

   (void)active;
   (void)image_active;
   (void)nbio_active;
   (void)http_active;
   (void)http_conn_active;
   (void)overlay_active;

#if 0
   RARCH_LOG("runloop nbio : %d, image: %d, http: %d, http conn: %d, overlay: %d\n", nbio_active, image_active,
         http_active, http_conn_active, overlay_active);
   RARCH_LOG("active: %d\n", active);
#endif

   return active;
}

void rarch_main_data_iterate(void)
{
   data_runloop_t *runloop      = rarch_main_data_get_ptr();
   settings_t     *settings     = config_get_ptr();
   
   (void)settings;

#ifdef HAVE_RPNG
   rarch_main_data_nbio_image_upload_iterate(runloop);
#endif
   menu_entries_refresh();
   
   data_runloop_iterate(runloop);
}

static data_runloop_t *rarch_main_data_new(void)
{
   data_runloop_t *runloop = (data_runloop_t*)calloc(1, sizeof(data_runloop_t));

   if (!runloop)
      return NULL;

   runloop->inited = true;

   return runloop;
}

void rarch_main_data_clear_state(void)
{
   rarch_main_data_deinit();
   rarch_main_data_free();
   g_data_runloop = rarch_main_data_new();
}

void rarch_main_data_init_queues(void)
{
   data_runloop_t *runloop = rarch_main_data_get_ptr();
#ifdef HAVE_NETWORKING
   if (!runloop->http.msg_queue)
      rarch_assert(runloop->http.msg_queue       = msg_queue_new(8));
#endif
   if (!runloop->nbio.msg_queue)
      rarch_assert(runloop->nbio.msg_queue       = msg_queue_new(8));
   if (!runloop->nbio.image.msg_queue)
      rarch_assert(runloop->nbio.image.msg_queue = msg_queue_new(8));
}

void rarch_main_data_msg_queue_push(unsigned type,
      const char *msg, const char *msg2, const char *msg3,
      unsigned prio, unsigned duration, bool flush)
{
   char *new_msg           = string_alloc(PATH_MAX_LENGTH);
   msg_queue_t *queue      = NULL;
   data_runloop_t *runloop = rarch_main_data_get_ptr();

   switch(type)
   {
      case DATA_TYPE_NONE:
         break;
      case DATA_TYPE_FILE:
         queue = runloop->nbio.msg_queue;
         snprintf(new_msg, PATH_MAX_LENGTH, "%s|%s", msg, msg2);
         break;
      case DATA_TYPE_IMAGE:
         queue = runloop->nbio.image.msg_queue;
         snprintf(new_msg, PATH_MAX_LENGTH, "%s|%s", msg, msg2);
         break;
#ifdef HAVE_NETWORKING
      case DATA_TYPE_HTTP:
         queue = runloop->http.msg_queue;
         snprintf(new_msg, PATH_MAX_LENGTH, "%s|%s|%s", msg, msg2, msg3);
         break;
#endif
#ifdef HAVE_OVERLAY
      case DATA_TYPE_OVERLAY:
         snprintf(new_msg, PATH_MAX_LENGTH, "%s|%s", msg, msg2);
         break;
#endif
   }

   if (!queue)
      goto end;

   if (flush)
      msg_queue_clear(queue);
   msg_queue_push(queue, new_msg, prio, duration);

end:
   free(new_msg);
}
