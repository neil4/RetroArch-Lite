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

#if defined(_MSC_VER) && !defined(_XBOX)
#pragma comment(lib, "ws2_32")
#endif

#include <stdlib.h>
#include <string.h>
#include <zlib.h>
#include <net/net_compat.h>
#include "netplay.h"
#include "general.h"
#include "autosave.h"
#include "dynamic.h"
#include "intl/intl.h"
#include "tasks/tasks.h"
#include "preempt.h"

struct delta_frame
{
   void *state;

   uint32_t self_crc;
   uint32_t peer_crc;

   uint16_t peer_input_state;
   uint16_t sim_peer_input_state;
   uint16_t self_input_state;

   uint16_t peer_lag;

   bool is_simulated;
};

#define RARCH_DEFAULT_PORT 55435
#define UDP_FRAME_PACKETS 20
#define NETPLAY_BUF_SIZE 10
#define SYNC_PERIOD 300

#define NETPLAY_CMD_ACK 0
#define NETPLAY_CMD_NAK 1
#define NETPLAY_CMD_FLIP_PLAYERS 2
#define NETPLAY_CMD_LOAD_SAVESTATE 3
#define NETPLAY_CMD_RESYNC 4

#define NETPLAY_PREV_PTR(x) ((x) == 0 ? NETPLAY_BUF_SIZE - 1 : (x) - 1)
#define NETPLAY_NEXT_PTR(x) ((x + 1) % NETPLAY_BUF_SIZE)

#define RETRY_MS 500

struct netplay
{
   char nick[32];
   char other_nick[32];
   struct sockaddr_storage other_addr;

   struct retro_callbacks cbs;
   /* TCP connection for state sending, etc. Also used for commands */
   int tcp_fd;
   /* UDP connection for game state updates. */
   int udp_fd;
   /* Which port is governed by netplay (other user)? */
   unsigned port;
   bool has_connection;
   bool is_host;

   /* Buffer of savestates and retropad input states.
    * Each savestate represents the frame start.
    * Each input state is applied to that frame. */
   struct delta_frame *buffer;

   /* Pointer where we are now. */
   size_t self_ptr; 
   /* Points to the last reliable state that self ever had. */
   size_t other_ptr;
   /* Pointer to where we are reading peer input.
    * Generally, other_ptr <= read_ptr <= self_ptr. */
   size_t read_ptr;
   /* A temporary pointer used on replay. */
   size_t tmp_ptr;

   size_t state_size;
   size_t sram_size;

   /* Are we replaying old frames? */
   bool in_replay;
   /* Use netplay-rollback or normal savestates? */
   bool use_rollback_states;

   /* To combat UDP packet loss we also send 
    * old data along with the packets. */
   uint32_t packet_buffer[UDP_FRAME_PACKETS * 3];
   uint32_t frame_count;
   uint32_t read_frame_count;
   uint32_t other_frame_count;
   uint32_t tmp_frame_count;
   struct addrinfo *addr;
   struct sockaddr_storage their_addr;
   bool has_client_addr;

   unsigned timeout_cnt;
   /* Set after sending or receiving a savestate */
   bool need_resync;

   /* User flipping
    * Flipping state. If ptr >= flip_frame, we apply the flip.
    * If not, we apply the opposite, effectively creating a trigger point.
    * To avoid collision we need to make sure our client/host is synced up
    * well after flip_frame before allowing another flip. */
   bool flip;
   uint32_t flip_frame;
};

/**
 * warn_hangup:
 *
 * Warns that netplay has disconnected.
 **/
static void warn_hangup(void)
{
   RARCH_WARN("Netplay has disconnected. Will continue without connection ...\n");
   rarch_main_msg_queue_push("Netplay has disconnected. Will continue without connection.", 0, 480, false);
}

/**
 * netplay_should_skip:
 * @netplay : pointer to netplay object
 *
 * If we're fast-forward replaying to resync, check if we 
 * should actually show frame.
 *
 * Returns: bool (1) if we should skip this frame, otherwise
 * false (0).
 **/
static bool netplay_should_skip(netplay_t *netplay)
{
   return netplay->in_replay && netplay->has_connection;
}

static bool send_chunk(netplay_t *netplay)
{
   const struct sockaddr *addr = NULL;

   if (netplay->addr)
      addr = netplay->addr->ai_addr;
   else if (netplay->has_client_addr)
      addr = (const struct sockaddr*)&netplay->their_addr;

   if (addr)
   {
      if (sendto(netplay->udp_fd, (const char*)netplay->packet_buffer,
               sizeof(netplay->packet_buffer), 0, addr,
               sizeof(struct sockaddr_in6)) != sizeof(netplay->packet_buffer))
      {
         netplay_disconnect();
         return false;
      }
   }
   return true;
}

/* Needed after sending/receiving a savestate */
static void netplay_resync(netplay_t *netplay)
{
   uint16_t last_peer_input =
         netplay->buffer[NETPLAY_PREV_PTR(netplay->read_ptr)].peer_input_state;
   int16_t i;

   /* Load state if we're the recipient */
   if (netplay->buffer[netplay->other_ptr].self_crc == 0xFEED)
      pretro_unserialize(netplay->buffer[netplay->other_ptr].state,
            netplay->state_size);

   netplay->self_ptr = netplay->other_ptr;
   netplay->read_ptr = netplay->other_ptr;

   for (i = 0; i < NETPLAY_BUF_SIZE; i++)
      netplay->buffer[i].self_crc = 0;
   netplay->buffer[NETPLAY_PREV_PTR(netplay->read_ptr)].peer_input_state
         = last_peer_input;

   netplay->other_frame_count = 1;
   netplay->frame_count = 1;
   netplay->read_frame_count = 1;

   netplay->flip_frame = netplay->flip ? 1 : 0;

   netplay->need_resync = false;
}

/**
 * get_self_input_state:
 * @netplay : pointer to netplay object
 *
 * Grab our own input state and send this over the network.
 *
 * Returns: true (1) if successful, otherwise false (0).
 **/
