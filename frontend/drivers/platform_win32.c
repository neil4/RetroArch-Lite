/* RetroArch - A frontend for libretro.
 * Copyright (C) 2011-2015 - Daniel De Matteis
 *
 * RetroArch is free software: you can redistribute it and/or modify it under the terms
 * of the GNU General Public License as published by the Free Software Found-
 * ation, either version 3 of the License, or (at your option) any later version.
 *
 * RetroArch is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY;
 * without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR
 * PURPOSE. See the GNU General Public License for more details.
 * * You should have received a copy of the GNU General Public License along with RetroArch.
 * If not, see <http://www.gnu.org/licenses/>.
 */

#include <stdint.h>
#include <boolean.h>
#include <stddef.h>
#include <string.h>

#include <windows.h>

#include <retro_miscellaneous.h>
#include <dynamic/dylib.h>
#include <file/file_list.h>
#include <file/file_path.h>

#include "../frontend_driver.h"
#include "../../general.h"
#include "../../menu/menu.h"

#if defined(_WIN32)
/* We only load this library once, so we let it be 
 * unloaded at application shutdown, since unloading 
 * it early seems to cause issues on some systems.
 */

static dylib_t dwmlib;

static bool dwm_composition_disabled;

static bool console_needs_free;

static void gfx_dwm_shutdown(void)
{
   if (dwmlib)
      dylib_close(dwmlib);
   dwmlib = NULL;
}

static bool gfx_init_dwm(void)
{
   static bool inited = false;

   if (inited)
      return true;

   dwmlib = dylib_load("dwmapi.dll");
   if (!dwmlib)
   {
      RARCH_LOG("Did not find dwmapi.dll.\n");
      return false;
   }
   atexit(gfx_dwm_shutdown);

   HRESULT (WINAPI *mmcss)(BOOL) = 
      (HRESULT (WINAPI*)(BOOL))dylib_proc(dwmlib, "DwmEnableMMCSS");
   if (mmcss)
   {
      RARCH_LOG("Setting multimedia scheduling for DWM.\n");
      mmcss(TRUE);
   }

   inited = true;
   return true;
}

static void gfx_set_dwm(void)
{
   HRESULT ret;
   settings_t *settings = config_get_ptr();

   if (!gfx_init_dwm())
      return;

   if (settings->video.disable_composition == dwm_composition_disabled)
      return;

   HRESULT (WINAPI *composition_enable)(UINT) = 
      (HRESULT (WINAPI*)(UINT))dylib_proc(dwmlib, "DwmEnableComposition");
   if (!composition_enable)
   {
      RARCH_ERR("Did not find DwmEnableComposition ...\n");
      return;
   }

   ret = composition_enable(!settings->video.disable_composition);
   if (FAILED(ret))
      RARCH_ERR("Failed to set composition state ...\n");
   dwm_composition_disabled = settings->video.disable_composition;
}
#endif

