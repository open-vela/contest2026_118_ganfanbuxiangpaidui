/****************************************************************************
 * elderly_care/src/alert_module.c
 *
 * Alert module using PWM buzzer and GPIO LED for alarm output.
 * Different alert levels trigger different PWM frequencies and duty cycles.
 ****************************************************************************/

#include "elderly_care.h"
#include <nuttx/timers/pwm.h>
#include <nuttx/ioexpander/gpio.h>

/* Module state */

static int g_pwm_fd = -1;
static int g_gpio_fd = -1;
static bool g_alert_active = false;

/****************************************************************************
 * Name: alert_module_init
 ****************************************************************************/

int alert_module_init(void)
{
  g_pwm_fd = open("/dev/pwm0", O_WRONLY);
  if (g_pwm_fd < 0)
    {
      syslog(LOG_WARNING, "PWM device not available\n");
    }

  g_gpio_fd = open("/dev/gpio0", O_RDWR);
  if (g_gpio_fd < 0)
    {
      syslog(LOG_WARNING, "GPIO device not available\n");
    }

  g_alert_active = false;
  syslog(LOG_INFO, "Alert module initialized\n");
  return 0;
}

/****************************************************************************
 * Name: alert_module_deinit
 ****************************************************************************/

int alert_module_deinit(void)
{
  /* Stop any active alert */

  alert_module_stop();

  if (g_pwm_fd >= 0)
    {
      close(g_pwm_fd);
      g_pwm_fd = -1;
    }

  if (g_gpio_fd >= 0)
    {
      close(g_gpio_fd);
      g_gpio_fd = -1;
    }

  g_alert_active = false;
  syslog(LOG_INFO, "Alert module deinitialized\n");
  return 0;
}

/****************************************************************************
 * Name: alert_module_trigger
 ****************************************************************************/

int alert_module_trigger(enum elderly_care_event_e event,
                         enum alert_level_e level,
                         const char *message)
{
  int freq;
  int duty;

  syslog(LOG_WARNING, "ALERT [%d] level=%d: %s\n",
         event, level, message ? message : "");

  /* Set PWM parameters based on alert level */

  switch (level)
    {
      case ALERT_CRITICAL:
        freq = ALERT_BUZZER_FREQ;
        duty = 80;
        break;

      case ALERT_WARNING:
        freq = ALERT_BUZZER_FREQ / 2;
        duty = 50;
        break;

      case ALERT_INFO:
        freq = ALERT_BUZZER_FREQ / 4;
        duty = 30;
        break;

      default:
        return 0;
    }

  /* Drive buzzer via PWM */

  if (g_pwm_fd >= 0)
    {
      struct pwm_info_s pwm;
      memset(&pwm, 0, sizeof(pwm));
      pwm.frequency = freq;
      pwm.duty = (uint16_t)(duty * 1000 / 100);

      ioctl(g_pwm_fd, PWMIOC_SETCHARACTERISTICS,
            (unsigned long)&pwm);
      ioctl(g_pwm_fd, PWMIOC_START, 0);
    }

  /* Drive LED via GPIO (turn on) */

#ifdef CONFIG_DEV_GPIO
  if (g_gpio_fd >= 0)
    {
      bool led_on = true;
      ioctl(g_gpio_fd, GPIOC_WRITE, (unsigned long)&led_on);
    }
#endif

  g_alert_active = true;

  /* For non-critical alerts, auto-stop after a brief period */

  if (level != ALERT_CRITICAL)
    {
      usleep(ALERT_LED_BLINK_MS * 1000);
      alert_module_stop();
    }

  return 0;
}

/****************************************************************************
 * Name: alert_module_stop
 ****************************************************************************/

int alert_module_stop(void)
{
  if (!g_alert_active)
    {
      return 0;
    }

  /* Stop buzzer */

  if (g_pwm_fd >= 0)
    {
      ioctl(g_pwm_fd, PWMIOC_STOP, 0);
    }

  /* Turn off LED */

#ifdef CONFIG_DEV_GPIO
  if (g_gpio_fd >= 0)
    {
      bool led_on = false;
      ioctl(g_gpio_fd, GPIOC_WRITE, (unsigned long)&led_on);
    }
#endif

  g_alert_active = false;
  return 0;
}
