/*  RetroArch - A frontend for libretro.
 *  Copyright (C) 2011-2015 - Daniel De Matteis
 *  Copyright (C) 2014-2015 - Alfred Agrell
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
#include <stdlib.h>
#include <ctype.h>
#include <net/net_http.h>
#include <net/net_compat.h>
#include <compat/strl.h>

enum
{
   P_HEADER_TOP = 0,
   P_HEADER,
   P_BODY,
   P_BODY_CHUNKLEN,
   P_DONE,
   P_ERROR
};

enum
{
   T_FULL = 0,
   T_LEN,
   T_CHUNK
};

struct http_t
{
	int fd;
	int status;
	
	char part;
	char bodytype;
	bool error;
	
	size_t pos;
	size_t len;
	size_t buflen;
	char * data;
};

struct http_connection_t
{
   char *domain;
   char *location;
   char *urlcopy;
   char *scan;
   int port;
};


static int net_http_new_socket(const char *domain, int port)
{
   int fd;
   struct addrinfo hints, *addr = NULL;
   char portstr[16] = {0};

   snprintf(portstr, sizeof(portstr), "%i", port);

   memset(&hints, 0, sizeof(hints));
   hints.ai_family   = AF_INET;
   hints.ai_socktype = SOCK_STREAM;
   hints.ai_flags    = 0;

   if (getaddrinfo_rarch(domain, portstr, &hints, &addr) < 0)
      return -1;
   if (!addr)
      return -1;

   fd = socket(addr->ai_family, addr->ai_socktype, addr->ai_protocol);
   if (socket_connect(fd, addr->ai_addr, addr->ai_addrlen, false, 4) != 0)
   {
      freeaddrinfo_rarch(addr);
      socket_close(fd);
      return -1;
   }

   freeaddrinfo_rarch(addr);

   return fd;
}

static void net_http_send(int fd, bool * error,
      const char * data, size_t len)
{
   if (*error)
      return;

   while (len)
   {
      ssize_t thislen = send(fd, data, len, MSG_NOSIGNAL);

      if (thislen <= 0)
      {
         if (!isagain(thislen))
            continue;

         *error=true;
         return;
      }

      data += thislen;
      len  -= thislen;
   }
}

static void net_http_send_str(int fd, bool *error, const char *text)
{
   net_http_send(fd, error, text, strlen(text));
}

static ssize_t net_http_recv(int fd, bool *error,
      uint8_t *data, size_t maxlen)
{
   ssize_t bytes;

   if (*error)
      return -1;

   bytes = recv(fd, (char*)data, maxlen, 0);

   if (bytes > 0)
      return bytes;
   else if (bytes == 0)
      return -1;
   else if (isagain(bytes))
      return 0;

   *error=true;
   return -1;
}

struct http_connection_t *net_http_connection_new(const char *url)
{
   char **domain = NULL;
   struct http_connection_t *conn = (struct http_connection_t*)calloc(1, 
         sizeof(struct http_connection_t));

   if (!conn)
      return NULL;

   conn->urlcopy      = (char*)malloc(strlen(url) + 1);

   if (!conn->urlcopy)
      goto error;

   strcpy(conn->urlcopy, url);

   if (strncmp(url, "http://", strlen("http://")) != 0)
      goto error;

   conn->scan    = conn->urlcopy + strlen("http://");

   domain        = &conn->domain;

   *domain = conn->scan;

   return conn;

error:
   if (conn->urlcopy)
      free(conn->urlcopy);
   conn->urlcopy = NULL;
   if (conn)
      free(conn);
   return NULL;
}

bool net_http_connection_iterate(struct http_connection_t *conn)
{
   if (!conn)
      return false;
   if (*conn->scan != '/' && *conn->scan != ':' && *conn->scan != '\0')
   {
      conn->scan++;
      return false;
   }
   return true;
}

bool net_http_connection_done(struct http_connection_t *conn)
{
   char **location = NULL;

   if (!conn)
      return false;

   location = &conn->location;

   if (*conn->scan == '\0')
      return false;

   *conn->scan   = '\0';
   conn->port   = 80;

   if (*conn->scan == ':')
   {

      if (!isdigit(conn->scan[1]))
         return false;

      conn->port = strtoul(conn->scan + 1, &conn->scan, 10);

      if (*conn->scan != '/')
         return false;
   }

   *location = conn->scan + 1;

   return true;
}

void net_http_connection_free(struct http_connection_t *conn)
{
   if (!conn)
      return;

   if (conn->urlcopy)
      free(conn->urlcopy);

   free(conn);
}

struct http_t *net_http_new(struct http_connection_t *conn)
{
   bool error;
   int fd = -1;
   struct http_t *state      = NULL;

   if (!conn)
      goto error;

   fd = net_http_new_socket(conn->domain, conn->port);
   if (fd == -1)
      goto error;

   error=false;

   /* This is a bit lazy, but it works. */
   net_http_send_str(fd, &error, "GET /");
   net_http_send_str(fd, &error, conn->location);
   net_http_send_str(fd, &error, " HTTP/1.1\r\n");

   net_http_send_str(fd, &error, "Host: ");
   net_http_send_str(fd, &error, conn->domain);

   if (conn->port != 80)
   {
      char portstr[16] = {0};

      snprintf(portstr, sizeof(portstr), ":%i", conn->port);
      net_http_send_str(fd, &error, portstr);
   }

   net_http_send_str(fd, &error, "\r\n");
   net_http_send_str(fd, &error, "Connection: close\r\n");
   net_http_send_str(fd, &error, "\r\n");

   if (error)
      goto error;

   state          = (struct http_t*)malloc(sizeof(struct http_t));
   state->fd      = fd;
   state->status  = -1;
   state->data    = NULL;
   state->part    = P_HEADER_TOP;
   state->bodytype= T_FULL;
   state->error   = false;
   state->pos     = 0;
   state->len     = 0;
   state->buflen  = 512;
   state->data    = (char*)malloc(state->buflen);

   if (!state->data)
      goto error;

   return state;

