# ##############################################################################
# Contest 2026 team 118 elderly care app
#
# Build registration for both Make and CMake. Per-module sources are gated by
# the same Kconfig switches used by Makefile so the two build systems stay in
# sync.
# ##############################################################################

if(CONFIG_CONTEST2026_118_ELDERLY_CARE)
  set(PROGNAME elderly_care)
  set(STACKSIZE ${CONFIG_ELDERLY_CARE_TASK_STACK_SIZE})
  set(MODULE ${CONFIG_CONTEST2026_118_ELDERLY_CARE})
  set(MAINSRC src/main.c)
  set(CSRCS "")

  if(CONFIG_ELDERLY_CARE_CAMERA_ENABLE)
    list(APPEND CSRCS src/camera_module.c)
  endif()
  if(CONFIG_ELDERLY_CARE_DSP_ENABLE)
    list(APPEND CSRCS src/dsp_inference.c)
  endif()
  if(CONFIG_ELDERLY_CARE_AUDIO_ENABLE)
    list(APPEND CSRCS src/audio_module.c)
  endif()
  if(CONFIG_ELDERLY_CARE_SENSOR_ENABLE)
    list(APPEND CSRCS src/sensor_module.c)
  endif()
  if(CONFIG_ELDERLY_CARE_ALERT_ENABLE)
    list(APPEND CSRCS src/alert_module.c)
  endif()
  if(CONFIG_ELDERLY_CARE_DISPLAY_ENABLE)
    list(APPEND CSRCS src/display_module.c)
  endif()
  if(CONFIG_ELDERLY_CARE_WIFI_ENABLE)
    list(APPEND CSRCS src/wifi_module.c)
  endif()
  if(CONFIG_ELDERLY_CARE_POWER_SAVE_ENABLE)
    list(APPEND CSRCS src/power_module.c)
  endif()
  if(CONFIG_ELDERLY_CARE_LOG_ENABLE)
    list(APPEND CSRCS src/log_module.c)
  endif()

  nuttx_add_application(
    NAME ${PROGNAME}
    STACKSIZE ${STACKSIZE}
    MODULE ${MODULE}
    SRCS ${MAINSRC} ${CSRCS}
  )
endif()
