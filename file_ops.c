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

#include "file_ops.h"
#include <file/file_path.h>
#include <stdlib.h>
#include <boolean.h>
#include <string.h>
#include <time.h>
#include <errno.h>
#include <compat/strl.h>
#include <compat/posix_string.h>
#include <retro_miscellaneous.h>

#ifdef HAVE_COMPRESSION
#include <file/file_extract.h>
#include "performance.h"
#include "runloop.h"

#define RZIP_VERSION 1
#define RZIP_COMPRESSION_LEVEL 6
#define RZIP_DEFAULT_CHUNK_SIZE 131072
#define RZIP_HEADER_SIZE 20
#define RZIP_CHUNK_HEADER_SIZE 4
#endif

#ifdef HAVE_7ZIP
#include "decompress/7zip_support.h"
#endif

#ifdef HAVE_ZLIB
#include "decompress/zip_support.h"
#endif

#ifdef __HAIKU__
#include <kernel/image.h>
#endif

#if defined(_WIN32)
#ifdef _MSC_VER
#define setmode _setmode
#endif
#ifdef _XBOX
#include <xtl.h>
#define INVALID_FILE_ATTRIBUTES -1
#else
#include <io.h>
#include <fcntl.h>
#include <direct.h>
#include <windows.h>
#endif
#else
#include <sys/types.h>
#include <sys/stat.h>
#include <dirent.h>
#include <unistd.h>
#endif

/**
 * write_file:
 * @path             : path to file.
 * @data             : contents to write to the file.
 * @size             : size of the contents.
 *
 * Writes data to a file.
 *
 * Returns: true (1) on success, false (0) otherwise.
 */
bool write_file(const char *path, const void *data, ssize_t size)
{
   bool ret   = false;
   FILE *file = fopen(path, "wb");
   if (!file)
      return false;

   ret = fwrite(data, 1, size, file) == size;
   fclose(file);
   return ret;
}

/**
 * read_generic_file:
 * @path             : path to file.
 * @buf              : buffer to allocate and read the contents of the
 *                     file into. Needs to be freed manually.
 *
 * Read the contents of a file into @buf.
 *
 * Returns: number of items read, -1 on error.
 */
static int read_generic_file(const char *path, void **buf, ssize_t *len)
{
   long ret                 = 0;
   ssize_t content_buf_size = 0;
   void *content_buf        = NULL;
   FILE *file               = fopen(path, "rb");

   if (!file)
      goto error;

   if (fseek(file, 0, SEEK_END) != 0)
      goto error;

   content_buf_size = ftell(file);
   if (content_buf_size < 0)
      goto error;

   rewind(file);

   content_buf = malloc(content_buf_size + 1);

   if (!content_buf)
      goto error;

   if ((ret = fread(content_buf, 1, content_buf_size, file)) < content_buf_size)
      RARCH_WARN("Didn't read whole file.\n");

   *buf    = content_buf;

   /* Allow for easy reading of strings to be safe.
    * Will only work with sane character formatting (Unix). */
   ((char*)content_buf)[content_buf_size] = '\0';

   if (fclose(file) != 0)
      RARCH_WARN("Failed to close file stream.\n");

   if (len)
      *len = ret;

   return 1;

error:
   if (file)
      fclose(file);
   if (content_buf)
      free(content_buf);
   if (len)
      *len = -1;
   *buf = NULL;
   return 0;
}

#ifdef HAVE_COMPRESSION
/* Generic compressed file loader.
 * Extracts to buf, unless optional_filename != 0
 * Then extracts to optional_filename and leaves buf alone.
 */
int read_compressed_file(const char * path, void **buf,
      const char* optional_filename, ssize_t *length)
{
   const char* file_ext               = NULL;
   char *archive_found                = NULL;
   char archive_path[PATH_MAX_LENGTH] = {0};

   if (optional_filename)
   {
      /* Safety check.  * If optional_filename and optional_filename 
       * exists, we simply return 0,
       * hoping that optional_filename is the 
       * same as requested.
       */
      if(path_file_exists(optional_filename))
      {
         *length = 0;
         return 1;
      }
   }

   /* We split carchive path and relative path: */
   strlcpy(archive_path, path, sizeof(archive_path));

   archive_found = (char*)strchr(archive_path,'#');

   rarch_assert(archive_found != NULL);

   /* We assure that there is something after the '#' symbol. */
   if (strlen(archive_found) <= 1)
   {
      /*
       * This error condition happens for example, when
       * path = /path/to/file.7z, or
       * path = /path/to/file.7z#
       */
      RARCH_ERR("Could not extract image path and carchive path from "
            "path: %s.\n", path);
      *length = 0;
      return 0;
   }

   /* We split the string in two, by putting a \0, where the hash was: */
   *archive_found  = '\0';
   archive_found  += 1;
   file_ext        = path_get_extension(archive_path);

#ifdef HAVE_7ZIP
   if (strcasecmp(file_ext,"7z") == 0)
   {
      *length = read_7zip_file(archive_path,archive_found,buf,optional_filename);
      if (*length != -1)
         return 1;
   }
#endif
#ifdef HAVE_ZLIB
   if (strcasecmp(file_ext,"zip") == 0)
   {
      *length = read_zip_file(archive_path,archive_found,buf,optional_filename);
      if (*length != -1)
         return 1;
   }
#endif
   return 0;
}
#endif