static bool get_self_input_state(netplay_t *netplay)
{
   driver_t *driver        = driver_get_ptr();
   settings_t *settings    = config_get_ptr();
   uint16_t state          = 0;
   uint16_t lag            = netplay->frame_count > netplay->read_frame_count
         ? netplay->frame_count - netplay->read_frame_count
         : 0;
   int i;

   if (!driver->block_libretro_input && netplay->frame_count > 0)
   {
      retro_input_state_t cb = netplay->cbs.state_cb;
      for (i = 0; i < RARCH_CUSTOM_BIND_LIST_END; i++)
      {
         int16_t tmp = cb(settings->input.netplay_client_swap_input ?
               0 : !netplay->port,
               RETRO_DEVICE_JOYPAD, 0, i);
         state |= tmp ? 1 << i : 0;
      }
   }
   else if (netplay->frame_count == 0 && !netplay_connect(netplay))
      return false;

   memmove(netplay->packet_buffer, netplay->packet_buffer + 3,
         sizeof(netplay->packet_buffer) - 3 * sizeof(uint32_t));
   netplay->packet_buffer[(UDP_FRAME_PACKETS - 1) * 3]
         = htonl(netplay->frame_count);
   netplay->packet_buffer[(UDP_FRAME_PACKETS - 1) * 3 + 1]
         = htonl(state | (lag << 16));
   netplay->packet_buffer[(UDP_FRAME_PACKETS - 1) * 3 + 2]
         = htonl(netplay->buffer[netplay->self_ptr].self_crc);

   if (!send_chunk(netplay))
   {
      netplay_disconnect();
      return false;
   }

   netplay->buffer[netplay->self_ptr].self_input_state = state;
   return true;
}

static bool netplay_cmd_ack(netplay_t *netplay)
{
   uint32_t cmd = htonl(NETPLAY_CMD_ACK);
   return socket_send_all_blocking(netplay->tcp_fd, &cmd, sizeof(cmd));
}

static bool netplay_cmd_nak(netplay_t *netplay)
{
   uint32_t cmd = htonl(NETPLAY_CMD_NAK);
   return socket_send_all_blocking(netplay->tcp_fd, &cmd, sizeof(cmd));
}

static bool netplay_get_response(netplay_t *netplay)
{
   uint32_t response;
   if (!socket_receive_all_blocking(netplay->tcp_fd,
         &response, sizeof(response)))
      return false;

   return ntohl(response) == NETPLAY_CMD_ACK;
}

static bool netplay_get_cmd(netplay_t *netplay)
{
   uint32_t cmd, flip_frame;
   size_t cmd_size;
   bool silent = false;

   if (!socket_receive_all_blocking(netplay->tcp_fd, &cmd, sizeof(cmd)))
      return false;

   cmd = ntohl(cmd);

   cmd_size = cmd & 0xffff;
   cmd      = cmd >> 16;

   switch (cmd)
   {
      case NETPLAY_CMD_FLIP_PLAYERS:
         if (cmd_size != sizeof(uint32_t))
         {
            RARCH_ERR("CMD_FLIP_PLAYERS has unexpected command size.\n");
            return netplay_cmd_nak(netplay);
         }

         if (!socket_receive_all_blocking(netplay->tcp_fd,
               &flip_frame, sizeof(flip_frame)))
         {
            RARCH_ERR("Failed to receive CMD_FLIP_PLAYERS argument.\n");
            return netplay_cmd_nak(netplay);
         }

         flip_frame = ntohl(flip_frame);

         if (flip_frame < netplay->flip_frame)
         {
            RARCH_ERR("Host asked us to flip users in the past. Not possible ...\n");
            return netplay_cmd_nak(netplay);
         }

         netplay->flip ^= true;
         netplay->flip_frame = flip_frame;

         RARCH_LOG("Netplay users are flipped.\n");
         rarch_main_msg_queue_push("Netplay users are flipped.", 1, 180, false);

         return netplay_cmd_ack(netplay);

      case NETPLAY_CMD_RESYNC:
         silent = true;
         /* fall-through */
      case NETPLAY_CMD_LOAD_SAVESTATE:
         if (!silent)
         {
            rarch_main_msg_queue_push("Receiving netplay state...", 0, 1, true);
            video_driver_cached_frame();
         }

         if (!socket_receive_all_blocking(netplay->tcp_fd,
               netplay->buffer[netplay->other_ptr].state,
               netplay->state_size))
         {
            RARCH_ERR("Failed to receive netplay state from peer.\n");
            rarch_main_msg_queue_push("Failed to receive netplay state "
                  "from peer.", 0, 1, true);
            return netplay_cmd_nak(netplay);
         }

         netplay->buffer[netplay->other_ptr].self_crc = 0xFEED;
         netplay->need_resync = true;

         if (!silent)
            rarch_main_msg_queue_push("Netplay state received.", 1, 180, true);
         return netplay_cmd_ack(netplay);

      default:
         break;
   }

   RARCH_ERR("Unknown netplay command received.\n");
   return netplay_cmd_nak(netplay);
}

static bool hold_back_to_cancel_iterate(const unsigned hold_limit)
{
   settings_t *settings       = config_get_ptr();
   driver_t *driver           = driver_get_ptr();
   netplay_t *netplay         = (netplay_t*)driver->netplay_data;
   char msg[64];
   static unsigned hold_count;

#ifdef HAVE_OVERLAY
   rarch_main_data_overlay_finish();
#endif

   netplay->cbs.poll_cb();
   if (input_driver_key_pressed(settings->menu_cancel_btn))
      hold_count--;
   else
      hold_count = hold_limit;

   if (hold_count == hold_limit)
      strlcpy(msg, "Waiting for peer...\n"
                   "Hold Back key to disconnect", sizeof(msg));
   else if (hold_count > 0)
      snprintf(msg, sizeof(msg), "Waiting for peer...\n"
                                 "Hold for %u", hold_count);
   else
      strlcpy(msg, "Disconnecting...", sizeof(msg));

   rarch_main_msg_queue_push(msg, 1, 50, true);
   video_driver_cached_frame();

   if (hold_count == 0)
   {
      hold_count = hold_limit;
      return true;
   }
   else
      return false;
}

static int poll_input(netplay_t *netplay, bool block)
{
   int max_fd        = (netplay->tcp_fd > netplay->udp_fd ?
         netplay->tcp_fd : netplay->udp_fd) + 1;
   struct timeval tv = {0};
   tv.tv_sec         = 0;
   tv.tv_usec        = block ? (RETRY_MS * 1000) : 0;

   do
   {
      fd_set fds;
      /* select() does not take pointer to const struct timeval.
       * Technically possible for select() to modify tmp_tv, so 
       * we go paranoia mode. */
      struct timeval tmp_tv = tv;

      netplay->timeout_cnt++;

      FD_ZERO(&fds);
      FD_SET(netplay->udp_fd, &fds);
      FD_SET(netplay->tcp_fd, &fds);

      if (socket_select(max_fd, &fds, NULL, NULL, &tmp_tv) < 0)
         return -1;

      /* Somewhat hacky,
       * but we aren't using the TCP connection for anything useful atm. */
      if (FD_ISSET(netplay->tcp_fd, &fds) && !netplay_get_cmd(netplay))
         return -1;
      /* netplay_get_cmd might set this flag */
      if (netplay->need_resync)
         return 0;

      if (FD_ISSET(netplay->udp_fd, &fds))
         return 1;

      if (!block)
         continue;

      if (!send_chunk(netplay))
      {
         netplay_disconnect();
         return -1;
      }

      if (hold_back_to_cancel_iterate(6))
         return -1;

      RARCH_LOG("Network is stalling, resending packet... Attempt # %u\n",
                netplay->timeout_cnt);
   } while (block);

   return 0;
}

