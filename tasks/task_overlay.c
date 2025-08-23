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

#ifdef HAVE_THREADS
#include <rthreads/rthreads.h>
#endif

#include "../input/input_overlay.h"
#include "../configuration.h"
#include "../runloop.h"
#include "../runloop_data.h"
#include "tasks.h"

#ifdef HAVE_OVERLAY
static enum overlay_status rarch_main_data_overlay_state(input_overlay_t *ol)
{
   enum overlay_status state;

#ifdef HAVE_THREADS
   if (ol->loader_thread)
   {
      slock_lock(ol->loader_mutex);
      state = ol->state;
      slock_unlock(ol->loader_mutex);
   }
   else
#endif
      state = ol->state;

   return state;
}

void rarch_main_data_overlay_iterate(void *data)
{
   driver_t *driver = NULL;
   input_overlay_t *ol;

   if (rarch_main_is_idle())
      return;

   driver = driver_get_ptr();

   if (!driver || !(ol = driver->overlay))
      return;

#ifdef HAVE_THREADS
   if (ol->loader_busy)
      return;
#endif

   switch (rarch_main_data_overlay_state(ol))
   {
      case OVERLAY_STATUS_NONE:
      case OVERLAY_STATUS_ALIVE:
         break;
      case OVERLAY_STATUS_DEFERRED_LOAD:
         input_overlay_loader_iterate(ol,
               input_overlay_load_overlays);
         break;
      case OVERLAY_STATUS_DEFERRED_LOADING:
         input_overlay_loader_iterate(ol,
               input_overlay_load_overlays_iterate);
         break;
      case OVERLAY_STATUS_DEFERRED_LOADING_RESOLVE:
         input_overlay_loader_iterate(ol,
               input_overlay_load_overlays_resolve_iterate);
         break;
      case OVERLAY_STATUS_DEFERRED_DONE:
         input_overlay_new_done(ol);
         break;
      case OVERLAY_STATUS_DEFERRED_ERROR:
         input_overlay_free(ol);
         driver->overlay = NULL;
         break;
      default:
         break;
   }
}

#endif  /* HAVE_OVERLAY */