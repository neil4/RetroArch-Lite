/*  RetroArch - A frontend for libretro.
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

#ifndef RGUI_H
#define RGUI_H

/* globals */
bool rgui_wallpaper_valid;
uint32_t rgui_hover_color;
uint32_t rgui_normal_color;
uint32_t rgui_title_color;
uint32_t rgui_bg_dark_color;
uint32_t rgui_bg_light_color;
uint32_t rgui_border_dark_color;
uint32_t rgui_border_light_color;

/* default colors (clean green) */
#define rgui_hover_color_default 0xFF64FF64
#define rgui_normal_color_default 0xFFFFFFFF
#define rgui_title_color_default 0xFF64FF64
#define rgui_bg_dark_color_default 0xC0303030
#define rgui_bg_light_color_default 0xC0303030
#define rgui_border_dark_color_default 0xC0408040
#define rgui_border_light_color_default 0xC0408040

#endif /* RGUI_H */

