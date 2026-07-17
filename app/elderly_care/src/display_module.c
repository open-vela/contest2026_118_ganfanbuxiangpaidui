/****************************************************************************
 * elderly_care/src/display_module.c
 *
 * Display module using LVGL API to show system status on LCD.
 * Displays temperature, humidity, human presence, alert count,
 * WiFi status, and uptime.
 ****************************************************************************/

#include "elderly_care.h"
#include <lvgl/lvgl.h>

/* LVGL objects */

static lv_obj_t *g_scr = NULL;
static lv_obj_t *g_lbl_temp = NULL;
static lv_obj_t *g_lbl_humi = NULL;
static lv_obj_t *g_lbl_human = NULL;
static lv_obj_t *g_lbl_alert = NULL;
static lv_obj_t *g_lbl_wifi = NULL;
static lv_obj_t *g_lbl_uptime = NULL;

/* Display text buffers */

static char g_temp_text[32];
static char g_humi_text[32];
static char g_human_text[32];
static char g_alert_text[32];
static char g_wifi_text[32];
static char g_uptime_text[32];

/* LVGL runtime state (initialized once, reused across update calls) */

static lv_nuttx_result_t g_lvgl_result;
static bool g_lvgl_initialized = false;

/****************************************************************************
 * Name: display_module_init
 ****************************************************************************/

int display_module_init(void)
{
  /* Initialize LVGL core and NuttX display driver (once).

     NuttX LVGL requires lv_init() + lv_nuttx_init() to bind the
     framebuffer (/dev/fb0) before any LVGL API (such as lv_scr_act)
     can be used.  See apps/examples/lvgldemo for the canonical pattern.
   */

  if (!g_lvgl_initialized)
    {
      lv_nuttx_dsc_t info;

      lv_init();
      lv_nuttx_dsc_init(&info);
      info.fb_path = "/dev/fb0";

      /* Disable input devices to avoid fdcheck assertion in lv_timer_handler.
         The touchscreen (/dev/input0, /dev/utouch) read path triggers a
         panic in fdcheck_protect when lv_indev_read calls read().  Since
         elderly_care only needs display output (no touch interaction),
         we NULL all input paths so LVGL creates no indev devices. */

      info.input_path = NULL;
      info.utouch_path = NULL;
      info.mouse_path = NULL;

      lv_nuttx_init(&info, &g_lvgl_result);

      if (g_lvgl_result.disp == NULL)
        {
          syslog(LOG_ERR, "lv_nuttx_init failed: no display\n");
          return -ENODEV;
        }

      g_lvgl_initialized = true;
      syslog(LOG_INFO, "LVGL initialized with /dev/fb0\n");
    }

  /* Get active screen */

  g_scr = lv_scr_act();
  if (g_scr == NULL)
    {
      syslog(LOG_ERR, "lv_scr_act failed\n");
      return -ENODEV;
    }

  /* Create labels */

  g_lbl_temp = lv_label_create(g_scr);
  lv_label_set_text(g_lbl_temp, "Temp: -- C");
  lv_obj_align(g_lbl_temp, LV_ALIGN_TOP_LEFT, 10, 10);

  g_lbl_humi = lv_label_create(g_scr);
  lv_label_set_text(g_lbl_humi, "Humi: --%");
  lv_obj_align_to(g_lbl_humi, g_lbl_temp, LV_ALIGN_OUT_BOTTOM_LEFT,
               0, 5);

  g_lbl_human = lv_label_create(g_scr);
  lv_label_set_text(g_lbl_human, "Human: N/A");
  lv_obj_align_to(g_lbl_human, g_lbl_humi, LV_ALIGN_OUT_BOTTOM_LEFT,
               0, 5);

  g_lbl_alert = lv_label_create(g_scr);
  lv_label_set_text(g_lbl_alert, "Alerts: 0");
  lv_obj_align_to(g_lbl_alert, g_lbl_human, LV_ALIGN_OUT_BOTTOM_LEFT,
               0, 5);

  g_lbl_wifi = lv_label_create(g_scr);
  lv_label_set_text(g_lbl_wifi, "WiFi: Disconnected");
  lv_obj_align_to(g_lbl_wifi, g_lbl_alert, LV_ALIGN_OUT_BOTTOM_LEFT,
               0, 5);

  g_lbl_uptime = lv_label_create(g_scr);
  lv_label_set_text(g_lbl_uptime, "Up: 0s");
  lv_obj_align_to(g_lbl_uptime, g_lbl_wifi, LV_ALIGN_OUT_BOTTOM_LEFT,
               0, 5);

  syslog(LOG_INFO, "Display module initialized\n");
  return 0;
}

/****************************************************************************
 * Name: display_module_deinit
 ****************************************************************************/

int display_module_deinit(void)
{
  if (g_scr != NULL)
    {
      lv_obj_clean(g_scr);
    }

  g_scr = NULL;
  g_lbl_temp = NULL;
  g_lbl_humi = NULL;
  g_lbl_human = NULL;
  g_lbl_alert = NULL;
  g_lbl_wifi = NULL;
  g_lbl_uptime = NULL;

  syslog(LOG_INFO, "Display module deinitialized\n");
  return 0;
}

/****************************************************************************
 * Name: display_module_update
 ****************************************************************************/

int display_module_update(struct system_state_s *state)
{
  time_t uptime;

  if (state == NULL || g_scr == NULL)
    {
      return -EINVAL;
    }

  /* Temperature */

  snprintf(g_temp_text, sizeof(g_temp_text), "Temp: %.1f C",
           state->sensor.temperature);
  lv_label_set_text(g_lbl_temp, g_temp_text);

  /* Humidity */

  snprintf(g_humi_text, sizeof(g_humi_text), "Humi: %.1f%%",
           state->sensor.humidity);
  lv_label_set_text(g_lbl_humi, g_humi_text);

  /* Human presence */

  snprintf(g_human_text, sizeof(g_human_text), "Human: %s",
           state->sensor.human_present ? "DETECTED" : "None");
  lv_label_set_text(g_lbl_human, g_human_text);

  /* Alert count */

  snprintf(g_alert_text, sizeof(g_alert_text), "Alerts: %u",
           state->alert_count);
  lv_label_set_text(g_lbl_alert, g_alert_text);

  /* WiFi status */

  snprintf(g_wifi_text, sizeof(g_wifi_text), "WiFi: %s",
           state->wifi_connected ? "Connected" : "Disconnected");
  lv_label_set_text(g_lbl_wifi, g_wifi_text);

  /* Uptime */

  uptime = time(NULL) - state->start_time;
  if (uptime < 60)
    {
      snprintf(g_uptime_text, sizeof(g_uptime_text), "Up: %lds",
               (long)uptime);
    }
  else if (uptime < 3600)
    {
      snprintf(g_uptime_text, sizeof(g_uptime_text), "Up: %ldm %lds",
               (long)(uptime / 60), (long)(uptime % 60));
    }
  else
    {
      snprintf(g_uptime_text, sizeof(g_uptime_text),
               "Up: %ldh %ldm",
               (long)(uptime / 3600),
               (long)((uptime % 3600) / 60));
    }

  lv_label_set_text(g_lbl_uptime, g_uptime_text);

  /* Drive LVGL task handler so rendering actually happens.
     Without this, labels are updated in memory but never flushed
     to /dev/fb0.  lv_timer_handler returns the idle time in ms. */

  if (g_lvgl_initialized)
    {
      lv_timer_handler();
    }

  return 0;
}