static void frontend_win32_get_os(char *s, size_t len, int *major, int *minor)
{
   char buildStr[11]      = {0};
   bool server            = false;
   const char *arch       = "";

#if defined(_WIN32_WINNT) && _WIN32_WINNT >= 0x0500
   /* Windows 2000 and later */
   SYSTEM_INFO si         = {{0}};
   OSVERSIONINFOEX vi     = {0};
   vi.dwOSVersionInfoSize = sizeof(OSVERSIONINFOEX);

   GetSystemInfo(&si);

   /* Available from NT 3.5 and Win95 */
   GetVersionEx((OSVERSIONINFO*)&vi);

   server = vi.wProductType != VER_NT_WORKSTATION;

   switch (si.wProcessorArchitecture)
   {
      case PROCESSOR_ARCHITECTURE_AMD64:
         arch = "x64";
         break;
      case PROCESSOR_ARCHITECTURE_INTEL:
         arch = "x86";
         break;
      case PROCESSOR_ARCHITECTURE_ARM:
         arch = "ARM";
         break;
      default:
         break;
   }
#else
   OSVERSIONINFO vi = {0};
   vi.dwOSVersionInfoSize = sizeof(OSVERSIONINFO);

   /* Available from NT 3.5 and Win95 */
   GetVersionEx(&vi);
#endif

   if (major)
      *major = vi.dwMajorVersion;

   if (minor)
      *minor = vi.dwMinorVersion;

   if (vi.dwMajorVersion == 4 && vi.dwMinorVersion == 0)
      snprintf(buildStr, sizeof(buildStr), "%lu", (DWORD)(LOWORD(vi.dwBuildNumber))); /* Windows 95 build number is in the low-order word only */
   else
      snprintf(buildStr, sizeof(buildStr), "%lu", vi.dwBuildNumber);

   switch (vi.dwMajorVersion)
   {
      case 10:
         if (server)
            strlcpy(s, "Windows Server 2016", len);
         else
            strlcpy(s, "Windows 10", len);
         break;
      case 6:
         switch (vi.dwMinorVersion)
         {
            case 3:
               if (server)
                  strlcpy(s, "Windows Server 2012 R2", len);
               else
                  strlcpy(s, "Windows 8.1", len);
               break;
            case 2:
               if (server)
                  strlcpy(s, "Windows Server 2012", len);
               else
                  strlcpy(s, "Windows 8", len);
               break;
            case 1:
               if (server)
                  strlcpy(s, "Windows Server 2008 R2", len);
               else
                  strlcpy(s, "Windows 7", len);
               break;
            case 0:
               if (server)
                  strlcpy(s, "Windows Server 2008", len);
               else
                  strlcpy(s, "Windows Vista", len);
               break;
            default:
               break;
         }
         break;
      case 5:
         switch (vi.dwMinorVersion)
         {
            case 2:
               if (server)
               {
                  strlcpy(s, "Windows Server 2003", len);
                  if (GetSystemMetrics(SM_SERVERR2))
                     strlcat(s, " R2", len);
               }
               else
               {
                  /* Yes, XP Pro x64 is a higher version number than XP x86 */
                  if (!strcmp(arch, "x64"))
                     strlcpy(s, "Windows XP", len);
               }
               break;
            case 1:
               strlcpy(s, "Windows XP", len);
               break;
            case 0:
               strlcpy(s, "Windows 2000", len);
               break;
         }
         break;
      case 4:
         switch (vi.dwMinorVersion)
         {
            case 0:
               if (vi.dwPlatformId == VER_PLATFORM_WIN32_WINDOWS)
                  strlcpy(s, "Windows 95", len);
               else if (vi.dwPlatformId == VER_PLATFORM_WIN32_NT)
                  strlcpy(s, "Windows NT 4.0", len);
               else
                  strlcpy(s, "Unknown", len);
               break;
            case 90:
               strlcpy(s, "Windows ME", len);
               break;
            case 10:
               strlcpy(s, "Windows 98", len);
               break;
         }
         break;
      default:
         snprintf(s, len, "Windows %i.%i", *major, *minor);
         break;
   }

   if (!*arch)
   {
      strlcat(s, " ", len);
      strlcat(s, arch, len);
   }

   strlcat(s, " Build ", len);
   strlcat(s, buildStr, len);

   if (!vi.szCSDVersion[0])
   {
      strlcat(s, " ", len);
      strlcat(s, vi.szCSDVersion, len);
   }
}

static void frontend_win32_init(void *data)
{
	typedef BOOL (WINAPI *isProcessDPIAwareProc)();
	typedef BOOL (WINAPI *setProcessDPIAwareProc)();
	HMODULE handle                         = GetModuleHandle(TEXT("User32.dll"));
	isProcessDPIAwareProc  isDPIAwareProc  = (isProcessDPIAwareProc)dylib_proc(handle, "IsProcessDPIAware");
	setProcessDPIAwareProc setDPIAwareProc = (setProcessDPIAwareProc)dylib_proc(handle, "SetProcessDPIAware");

	if (isDPIAwareProc)
	{
		if (!isDPIAwareProc())
		{
			if (setDPIAwareProc)
				setDPIAwareProc();
		}
	}
   
   gfx_set_dwm();
}

enum frontend_powerstate frontend_win32_get_powerstate(int *seconds, int *percent)
{
    SYSTEM_POWER_STATUS status;
	enum frontend_powerstate ret = FRONTEND_POWERSTATE_NONE;

	if (!GetSystemPowerStatus(&status))
		return ret;

	if (status.BatteryFlag == 0xFF)
		ret = FRONTEND_POWERSTATE_NONE;
	if (status.BatteryFlag & (1 << 7))
		ret = FRONTEND_POWERSTATE_NO_SOURCE;
	else if (status.BatteryFlag & (1 << 3))
		ret = FRONTEND_POWERSTATE_CHARGING;
	else if (status.ACLineStatus == 1)
		ret = FRONTEND_POWERSTATE_CHARGED;
	else
		ret = FRONTEND_POWERSTATE_ON_POWER_SOURCE;

	*percent  = (int)status.BatteryLifePercent;
	*seconds  = (int)status.BatteryLifeTime;

	return ret;
}

enum frontend_architecture frontend_win32_get_architecture(void)
{
   /* stub */
   return FRONTEND_ARCH_NONE;
}

static int frontend_win32_parse_drive_list(void *data)
{
   size_t i = 0;
   unsigned drives = GetLogicalDrives();
   char    drive[] = " :\\";
   file_list_t *list = (file_list_t*)data;

   for (i = 0; i < 32; i++)
   {
      drive[0] = 'A' + i;
      if (drives & (1 << i))
         menu_list_push(list,
               drive, "", MENU_FILE_DIRECTORY, 0, 0);
   }

   return 0;
}