static bool receive_data(netplay_t *netplay, uint32_t *buffer, size_t size)
{
   socklen_t addrlen = sizeof(netplay->their_addr);

   if (recvfrom(netplay->udp_fd, (char*)buffer, size, 0,
            (struct sockaddr*)&netplay->their_addr, &addrlen) != (ssize_t)size)
      return false;

   netplay->has_client_addr = true;

   return true;
}

static void netplay_parse_packet(netplay_t *netplay, uint32_t *buffer)
{
   int i;

   for (i = 0; i < UDP_FRAME_PACKETS * 3; i++)
      buffer[i] = ntohl(buffer[i]);

   for (i = 0; i < UDP_FRAME_PACKETS
         && netplay->read_frame_count <= netplay->frame_count;
         i++)
   {
      uint32_t frame, crc;
      uint16_t state, lag;

      frame = buffer[3 * i];
      if (frame != netplay->read_frame_count)
         continue;

      state = buffer[3 * i + 1] & 0xFFFF;
      lag   = buffer[3 * i + 1] >> 16;
      crc   = buffer[3 * i + 2];

      netplay->buffer[netplay->read_ptr].is_simulated = false;
      netplay->buffer[netplay->read_ptr].peer_input_state = state;
      netplay->buffer[netplay->read_ptr].peer_crc = crc;
      netplay->buffer[netplay->read_ptr].peer_lag = lag;
      netplay->read_ptr = NETPLAY_NEXT_PTR(netplay->read_ptr);
      netplay->read_frame_count++;
      netplay->timeout_cnt = 0;
   }
}

static void netplay_simulate_input(netplay_t *netplay)
{
   struct delta_frame *ptr;

   if (netplay->read_frame_count <= netplay->frame_count)
   {
      size_t last_read = NETPLAY_PREV_PTR(netplay->read_ptr);
      size_t end_ptr   = NETPLAY_NEXT_PTR(netplay->self_ptr);
      netplay->tmp_ptr = netplay->read_ptr;

      do {
         ptr = &netplay->buffer[netplay->tmp_ptr];

         ptr->sim_peer_input_state =
               netplay->buffer[last_read].peer_input_state;
         ptr->is_simulated = true;

         netplay->tmp_ptr = NETPLAY_NEXT_PTR(netplay->tmp_ptr);
      } while (netplay->tmp_ptr != end_ptr);
   }
   else
   {
      /* If read_ptr is ahead, consider upcoming frame correctly simulated */
      ptr = &netplay->buffer[netplay->self_ptr];
      ptr->sim_peer_input_state = ptr->peer_input_state;
   }
}

static void netplay_adjust_speed(netplay_t *netplay)
{
   size_t prev  = NETPLAY_PREV_PTR(netplay->read_ptr);
   uint16_t lag = netplay->frame_count > netplay->read_frame_count
         ? netplay->frame_count - netplay->read_frame_count
         : 0;

   if (lag > netplay->buffer[prev].peer_lag)
      rarch_main_get_ptr()->is_slowmotion = true;
   else
      rarch_main_get_ptr()->is_slowmotion = false;
}

/**
 * netplay_poll:
 * @netplay : pointer to netplay object
 *
 * Polls network to see if we have anything new. If our 
 * network buffer is full, we simply have to block 
 * for new input data.
 **/
static void netplay_poll(netplay_t *netplay)
{
   size_t end_ptr = NETPLAY_PREV_PTR(netplay->other_ptr);
   int res;

   netplay->cbs.poll_cb();

   if (!get_self_input_state(netplay))
      return;

   /* We skip reading the first frame so the host has a chance to grab 
    * our host info so we don't block forever :') */
   if (netplay->frame_count == 0)
   {
      netplay->buffer[0].is_simulated     = false;
      netplay->buffer[0].peer_input_state = 0;
      netplay->read_ptr                   = NETPLAY_NEXT_PTR(netplay->read_ptr);
      netplay->read_frame_count++;
      return;
   }

   /* We might have reached the end of the buffer, where we 
    * simply have to block. */
   res = poll_input(netplay, netplay->self_ptr == end_ptr);

   /* Read peer input */
   if (res == 1)
   {
      uint32_t first_read = netplay->read_frame_count;
      do 
      {
         uint32_t buffer[UDP_FRAME_PACKETS * 3];
         if (!receive_data(netplay, buffer, sizeof(buffer)))
         {
            netplay_disconnect();
            return;
         }
         netplay_parse_packet(netplay, buffer);

      } while ((netplay->read_frame_count <= netplay->frame_count)
            && (res = poll_input(netplay, netplay->self_ptr == end_ptr
                     && first_read == netplay->read_frame_count)) == 1);
   }

   if (res == -1)
   {
      netplay_disconnect();
      return;
   }

   /* Simulate peer input from read_ptr to self_ptr */
   netplay_simulate_input(netplay);

   /* Avoid drifting from peer */
   netplay_adjust_speed(netplay);
}

void input_poll_net(void)
{
   /* no-op. Polling is done in netplay_poll */
}

void video_frame_net(const void *data, unsigned width,
      unsigned height, size_t pitch)
{
   driver_t *driver = driver_get_ptr();
   netplay_t *netplay = (netplay_t*)driver->netplay_data;
   if (!netplay_should_skip(netplay))
      netplay->cbs.frame_cb(data, width, height, pitch);
}

void audio_sample_net(int16_t left, int16_t right)
{
   driver_t *driver = driver_get_ptr();
   netplay_t *netplay = (netplay_t*)driver->netplay_data;
   if (!netplay_should_skip(netplay))
      netplay->cbs.sample_cb(left, right);
}

size_t audio_sample_batch_net(const int16_t *data, size_t frames)
{
   driver_t *driver = driver_get_ptr();
   netplay_t *netplay = (netplay_t*)driver->netplay_data;
   if (!netplay_should_skip(netplay))
      return netplay->cbs.sample_batch_cb(data, frames);
   return frames;
}

