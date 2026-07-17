#ifndef __APPS_ELDERLY_CARE_SRC_ELDERLY_CARE_H
#define __APPS_ELDERLY_CARE_SRC_ELDERLY_CARE_H

#include <nuttx/config.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <fcntl.h>
#include <syslog.h>
#include <pthread.h>
#include <signal.h>
#include <time.h>
#include <math.h>
#include <sys/ioctl.h>
#include <sys/types.h>
#include <sys/stat.h>

/* Image dimensions */

#define CAMERA_WIDTH           240
#define CAMERA_HEIGHT          320
#define CAMERA_FRAME_SIZE      (CAMERA_WIDTH * CAMERA_HEIGHT * 2)

/* Audio parameters */

#define AUDIO_SAMPLE_RATE      16000
#define AUDIO_CHANNELS         2
#define AUDIO_FRAME_MS         30
#define AUDIO_FRAME_SAMPLES    (AUDIO_SAMPLE_RATE * AUDIO_FRAME_MS / 1000)
#define AUDIO_BUFFER_SIZE      (AUDIO_FRAME_SAMPLES * AUDIO_CHANNELS * 2)

/* AI model parameters */

#define AI_MODEL_INPUT_W       96
#define AI_MODEL_INPUT_H       96
#define AI_MODEL_INPUT_SIZE    (AI_MODEL_INPUT_W * AI_MODEL_INPUT_H * 3)
#define AI_PERSON_THRESHOLD    0.5f
#define AI_FALL_THRESHOLD      0.7f

/* Sensor thresholds */

#define SENSOR_READ_INTERVAL   5000
#define TEMP_HIGH_THRESHOLD    38.0f
#define TEMP_LOW_THRESHOLD     10.0f
#define HUMIDITY_HIGH_THRESHOLD 80.0f
#define HUMIDITY_LOW_THRESHOLD  20.0f

/* Alert parameters */

#define ALERT_BUZZER_FREQ      2000
#define ALERT_LED_BLINK_MS     200

/* Power management */

#define POWER_SAVE_TIMEOUT     1800
#define POWER_SAVE_CHECK_MS    10000

/* WiFi/MQTT parameters */

#define MQTT_BROKER_HOST       "broker.emqx.io"
#define MQTT_BROKER_PORT       1883
#define MQTT_TOPIC_PREFIX      "elderly_care/118/"
#define MQTT_CLIENT_ID         "elderly_care_118"

/* Log parameters */

#define LOG_FLASH_PARTITION    "/dev/elog"
#define LOG_MAX_ENTRIES        1000
#define LOG_ENTRY_MAX_LEN      256

/* Event types */

enum elderly_care_event_e
{
  EVENT_NONE = 0,
  EVENT_HUMAN_DETECTED,
  EVENT_FALL_DETECTED,
  EVENT_VOICE_HELP,
  EVENT_VOICE_MOAN,
  EVENT_TEMP_HIGH,
  EVENT_TEMP_LOW,
  EVENT_HUMIDITY_HIGH,
  EVENT_HUMIDITY_LOW,
  EVENT_NO_PERSON_TIMEOUT,
  EVENT_WAKE_UP,
  EVENT_MAX
};

/* Alert level */

enum alert_level_e
{
  ALERT_NONE = 0,
  ALERT_INFO,
  ALERT_WARNING,
  ALERT_CRITICAL
};

/* Sensor data */

struct sensor_data_s
{
  float temperature;
  float humidity;
  bool human_present;
  struct timespec timestamp;
};

/* AI detection result */

struct ai_result_s
{
  bool human_detected;
  float human_confidence;
  bool fall_detected;
  float fall_confidence;
  int human_x;
  int human_y;
  int human_w;
  int human_h;
};

/* Audio detection result */

struct audio_result_s
{
  bool help_detected;
  float help_confidence;
  bool moan_detected;
  float moan_confidence;
  float noise_level;
};

/* Alert record */

struct alert_record_s
{
  enum elderly_care_event_e type;
  enum alert_level_e level;
  struct timespec timestamp;
  char message[128];
};

/* System state */

struct system_state_s
{
  bool running;
  bool power_save_mode;
  bool wifi_connected;
  bool mqtt_connected;

  struct sensor_data_s sensor;
  struct ai_result_s ai_result;
  struct audio_result_s audio_result;

  uint32_t frame_count;
  uint32_t alert_count;
  time_t last_person_time;
  time_t start_time;

  pthread_mutex_t mutex;
};

/* Function prototypes */

int camera_module_init(void);
int camera_module_deinit(void);
int camera_module_capture(uint8_t *buffer, size_t *size);

int dsp_inference_init(void);
int dsp_inference_deinit(void);
int dsp_inference_detect(uint8_t *frame, struct ai_result_s *result);

int audio_module_init(void);
int audio_module_deinit(void);
int audio_module_capture(int16_t *buffer, size_t samples);
int audio_module_detect(int16_t *buffer, struct audio_result_s *result);

int sensor_module_init(void);
int sensor_module_deinit(void);
int sensor_module_read(struct sensor_data_s *data);

int alert_module_init(void);
int alert_module_deinit(void);
int alert_module_trigger(enum elderly_care_event_e event,
                         enum alert_level_e level,
                         const char *message);
int alert_module_stop(void);

int display_module_init(void);
int display_module_deinit(void);
int display_module_update(struct system_state_s *state);

int wifi_module_init(void);
int wifi_module_deinit(void);
int wifi_module_connect(const char *ssid, const char *password);
int wifi_module_send_alert(struct alert_record_s *record);

int power_module_init(void);
int power_module_deinit(void);
int power_module_check_idle(struct system_state_s *state);
int power_module_enter_sleep(void);
int power_module_wake_up(void);

int log_module_init(void);
int log_module_deinit(void);
int log_module_write(struct alert_record_s *record);
int log_module_flush_pending(void);

#endif /* __APPS_ELDERLY_CARE_SRC_ELDERLY_CARE_H */
