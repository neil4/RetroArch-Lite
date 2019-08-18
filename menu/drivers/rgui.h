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

/* default colors (clean green) */
#define rgui_hover_32b_default 0xFF64FF64
#define rgui_normal_32b_default 0xFFFFFFFF
#define rgui_title_32b_default 0xFF64FF64
#define rgui_bg_dark_32b_default 0xC0303030
#define rgui_bg_light_32b_default 0xC0303030
#define rgui_border_dark_32b_default 0xC0408040
#define rgui_border_light_32b_default 0xC0408040
#define rgui_particle_32b_default 0xC0879E87

enum rgui_particle_animation_effect
{
   RGUI_PARTICLE_EFFECT_NONE = 0,
   RGUI_PARTICLE_EFFECT_SNOW,
   RGUI_PARTICLE_EFFECT_SNOW_ALT,
   RGUI_PARTICLE_EFFECT_RAIN,
   RGUI_PARTICLE_EFFECT_VORTEX,
   RGUI_PARTICLE_EFFECT_STARFIELD,
   NUM_RGUI_PARTICLE_EFFECTS
};

#endif /* RGUI_H */