static void frontend_win32_get_environment_settings(int *argc, char *argv[],
      void *args, void *params_data)
{
   HMODULE hModule = GetModuleHandleW(NULL);
   WCHAR exe_path[PATH_MAX_LENGTH];
   char exe_dir[PATH_MAX_LENGTH];
   
   GetModuleFileNameW(hModule, exe_path, PATH_MAX_LENGTH);
   wcstombs(exe_dir, exe_path, PATH_MAX_LENGTH);
   path_parent_dir(exe_dir);
   
   fill_pathname_join(g_defaults.core_info_dir, exe_dir, "info",
                      sizeof(g_defaults.core_info_dir));
   fill_pathname_join(g_defaults.core_dir, exe_dir, "cores",
                      sizeof(g_defaults.core_dir));
   fill_pathname_join(g_defaults.menu_config_dir, exe_dir, "config",
                      sizeof(g_defaults.menu_config_dir));
   fill_pathname_join(g_defaults.savestate_dir, exe_dir, "state",
                      sizeof(g_defaults.savestate_dir));
   fill_pathname_join(g_defaults.sram_dir, exe_dir, "save",
                      sizeof(g_defaults.sram_dir));
   fill_pathname_join(g_defaults.system_dir, exe_dir, "system",
                      sizeof(g_defaults.system_dir));
   fill_pathname_join(g_defaults.shader_dir, exe_dir, "shaders_glsl",
                      sizeof(g_defaults.shader_dir));
   fill_pathname_join(g_defaults.video_filter_dir, exe_dir, "video_filters",
                      sizeof(g_defaults.video_filter_dir));
   fill_pathname_join(g_defaults.overlay_dir, exe_dir, "overlays",
                      sizeof(g_defaults.overlay_dir));
   fill_pathname_join(g_defaults.osk_overlay_dir, exe_dir, "overlays\\keyboards",
                      sizeof(g_defaults.osk_overlay_dir));
   fill_pathname_join(g_defaults.menu_theme_dir, exe_dir, "themes_rgui",
                      sizeof(g_defaults.menu_theme_dir));
   fill_pathname_join(g_defaults.audio_filter_dir, exe_dir, "audio_filters",
                      sizeof(g_defaults.audio_filter_dir));
}

static void frontend_win32_attach_console(void)
{
#ifdef _WIN32
#ifdef _WIN32_WINNT_WINXP
   /* msys will start the process with FILE_TYPE_PIPE connected.
    *   cmd will start the process with FILE_TYPE_UNKNOWN connected
    *   (since this is subsystem windows application
    * ... UNLESS stdout/stderr were redirected (then FILE_TYPE_DISK
    * will be connected most likely)
    * explorer will start the process with NOTHING connected.
    *
    * Now, let's not reconnect anything that's already connected.
    * If any are disconnected, open a console, and connect to them.
    * In case we're launched from msys or cmd, try attaching to the
    * parent process console first.
    *
    * Take care to leave a record of what we did, so we can
    * undo it precisely.
    */

   bool need_stdout = (GetFileType(GetStdHandle(STD_OUTPUT_HANDLE))
         == FILE_TYPE_UNKNOWN);
   bool need_stderr = (GetFileType(GetStdHandle(STD_ERROR_HANDLE))
         == FILE_TYPE_UNKNOWN);

   if(need_stdout || need_stderr)
   {
      if(!AttachConsole(ATTACH_PARENT_PROCESS))
         AllocConsole();

      SetConsoleTitle("Log Console");

      if(need_stdout) freopen( "CONOUT$", "w", stdout );
      if(need_stderr) freopen( "CONOUT$", "w", stderr );

      console_needs_free = true;
   }

#endif
#endif
}

static void frontend_win32_detach_console(void)
{
#if defined(_WIN32) && !defined(_XBOX)
#ifdef _WIN32_WINNT_WINXP
   if(console_needs_free)
   {
      /* we don't reconnect stdout/stderr to anything here,
       * because by definition, they weren't connected to
       * anything in the first place. */
      FreeConsole();
      console_needs_free = false;
   }
#endif
#endif
}

const frontend_ctx_driver_t frontend_ctx_win32 = {
   frontend_win32_get_environment_settings,
   frontend_win32_init,
   NULL,                           /* deinit */
   NULL,                           /* exitspawn */
   NULL,                           /* process_args */
   NULL,                           /* exec */
   NULL,                           /* set_fork */
   NULL,                           /* shutdown */
   NULL,                           /* get_name */
   frontend_win32_get_os,
   NULL,                           /* get_rating */
   NULL,                           /* load_content */
   frontend_win32_get_architecture,
   frontend_win32_get_powerstate,
   frontend_win32_parse_drive_list,
   frontend_win32_attach_console,   /* attach_console */
   frontend_win32_detach_console,   /* detach_console */
   "win32",
};