/**
 * netplay_is_alive:
 * @netplay              : pointer to netplay object
 *
 * Checks if input port/index is controlled by netplay or not.
 *
 * Returns: true (1) if alive, otherwise false (0).
 **/
static bool netplay_is_alive(netplay_t *netplay)
{
   if (!netplay)
      return false;
   return netplay->has_connection;
}

static bool netplay_flip_port(netplay_t *netplay, bool port)
{
   size_t frame;

   if (netplay->flip_frame == 0)
      return port;

   frame = netplay->in_replay ?
      netplay->tmp_frame_count : netplay->frame_count;

   return port ^ netplay->flip ^ (frame < netplay->flip_frame);
}

static int16_t netplay_input_state(netplay_t *netplay, bool port,
      unsigned device, unsigned idx, unsigned id)
{
   size_t ptr = netplay->in_replay ? netplay->tmp_ptr : netplay->self_ptr;
   uint16_t input_state;

   if (netplay->port == (netplay_flip_port(netplay, port) ? 1 : 0))
   {
      if (netplay->buffer[ptr].is_simulated)
         input_state = netplay->buffer[ptr].sim_peer_input_state;
      else
         input_state = netplay->buffer[ptr].peer_input_state;
   }
   else
      input_state = netplay->buffer[ptr].self_input_state;

   if (id == RETRO_DEVICE_ID_JOYPAD_MASK)
      return input_state;
   else
      return ((1 << id) & input_state) ? 1 : 0;
}

int16_t input_state_net(unsigned port, unsigned device,
      unsigned idx, unsigned id)
{
   driver_t *driver = driver_get_ptr();
   netplay_t *netplay = (netplay_t*)driver->netplay_data;
   if (netplay_is_alive(netplay))
      return netplay_input_state(netplay, port, device, idx, id);
   return netplay->cbs.state_cb(port, device, idx, id);
}

#ifndef HAVE_SOCKET_LEGACY
/* Custom inet_ntop. Win32 doesn't seem to support this ... */
static void log_connection(const struct sockaddr_storage *their_addr,
      unsigned slot, const char *nick)
{
   union
   {
      const struct sockaddr_storage *storage;
      const struct sockaddr_in *v4;
      const struct sockaddr_in6 *v6;
   } u;
   const char *str               = NULL;
   char buf_v4[INET_ADDRSTRLEN]  = {0};
   char buf_v6[INET6_ADDRSTRLEN] = {0};

   u.storage = their_addr;

   if (their_addr->ss_family == AF_INET)
   {
      struct sockaddr_in in;

      str = buf_v4;

      memset(&in, 0, sizeof(in));
      in.sin_family = AF_INET;
      memcpy(&in.sin_addr, &u.v4->sin_addr, sizeof(struct in_addr));

      getnameinfo((struct sockaddr*)&in, sizeof(struct sockaddr_in),
            buf_v4, sizeof(buf_v4),
            NULL, 0, NI_NUMERICHOST);
   }
   else if (their_addr->ss_family == AF_INET6)
   {
      struct sockaddr_in6 in;

      str = buf_v6;
      memset(&in, 0, sizeof(in));
      in.sin6_family = AF_INET6;
      memcpy(&in.sin6_addr, &u.v6->sin6_addr, sizeof(struct in6_addr));

      getnameinfo((struct sockaddr*)&in, sizeof(struct sockaddr_in6),
            buf_v6, sizeof(buf_v6), NULL, 0, NI_NUMERICHOST);
   }

   if (str)
   {
      char msg[512] = {0};

      snprintf(msg, sizeof(msg), "Got connection from: \"%s (%s)\" (#%u)",
            nick, str, slot);
      rarch_main_msg_queue_push(msg, 2, 180, true);
      RARCH_LOG("%s\n", msg);
   }
}
#endif

static void set_tcp_nodelay(int tcp_fd)
{
#if defined(IPPROTO_TCP) && defined(TCP_NODELAY)
   int flag = 1;
   if (setsockopt(tcp_fd, IPPROTO_TCP, TCP_NODELAY,
#ifdef _WIN32
      (const char*)
#else
      (const void*)
#endif
      &flag,
      sizeof(int)) < 0)
#endif
      RARCH_WARN("Could not set netplay TCP socket to nodelay. Expect jitter.\n");
}

static bool check_for_client(int *fd)
{
   fd_set fds;
   struct timeval tmp_tv   = {0};

   FD_ZERO(&fds);
   FD_SET(*fd, &fds);
   if (socket_select(*fd + 1, &fds, NULL, NULL, &tmp_tv) > 0
         && FD_ISSET(*fd, &fds))
      return true;

   return false;
}

static int init_tcp_connection(const struct addrinfo *res,
      bool server, struct sockaddr *other_addr, socklen_t addr_size, int *tcp_fd)
{
   bool init_fd = (*tcp_fd < 0);

   if (init_fd)
      *tcp_fd = socket(res->ai_family, res->ai_socktype, res->ai_protocol);

   if (*tcp_fd < 0)
      goto fail;

   if (server)  /* Client */
   {
      struct timeval timeout;
      const int retries = 2;
      int i;

      timeout.tv_sec  = 1;
      timeout.tv_usec = 0;
      setsockopt(*tcp_fd, SOL_SOCKET, SO_SNDTIMEO, (char*)&timeout, sizeof timeout);
      set_tcp_nodelay(*tcp_fd);

      for (i = 0;;)
      {
         char msg[32];
         snprintf(msg, sizeof(msg), "TCP Connection attempt %i", i+1);
         rarch_main_msg_queue_push(msg, 2, 1, true);
         video_driver_cached_frame();

         if (connect(*tcp_fd, res->ai_addr, res->ai_addrlen) < 0)
         {
            if (i++ < retries)
               continue;
            rarch_main_msg_queue_push("TCP failed to connect", 3, 240, true);
            goto fail;
         }
         return 1;
      }
   }
   else  /* Host */
   {
      int yes = 1;
      if (init_fd)
      {
         setsockopt(*tcp_fd, SOL_SOCKET, SO_REUSEADDR,
               (const char*)&yes, sizeof(int));

         if (bind(*tcp_fd, res->ai_addr, res->ai_addrlen) < 0 || listen(*tcp_fd, 1) < 0)
         {
            rarch_main_msg_queue_push("TCP failed to bind", 2, 240, true);
            *tcp_fd = -1;
            goto fail;
         }
      }

      if (check_for_client(tcp_fd))
      {
         int new_fd = accept(*tcp_fd, other_addr, &addr_size);
         socket_close(*tcp_fd);
         *tcp_fd = -1;

         if (new_fd < 0)
         {
            rarch_main_msg_queue_push("TCP failed to accept", 2, 240, true);
            goto fail;
         }
         *tcp_fd = new_fd;
         set_tcp_nodelay(*tcp_fd);
         return 1;
      }

      return 0;
   }

fail:
   if (*tcp_fd >= 0)
   {
      socket_close(*tcp_fd);
      *tcp_fd = -1;
   }
   return -1;
}

