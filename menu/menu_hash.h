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

#ifndef MENU_HASH_H__
#define MENU_HASH_H__

#ifdef __cplusplus
extern "C" {
#endif

#define MENU_LABEL_RESET                                                       0x10474288U
#define MENU_LABEL_SLOWMOTION                                                  0x6a269ea0U
#define MENU_LABEL_HOLD_FAST_FORWARD                                           0xebe2e4cdU
#define MENU_LABEL_CHEAT_TOGGLE                                                0xe515e0cbU
#define MENU_LABEL_PAUSE_TOGGLE                                                0x557634e4U

#define MENU_LABEL_VALUE_DIRECTORY_NOT_FOUND                                   0xcdf3c0d5U
#define MENU_LABEL_VALUE_NO_ITEMS                                              0x7d33e412U

#define MENU_LABEL_VALUE_SHADER_OPTIONS                                        0xf3fb0028U
#define MENU_LABEL_VALUE_USE_THIS_DIRECTORY                                    0xc5fc9ed9U
#define MENU_LABEL_USE_THIS_DIRECTORY                                          0xc51d351dU
#define MENU_LABEL_VALUE_NO_SHADER_PARAMETERS                                  0x8ccc809bU
#define MENU_LABEL_VALUE_NO_CORE_INFORMATION_AVAILABLE                         0x2a11fe80U
#define MENU_LABEL_VALUE_NO_CORES_AVAILABLE                                    0xe16bfd0dU
#define MENU_LABEL_VALUE_NO_PLAYLIST_ENTRIES_AVAILABLE                         0xea82695dU
#define MENU_LABEL_SAVE_STATE                                                  0x54a257f4U
#define MENU_LABEL_VALUE_LOAD_STATE                                            0xd23ba706U
#define MENU_LABEL_VALUE_SAVE_STATE                                            0x3e182415U
#define MENU_LABEL_LOAD_STATE                                                  0xe8c5dae5U
#define MENU_LABEL_REWIND                                                      0x1931d5aeU
#define MENU_LABEL_NETPLAY_FLIP_PLAYERS                                        0x801425abU
#define MENU_LABEL_CHEAT_INDEX_MINUS                                           0x57f58b6cU
#define MENU_LABEL_CHEAT_INDEX_PLUS                                            0x678542a4U
#define MENU_LABEL_AUDIO_ENABLE                                                0x28614f5dU
#define MENU_LABEL_VALUE_AUDIO_ENABLE                                          0xcdbb9b9eU
#define MENU_LABEL_SCREENSHOT_DIRECTORY                                        0x552612d7U
#define MENU_LABEL_SHADER_NEXT                                                 0x54d359baU
#define MENU_LABEL_SHADER_PREV                                                 0x54d4a758U
#define MENU_LABEL_FRAME_ADVANCE                                               0xd80302a1U
#define MENU_LABEL_FPS_SHOW                                                    0x5ea1e10eU
#define MENU_LABEL_VALUE_FPS_SHOW                                              0x92588792U
#define MENU_LABEL_L_X_PLUS                                                    0xd7370d4bU
#define MENU_LABEL_L_X_MINUS                                                   0xbde0aaf3U
#define MENU_LABEL_L_Y_PLUS                                                    0xd98c35ecU
#define MENU_LABEL_L_Y_MINUS                                                   0x0adae7b4U
#define MENU_LABEL_R_X_PLUS                                                    0x60c20a91U
#define MENU_LABEL_R_X_MINUS                                                   0x78cb50f9U
#define MENU_LABEL_R_Y_MINUS                                                   0xc5c58dbaU
#define MENU_LABEL_R_Y_PLUS                                                    0x63173332U
#define MENU_LABEL_VIDEO_SWAP_INTERVAL                                         0x5673ff9aU
#define MENU_LABEL_VIDEO_FAKE_SWAP_INTERVAL                                    0xc83434f0U
#define MENU_LABEL_VIDEO_GPU_SCREENSHOT                                        0xee2fcb44U
#define MENU_LABEL_PAUSE_NONACTIVE                                             0x580bf549U
#define MENU_LABEL_BLOCK_SRAM_OVERWRITE                                        0xc4e88d08U
#define MENU_LABEL_VIDEO_FULLSCREEN                                            0x9506dd4eU
#define MENU_LABEL_CORE_SPECIFIC_CONFIG                                        0x3c9a55e8U
#define MENU_LABEL_RGUI_SHOW_START_SCREEN                                      0x6b38f0e8U
#define MENU_LABEL_VIDEO_BLACK_FRAME_INSERTION                                 0x53477f5cU
#define MENU_LABEL_VIDEO_HARD_SYNC_FRAMES                                      0xce0ece13U
#define MENU_LABEL_VIDEO_FRAME_DELAY                                           0xd4aa9df4U
#define MENU_LABEL_SCREENSHOT                                                  0x9a37f083U
#define MENU_LABEL_REWIND_GRANULARITY                                          0xe859cbdfU
#define MENU_LABEL_VALUE_REWIND_GRANULARITY                                    0x6e1ae4c0U
#define MENU_LABEL_VIDEO_THREADED                                              0x0626179cU
#define MENU_LABEL_AUDIO_SYNC                                                  0xe0cd6bd3U
#define MENU_LABEL_VALUE_AUDIO_SYNC                                            0xcbeb903bU
#define MENU_LABEL_VIDEO_VSYNC                                                 0x09c2d34eU
#define MENU_LABEL_VIDEO_HARD_SYNC                                             0xdcd623b6U
#define MENU_LABEL_SAVESTATE_AUTO_SAVE                                         0xf6f4a05bU
#define MENU_LABEL_SYSTEM_DIRECTORY                                            0x35a6fb9eU
#define MENU_LABEL_VIDEO_DISABLE_COMPOSITION                                   0x5cbb6222U
#define MENU_LABEL_VOLUME_UP                                                   0xa66e9681U
#define MENU_LABEL_VOLUME_DOWN                                                 0xfc64f3d4U
#define MENU_LABEL_LOG_VERBOSITY                                               0x6648c96dU
#define MENU_LABEL_OVERLAY_NEXT                                                0x7a459145U
#define MENU_LABEL_AUDIO_VOLUME                                                0x502173aeU
#define MENU_LABEL_VALUE_AUDIO_VOLUME                                          0x0fa6ccfeU
#define MENU_LABEL_AUDIO_LATENCY                                               0x32695386U
#define MENU_LABEL_VALUE_AUDIO_LATENCY                                         0x89900e38U
#define MENU_LABEL_PREEMPTIVE_FRAMES                                           0x823f2a1fU
#define MENU_LABEL_NETPLAY_DELAY_FRAMES                                        0x86b2c48dU
#define MENU_LABEL_NETPLAY_IP_ADDRESS                                          0xac9a53ffU
#define MENU_LABEL_NETPLAY_MODE                                                0xc1cf6506U
#define MENU_LABEL_PERFCNT_ENABLE                                              0x6823dbddU
#define MENU_LABEL_OVERLAY_SCALE                                               0x2dce2a3dU
#define MENU_LABEL_VALUE_OVERLAY_SCALE                                         0x4237794fU
#define MENU_LABEL_OVERLAY_BISECT_TO_ASPECT                                    0x0c535c4cU
#define MENU_LABEL_OVERLAY_SHIFT_Y                                             0x38b82639U
#define MENU_LABEL_OVERLAY_SHIFT_Y_LOCK_EDGES                                  0x7e98bce8U
#define MENU_LABEL_OVERLAY_MOUSE_HOLD_TO_DRAG                                  0x043156a3U
#define MENU_LABEL_OVERLAY_MOUSE_HOLD_MS                                       0x3b937a63U
#define MENU_LABEL_OVERLAY_MOUSE_HOLD_ZONE                                     0x6e62bd9fU
#define MENU_LABEL_OVERLAY_MOUSE_CLICK_DUR                                     0x7ab5550dU
#define MENU_LABEL_KEYBOARD_OVERLAY_PRESET                                     0x11f1c582U
#define MENU_LABEL_VALUE_KEYBOARD_OVERLAY_PRESET                               0x93ac8293U
#define MENU_LABEL_OVERLAY_PRESET                                              0x24e24796U
#define MENU_LABEL_VALUE_OVERLAY_PRESET                                        0x8338e89aU
#define MENU_LABEL_OVERLAY_ASPECT_RATIO_INDEX                                  0x8fda534aU
#define MENU_LABEL_OVERLAY_DPAD_EIGHTWAY_METHOD                                0x6fbc2cceU
#define MENU_LABEL_OVERLAY_ABXY_EIGHTWAY_METHOD                                0x7fa6bac9U
#define MENU_LABEL_OVERLAY_DPAD_DIAGONAL_SENS                                  0x1de994f6U
#define MENU_LABEL_OVERLAY_ABXY_DIAGONAL_SENS                                  0x2dcf7e31U
#define MENU_LABEL_OVERLAY_EIGHTWAY_TOUCH_MAGNIFY                              0x82fc628eU
#define MENU_LABEL_LIGHTGUN_TRIGGER_DELAY                                      0x60f3b257U
#define MENU_LABEL_OVERLAY_OPACITY                                             0xc466fbaeU
#define MENU_LABEL_VALUE_OVERLAY_OPACITY                                       0x98605740U
#define MENU_LABEL_OSK_OPACITY                                                 0x03a22bb9U
#define MENU_LABEL_VALUE_OSK_OPACITY                                           0x850837efU

#define MENU_LABEL_MENU_THEME                                                  0x7bf56e4cU
#define MENU_LABEL_VALUE_MENU_THEME                                            0x656b3a6dU
#define MENU_LABEL_MENU_WALLPAPER                                              0x3b84de01U
#define MENU_LABEL_VALUE_MENU_WALLPAPER                                        0x4555d2a2U
#define MENU_LABEL_DYNAMIC_WALLPAPER                                           0xf011ccabU
#define MENU_LABEL_VALUE_DYNAMIC_WALLPAPER                                     0x66928c32U
#define MENU_LABEL_VALUE_BOXART                                                0x716441ebU
#define MENU_LABEL_BOXART                                                      0xa269b0afU
#define MENU_LABEL_PAL60_ENABLE                                                0x62bc416eU
#define MENU_LABEL_SYSTEM_BGM_ENABLE                                           0x9287a1c5U

#define MENU_LABEL_FASTFORWARD_RATIO                                           0x3a0c2706U
#define MENU_LABEL_CORE_THROTTLE_ENABLE                                        0x87359049U
#define MENU_LABEL_VIDEO_MONITOR_INDEX                                         0xb6fcdc9aU
#define MENU_LABEL_VIDEO_REFRESH_RATE_AUTO                                     0x9addb6cdU
#define MENU_LABEL_VIDEO_ROTATION                                              0x4ce6882bU
#define MENU_LABEL_VIDEO_SCALE                                                 0x09835d63U
#define MENU_LABEL_VIDEO_SMOOTH                                                0x3aabbb35U
#define MENU_LABEL_VIDEO_CROP_OVERSCAN                                         0x861f7a2fU
#define MENU_LABEL_VIDEO_SCALE_INTEGER                                         0x65c4b090U

#define MENU_LABEL_AUDIO_RATE_CONTROL_DELTA                                    0xc8bde3cbU
#define MENU_LABEL_VALUE_AUDIO_RATE_CONTROL_DELTA                              0x8d242b0eU
#define MENU_LABEL_AUDIO_MAX_TIMING_SKEW                                       0x4c96f75cU

#define MENU_LABEL_INPUT_PLAYER1_JOYPAD_INDEX                                  0xfad6ab2fU
#define MENU_LABEL_INPUT_PLAYER2_JOYPAD_INDEX                                  0x3616e4d0U
#define MENU_LABEL_INPUT_PLAYER3_JOYPAD_INDEX                                  0x71571e71U
#define MENU_LABEL_INPUT_PLAYER4_JOYPAD_INDEX                                  0xac975812U
#define MENU_LABEL_INPUT_PLAYER5_JOYPAD_INDEX                                  0xe7d791b3U

#define MENU_LABEL_LIBRETRO_DIR_PATH                                           0x1af1eb72U

#define MENU_LABEL_AUDIO_DEVICE                                                0x2574eac6U
#define MENU_LABEL_VALUE_AUDIO_DEVICE                                          0xcacf3707U

#define MENU_LABEL_REWIND_ENABLE                                               0x9761e074U
#define MENU_LABEL_VALUE_REWIND_ENABLE                                         0xce8cc18eU
#define MENU_LABEL_ENABLE_HOTKEY                                               0xc04037bfU
#define MENU_LABEL_KEYBOARD_FOCUS_HOTKEY                                       0x423d38f6U
#define MENU_LABEL_DISK_EJECT_TOGGLE                                           0x49633fbbU
#define MENU_LABEL_DISK_NEXT                                                   0xeeaf6c6eU
#define MENU_LABEL_GRAB_MOUSE_TOGGLE                                           0xb2869aaaU
#define MENU_LABEL_MENU_TOGGLE                                                 0xfb22e3dbU
#define MENU_LABEL_MENU_TOGGLE_BTN_COMBO                                       0x8f14b89cU
#define MENU_LABEL_STATE_SLOT_DECREASE                                         0xe48b8082U
#define MENU_LABEL_STATE_SLOT_INCREASE                                         0x36a0cbb0U

#define MENU_LABEL_LIBRETRO_LOG_LEVEL                                          0x57971ac0U

#define MENU_LABEL_INPUT_BIND_DEVICE_ID                                        0xd1ea94ecU
#define MENU_LABEL_INPUT_BIND_DEVICE_TYPE                                      0xf6e9f041U

#define MENU_LABEL_AUTOSAVE_INTERVAL                                           0xecc87351U
#define MENU_LABEL_CONFIG_SAVE_ON_EXIT                                         0x79b590feU

#define MENU_LABEL_INPUT_DRIVER                                                0x4c087840U
#define MENU_LABEL_INPUT_DRIVER_LINUXRAW                                       0xc33c6b9fU
#define MENU_LABEL_INPUT_DRIVER_UDEV                                           0x7c9eeeb9U

#define MENU_LABEL_VIDEO_DRIVER                                                0x1805a5e7U
#define MENU_LABEL_VIDEO_DRIVER_GL                                             0x005977f8U
#define MENU_LABEL_VIDEO_DRIVER_SDL2                                           0x7c9dd69aU
#define MENU_LABEL_VIDEO_DRIVER_SDL1                                           0x0b88a968U
#define MENU_LABEL_VIDEO_DRIVER_D3D                                            0x0b886340U
#define MENU_LABEL_VIDEO_DRIVER_EXYNOS                                         0xfc37c54bU
#define MENU_LABEL_VIDEO_DRIVER_SUNXI                                          0x10620e3cU

#define MENU_LABEL_AUDIO_RESAMPLER_DRIVER                                      0xedcba9ecU
#define MENU_LABEL_AUDIO_RESAMPLER_DRIVER_SINC                                 0x7c9dec52U
#define MENU_LABEL_AUDIO_RESAMPLER_DRIVER_CC                                   0x0059732bU
#define MENU_LABEL_AUDIO_RESAMPLER_DRIVER_NEAREST                              0x09fa48d7U

#define MENU_LABEL_SLOWMOTION_RATIO                                            0x626b3ffeU
#define MENU_LABEL_INPUT_AXIS_THRESHOLD                                        0xe95c2095U
#define MENU_LABEL_INPUT_AUTODETECT_ENABLE                                     0xb1e07facU
#define MENU_LABEL_INPUT_HOST_DEVICE_1                                         0xfad6ab2fU
#define MENU_LABEL_INPUT_HOST_DEVICE_2                                         0x3616e4d0U
#define MENU_LABEL_INPUT_HOST_DEVICE_3                                         0x71571e71U
#define MENU_LABEL_INPUT_HOST_DEVICE_4                                         0xac975812U
#define MENU_LABEL_INPUT_LIBRETRO_DEVICE_1                                     0x4d049cc6U
#define MENU_LABEL_INPUT_LIBRETRO_DEVICE_2                                     0x4d049cc7U
#define MENU_LABEL_INPUT_LIBRETRO_DEVICE_3                                     0x4d049cc8U
#define MENU_LABEL_INPUT_LIBRETRO_DEVICE_4                                     0x4d049cc9U
#define MENU_LABEL_INPUT_BIND_ALL_PORT_1                                       0xa1013646U
#define MENU_LABEL_INPUT_BIND_ALL_PORT_1                                       0xa1013646U
#define MENU_LABEL_INPUT_BIND_ALL_PORT_2                                       0xa50bc767U
#define MENU_LABEL_INPUT_BIND_ALL_PORT_3                                       0xa9165888U
#define MENU_LABEL_INPUT_BIND_ALL_PORT_4                                       0xad20e9a9U
#define MENU_LABEL_INPUT_TURBO_BIND_ENABLE                                     0x024920d5U
#define MENU_LABEL_INPUT_ANALOG_DPAD_MODE                                      0xc39b1842U
#define MENU_LABEL_INPUT_ANALOG_DPAD_DIAGONAL_SENS                             0x95ed878eU
#define MENU_LABEL_INPUT_BIND_MODE                                             0x90281b55U
#define MENU_LABEL_INPUT_LIGHTGUN_ALLOW_OOB                                    0xcda84fb3U

#define MENU_LABEL_CAMERA_ALLOW                                                0xc14d302cU
#define MENU_LABEL_LOCATION_ALLOW                                              0xf089275cU

#define MENU_LABEL_VIDEO_SHADER_DEFAULT_FILTER                                 0x4468cb1bU
#define MENU_LABEL_VIDEO_SHADER_FILTER_PASS                                    0x1906c38dU
#define MENU_LABEL_VIDEO_SHADER_SCALE_PASS                                     0x18f7b82fU
#define MENU_LABEL_VALUE_VIDEO_SHADER_NUM_PASSES                               0xb354b30bU
#define MENU_LABEL_VIDEO_SHADER_NUM_PASSES                                     0x79b2992fU
#define MENU_LABEL_CHEAT_NUM_PASSES                                            0x1910eb87U

#define MENU_VALUE_MD5                                                         0x0b888fabU
#define MENU_VALUE_SHA1                                                        0x7c9de632U
#define MENU_VALUE_CRC                                                         0x0b88671dU
#define MENU_VALUE_MORE                                                        0x0b877cafU
#define MENU_VALUE_HORIZONTAL_MENU                                             0x35761704U
#define MENU_VALUE_MAIN_MENU                                                   0x1625971fU   
#define MENU_LABEL_SETTINGS                                                    0x1304dc16U
#define MENU_LABEL_VALUE_SETTINGS                                              0x8aca3ff6U
#define MENU_LABEL_VALUE_OVERLAY_KEYBOARD_SETTINGS                             0x724f6c09U
#define MENU_LABEL_VALUE_OVERLAY_MOUSE_SETTINGS                                0x5a16d0e1U
#define MENU_LABEL_VALUE_OVERLAY_LIGHTGUN_SETTINGS                             0x8988b29aU
#define MENU_LABEL_VALUE_MENU_VISIBILITIES                                     0xc4b850b1U
#define MENU_VALUE_RECORDING_SETTINGS                                          0x1a80b313U
#define MENU_VALUE_INPUT_SETTINGS                                              0xddd30846U
#define MENU_VALUE_ON                                                          0x005974c2U
#define MENU_VALUE_OFF                                                         0x0b880c40U
#define MENU_VALUE_TRUE                                                        0x7c9e9fe5U
#define MENU_VALUE_FALSE                                                       0x0f6bcef0U
#define MENU_VALUE_COMP                                                        0x6a166ba5U
#define MENU_VALUE_CORE                                                        0x6a167f7fU
#define MENU_VALUE_CURSOR                                                      0x57bba8b4U
#define MENU_VALUE_FILE                                                        0x6a496536U
#define MENU_VALUE_RDB                                                         0x0b00f54eU
#define MENU_VALUE_DIR                                                         0x0af95f55U
#define MENU_VALUE_NO_CORE                                                     0x7d5472cbU
#define MENU_VALUE_DETECT                                                      0xab8da89eU
#define MENU_VALUE_GLSLP                                                       0x0f840c87U
#define MENU_VALUE_CGP                                                         0x0b8865bfU
#define MENU_VALUE_GLSL                                                        0x7c976537U
#define MENU_VALUE_CG                                                          0x0059776fU

#define MENU_LABEL_CONTENT_ACTIONS                                             0xa0d76970U
#define MENU_LABEL_DETECT_CORE_LIST                                            0xaa07c341U
#define MENU_LABEL_VALUE_DETECT_CORE_LIST                                      0x69f11813U
#define MENU_LABEL_DETECT_CORE_LIST_OK                                         0xabba2a7aU
#define MENU_LABEL_LOAD_CONTENT                                                0x5745de1fU
#define MENU_LABEL_VALUE_LOAD_CONTENT                                          0x755f3680U
#define MENU_LABEL_VALUE_CORE_UPDATER_LIST                                     0x0372767dU
#define MENU_LABEL_CORE_UPDATER_LIST                                           0xe12f4ee3U
#define MENU_LABEL_DOWNLOADABLE_CORE                                           0x4d82fb99U
#define MENU_LABEL_UNLOAD_CORE                                                 0x4b622170U
#define MENU_LABEL_VALUE_UNLOAD_CORE                                           0x9b45e571U
#define MENU_LABEL_QUIT_RETROARCH                                              0x84b0bc71U
#define MENU_LABEL_VALUE_QUIT_RETROARCH                                        0x8e7024f2U
#define MENU_LABEL_DEFERRED_VIDEO_FILTER                                       0x966ad201U
#define MENU_LABEL_VALUE_STARTING_DOWNLOAD                                     0x42e10f03U
#define MENU_LABEL_DEFERRED_CORE_LIST                                          0xf157d289U
#define MENU_LABEL_DEFERRED_CORE_UPDATER_LIST                                  0xbd4d493dU
#define MENU_LABEL_DISK_IMAGE_APPEND                                           0x5af7d709U
#define MENU_LABEL_PERFORMANCE_COUNTERS                                        0xd8ab5049U
#define MENU_LABEL_VALUE_PERFORMANCE_COUNTERS                                  0x20eb18caU
#define MENU_LABEL_CORE_LIST                                                   0xa8c3bfc9U
#define MENU_LABEL_VALUE_OPTIONS                                               0xd2a8b871U
#define MENU_LABEL_SETTINGS                                                    0x1304dc16U
#define MENU_LABEL_FRONTEND_COUNTERS                                           0xe5696877U
#define MENU_LABEL_VALUE_FRONTEND_COUNTERS                                     0x5752bcf8U
#define MENU_LABEL_VALUE_CORE_COUNTERS                                         0x4610e861U
#define MENU_LABEL_CORE_COUNTERS                                               0x64cc83e0U
#define MENU_LABEL_SYSTEM_INFORMATION                                          0x206ebf0fU
#define MENU_LABEL_VALUE_SYSTEM_INFORMATION                                    0xa62fd7f0U
#define MENU_LABEL_CORE_INFORMATION                                            0xb638e0d3U
#define MENU_LABEL_VALUE_CORE_INFORMATION                                      0x781981b4U
#define MENU_LABEL_VIDEO_SHADER_PARAMETERS                                     0x9895c3e5U
#define MENU_LABEL_DISK_CONTROL                                                0x83c0ae48U
#define MENU_LABEL_VALUE_DISK_CONTROL                                          0xa1da06a9U
#define MENU_LABEL_CORE_OPTIONS                                                0xf65e60f9U
#define MENU_LABEL_CORE_OPTION                                                 0xf03167c6U
#define MENU_LABEL_CORE_OPTION_CATEGORY                                        0xbd3a46d6U
#define MENU_LABEL_CORE_OPTION_CATEGORIES                                      0x2bec2a6bU
#define MENU_LABEL_DISK_CYCLE_TRAY_STATUS                                      0x3035cdc1U
#define MENU_LABEL_VALUE_DISK_CYCLE_TRAY_STATUS                                0xf44928c4U
#define MENU_LABEL_VALUE_NO_CORE_OPTIONS_AVAILABLE                             0xd064cbe6U
#define MENU_LABEL_VALUE_DISK_INDEX                                            0xadbce4a8U
#define MENU_LABEL_VALUE_DISK_IMAGE_APPEND                                     0x1cb28c6bU
#define MENU_LABEL_DISK_INDEX                                                  0x6c14bf54U
#define MENU_LABEL_SHADER_OPTIONS                                              0x1f7d2fc7U
#define MENU_LABEL_VALUE_VIDEO_OPTIONS                                         0xb4349948U
#define MENU_LABEL_VALUE_CORE_CHEAT_OPTIONS                                    0x8ba478bfU
#define MENU_LABEL_CORE_CHEAT_OPTIONS                                          0x9293171dU
#define MENU_LABEL_VALUE_INPUT_REMAPPING                                       0xdd0dbf58U
#define MENU_LABEL_INPUT_REMAPPING                                             0xd4234eb7U
#define MENU_LABEL_JOYPAD_TO_KEYBOARD_BIND                                     0x93c4e81aU
#define MENU_LABEL_VALUE_CURSOR_MANAGER                                        0xc3696afeU
#define MENU_LABEL_VIDEO_SHADER_PASS                                           0x4fa31028U
#define MENU_LABEL_VALUE_VIDEO_SHADER_PRESET                                   0x5990f82eU
#define MENU_LABEL_VIDEO_SHADER_PRESET                                         0xc5d3bae4U
#define MENU_LABEL_CHEAT_FILE_LOAD                                             0x57336148U
#define MENU_LABEL_VALUE_CHEAT_FILE_LOAD                                       0x5b983e0aU
#define MENU_LABEL_REMAP_FILE_LOAD                                             0x9c2799b8U
#define MENU_LABEL_VALUE_REMAP_FILE_LOAD                                       0xabdd415aU
#define MENU_LABEL_MESSAGE                                                     0xbe463eeaU
#define MENU_LABEL_INFO                                                        0x7c9884d1U
#define MENU_LABEL_INFO_SCREEN                                                 0xd97853d0U
#define MENU_LABEL_LOAD_OPEN_ZIP                                               0x8aa3c068U
#define MENU_LABEL_CUSTOM_VIEWPORT                                             0xdb33a93fU
#define MENU_LABEL_CUSTOM_RATIO                                                0xf038731eU
#define MENU_LABEL_VALUE_CUSTOM_RATIO                                          0x3c94b73fU
#define MENU_LABEL_CONFIRM_CORE_DELETION                                       0x111b482eU
#define MENU_LABEL_CONFIRM_SHADER_PRESET_DELETION                              0xd43a4c8eU
#define MENU_LABEL_HELP                                                        0x7c97d2eeU
#define MENU_LABEL_VALUE_HELP                                                  0x7c8646ceU
#define MENU_LABEL_INPUT_OVERLAY                                               0x24e24796U
#define MENU_LABEL_INPUT_OSK_OVERLAY                                           0x11f1c582U
#define MENU_LABEL_CHEAT_DATABASE_PATH                                         0x01388b8aU
#define MENU_LABEL_OSK_OVERLAY_DIRECTORY                                       0xcce86287U
#define MENU_LABEL_RECORDING_OUTPUT_DIRECTORY                                  0x30bece06U
#define MENU_LABEL_RECORDING_CONFIG_DIRECTORY                                  0x3c3f274bU
#define MENU_LABEL_VIDEO_FONT_PATH                                             0xd0de729eU
#define MENU_LABEL_VIDEO_FILTER                                                0x1c0eb741U
#define MENU_LABEL_VALUE_VIDEO_FILTER                                          0xc1690382U
#define MENU_LABEL_AUDIO_DSP_PLUGIN                                            0x4a69572bU
#define MENU_LABEL_VALUE_AUDIO_DSP_PLUGIN                                      0x1c9f180dU
#define MENU_LABEL_RGUI_BROWSER_DIRECTORY                                      0xa86cba73U
#define MENU_LABEL_CORE_ASSETS_DIRECTORY                                       0x8ba5ee54U
#define MENU_LABEL_CONTENT_DIRECTORY                                           0x7738dc14U
#define MENU_LABEL_CORE_CONTENT_DIRECTORY                                      0xd6c1ceefU
#define MENU_LABEL_CORE_CONTENT_DIRECTORY_QUICKSET                             0x10bf7ab6U
#define MENU_LABEL_SCREENSHOT_DIRECTORY                                        0x552612d7U
#define MENU_LABEL_INPUT_REMAPPING_DIRECTORY                                   0x5233c20bU
#define MENU_LABEL_VIDEO_SHADER_DIR                                            0x30f53b10U
#define MENU_LABEL_VIDEO_FILTER_DIR                                            0x67603f1fU
#define MENU_LABEL_AUDIO_FILTER_DIR                                            0x4bd96ebaU
#define MENU_LABEL_SAVESTATE_DIRECTORY                                         0x90551289U
#define MENU_LABEL_LIBRETRO_INFO_PATH                                          0xe552b25fU
#define MENU_LABEL_MENU_THEME_DIRECTORY                                        0x416e1fe0U
#define MENU_LABEL_RGUI_CONFIG_DIRECTORY                                       0x0cb3e005U
#define MENU_LABEL_SAVEFILE_DIRECTORY                                          0x92773488U
#define MENU_LABEL_OVERLAY_DIRECTORY                                           0xc4ed3d1bU
#define MENU_LABEL_SYSTEM_DIRECTORY                                            0x35a6fb9eU
#define MENU_LABEL_ASSETS_DIRECTORY                                            0xde1ae8ecU
#define MENU_LABEL_EXTRACTION_DIRECTORY                                        0x33b55ffaU
#define MENU_LABEL_JOYPAD_AUTOCONFIG_DIR                                       0x2f4822d8U
#define MENU_LABEL_DYNAMIC_WALLPAPERS_DIRECTORY                                0x62f975b8U
#define MENU_LABEL_DRIVER_SETTINGS                                             0x81cd2d62U
#define MENU_LABEL_CORE_SETTINGS                                               0x06795dffU
#define MENU_LABEL_CONFIGURATION_SETTINGS                                      0x5a1558ceU
#define MENU_LABEL_LOGGING_SETTINGS                                            0x902c003dU
#define MENU_LABEL_SAVING_SETTINGS                                             0x32fea87eU
#define MENU_LABEL_REWIND_SETTINGS                                             0xbff7775fU
#define MENU_LABEL_VIDEO_SETTINGS                                              0x9dd23badU
#define MENU_LABEL_RECORDING_SETTINGS                                          0x1a80b313U
#define MENU_LABEL_FRAME_THROTTLE_SETTINGS                                     0x573b8837U
#define MENU_LABEL_SHADER_SETTINGS                                             0xd6657e8dU
#define MENU_LABEL_FONT_SETTINGS                                               0x67571029U
#define MENU_LABEL_AUDIO_SETTINGS                                              0x8f74c888U
#define MENU_LABEL_INPUT_SETTINGS                                              0xddd30846U
#define MENU_LABEL_INPUT_HOTKEY_SETTINGS                                       0x1cb39c19U
#define MENU_LABEL_OVERLAY_SETTINGS                                            0x997b2fd5U
#define MENU_LABEL_OVERLAY_MOUSE_SETTINGS                                      0x7915c6ffU
#define MENU_LABEL_OVERLAY_KEYBOARD_SETTINGS                                   0x1ae2ddc7U
#define MENU_LABEL_OVERLAY_LIGHTGUN_SETTINGS                                   0x321c2458U
#define MENU_LABEL_MENU_SETTINGS                                               0x61e4544bU
#define MENU_LABEL_MENU_VISIBILITIES                                           0x62b68209U
#define MENU_LABEL_UI_SETTINGS                                                 0xf8da6ef4U
#define MENU_LABEL_PATCH_SETTINGS                                              0xa78b0986U
#define MENU_LABEL_PLAYLIST_SETTINGS                                           0x4d276288U
#define MENU_LABEL_CORE_UPDATER_SETTINGS                                       0x124ad454U
#define MENU_LABEL_NETWORK_SETTINGS                                            0x8b50d180U
#define MENU_LABEL_ARCHIVE_SETTINGS                                            0x78e85398U
#define MENU_LABEL_USER_SETTINGS                                               0xcdc9a8f5U
#define MENU_LABEL_INPUT_USER_1_BINDS                                          0x4d2b4e35U
#define MENU_LABEL_INPUT_USER_2_BINDS                                          0x9a258af6U
#define MENU_LABEL_INPUT_USER_3_BINDS                                          0xe71fc7b7U
#define MENU_LABEL_INPUT_USER_4_BINDS                                          0x341a0478U
#define MENU_LABEL_INPUT_USER_5_BINDS                                          0x81144139U
#define MENU_LABEL_INPUT_USER_6_BINDS                                          0xce0e7dfaU
#define MENU_LABEL_INPUT_USER_7_BINDS                                          0x1b08babbU
#define MENU_LABEL_INPUT_USER_8_BINDS                                          0x6802f77cU
#define MENU_LABEL_INPUT_USER_9_BINDS                                          0xb4fd343dU
#define MENU_LABEL_INPUT_USER_10_BINDS                                         0x70252b05U
#define MENU_LABEL_INPUT_USER_11_BINDS                                         0xbd1f67c6U
#define MENU_LABEL_INPUT_USER_12_BINDS                                         0x0a19a487U
#define MENU_LABEL_INPUT_USER_13_BINDS                                         0x5713e148U
#define MENU_LABEL_INPUT_USER_14_BINDS                                         0xa40e1e09U
#define MENU_LABEL_INPUT_USER_15_BINDS                                         0xf1085acaU
#define MENU_LABEL_INPUT_USER_16_BINDS                                         0x3e02978bU
#define MENU_LABEL_DIRECTORY_SETTINGS                                          0xb817bd2bU
#define MENU_LABEL_PRIVACY_SETTINGS                                            0xce106254U
#define MENU_LABEL_SHADER_APPLY_CHANGES                                        0x4f7306b9U
#define MENU_LABEL_VALUE_SHADER_APPLY_CHANGES                                  0x5ecf945bU
#define MENU_LABEL_VALUE_SAVE_NEW_CONFIG                                       0xd49f2c94U
#define MENU_LABEL_ONSCREEN_DISPLAY_SETTINGS                                   0x67571029U
#define MENU_LABEL_CHEAT_APPLY_CHANGES                                         0xde88aa27U
#define MENU_LABEL_CUSTOM_BIND                                                 0x1e84b3fcU
#define MENU_LABEL_CUSTOM_BIND_ALL                                             0x79ac14f4U
#define MENU_LABEL_CUSTOM_BIND_DEFAULTS                                        0xe88f7b13U
#define MENU_LABEL_SAVESTATE                                                   0x3a4849b5U
#define MENU_LABEL_LOADSTATE                                                   0xa39eb286U
#define MENU_LABEL_RESTART_CONTENT                                             0x1ea2e224U
#define MENU_LABEL_DISK_IMAGE_APPEND                                           0x5af7d709U
#define MENU_LABEL_VALUE_CONFIGURATIONS                                        0x88eb27f0U
#define MENU_LABEL_VALUE_CHEAT_FILE_SAVE_AS                                    0xf2498a2dU
#define MENU_LABEL_CHEAT_FILE_SAVE_AS                                          0x1f58dccaU
#define MENU_LABEL_VIDEO_SHADER_PRESET_SAVE_AS                                 0x3d6e5ce5U
#define MENU_LABEL_VALUE_VIDEO_SHADER_PRESET_SAVE_AS                           0x405d77b2U
#define MENU_LABEL_REMAPPING_SCOPE                                             0x4e1c9161U
#define MENU_LABEL_OPTIONS_SCOPE                                               0xd89bff8aU
#define MENU_LABEL_OPTIONS_FILE_LOAD                                           0x1dd28910U
#define MENU_LABEL_VALUE_UNABLE_TO_READ_COMPRESSED_FILE                        0xbae7be3eU
#define MENU_LABEL_OSK_ENABLE                                                  0x8e208498U
#define MENU_LABEL_AUDIO_MUTE                                                  0xe0ca1151U
#define MENU_LABEL_VALUE_AUDIO_MUTE                                            0x5af25952U
#define MENU_LABEL_EXIT_EMULATOR                                               0x86d5d467U
#define MENU_LABEL_SHARED_CONTEXT                                              0x7d7dad16U
#define MENU_LABEL_DUMMY_ON_CORE_SHUTDOWN                                      0x78579f70U
#define MENU_LABEL_LIBRETRO_DEVICE_SCOPE                                       0xb4cf4a7fU
#define MENU_LABEL_INPUT_TURBO_ID                                              0xc8fd7d5dU

#define MENU_VALUE_SECONDS                                                     0x8b0028d4U

const char *menu_hash_to_str(uint32_t hash);

uint32_t menu_hash_calculate(const char *s);

#ifdef __cplusplus
}
#endif

#endif
