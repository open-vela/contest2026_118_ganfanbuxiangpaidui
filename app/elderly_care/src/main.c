/****************************************************************************
 * elderly_care/src/main.c
 *
 * Main control logic for the elderly care terminal application.
 * Manages AI inference, audio detection, sensor reading, display update,
 * and power management threads.
 ****************************************************************************/

#include "elderly_care.h"

/* Global system state */

struct system_state_s g_state;

/* Thread IDs for cleanup */

static pthread_t g_ai_tid;
static pthread_t g_audio_tid;
static pthread_t g_sensor_tid;
static pthread_t g_display_tid;
static pthread_t g_power_tid;

/****************************************************************************
 * Name: process_ai_results
 ****************************************************************************/

static void process_ai_results(void)
{
  pthread_mutex_lock(&g_state.mutex);

  if (g_state.ai_result.fall_detected &&
      g_state.ai_result.fall_confidence >= AI_FALL_THRESHOLD)
    {
      struct alert_record_s record;
      memset(&record, 0, sizeof(record));
      record.type = EVENT_FALL_DETECTED;
      record.level = ALERT_CRITICAL;
      clock_gettime(CLOCK_REALTIME, &record.timestamp);
      snprintf(record.message, sizeof(record.message),
               "FALL DETECTED conf=%.2f pos=(%d,%d)",
               g_state.ai_result.fall_confidence,
               g_state.ai_result.human_x,
               g_state.ai_result.human_y);

      g_state.alert_count++;
      g_state.last_person_time = time(NULL);

      pthread_mutex_unlock(&g_state.mutex);

      alert_module_trigger(EVENT_FALL_DETECTED, ALERT_CRITICAL,
                           record.message);
#ifdef CONFIG_ELDERLY_CARE_WIFI_ENABLE
      wifi_module_send_alert(&record);
#endif
#ifdef CONFIG_ELDERLY_CARE_LOG_ENABLE
      log_module_write(&record);
#endif
      return;
    }

  if (g_state.ai_result.human_detected &&
      g_state.ai_result.human_confidence >= AI_PERSON_THRESHOLD)
    {
      g_state.sensor.human_present = true;
      g_state.last_person_time = time(NULL);
    }
  else
    {
      g_state.sensor.human_present = false;
    }

  pthread_mutex_unlock(&g_state.mutex);
}

/****************************************************************************
 * Name: process_audio_results
 ****************************************************************************/

static void process_audio_results(void)
{
  pthread_mutex_lock(&g_state.mutex);

  if (g_state.audio_result.help_detected &&
      g_state.audio_result.help_confidence >= 0.5f)
    {
      struct alert_record_s record;
      memset(&record, 0, sizeof(record));
      record.type = EVENT_VOICE_HELP;
      record.level = ALERT_CRITICAL;
      clock_gettime(CLOCK_REALTIME, &record.timestamp);
      snprintf(record.message, sizeof(record.message),
               "HELP VOICE DETECTED conf=%.2f",
               g_state.audio_result.help_confidence);

      g_state.alert_count++;

      pthread_mutex_unlock(&g_state.mutex);

      alert_module_trigger(EVENT_VOICE_HELP, ALERT_CRITICAL,
                           record.message);
#ifdef CONFIG_ELDERLY_CARE_WIFI_ENABLE
      wifi_module_send_alert(&record);
#endif
#ifdef CONFIG_ELDERLY_CARE_LOG_ENABLE
      log_module_write(&record);
#endif
      return;
    }

  pthread_mutex_unlock(&g_state.mutex);
}

/****************************************************************************
 * Name: process_sensor_data
 ****************************************************************************/

