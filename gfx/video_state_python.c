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

#include <Python.h>
#include <stdlib.h>
#include <ctype.h>
#include <string.h>

#include "video_state_python.h"
#include "../dynamic.h"
#include <libretro.h>
#include "../general.h"
#include <compat/strl.h>
#include <compat/posix_string.h>
#include "../input/input_common.h"
#include "../file_ops.h"

static PyObject* py_read_wram(PyObject *self, PyObject *args)
{
   unsigned addr;
   size_t   max;
   const uint8_t *data = (const uint8_t*)
      pretro_get_memory_data(RETRO_MEMORY_SYSTEM_RAM);

   (void)self;

   if (!data)
   {
      Py_INCREF(Py_None);
      return Py_None;
   }

   max = pretro_get_memory_size(RETRO_MEMORY_SYSTEM_RAM);

   if (!PyArg_ParseTuple(args, "I", &addr))
      return NULL;

   if (addr >= max)
   {
      Py_INCREF(Py_None);
      return Py_None;
   }

   return PyLong_FromLong(data[addr]);
}

static PyObject* py_read_vram(PyObject *self, PyObject *args)
{
   unsigned addr;
   size_t max;
   const uint8_t *data = (const uint8_t*)
      pretro_get_memory_data(RETRO_MEMORY_VIDEO_RAM);

   (void)self;

   if (!data)
   {
      Py_INCREF(Py_None);
      return Py_None;
   }

   max = pretro_get_memory_size(RETRO_MEMORY_VIDEO_RAM);

   if (!PyArg_ParseTuple(args, "I", &addr))
      return NULL;

   if (addr >= max)
   {
      Py_INCREF(Py_None);
      return Py_None;
   }

   return PyLong_FromLong(data[addr]);
}


static PyObject *py_read_input(PyObject *self, PyObject *args)
{
   unsigned user, key;
   int16_t res = 0;
   driver_t *driver = driver_get_ptr();
   settings_t *settings = config_get_ptr();
   const struct retro_keybind *py_binds[MAX_USERS] = {
      settings->input.binds[0],
      settings->input.binds[1],
      settings->input.binds[2],
      settings->input.binds[3],
      settings->input.binds[4],
      settings->input.binds[5],
      settings->input.binds[6],
      settings->input.binds[7],
      settings->input.binds[8],
      settings->input.binds[9],
      settings->input.binds[10],
      settings->input.binds[11],
      settings->input.binds[12],
      settings->input.binds[13],
      settings->input.binds[14],
      settings->input.binds[15],
   };
   
   (void)self;

   if (!driver->input_data)
      return PyBool_FromLong(0);

   if (!PyArg_ParseTuple(args, "II", &user, &key))
      return NULL;

   if (user > MAX_USERS || user < 1 || key >= RARCH_FIRST_META_KEY)
      return NULL;

   if (!driver->block_libretro_input)
      res = input_driver_state(py_binds, user - 1, RETRO_DEVICE_JOYPAD, 0, key);
   return PyBool_FromLong(res);
}

static PyObject *py_read_analog(PyObject *self, PyObject *args)
{
   unsigned user, index, id;
   int16_t res = 0;
   driver_t *driver     = driver_get_ptr();
   settings_t *settings = config_get_ptr();
   const struct retro_keybind *py_binds[MAX_USERS] = {
      settings->input.binds[0],
      settings->input.binds[1],
      settings->input.binds[2],
      settings->input.binds[3],
      settings->input.binds[4],
      settings->input.binds[5],
      settings->input.binds[6],
      settings->input.binds[7],
      settings->input.binds[8],
      settings->input.binds[9],
      settings->input.binds[10],
      settings->input.binds[11],
      settings->input.binds[12],
      settings->input.binds[13],
      settings->input.binds[14],
      settings->input.binds[15],
   };

   (void)self;

   if (!driver->input_data)
      return PyBool_FromLong(0);

   if (!PyArg_ParseTuple(args, "III", &user, &index, &id))
      return NULL;

   if (user > MAX_USERS || user < 1 || index > 1 || id > 1)
      return NULL;

   res = input_driver_state(py_binds, user - 1, RETRO_DEVICE_ANALOG, index, id);
   return PyFloat_FromDouble((double)res / 0x7fff);
}

static PyMethodDef RarchMethods[] = {
   { "read_wram",    py_read_wram,   METH_VARARGS, "Read WRAM from system." },
   { "read_vram",    py_read_vram,   METH_VARARGS, "Read VRAM from system." },
   { "input",        py_read_input,  METH_VARARGS, "Read input state from system." },
   { "input_analog", py_read_analog, METH_VARARGS, "Read analog input state from system." },
   { NULL, NULL, 0, NULL }
};

#define DECL_ATTR_RETRO(attr) PyObject_SetAttrString(mod, #attr, PyLong_FromLong(RETRO_DEVICE_ID_JOYPAD_##attr))
static void py_set_attrs(PyObject *mod)
{
   DECL_ATTR_RETRO(B);
   DECL_ATTR_RETRO(Y);
   DECL_ATTR_RETRO(SELECT);
   DECL_ATTR_RETRO(START);
   DECL_ATTR_RETRO(UP);
   DECL_ATTR_RETRO(DOWN);
   DECL_ATTR_RETRO(LEFT);
   DECL_ATTR_RETRO(RIGHT);
   DECL_ATTR_RETRO(A);
   DECL_ATTR_RETRO(X);
   DECL_ATTR_RETRO(L);
   DECL_ATTR_RETRO(R);
   DECL_ATTR_RETRO(L2);
   DECL_ATTR_RETRO(R2);
   DECL_ATTR_RETRO(L3);
   DECL_ATTR_RETRO(R3);

   PyObject_SetAttrString(mod, "ANALOG_LEFT",
         PyLong_FromLong(RETRO_DEVICE_INDEX_ANALOG_LEFT));
   PyObject_SetAttrString(mod, "ANALOG_RIGHT",
         PyLong_FromLong(RETRO_DEVICE_INDEX_ANALOG_RIGHT));
   PyObject_SetAttrString(mod, "ANALOG_X",
         PyLong_FromLong(RETRO_DEVICE_ID_ANALOG_X));
   PyObject_SetAttrString(mod, "ANALOG_Y",
         PyLong_FromLong(RETRO_DEVICE_ID_ANALOG_Y));
}

