/****************************************************************************
 * elderly_care/src/sensor_module.c
 *
 * Environment sensor module for temperature and humidity monitoring.
 * Reads from uORB sensor_temp_humi topic.
 * Falls back to mock data when no sensor hardware is present.
 ****************************************************************************/

#include "elderly_care.h"

/* uORB temperature/humidity sensor data structure */

struct sensor_temp_humi_s
{
  float temperature;
  float humidity;
};

/* Module state */

static int g_sensor_fd = -1;
static bool g_sensor_available = false;

/* Mock data for simulation */

static float g_mock_temp = 25.0f;
static float g_mock_humi = 50.0f;

/****************************************************************************
 * Name: sensor_module_init
 ****************************************************************************/

int sensor_module_init(void)
{
  g_sensor_fd = open("/dev/uorb/sensor_temp_humi", O_RDONLY);
  if (g_sensor_fd < 0)
    {
      syslog(LOG_WARNING,
             "Sensor not available, using mock data\n");
      g_sensor_available = false;
      return 0;
    }

  g_sensor_available = true;
  syslog(LOG_INFO, "Sensor module initialized\n");
  return 0;
}

/****************************************************************************
 * Name: sensor_module_deinit
 ****************************************************************************/

int sensor_module_deinit(void)
{
  if (g_sensor_fd >= 0)
    {
      close(g_sensor_fd);
      g_sensor_fd = -1;
    }

  g_sensor_available = false;
  syslog(LOG_INFO, "Sensor module deinitialized\n");
  return 0;
}

/****************************************************************************
 * Name: sensor_module_read
 ****************************************************************************/

int sensor_module_read(struct sensor_data_s *data)
{
  if (data == NULL)
    {
      return -EINVAL;
    }

  if (g_sensor_available && g_sensor_fd >= 0)
    {
      /* Read real sensor data from uORB */

      struct sensor_temp_humi_s raw;
      ssize_t n = read(g_sensor_fd, &raw, sizeof(raw));

      if (n >= (ssize_t)sizeof(raw))
        {
          data->temperature = raw.temperature;
          data->humidity = raw.humidity;
          clock_gettime(CLOCK_REALTIME, &data->timestamp);
          return 0;
        }
    }

  /* Mock data with slight variation */

  g_mock_temp += ((float)(rand() % 10 - 5)) / 10.0f;
  g_mock_humi += ((float)(rand() % 6 - 3)) / 10.0f;

  /* Clamp to reasonable range */

  if (g_mock_temp < 15.0f)
    {
      g_mock_temp = 15.0f;
    }

  if (g_mock_temp > 40.0f)
    {
      g_mock_temp = 40.0f;
    }

  if (g_mock_humi < 30.0f)
    {
      g_mock_humi = 30.0f;
    }

  if (g_mock_humi > 90.0f)
    {
      g_mock_humi = 90.0f;
    }

  data->temperature = g_mock_temp;
  data->humidity = g_mock_humi;
  clock_gettime(CLOCK_REALTIME, &data->timestamp);

  return 0;
}