static void process_sensor_data(void)
{
  pthread_mutex_lock(&g_state.mutex);

  float temp = g_state.sensor.temperature;
  float humi = g_state.sensor.humidity;

  if (temp > TEMP_HIGH_THRESHOLD)
    {
      struct alert_record_s record;
      memset(&record, 0, sizeof(record));
      record.type = EVENT_TEMP_HIGH;
      record.level = ALERT_WARNING;
      clock_gettime(CLOCK_REALTIME, &record.timestamp);
      snprintf(record.message, sizeof(record.message),
               "TEMP HIGH %.1f C", temp);
      g_state.alert_count++;

      pthread_mutex_unlock(&g_state.mutex);

      alert_module_trigger(EVENT_TEMP_HIGH, ALERT_WARNING,
                           record.message);
#ifdef CONFIG_ELDERLY_CARE_WIFI_ENABLE
      wifi_module_send_alert(&record);
#endif
#ifdef CONFIG_ELDERLY_CARE_LOG_ENABLE
      log_module_write(&record);
#endif
      return;
    }

  if (temp < TEMP_LOW_THRESHOLD)
    {
      struct alert_record_s record;
      memset(&record, 0, sizeof(record));
      record.type = EVENT_TEMP_LOW;
      record.level = ALERT_WARNING;
      clock_gettime(CLOCK_REALTIME, &record.timestamp);
      snprintf(record.message, sizeof(record.message),
               "TEMP LOW %.1f C", temp);
      g_state.alert_count++;

      pthread_mutex_unlock(&g_state.mutex);

      alert_module_trigger(EVENT_TEMP_LOW, ALERT_WARNING,
                           record.message);
#ifdef CONFIG_ELDERLY_CARE_WIFI_ENABLE
      wifi_module_send_alert(&record);
#endif
#ifdef CONFIG_ELDERLY_CARE_LOG_ENABLE
      log_module_write(&record);
#endif
      return;
    }

  if (humi > HUMIDITY_HIGH_THRESHOLD)
    {
      struct alert_record_s record;
      memset(&record, 0, sizeof(record));
      record.type = EVENT_HUMIDITY_HIGH;
      record.level = ALERT_WARNING;
      clock_gettime(CLOCK_REALTIME, &record.timestamp);
      snprintf(record.message, sizeof(record.message),
               "HUMIDITY HIGH %.1f%%", humi);
      g_state.alert_count++;

      pthread_mutex_unlock(&g_state.mutex);

      alert_module_trigger(EVENT_HUMIDITY_HIGH, ALERT_WARNING,
                           record.message);
#ifdef CONFIG_ELDERLY_CARE_WIFI_ENABLE
      wifi_module_send_alert(&record);
#endif
#ifdef CONFIG_ELDERLY_CARE_LOG_ENABLE
      log_module_write(&record);
#endif
      return;
    }

  if (humi < HUMIDITY_LOW_THRESHOLD)
    {
      struct alert_record_s record;
      memset(&record, 0, sizeof(record));
      record.type = EVENT_HUMIDITY_LOW;
      record.level = ALERT_WARNING;
      clock_gettime(CLOCK_REALTIME, &record.timestamp);
      snprintf(record.message, sizeof(record.message),
               "HUMIDITY LOW %.1f%%", humi);
      g_state.alert_count++;

      pthread_mutex_unlock(&g_state.mutex);

      alert_module_trigger(EVENT_HUMIDITY_LOW, ALERT_WARNING,
                           record.message);
#ifdef CONFIG_ELDERLY_CARE_WIFI_ENABLE
      wifi_module_send_alert(&record);
#endif
#ifdef CONFIG_ELDERLY_CARE_LOG_ENABLE
      log_module_write(&record);
#endif
      return;
    }

  pthread_mutex_unlock(&g_state.mutex);
}

/****************************************************************************
 * Name: ai_inference_thread
 ****************************************************************************/

static FAR void *ai_inference_thread(FAR void *arg)
{
  static uint8_t frame_buffer[CAMERA_FRAME_SIZE];
  size_t frame_size;

  while (g_state.running)
    {
#ifdef CONFIG_ELDERLY_CARE_CAMERA_ENABLE
      if (camera_module_capture(frame_buffer, &frame_size) == 0)
        {
          g_state.frame_count++;
#ifdef CONFIG_ELDERLY_CARE_DSP_ENABLE
          pthread_mutex_lock(&g_state.mutex);
          dsp_inference_detect(frame_buffer, &g_state.ai_result);
          pthread_mutex_unlock(&g_state.mutex);
#endif
          process_ai_results();
        }
#endif
      usleep(100000); /* 100ms interval */
    }

  return NULL;
}

