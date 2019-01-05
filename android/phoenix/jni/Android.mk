LOCAL_PATH := $(call my-dir)
include $(CLEAR_VARS)

LOCAL_MODULE := retroarch-jni
RARCH_DIR := ../../..
LOCAL_CFLAGS += -std=gnu99 -Wall -DHAVE_LOGGER -DRARCH_DUMMY_LOG -DHAVE_ZLIB -DHAVE_MMAP -DRARCH_INTERNAL
LOCAL_LDLIBS := -llog -lz
LOCAL_SRC_FILES := apk-extract/apk-extract.c $(RARCH_DIR)/libretro-common/file/file_extract.c $(RARCH_DIR)/libretro-common/file/file_path.c $(RARCH_DIR)/file_ops.c $(RARCH_DIR)/libretro-common/string/string_list.c $(RARCH_DIR)/libretro-common/compat/compat.c

LOCAL_C_INCLUDES := $(LOCAL_PATH)/$(RARCH_DIR)/libretro-common/include/

include $(BUILD_SHARED_LIBRARY)

HAVE_LOGGER := 1

include $(CLEAR_VARS)

ifeq ($(TARGET_ARCH),x86)
   LOCAL_CFLAGS += -DANDROID_X86 -DHAVE_SSSE3
endif
ifeq ($(TARGET_ARCH_ABI),x86_64)
   LOCAL_CFLAGS += -DANDROID_X86 -DHAVE_SSSE3
endif

ifeq ($(TARGET_ARCH),arm)
   LOCAL_CFLAGS += -DANDROID_ARM -marm
   LOCAL_ARM_MODE := arm
endif
ifeq ($(TARGET_ARCH_ABI),armeabi-v7a)
   LOCAL_CFLAGS += -D__ARM_NEON__
   LOCAL_SRC_FILES += $(RARCH_DIR)/audio/audio_utils_neon.S.neon
   LOCAL_SRC_FILES += $(RARCH_DIR)/audio/drivers_resampler/sinc_neon.S.neon
   LOCAL_SRC_FILES += $(RARCH_DIR)/audio/drivers_resampler/cc_resampler_neon.S.neon
   LOCAL_CFLAGS += -DSINC_LOWER_QUALITY
endif
ifeq ($(TARGET_ARCH_ABI),arm64-v8a)
   LOCAL_CFLAGS += -DANDROID_AARCH64
   LOCAL_CFLAGS += -march=armv8-a
   LOCAL_CFLAGS += -DSINC_LOWER_QUALITY
endif

ifeq ($(TARGET_ARCH),mips)
   LOCAL_CFLAGS += -DANDROID_MIPS -D__mips__ -D__MIPSEL__
endif

LOCAL_MODULE := retroarch-activity

LOCAL_SRC_FILES  +=	$(RARCH_DIR)/griffin/griffin.c

ifeq ($(HAVE_LOGGER), 1)
   LOCAL_CFLAGS += -DHAVE_LOGGER
   LOGGER_LDLIBS := -llog
endif

ifeq ($(GLES),3)
   GLES_LIB := -lGLESv3
   LOCAL_CFLAGS += -DHAVE_OPENGLES3 -DHAVE_GL_SYNC
else
   GLES_LIB := -lGLESv2
   LOCAL_CFLAGS += -DNO_SHADER_MANAGER
endif


LOCAL_CFLAGS += -Wall -pthread -Wno-unused-function -fno-stack-protector -funroll-loops -DRARCH_MOBILE -DHAVE_GRIFFIN -DANDROID -DHAVE_DYNAMIC -DHAVE_OPENGL -DHAVE_FBO -DHAVE_OVERLAY -DHAVE_OPENGLES -DHAVE_OPENGLES2 -DGLSL_DEBUG -DHAVE_DYLIB -DHAVE_GLSL -DHAVE_MENU -DHAVE_RGUI -DHAVE_ZLIB -DHAVE_RPNG -DINLINE=inline -DLSB_FIRST -DHAVE_THREADS -D__LIBRETRO__ -DHAVE_NETPLAY -DHAVE_NETWORKING -DRARCH_INTERNAL -DHAVE_FILTERS_BUILTIN -std=gnu99 -DSINGLE_CORE
LOCAL_CFLAGS += -DHAVE_7ZIP

ifeq ($(NDK_DEBUG),1)
LOCAL_CFLAGS += -O0 -g
else
LOCAL_CFLAGS += -O2 -DNDEBUG
endif

LOCAL_LDLIBS	:= -L$(SYSROOT)/usr/lib -landroid -lEGL $(GLES_LIB) $(LOGGER_LDLIBS) -ldl
LOCAL_C_INCLUDES := $(LOCAL_PATH)/$(RARCH_DIR)/libretro-common/include/

LOCAL_CFLAGS += -DHAVE_SL
LOCAL_LDLIBS += -lOpenSLES -lz

include $(BUILD_SHARED_LIBRARY)

