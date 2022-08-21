ifeq ($(GLES),2)
   APP_PLATFORM := android-9
else  # default to OpenGL ES 3.0
   GLES := 3
   APP_PLATFORM := android-21
endif

ifndef TARGET_ABIS
   APP_ABI := armeabi-v7a
else
   APP_ABI := $(TARGET_ABIS)
endif