error:
   if (fd != -1)
      socket_close(fd);
   return NULL;
}

int net_http_fd(struct http_t *state)
{
   if (!state)
      return 0;
   return state->fd;
}

bool net_http_update(struct http_t *state, size_t* progress, size_t* total)
{
   ssize_t newlen = 0;

   if (!state || state->error)
      goto fail;

   if (state->part < P_BODY)
   {
      newlen = net_http_recv(state->fd, &state->error,
            (uint8_t*)state->data + state->pos, state->buflen - state->pos);

      if (newlen < 0)
         goto fail;

      if (state->pos + newlen >= state->buflen - 64)
      {
         state->buflen *= 2;
         state->data = (char*)realloc(state->data, state->buflen);
      }
      state->pos += newlen;

      while (state->part < P_BODY)
      {
         char *dataend = state->data + state->pos;
         char *lineend = (char*)memchr(state->data, '\n', state->pos);

         if (!lineend)
            break;
         *lineend='\0';
         if (lineend != state->data && lineend[-1]=='\r')
            lineend[-1]='\0';

         if (state->part == P_HEADER_TOP)
         {
            if (strncmp(state->data, "HTTP/1.", strlen("HTTP/1."))!=0)
               goto fail;
            state->status = strtoul(state->data + strlen("HTTP/1.1 "), NULL, 10);
            state->part   = P_HEADER;
         }
         else
         {
            if (!strncmp(state->data, "Content-Length: ",
                     strlen("Content-Length: ")))
            {
               state->bodytype = T_LEN;
               state->len = strtol(state->data + 
                     strlen("Content-Length: "), NULL, 10);
            }
            if (!strcmp(state->data, "Transfer-Encoding: chunked"))
               state->bodytype = T_CHUNK;

            /* TODO: save headers somewhere */
            if (state->data[0]=='\0')
            {
               state->part = P_BODY;
               if (state->bodytype == T_CHUNK)
                  state->part = P_BODY_CHUNKLEN;
            }
         }

         memmove(state->data, lineend + 1, dataend-(lineend+1));
         state->pos = (dataend-(lineend + 1));
      }
      if (state->part >= P_BODY)
      {
         newlen = state->pos;
         state->pos = 0;
      }
   }

   if (state->part >= P_BODY && state->part < P_DONE)
   {
      if (!newlen)
      {
         newlen = net_http_recv(state->fd, &state->error,
               (uint8_t*)state->data + state->pos,
               state->buflen - state->pos);

         if (newlen < 0)
         {
            if (state->bodytype == T_FULL)
            {
               state->part = P_DONE;
               state->data = (char*)realloc(state->data, state->len);
            }
            else
               goto fail;
            newlen=0;
         }

         if (state->pos + newlen >= state->buflen - 64)
         {
            state->buflen *= 2;
            state->data = (char*)realloc(state->data, state->buflen);
         }
      }

parse_again:
      if (state->bodytype == T_CHUNK)
      {
         if (state->part == P_BODY_CHUNKLEN)
         {
            state->pos += newlen;
            if (state->pos - state->len >= 2)
            {
               /*
                * len=start of chunk including \r\n
                * pos=end of data
                */

               char *fullend = state->data + state->pos;
               char *end     = (char*)memchr(state->data + state->len + 2,
                     '\n', state->pos - state->len - 2);

               if (end)
               {
                  size_t chunklen = strtoul(state->data+state->len, NULL, 16);
                  state->pos      = state->len;
                  end++;

                  memmove(state->data+state->len, end, fullend-end);

                  state->len      = chunklen;
                  newlen          = (fullend - end);

                  /*
                     len=num bytes
                     newlen=unparsed bytes after \n
                     pos=start of chunk including \r\n
                     */

                  state->part = P_BODY;
                  if (state->len == 0)
                  {
                     state->part = P_DONE;
                     state->len  = state->pos;
                     state->data = (char*)realloc(state->data, state->len);
                  }
                  goto parse_again;
               }
            }
         }
         else if (state->part == P_BODY)
         {
            if ((size_t)newlen >= state->len)
            {
               state->pos += state->len;
               newlen     -= state->len;
               state->len  = state->pos;
               state->part = P_BODY_CHUNKLEN;
               goto parse_again;
            }
            else
            {
               state->pos += newlen;
               state->len -= newlen;
            }
         }
      }
      else
      {
         state->pos += newlen;

         if (state->pos == state->len)
         {
            state->part = P_DONE;
            state->data = (char*)realloc(state->data, state->len);
         }
         if (state->pos > state->len)
            goto fail;
      }
   }

   if (progress)
      *progress = state->pos;

   if (total)
   {
      if (state->bodytype == T_LEN)
         *total=state->len;
      else
         *total=0;
   }

   return (state->part == P_DONE);

fail:
   if (state)
   {
      state->error  = true;
      state->part   = P_ERROR;
      state->status = -1;
   }

   return false;
}

int net_http_status(struct http_t *state)
{
   if (state)
      return state->status;
   return -1;
}

uint8_t* net_http_data(struct http_t *state, size_t* len, bool accept_error)
{
   if (!state)
      return NULL;

   if (!accept_error && 
         (state->error || state->status<200 || state->status>299))
   {
      if (len)
         *len=0;
      return NULL;
   }

   if (len)
      *len=state->len;

   return (uint8_t*)state->data;
}

void net_http_delete(struct http_t *state)
{
   if (!state)
      return;

   if (state->fd != -1)
      socket_close(state->fd);
   if (state->data)
      free(state->data);
   free(state);
}
