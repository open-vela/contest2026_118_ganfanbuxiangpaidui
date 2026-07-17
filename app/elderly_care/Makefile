include $(APPDIR)/Make.defs

CSRCS := src/main.c

ifeq ($(CONFIG_ELDERLY_CARE_CAMERA_ENABLE),y)
CSRCS += src/camera_module.c
endif

ifeq ($(CONFIG_ELDERLY_CARE_DSP_ENABLE),y)
CSRCS += src/dsp_inference.c
endif

ifeq ($(CONFIG_ELDERLY_CARE_AUDIO_ENABLE),y)
CSRCS += src/audio_module.c
endif

ifeq ($(CONFIG_ELDERLY_CARE_SENSOR_ENABLE),y)
CSRCS += src/sensor_module.c
endif

ifeq ($(CONFIG_ELDERLY_CARE_ALERT_ENABLE),y)
CSRCS += src/alert_module.c
endif

ifeq ($(CONFIG_ELDERLY_CARE_DISPLAY_ENABLE),y)
CSRCS += src/display_module.c
endif

ifeq ($(CONFIG_ELDERLY_CARE_WIFI_ENABLE),y)
CSRCS += src/wifi_module.c
endif

ifeq ($(CONFIG_ELDERLY_CARE_POWER_SAVE_ENABLE),y)
CSRCS += src/power_module.c
endif

ifeq ($(CONFIG_ELDERLY_CARE_LOG_ENABLE),y)
CSRCS += src/log_module.c
endif

PROGNAME  = elderly_care
PRIORITY  = SCHED_PRIORITY_DEFAULT
STACKSIZE = $(CONFIG_ELDERLY_CARE_TASK_STACK_SIZE)
MODULE    = $(CONFIG_CONTEST2026_118_ELDERLY_CARE)

include $(APPDIR)/Application.mk