/****************************************************************************
 * Name: audio_thread
 ****************************************************************************/

static FAR void *audio_thread(FAR void *arg)
{
  int16_t audio_buffer[AUDIO_BUFFER_SIZE / 2];

  while (g_state.running)
    {
#ifdef CONFIG_ELDERLY_CARE_AUDIO_ENABLE
      if (audio_module_capture(audio_buffer,
                               AUDIO_BUFFER_SIZE / 2) == 0)
        {
          pthread_mutex_lock(&g_state.mutex);
          audio_module_detect(audio_buffer, &g_state.audio_result);
          pthread_mutex_unlock(&g_state.mutex);

          process_audio_results();
        }
#endif
      usleep(AUDIO_FRAME_MS * 1000);
    }

  return NULL;
}

/****************************************************************************
 * Name: sensor_thread
 ****************************************************************************/

static FAR void *sensor_thread(FAR void *arg)
{
  while (g_state.running)
    {
#ifdef CONFIG_ELDERLY_CARE_SENSOR_ENABLE
      pthread_mutex_lock(&g_state.mutex);
      sensor_module_read(&g_state.sensor);
      pthread_mutex_unlock(&g_state.mutex);

      process_sensor_data();
#endif
      usleep(SENSOR_READ_INTERVAL * 1000);
    }

  return NULL;
}

/****************************************************************************
 * Name: display_thread
 ****************************************************************************/

static FAR void *display_thread(FAR void *arg)
{
  while (g_state.running)
    {
#ifdef CONFIG_ELDERLY_CARE_DISPLAY_ENABLE
      display_module_update(&g_state);
#endif
      usleep(500000); /* 500ms refresh */
    }

  return NULL;
}

/****************************************************************************
 * Name: power_manage_thread
 ****************************************************************************/

static FAR void *power_manage_thread(FAR void *arg)
{
  while (g_state.running)
    {
#ifdef CONFIG_ELDERLY_CARE_POWER_SAVE_ENABLE
      power_module_check_idle(&g_state);
#endif
      usleep(POWER_SAVE_CHECK_MS * 1000);
    }

  return NULL;
}

/****************************************************************************
 * Name: init_all_modules
 ****************************************************************************/

static int init_all_modules(void)
{
  int ret = 0;

#ifdef CONFIG_ELDERLY_CARE_CAMERA_ENABLE
  ret = camera_module_init();
  if (ret < 0)
    {
      syslog(LOG_ERR, "camera_module_init failed: %d\n", ret);
    }
#endif

#ifdef CONFIG_ELDERLY_CARE_DSP_ENABLE
  ret = dsp_inference_init();
  if (ret < 0)
    {
      syslog(LOG_ERR, "dsp_inference_init failed: %d\n", ret);
    }
#endif

#ifdef CONFIG_ELDERLY_CARE_AUDIO_ENABLE
  ret = audio_module_init();
  if (ret < 0)
    {
      syslog(LOG_ERR, "audio_module_init failed: %d\n", ret);
    }
#endif

#ifdef CONFIG_ELDERLY_CARE_SENSOR_ENABLE
  ret = sensor_module_init();
  if (ret < 0)
    {
      syslog(LOG_ERR, "sensor_module_init failed: %d\n", ret);
    }
#endif

#ifdef CONFIG_ELDERLY_CARE_ALERT_ENABLE
  ret = alert_module_init();
  if (ret < 0)
    {
      syslog(LOG_ERR, "alert_module_init failed: %d\n", ret);
    }
#endif

#ifdef CONFIG_ELDERLY_CARE_DISPLAY_ENABLE
  ret = display_module_init();
  if (ret < 0)
    {
      syslog(LOG_ERR, "display_module_init failed: %d\n", ret);
    }
#endif

#ifdef CONFIG_ELDERLY_CARE_WIFI_ENABLE
  ret = wifi_module_init();
  if (ret < 0)
    {
      syslog(LOG_ERR, "wifi_module_init failed: %d\n", ret);
    }
#endif

#ifdef CONFIG_ELDERLY_CARE_POWER_SAVE_ENABLE
  ret = power_module_init();
  if (ret < 0)
    {
      syslog(LOG_ERR, "power_module_init failed: %d\n", ret);
    }
#endif

#ifdef CONFIG_ELDERLY_CARE_LOG_ENABLE
  ret = log_module_init();
  if (ret < 0)
    {
      syslog(LOG_ERR, "log_module_init failed: %d\n", ret);
    }
#endif

  return 0;
}