/**
 * read_file:
 * @path             : path to file.
 * @buf              : buffer to allocate and read the contents of the
 *                     file into. Needs to be freed manually.
 * @length           : Number of items read, -1 on error.
 *
 * Read the contents of a file into @buf. Will call read_compressed_file
 * if path contains a compressed file, otherwise will call read_generic_file.
 *
 * Returns: 1 if file read, 0 on error.
 */
int read_file(const char *path, void **buf, ssize_t *length)
{
#ifdef HAVE_COMPRESSION
   if (path_contains_compressed_file(path))
   {
      if (read_compressed_file(path, buf, NULL, length))
         return 1;
   }
#endif
   return read_generic_file(path, buf, length);
}

struct string_list *compressed_file_list_new(const char *path,
      const char* ext)
{
#ifdef HAVE_COMPRESSION
   const char* file_ext = path_get_extension(path);
#ifdef HAVE_7ZIP
   if (strcasecmp(file_ext,"7z") == 0)
      return compressed_7zip_file_list_new(path,ext);
#endif
#ifdef HAVE_ZLIB
   if (strcasecmp(file_ext,"zip") == 0)
      return zlib_get_file_list(path, ext);
#endif

#endif
   return NULL;
}

#ifdef HAVE_COMPRESSION
/* From RA v1.8.7 rzipstream_write_file_header */
static bool write_rzip_file_header(FILE *file, uint64_t data_size)
{
   uint8_t header_bytes[RZIP_HEADER_SIZE] = {0};

   /* > 'Magic numbers' - first 8 bytes */
   header_bytes[0] =           35; /* # */
   header_bytes[1] =           82; /* R */
   header_bytes[2] =           90; /* Z */
   header_bytes[3] =           73; /* I */
   header_bytes[4] =           80; /* P */
   header_bytes[5] =          118; /* v */
   header_bytes[6] = RZIP_VERSION; /* file format version number */
   header_bytes[7] =           35; /* # */

   /* > Uncompressed chunk size - next 4 bytes */
   header_bytes[11] = (RZIP_DEFAULT_CHUNK_SIZE >> 24) & 0xFF;
   header_bytes[10] = (RZIP_DEFAULT_CHUNK_SIZE >> 16) & 0xFF;
   header_bytes[9]  = (RZIP_DEFAULT_CHUNK_SIZE >>  8) & 0xFF;
   header_bytes[8]  =  RZIP_DEFAULT_CHUNK_SIZE        & 0xFF;

   /* > Total uncompressed data size - next 8 bytes */
   header_bytes[19] = (data_size >> 56) & 0xFF;
   header_bytes[18] = (data_size >> 48) & 0xFF;
   header_bytes[17] = (data_size >> 40) & 0xFF;
   header_bytes[16] = (data_size >> 32) & 0xFF;
   header_bytes[15] = (data_size >> 24) & 0xFF;
   header_bytes[14] = (data_size >> 16) & 0xFF;
   header_bytes[13] = (data_size >>  8) & 0xFF;
   header_bytes[12] =  data_size        & 0xFF;

   /* Write header bytes */
   if (fwrite(&header_bytes, 1, RZIP_HEADER_SIZE, file) != RZIP_HEADER_SIZE)
      return false;

   return true;
}