static int init_tcp_socket(netplay_t *netplay, const char *server,
                            uint16_t port)
{
   char port_buf[16]               = {0};
   int ret                         = -1;
   const struct addrinfo *tmp_info = NULL;
   struct addrinfo hints, *res     = NULL;

   memset(&hints, 0, sizeof(hints));

#if defined(_WIN32) || defined(HAVE_SOCKET_LEGACY)
   hints.ai_family = AF_INET;
#else
   hints.ai_family = AF_UNSPEC;
#endif

   hints.ai_socktype = SOCK_STREAM;
   if (!server)
      hints.ai_flags = AI_PASSIVE;

   snprintf(port_buf, sizeof(port_buf), "%hu", (unsigned short)port);
   if (getaddrinfo_rarch(server, port_buf, &hints, &res) < 0)
      return -1;

   if (!res)
      return -1;

   /* If "localhost" is used, it is important to check every possible 
    * address for IPv4/IPv6. */
   tmp_info = res;

   while (tmp_info)
   {
      if ((ret = init_tcp_connection(tmp_info, server,
            (struct sockaddr*)&netplay->other_addr,
            sizeof(netplay->other_addr), &netplay->tcp_fd)) >= 0)
         break;

      tmp_info = tmp_info->ai_next;
   }

   if (res)
      freeaddrinfo_rarch(res);

   if (ret < 0)
      RARCH_ERR("Failed to set up netplay sockets.\n");

   return ret;
}

static bool init_udp_socket(netplay_t *netplay, const char *server,
      uint16_t port)
{
   char port_buf[16]     = {0};
   struct addrinfo hints = {0};

   memset(&hints, 0, sizeof(hints));
#if defined(_WIN32) || defined(HAVE_SOCKET_LEGACY)
   hints.ai_family = AF_INET;
#else
   hints.ai_family = AF_UNSPEC;
#endif
   hints.ai_socktype = SOCK_DGRAM;
   if (!server)
      hints.ai_flags = AI_PASSIVE;

   snprintf(port_buf, sizeof(port_buf), "%hu", (unsigned short)port);

   if (getaddrinfo_rarch(server, port_buf, &hints, &netplay->addr) < 0)
      return false;

   if (!netplay->addr)
      return false;

   netplay->udp_fd = socket(netplay->addr->ai_family,
         netplay->addr->ai_socktype, netplay->addr->ai_protocol);

   if (netplay->udp_fd < 0)
      goto end;

   if (!server)
   {
      /* Not sure if we have to do this for UDP, but hey :) */
      int yes = 1;

      setsockopt(netplay->udp_fd, SOL_SOCKET, SO_REUSEADDR,
            (const char*)&yes, sizeof(int));

      if (bind(netplay->udp_fd, netplay->addr->ai_addr,
               netplay->addr->ai_addrlen) < 0)
      {
         RARCH_ERR("Failed to bind socket.\n");
         socket_close(netplay->udp_fd);
         netplay->udp_fd = -1;
      }

      freeaddrinfo_rarch(netplay->addr);
      netplay->addr = NULL;
   }

end:
   if (netplay->udp_fd < 0)
      return false;

   return true;
}

static int init_socket(netplay_t *netplay, const char *server, uint16_t port)
{
   int tcp_ret;

   if (!network_init())
      return -1;

   tcp_ret = init_tcp_socket(netplay, server, port);
   if (tcp_ret != 1)
      return tcp_ret;

   if (!init_udp_socket(netplay, server, port))
   {
      rarch_main_msg_queue_push("Failed to init UDP socket.", 1, 240, true);
      return -1;
   }

   return 1;
}


/**
 * implementation_magic_value:
 *
 * Not really a hash, but should be enough to differentiate 
 * implementations from each other.
 *
 * Subtle differences in the implementation will not be possible to spot.
 * The alternative would have been checking serialization sizes, but it 
 * was troublesome for cross platform compat.
 **/
static uint32_t implementation_magic_value(void)
{
   size_t i, len;
   uint32_t res     = 0;
   const char *lib  = NULL;
   const char *ver  = PACKAGE_VERSION;
   unsigned api     = pretro_api_version();
   global_t *global = global_get_ptr();
   lib              = global->system.info.library_name;

   res |= api;

   len = strlen(lib);
   for (i = 0; i < len; i++)
      res ^= lib[i] << (i & 0xf);

   lib = global->system.info.library_version;
   len = strlen(lib);

   for (i = 0; i < len; i++)
      res ^= lib[i] << (i & 0xf);

   len = strlen(ver);
   for (i = 0; i < len; i++)
      res ^= ver[i] << ((i & 0xf) + 16);

   return res;
}

static bool send_nickname(netplay_t *netplay, int fd)
{
   uint8_t nick_size = strlen(netplay->nick);

   if (!socket_send_all_blocking(fd, &nick_size, sizeof(nick_size)))
   {
      RARCH_ERR("Failed to send nick size.\n");
      return false;
   }

   if (!socket_send_all_blocking(fd, netplay->nick, nick_size))
   {
      RARCH_ERR("Failed to send nick.\n");
      return false;
   }

   return true;
}

static bool get_nickname(netplay_t *netplay, int fd)
{
   uint8_t nick_size;

   if (!socket_receive_all_blocking(fd, &nick_size, sizeof(nick_size)))
   {
      RARCH_ERR("Failed to receive nick size from host.\n");
      return false;
   }

   if (nick_size >= sizeof(netplay->other_nick))
   {
      RARCH_ERR("Invalid nick size.\n");
      return false;
   }

   if (!socket_receive_all_blocking(fd, netplay->other_nick, nick_size))
   {
      RARCH_ERR("Failed to receive nick.\n");
      return false;
   }

   return true;
}