/****************************************************************************
 * Name: deinit_all_modules
 ****************************************************************************/

static void deinit_all_modules(void)
{
#ifdef CONFIG_ELDERLY_CARE_LOG_ENABLE
  log_module_deinit();
#endif
#ifdef CONFIG_ELDERLY_CARE_POWER_SAVE_ENABLE
  power_module_deinit();
#endif
#ifdef CONFIG_ELDERLY_CARE_WIFI_ENABLE
  wifi_module_deinit();
#endif
#ifdef CONFIG_ELDERLY_CARE_DISPLAY_ENABLE
  display_module_deinit();
#endif
#ifdef CONFIG_ELDERLY_CARE_ALERT_ENABLE
  alert_module_deinit();
#endif
#ifdef CONFIG_ELDERLY_CARE_SENSOR_ENABLE
  sensor_module_deinit();
#endif
#ifdef CONFIG_ELDERLY_CARE_AUDIO_ENABLE
  audio_module_deinit();
#endif
#ifdef CONFIG_ELDERLY_CARE_DSP_ENABLE
  dsp_inference_deinit();
#endif
#ifdef CONFIG_ELDERLY_CARE_CAMERA_ENABLE
  camera_module_deinit();
#endif
}

/****************************************************************************
 * Name: signal_handler
 ****************************************************************************/

static void signal_handler(int signo)
{
  (void)signo;
  g_state.running = false;
}

/****************************************************************************
 * Name: main
 ****************************************************************************/

int main(int argc, FAR char *argv[])
{
  int ret;

  memset(&g_state, 0, sizeof(g_state));
  g_state.running = true;
  g_state.start_time = time(NULL);
  g_state.last_person_time = time(NULL);
  pthread_mutex_init(&g_state.mutex, NULL);

  ret = init_all_modules();
  if (ret < 0)
    {
      syslog(LOG_ERR, "init_all_modules failed\n");
      return EXIT_FAILURE;
    }

  /* Install signal handler for graceful shutdown */

  signal(SIGINT, signal_handler);
  signal(SIGTERM, signal_handler);

  /* Create worker threads */

  pthread_create(&g_ai_tid, NULL, ai_inference_thread, NULL);
  pthread_create(&g_audio_tid, NULL, audio_thread, NULL);
  pthread_create(&g_sensor_tid, NULL, sensor_thread, NULL);
  pthread_create(&g_display_tid, NULL, display_thread, NULL);
  pthread_create(&g_power_tid, NULL, power_manage_thread, NULL);

  syslog(LOG_INFO, "Elderly care system started\n");

  /* Main loop: wait until signal */

  while (g_state.running)
    {
      sleep(1);
    }

  /* Cleanup */

  pthread_join(g_ai_tid, NULL);
  pthread_join(g_audio_tid, NULL);
  pthread_join(g_sensor_tid, NULL);
  pthread_join(g_display_tid, NULL);
  pthread_join(g_power_tid, NULL);

  deinit_all_modules();
  pthread_mutex_destroy(&g_state.mutex);

  syslog(LOG_INFO, "Elderly care system stopped\n");
  return EXIT_SUCCESS;
}