/* From RA v1.8.7 rzipstream_read_file_header */
static bool read_rzip_file_header(FILE *file,
                                  uint64_t *data_size, uint32_t *chunk_size)
{
   uint8_t header_bytes[RZIP_HEADER_SIZE] = {0};

   /* Attempt to read header bytes */
   if (fread(header_bytes, 1, RZIP_HEADER_SIZE, file) < RZIP_HEADER_SIZE)
      return false;

   /* Check 'magic numbers' - first 8 bytes of header */
   if ((header_bytes[0] !=           35) || /* # */
       (header_bytes[1] !=           82) || /* R */
       (header_bytes[2] !=           90) || /* Z */
       (header_bytes[3] !=           73) || /* I */
       (header_bytes[4] !=           80) || /* P */
       (header_bytes[5] !=          118) || /* v */
       (header_bytes[6] != RZIP_VERSION) || /* file format version number */
       (header_bytes[7] !=           35))   /* # */
      return false;

   /* Get uncompressed chunk size - next 4 bytes */
   *chunk_size = ((uint32_t)header_bytes[11] << 24) |
                 ((uint32_t)header_bytes[10] << 16) |
                 ((uint32_t)header_bytes[9]  <<  8) |
                  (uint32_t)header_bytes[8];
   if (*chunk_size == 0)
      return false;

   /* Get total uncompressed data size - next 8 bytes */
   *data_size = ((uint64_t)header_bytes[19] << 56) |
                ((uint64_t)header_bytes[18] << 48) |
                ((uint64_t)header_bytes[17] << 40) |
                ((uint64_t)header_bytes[16] << 32) |
                ((uint64_t)header_bytes[15] << 24) |
                ((uint64_t)header_bytes[14] << 16) |
                ((uint64_t)header_bytes[13] <<  8) |
                 (uint64_t)header_bytes[12];
   if (*data_size == 0)
      return false;

   return true;
}
#endif /* HAVE_COMPRESSION */

/**
 * write_rzip_file:
 * @path             : path to file.
 * @data             : contents to compress and write to file.
 * @size             : size of the uncompressed contents.
 *
 * Writes @data to @path in RZIP format.
 *
 * Returns: true on success, false otherwise.
 */
bool write_rzip_file(const char *path, const void *data, uint64_t size)
{
#ifdef HAVE_COMPRESSION
   bool     ret     = false;
   void*    stream  = NULL;
   uint8_t* next_in = (uint8_t*)data;
   uint8_t* buf     = NULL;
   uint8_t  chunk_header[RZIP_CHUNK_HEADER_SIZE];
   uint32_t chunk_infl_size;
   uint64_t total_read = 0;
   uint32_t zlib_defl_size;
   char     msg[32];

   const uint32_t buf_size = RZIP_DEFAULT_CHUNK_SIZE * 2;

   FILE *file = fopen(path, "wb");

   retro_time_t now_usec, prev_usec;
   prev_usec = rarch_get_time_usec() + 150000; /* Run .2s before showing progress */

   if (!file)
      return false;

   if (!write_rzip_file_header(file, size))
      goto end;

   /* Init zlib stream */
   stream = zlib_stream_new();
   buf = malloc(buf_size);
   if (!buf || !stream)
      goto end;
   zlib_deflate_init(stream, RZIP_COMPRESSION_LEVEL);

   while (total_read < size)
   {
      /* Deflate chunk */
      chunk_infl_size = min(RZIP_DEFAULT_CHUNK_SIZE, size - total_read);
      zlib_set_stream(stream, chunk_infl_size, buf_size, next_in, buf);
      if (zlib_deflate(stream) != 1)
         goto end;

      zlib_defl_size = zlib_stream_get_total_out(stream);
      if (zlib_defl_size == 0 || zlib_defl_size > buf_size)
         goto end;

      /* Create header */
      chunk_header[3] = (zlib_defl_size >> 24) & 0xFF;
      chunk_header[2] = (zlib_defl_size >> 16) & 0xFF;
      chunk_header[1] = (zlib_defl_size >>  8) & 0xFF;
      chunk_header[0] =  zlib_defl_size        & 0xFF;

      /* Write header */
      if (fwrite(&chunk_header, 1, RZIP_CHUNK_HEADER_SIZE, file)
               != RZIP_CHUNK_HEADER_SIZE)
         goto end;

      /* Write chunk */
      if (fwrite(buf, 1, zlib_defl_size, file) != zlib_defl_size)
         goto end;

      /* Setup next */
      total_read += chunk_infl_size;
      next_in    += chunk_infl_size;
      zlib_stream_deflate_reset(stream);

      /* Show progress at ~20fps */
      now_usec = rarch_get_time_usec();
      if (now_usec - prev_usec > 50000)
      {
         snprintf(msg, sizeof(msg), "Compressing %u%%",
                  (unsigned)((100 * total_read) / size));
         rarch_main_msg_queue_push(msg, 1, 1, true);
         video_driver_cached_frame();
         prev_usec = now_usec;
      }
   }

   ret = true;
end:
   if (stream)
      zlib_stream_deflate_free(stream);
   if (file)
      fclose(file);
   if (buf)
      free(buf);
   if (!ret)
      RARCH_ERR("Failed to write RZIP file to \"%s\".\n", path);
   return ret;
#else
   return write_file(path, data, size);
#endif
}