static PyModuleDef RarchModule = {
   PyModuleDef_HEAD_INIT, "rarch", NULL, -1, RarchMethods,
   NULL, NULL, NULL, NULL
};

static PyObject* PyInit_Retro(void)
{
   PyObject *mod = PyModule_Create(&RarchModule);
   if (!mod)
      return NULL;

   py_set_attrs(mod);
   return mod;
}

struct py_state
{
   PyObject *main;
   PyObject *dict;
   PyObject *inst;

   bool warned_ret;
   bool warned_type;
};

static char *dupe_newline(const char *str)
{
   unsigned size;
   char *ret = NULL;

   if (!str)
      return NULL;

   size = strlen(str) + 2;
   ret = (char*)malloc(size);
   if (!ret)
      return NULL;

   strlcpy(ret, str, size);
   ret[size - 2] = '\n';
   ret[size - 1] = '\0';
   return ret;
}

/* Need to make sure that first-line indentation is 0. */
static char *align_program(const char *program)
{
   size_t prog_size;
   char *new_prog      = NULL;
   char *save          = NULL;
   char *line          = NULL;
   unsigned skip_chars = 0;
   char *prog          = strdup(program);
   if (!prog)
      return NULL;

   prog_size = strlen(program) + 1;
   new_prog = (char*)calloc(1, prog_size);
   if (!new_prog)
   {
      free(prog);
      return NULL;
   }

   line = dupe_newline(strtok_r(prog, "\n", &save));
   if (!line)
   {
      free(prog);
      free(new_prog);
      return NULL;
   }

   while (isblank(line[skip_chars]) && line[skip_chars])
      skip_chars++;

   while (line)
   {
      unsigned length = strlen(line);
      unsigned skip_len = skip_chars > length ? length : skip_chars;

      strlcat(new_prog, line + skip_len, prog_size);

      free(line);
      line = dupe_newline(strtok_r(NULL, "\n", &save));
   }

   free(prog);
   return new_prog;
}

py_state_t *py_state_new(const char *script,
      unsigned is_file, const char *pyclass)
{
   RARCH_LOG("Initializing Python runtime ...\n");
   PyImport_AppendInittab("rarch", &PyInit_Retro);
   Py_Initialize();
   RARCH_LOG("Initialized Python runtime.\n");

   py_state_t *handle = (py_state_t*)calloc(1, sizeof(*handle));
   PyObject *hook = NULL;

   handle->main = PyImport_AddModule("__main__");
   if (!handle->main)
      goto error;
   Py_INCREF(handle->main);

   if (is_file)
   {
      /* Have to hack around the fact that the FILE struct
       * isn't standardized across environments.
       * PyRun_SimpleFile() breaks on Windows because it's 
       * compiled with MSVC. */
      ssize_t len;
      char *script_ = NULL;
      bool ret = read_file(script, (void**)&script_, &len);
      if (!ret || len < 0)
      {
         RARCH_ERR("Python: Failed to read script\n");
         goto error;
      }

      PyRun_SimpleString(script_);
      free(script_);
   }
   else
   {
      char *script_ = align_program(script);
      if (script_)
      {
         PyRun_SimpleString(script_);
         free(script_);
      }
   }

   RARCH_LOG("Python: Script loaded.\n");
   handle->dict = PyModule_GetDict(handle->main);
   if (!handle->dict)
   {
      RARCH_ERR("Python: PyModule_GetDict() failed.\n");
      goto error;
   }
   Py_INCREF(handle->dict);

   hook = PyDict_GetItemString(handle->dict, pyclass);
   if (!hook)
   {
      RARCH_ERR("Python: PyDict_GetItemString() failed.\n");
      goto error;
   }

   handle->inst = PyObject_CallFunction(hook, NULL);
   if (!handle->inst)
   {
      RARCH_ERR("Python: PyObject_CallFunction() failed.\n");
      goto error;
   }
   Py_INCREF(handle->inst);

   return handle;

error:
   PyErr_Print();
   PyErr_Clear();
   py_state_free(handle);
   return NULL;
}

void py_state_free(py_state_t *handle)
{
   if (!handle)
      return;

   PyErr_Print();
   PyErr_Clear();

   Py_CLEAR(handle->inst);
   Py_CLEAR(handle->dict);
   Py_CLEAR(handle->main);

   free(handle);
   Py_Finalize();
}

float py_state_get(py_state_t *handle, const char *id,
      unsigned frame_count)
{
   unsigned i;
   float retval;
   PyObject        *ret = NULL;
   settings_t *settings = config_get_ptr();

   ret = PyObject_CallMethod(handle->inst, (char*)id, (char*)"I", frame_count);

   if (!ret)
   {
      if (!handle->warned_ret)
      {
         RARCH_WARN("Didn't get return value from script. Bug?\n");
         PyErr_Print();
         PyErr_Clear();
      }

      handle->warned_ret = true;
      return 0.0f;
   }

   retval = (float)PyFloat_AsDouble(ret);
   Py_DECREF(ret);
   return retval;
}