static bool send_info(netplay_t *netplay)
{
   char msg[512]      = {0};
   void *sram         = NULL;
   uint32_t header[3] = {0};
   global_t *global   = global_get_ptr();
   
   header[0] = htonl(global->content_crc);
   header[1] = htonl(implementation_magic_value());
   header[2] = htonl(netplay->sram_size);

   if (!socket_send_all_blocking(netplay->tcp_fd, header, sizeof(header)))
      return false;

   if (!send_nickname(netplay, netplay->tcp_fd))
   {
      RARCH_ERR("Failed to send nick to host.\n");
      rarch_main_msg_queue_push("Failed to send nick to host.",
            1, 240, true);
      return false;
   }

   /* Get SRAM data from User 1. */
   sram = pretro_get_memory_data(RETRO_MEMORY_SAVE_RAM);

   if (!socket_receive_all_blocking(netplay->tcp_fd, sram, netplay->sram_size))
   {
      RARCH_ERR("Failed to receive SRAM data from host.\n");
      rarch_main_msg_queue_push("Failed to receive SRAM data from host.",
            1, 240, true);
      return false;
   }

   if (!get_nickname(netplay, netplay->tcp_fd))
   {
      RARCH_ERR("Failed to receive nick from host.\n");
      rarch_main_msg_queue_push("Failed to receive nick from host.",
            1, 240, true);
      return false;
   }

   snprintf(msg, sizeof(msg), "Connected to: \"%s (%s)\"",
            netplay->other_nick, global->netplay_server);
   RARCH_LOG("%s\n", msg);
   rarch_main_msg_queue_push(msg, 2, 180, true);

   return true;
}

static bool get_info(netplay_t *netplay)
{
   uint32_t header[3];
   const void *sram = NULL;
   global_t *global = global_get_ptr();

   if (!socket_receive_all_blocking(netplay->tcp_fd, header, sizeof(header)))
   {
      RARCH_ERR("Failed to receive header from client.\n");
      rarch_main_msg_queue_push("Failed to receive header from client.",
            1, 240, true);
      return false;
   }

   if (global->content_crc != ntohl(header[0]))
   {
      RARCH_ERR("Content CRC32s differ. Cannot use different games.\n");
      rarch_main_msg_queue_push("Content CRC32s differ. "
            "Cannot use different games.", 1, 240, true);
      return false;
   }

   if (implementation_magic_value() != ntohl(header[1]))
   {
      RARCH_ERR("Implementations differ. "
            "Netplay requires identical core and RetroArch versions.\n");
      rarch_main_msg_queue_push("Implementations differ. "
            "Netplay requires identical core and RetroArch versions.",
            1, 240, true);
      return false;
   }

   if (netplay->sram_size != ntohl(header[2]))
   {
      RARCH_ERR("Content SRAM sizes do not correspond.\n");
      rarch_main_msg_queue_push("Content SRAM sizes do not correspond.",
            1, 240, true);
      return false;
   }

   if (!get_nickname(netplay, netplay->tcp_fd))
   {
      RARCH_ERR("Failed to get nick from client.\n");
      rarch_main_msg_queue_push("Failed to get nick from client.",
            1, 240, true);
      return false;
   }

   /* Send SRAM data to our User 2. */
   sram = pretro_get_memory_data(RETRO_MEMORY_SAVE_RAM);

   if (!socket_send_all_blocking(netplay->tcp_fd, sram, netplay->sram_size))
   {
      RARCH_ERR("Failed to send SRAM data to client.\n");
      rarch_main_msg_queue_push("Failed to send SRAM data to client.",
            1, 240, true);
      return false;
   }

   if (!send_nickname(netplay, netplay->tcp_fd))
   {
      RARCH_ERR("Failed to send nick to client.\n");
      rarch_main_msg_queue_push("Failed to send nick to client.",
            1, 240, true);
      return false;
   }

#ifndef HAVE_SOCKET_LEGACY
   log_connection(&netplay->other_addr, 0, netplay->other_nick);
#endif

   return true;
}

static bool netplay_init_buffers(netplay_t *netplay)
{
   int i;

   if (!netplay)
      return false;

   netplay->buffer = (struct delta_frame*)calloc(NETPLAY_BUF_SIZE,
         sizeof(*netplay->buffer));
   
   if (!netplay->buffer)
      return false;

   netplay->use_rollback_states = true;
   netplay->state_size = pretro_serialize_size();

   for (i = 0; i < NETPLAY_BUF_SIZE; i++)
   {
      netplay->buffer[i].state = calloc(netplay->state_size, 1);

      if (!netplay->buffer[i].state)
         return false;

      netplay->buffer[i].is_simulated = true;
   }

   return true;
}

/**
 * netplay_new:
 * @server : IP address of server.
 * @cb     : Libretro callbacks.
 * @nick   : Nickname of user.
 * @cb     : set by retro_set_default_callbacks
 *
 * Creates a new netplay handle. A NULL host means we're 
 * hosting (user 1).
 *
 * Returns: new netplay handle.
 **/
netplay_t *netplay_new(const char *server, const char *nick,
                       const struct retro_callbacks *cb)
{
   netplay_t *netplay = NULL;

   netplay = (netplay_t*)calloc(1, sizeof(*netplay));
   if (!netplay)
      return NULL;

   netplay->tcp_fd  = -1;
   netplay->udp_fd  = -1;
   netplay->cbs     = *cb;
   netplay->is_host = server ? false : true;
   netplay->port    = server ? 0 : 1;
   strlcpy(netplay->nick, nick, sizeof(netplay->nick));

   if (!netplay_init_buffers(netplay))
   {
      netplay_free(netplay);
      netplay = NULL;
   }

   /* Get SRAM size at frame 0 for consistency (hopefully) */
   netplay->sram_size = pretro_get_memory_size(RETRO_MEMORY_SAVE_RAM);
   return netplay;
}

bool netplay_connect(netplay_t *netplay)
{
   global_t *global = global_get_ptr();
   char* server = netplay->is_host ? NULL : global->netplay_server;
   uint16_t port = global->netplay_port ? global->netplay_port : RARCH_DEFAULT_PORT;
   int ret = 0;

   ret = init_socket(netplay, server, port);

   if (ret == -1)
      goto error;
   else if (ret == 1)
   {
      if (server)
      {
         if (!send_info(netplay))
            goto error;
      }
      else
      {
         if (!get_info(netplay))
            goto error;
         if (!netplay_send_savestate(true))
            goto error;
      }

      netplay->has_connection = true;
      return true;
   }
   else
      return false;

error:
   deinit_netplay();
   RARCH_WARN(RETRO_LOG_INIT_NETPLAY_FAILED);
   rarch_main_msg_queue_push(RETRO_MSG_INIT_NETPLAY_FAILED, 0, 180, false);

   return false;
}