/**
 * read_rzip_file:
 * @path             : path to file.
 * @buf              : buffer to allocate and read the contents of the
 *                     file into. Needs to be freed manually.
 * @len              : Number of items read. Not updated on failure
 *
 * Decompresses contents from an RZIP file to @buf.
 *
 * Returns: true if file read, false on error.
 */
bool read_rzip_file(const char *path, void **buf, ssize_t *len)
{
#ifdef HAVE_COMPRESSION
   bool     ret       = false;
   void*    stream    = NULL;
   uint32_t file_size = 0;
   uint64_t data_size = 0;
   uint64_t total_out = 0;
   void*    out_buf   = NULL;
   uint8_t* next_out;
   uint8_t* chunk_buf = NULL;
   uint32_t chunk_defl_size;
   uint32_t chunk_infl_size;
   uint32_t zlib_infl_size;
   uint8_t  chunk_header[RZIP_CHUNK_HEADER_SIZE];
   char     msg[32];

   FILE *file = fopen(path, "rb");

   retro_time_t now_usec, prev_usec;
   prev_usec = rarch_get_time_usec() + 150000; /* Run .2s before showing progress */

   if (!file)
      return false;

   if (fseek(file, 0, SEEK_END) != 0)
      goto end;

   file_size = ftell(file);
   if (file_size < 0)
      goto end;

   rewind(file);

   if (!read_rzip_file_header(file, &data_size, &chunk_infl_size))
      goto end;

   out_buf   = malloc(data_size + 1);
   next_out  = out_buf;
   chunk_buf = malloc(chunk_infl_size * 2);
   if (!out_buf || !chunk_buf)
      goto end;

   /* Init zlib stream */
   stream = zlib_stream_new();
   if (!stream)
      goto end;
   zlib_inflate_init(stream);

   while (total_out < data_size)
   {
      /* Read header */
      if (fread(chunk_header, 1, RZIP_CHUNK_HEADER_SIZE, file)
               < RZIP_CHUNK_HEADER_SIZE)
         goto end;

      /* Get size of next chunk */
      chunk_defl_size = ((uint32_t)chunk_header[3] << 24) |
                        ((uint32_t)chunk_header[2] << 16) |
                        ((uint32_t)chunk_header[1] <<  8) |
                         (uint32_t)chunk_header[0];
      if (chunk_defl_size == 0)
         goto end;

      /* Read chunk */
      if (fread(chunk_buf, 1, chunk_defl_size, file) < chunk_defl_size)
         goto end;

      /* Inflate chunk to out_buf */
      zlib_set_stream(stream, chunk_defl_size, chunk_infl_size,
                      chunk_buf, next_out);
      if (zlib_inflate(stream) != 1)
         goto end;

      zlib_infl_size = zlib_stream_get_total_out(stream);
      if (zlib_infl_size == 0 || zlib_infl_size > chunk_infl_size)
         goto end;

      /* Setup next */
      next_out  += zlib_infl_size;
      total_out += zlib_infl_size;
      zlib_stream_inflate_reset(stream);

      /* Show progress at ~20fps */
      now_usec = rarch_get_time_usec();
      if (now_usec - prev_usec > 50000)
      {
         snprintf(msg, sizeof(msg), "Decompressing %u%%",
                  (unsigned)((100 * total_out) / data_size));
         rarch_main_msg_queue_push(msg, 1, 1, true);
         video_driver_cached_frame();
         prev_usec = now_usec;
      }
   }

   /* Allow for easy reading of strings to be safe.
    * Will only work with sane character formatting (Unix). */
   ((char*)out_buf)[total_out] = '\0';

   if (len)
      *len = (ssize_t)total_out;
   ret = true;
end:
   if (file)
      fclose(file);
   if (stream)
      zlib_stream_inflate_free(stream);
   if (chunk_buf)
      free(chunk_buf);
   if (!ret && out_buf)
      free(out_buf);
   else
      *buf = out_buf;

   return ret;
#else
   return false;
#endif
}
