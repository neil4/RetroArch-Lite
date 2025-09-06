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

#ifndef __RETROARCH_DATA_RUNLOOP_H
#define __RETROARCH_DATA_RUNLOOP_H

#include <file/nbio.h>
#include <formats/image.h>
#include <formats/rpng.h>
#include <queues/message_queue.h>
#include <retro_miscellaneous.h>
#ifdef HAVE_THREADS
#include <rthreads/rthreads.h>
#endif
#include "tasks/tasks.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef int (*transfer_cb_t)(void *data, size_t len);

enum runloop_data_type
{
   DATA_TYPE_NONE = 0,
   DATA_TYPE_FILE,
   DATA_TYPE_IMAGE,
   DATA_TYPE_HTTP,
   DATA_TYPE_OVERLAY,
   DATA_TYPE_DB,
};

#ifdef HAVE_NETWORKING

typedef struct http_handle
{
   struct
   {
      struct http_connection_t *handle;
      transfer_cb_t  cb;
      char elem1[PATH_MAX_LENGTH];
      char filename[NAME_MAX_LENGTH];
   } connection;
   msg_queue_t *msg_queue;
   struct http_t *handle;
   transfer_cb_t  cb;
   unsigned status;

   char msg_title[NAME_MAX_LENGTH];
} http_handle_t;
#endif

typedef struct nbio_image_handle
{
   struct texture_image ti;
   bool is_blocking;
   bool is_blocking_on_processing;
   bool is_finished;
   transfer_cb_t  cb;
#ifdef HAVE_RPNG
   struct rpng_t *handle;
#endif
   unsigned processing_pos_increment;
   unsigned pos_increment;
   uint64_t frame_count;
   uint64_t processing_frame_count;
   int processing_final_state;
   msg_queue_t *msg_queue;
   unsigned status;
} nbio_image_handle_t;

typedef struct nbio_handle
{
   nbio_image_handle_t image;
   bool is_finished;
   transfer_cb_t  cb;
   struct nbio_t *handle;
   unsigned pos_increment;
   uint64_t frame_count;
   msg_queue_t *msg_queue;
   unsigned status;
} nbio_handle_t;

typedef struct data_runloop
{
#ifdef HAVE_NETWORKING
   http_handle_t http;
#endif

   nbio_handle_t nbio;
   bool inited;

} data_runloop_t;

void rarch_main_data_msg_queue_push(unsigned type,
      const char *msg, const char *msg2, const char *msg3,
      unsigned prio, unsigned duration, bool flush);

void rarch_main_data_clear_state(void);

void rarch_main_data_iterate(void);

void rarch_main_data_deinit(void);

void rarch_main_data_free(void);

void rarch_main_data_init_queues(void);

bool rarch_main_data_active(data_runloop_t *runloop);

data_runloop_t *rarch_main_data_get_ptr(void);

#ifdef __cplusplus
}
#endif

#endif