static bool netplay_send_cmd(netplay_t *netplay, uint32_t cmd,
      const void *data, size_t size)
{
   cmd = (cmd << 16) | (size & 0xffff);
   cmd = htonl(cmd);

   if (!socket_send_all_blocking(netplay->tcp_fd, &cmd, sizeof(cmd)))
      return false;

   if (!socket_send_all_blocking(netplay->tcp_fd, data, size))
      return false;

   return true;
}

/**
 * netplay_flip_users:
 * @netplay : pointer to netplay object
 *
 * On regular netplay, flip who controls user 1 and 2.
 **/
void netplay_flip_users(netplay_t *netplay)
{
   uint32_t flip_frame     = netplay->frame_count + UDP_FRAME_PACKETS;
   uint32_t flip_frame_net = htonl(flip_frame);
   const char *msg         = NULL;

   if (netplay->port == 0)
   {
      msg = "Cannot flip users if you're not the host.";
      goto error;
   }

   /* Make sure both clients are definitely synced up. */
   if (netplay->frame_count < (netplay->flip_frame + UDP_FRAME_PACKETS))
   {
      msg = "Cannot flip users yet. Wait a second or two before attempting flip.";
      goto error;
   }

   if (netplay_send_cmd(netplay, NETPLAY_CMD_FLIP_PLAYERS,
            &flip_frame_net, sizeof(flip_frame_net))
         && netplay_get_response(netplay))
   {
      RARCH_LOG("Netplay users are flipped.\n");
      rarch_main_msg_queue_push("Netplay users are flipped.", 1, 180, false);

      /* Queue up a flip well enough in the future. */
      netplay->flip ^= true;
      netplay->flip_frame = flip_frame;
   }
   else
   {
      msg = "Failed to flip users.";
      goto error;
   }

   return;

error:
   RARCH_WARN("%s\n", msg);
   rarch_main_msg_queue_push(msg, 1, 180, false);
}

bool netplay_send_savestate(bool silent)
{
   driver_t *driver   = driver_get_ptr();
   netplay_t *netplay = (netplay_t*)driver->netplay_data;
   void *savestate    = netplay->buffer[netplay->other_ptr].state;

   netplay->use_rollback_states = true;
   pretro_serialize(savestate, netplay->state_size);

   if (!silent)
   {
      rarch_main_msg_queue_push("Sending netplay state...", 0, 1, true);
      video_driver_cached_frame();
   }

   if (!netplay_send_cmd(netplay, silent ?
         NETPLAY_CMD_RESYNC : NETPLAY_CMD_LOAD_SAVESTATE,
         savestate, netplay->state_size)
         || !netplay_get_response(netplay))
   {
      RARCH_LOG("Failed to send netplay state.\n");
      rarch_main_msg_queue_push("Failed to send netplay state.", 1, 180, true);
      return false;
   }

   if (!silent)
      rarch_main_msg_queue_push("Netplay state sent.", 0, 120, true);

   netplay->need_resync = true;

   return true;
}

/**
 * netplay_free:
 * @netplay : pointer to netplay object
 *
 * Frees netplay handle.
 **/
void netplay_free(netplay_t *netplay)
{
   unsigned i;

   for (i = 0; i < NETPLAY_BUF_SIZE; i++)
      free(netplay->buffer[i].state);
   free(netplay->buffer);

   if (netplay->addr)
      freeaddrinfo_rarch(netplay->addr);

   free(netplay);
}

static void netplay_update_state_crc(netplay_t *netplay,
      size_t buf_idx, size_t frame)
{
   struct delta_frame *ptr = &netplay->buffer[buf_idx];

   if (frame % SYNC_PERIOD == SYNC_PERIOD - 1)
      ptr->self_crc = crc32(0, ptr->state, netplay->state_size);
   else
      ptr->self_crc = 0;
}

static void netplay_replay_frames(netplay_t *netplay)
{
   driver_t *driver = driver_get_ptr();

   driver->audio_suspended  = true;
   driver->video_active     = false;
   netplay->tmp_ptr         = netplay->other_ptr;
   netplay->tmp_frame_count = netplay->other_frame_count;

   if (config_get_ptr()->netplay_show_rollback)
   {
      char msg[32];
      snprintf(msg, sizeof(msg), "Rollback: %i\nFrame: %i",
            netplay->frame_count - netplay->other_frame_count,
            netplay->frame_count + 1);
      rarch_main_msg_queue_push(msg, 0, 60, true);
   }

   pretro_unserialize(netplay->buffer[netplay->other_ptr].state,
         netplay->state_size);

   do {
#if defined(HAVE_THREADS) && !defined(RARCH_CONSOLE)
      lock_autosave();
#endif
      pretro_run();
#if defined(HAVE_THREADS) && !defined(RARCH_CONSOLE)
      unlock_autosave();
#endif
      if (!netplay->buffer[netplay->tmp_ptr].is_simulated)
      {
         netplay->other_ptr = NETPLAY_NEXT_PTR(netplay->other_ptr);
         netplay->other_frame_count++;
      }
      netplay->tmp_ptr = NETPLAY_NEXT_PTR(netplay->tmp_ptr);
      netplay->tmp_frame_count++;

      pretro_serialize(netplay->buffer[netplay->tmp_ptr].state,
            netplay->state_size);

      /* Recompute CRC */
      netplay_update_state_crc(netplay,
            netplay->tmp_ptr, netplay->tmp_frame_count);

   } while (netplay->tmp_frame_count < netplay->frame_count);

   netplay->in_replay         = false;
   driver->audio_suspended    = false;
   driver->video_active       = true;
}

/**
 * netplay_pre_frame:   
 * @netplay : pointer to netplay object
 *
 * Pre-frame for Netplay. We check if we have new input, and if needed,
 * replay from recorded input to correct the current state.
 **/
void netplay_pre_frame(netplay_t *netplay)
{
   struct delta_frame *ptr;
   netplay->use_rollback_states = true;

   if (!netplay->need_resync)
   {
      ptr = &netplay->buffer[netplay->self_ptr];
      pretro_serialize(ptr->state, netplay->state_size);

      /* Periodically compute savestate CRC */
      netplay_update_state_crc(netplay,
            netplay->self_ptr, netplay->frame_count);
   }

   /* Update input buffer and simulate missing input */
   do {
      if (netplay->need_resync)
         netplay_resync(netplay);
      netplay_poll(netplay);
   } while (netplay->need_resync);

   /* Skip ahead if we predicted correctly.
    * Skip until our simulation failed. */
   while (netplay->other_frame_count
         < min(netplay->read_frame_count, netplay->frame_count))
   {
      ptr = &netplay->buffer[netplay->other_ptr];
      if (ptr->sim_peer_input_state != ptr->peer_input_state)
      {
         netplay->in_replay = true;
         break;
      }
      netplay->other_ptr = NETPLAY_NEXT_PTR(netplay->other_ptr);
      netplay->other_frame_count++;
   }

   /* Update core state to match latest input state */
   if (netplay->in_replay)
      netplay_replay_frames(netplay);
}

/**
 * netplay_post_frame:   
 * @netplay : pointer to netplay object
 *
 * Post-frame for Netplay.
 * Increment our frame count and check if a resync is needed.
 * Call this after running retro_run().
 **/
void netplay_post_frame(netplay_t *netplay)
{
   settings_t *settings    = config_get_ptr();
   size_t end_ptr          = NETPLAY_NEXT_PTR(netplay->self_ptr);
   struct delta_frame *ptr = &netplay->buffer[end_ptr];

   if (netplay->has_connection)
   {
      netplay->frame_count++;
      netplay->self_ptr = end_ptr;
   }
   else if (netplay->is_host)
      rarch_main_msg_queue_push("Waiting for client", 0, 1, false);

   /* Resync if peer state doesn't match */
   if (ptr->self_crc && ptr->peer_crc)
   {
      bool mismatch = (ptr->self_crc != ptr->peer_crc);

      if (settings->netplay_show_crc_checks)
      {
         char msg[32];
         snprintf(msg, sizeof(msg), mismatch ?
            "CRC mismatch\nFrame: %d" : "CRCs equal\nFrame: %d",
               netplay->frame_count - NETPLAY_BUF_SIZE);
         rarch_main_msg_queue_push(msg, 1, 120, true);
      }

      if (mismatch && netplay->is_host
            && settings->netplay_periodic_resync)
         netplay_send_savestate(true);
   }

   netplay->use_rollback_states = false;
}

static void netplay_mask_unmask_config(bool starting)
{
   settings_t *settings = config_get_ptr();
   
   static bool has_started;
   static unsigned video_frame_delay;
   static bool menu_pause_libretro;
   static bool pause_nonactive;
   static float slowmotion_ratio;
   
   if (starting && !has_started)
   {  /* mask */
      video_frame_delay = settings->video.frame_delay;
      settings->video.frame_delay = 0;
      
      menu_pause_libretro = settings->menu.pause_libretro;
      settings->menu.pause_libretro = false;

      pause_nonactive = settings->pause_nonactive;
      settings->pause_nonactive = false;

      slowmotion_ratio = settings->slowmotion_ratio;
      settings->slowmotion_ratio = 1.033333;  /* shave 2fps for peer catch-up */
      
      preempt_deinit(); /* Netplay overrides the same libretro calls */
      
      has_started = true;
   }
   else if (has_started)
   {  /* unmask */
      settings->video.frame_delay = video_frame_delay;
      settings->menu.pause_libretro = menu_pause_libretro;
      settings->pause_nonactive = pause_nonactive;
      settings->slowmotion_ratio = slowmotion_ratio;
      
      preempt_init(); /* skips if preempt_frames == 0 */
      
      has_started = false;
   }
}

void netplay_mask_config(void)
{
   netplay_mask_unmask_config(true);
}

void netplay_unmask_config(void)
{
   netplay_mask_unmask_config(false);
}

void netplay_disconnect(void)
{
   warn_hangup();
   deinit_netplay();
}

void deinit_netplay(void)
{
   driver_t *driver = driver_get_ptr();
   netplay_t *netplay = (netplay_t*)driver->netplay_data;
   if (netplay)
   {
      if (netplay->tcp_fd >= 0)
         socket_close(netplay->tcp_fd);
      if (netplay->udp_fd >= 0)
         socket_close(netplay->udp_fd);

      retro_init_libretro_cbs(&driver->retro_ctx);
      netplay_free(netplay);
      driver->netplay_data = NULL;
      netplay_unmask_config();
   }
}

/**
 * init_netplay:
 *
 * Initializes netplay.
 *
 * If netplay is already initialized, will return false (0).
 *
 * Returns: true (1) if successful, otherwise false (0).
 **/
bool init_netplay(void)
{
   struct retro_callbacks cbs = {0};
   driver_t *driver     = driver_get_ptr();
   settings_t *settings = config_get_ptr();
   global_t *global     = global_get_ptr();

   if (!global->netplay_enable)
      return false;

   retro_set_default_callbacks(&cbs);

   if (global->netplay_is_client)
      RARCH_LOG("Connecting to netplay host...\n");
   else
      RARCH_LOG("Waiting for client...\n");

   driver->netplay_data = (netplay_t*)netplay_new(
         global->netplay_is_client ? global->netplay_server : NULL,
         settings->username, &cbs);

   if (driver->netplay_data)
   {
      netplay_mask_config();
      return true;
   }

   RARCH_WARN(RETRO_LOG_INIT_NETPLAY_FAILED);

   rarch_main_msg_queue_push(
         RETRO_MSG_INIT_NETPLAY_FAILED,
         0, 180, false);
   return false;
}

bool netplay_use_rollback_states(netplay_t *netplay)
{
   if (!netplay)
      return false;
   return netplay->use_rollback_states;
}

#ifdef HAVE_SOCKET_LEGACY

#undef sockaddr_storage
#undef addrinfo

#include <stdlib.h>
#include <ctype.h>
#include <string.h>

#define addrinfo addrinfo_rarch__

#ifdef _XBOX
/* TODO - implement h_length and h_addrtype */
struct hostent
{
   int h_addrtype;     /* host address type   */
   int h_length;       /* length of addresses */
   char **h_addr_list; /* list of addresses   */
};

static struct hostent *gethostbyname(const char *name)
{
   WSAEVENT event;
   static struct hostent he;
   static struct in_addr addr;
   static char *addr_ptr;
   XNDNS *dns = NULL;

   he.h_addr_list = &addr_ptr;
   addr_ptr = (char*)&addr;

   if (!name)
      return NULL;

   event = WSACreateEvent();
   XNetDnsLookup(name, event, &dns);
   if (!dns)
      goto error;

   WaitForSingleObject((HANDLE)event, INFINITE);
   if (dns->iStatus)
      goto error;

   memcpy(&addr, dns->aina, sizeof(addr));

   WSACloseEvent(event);
   XNetDnsRelease(dns);

   return &he;

error:
   if (event)
      WSACloseEvent(event);
   return NULL;
}
#endif


#endif
