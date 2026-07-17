/****************************************************************************
 * elderly_care/src/power_module.c
 *
 * Power management module using NuttX PM API.
 * Enters low-power sleep mode when no person is detected for 30 minutes.
 ****************************************************************************/

#include "elderly_care.h"
#include <nuttx/power/pm.h>

/* Module state */

static bool g_in_sleep = false;

/****************************************************************************
 * Name: power_module_init
 ****************************************************************************/

int power_module_init(void)
{
  g_in_sleep = false;
  syslog(LOG_INFO, "Power module initialized\n");
  return 0;
}

/****************************************************************************
 * Name: power_module_deinit
 ****************************************************************************/

int power_module_deinit(void)
{
  if (g_in_sleep)
    {
      power_module_wake_up();
    }

  syslog(LOG_INFO, "Power module deinitialized\n");
  return 0;
}

/****************************************************************************
 * Name: power_module_check_idle
 ****************************************************************************/

int power_module_check_idle(struct system_state_s *state)
{
  time_t now;
  time_t idle_seconds;

  if (state == NULL)
    {
      return -EINVAL;
    }

  now = time(NULL);
  idle_seconds = now - state->last_person_time;

  /* Check if system has been idle for more than 30 minutes */

  if (idle_seconds >= POWER_SAVE_TIMEOUT && !state->power_save_mode)
    {
      syslog(LOG_INFO,
             "No person detected for %ld seconds, entering low power\n",
             (long)idle_seconds);

      state->power_save_mode = true;
      g_in_sleep = true;

      /* Notify PM subsystem of activity to reset timer */

      pm_activity(CONFIG_PM_IDLE_DOMAIN, 0);

      return power_module_enter_sleep();
    }

  /* Keep PM awake while people are present */

  if (state->sensor.human_present)
    {
      pm_activity(CONFIG_PM_IDLE_DOMAIN, 0);
    }

  return 0;
}

/****************************************************************************
 * Name: power_module_enter_sleep
 ****************************************************************************/

int power_module_enter_sleep(void)
{
  if (g_in_sleep)
    {
      return 0;
    }

  g_in_sleep = true;

  syslog(LOG_INFO, "Entering low power sleep mode\n");

  /* Reduce system activity - actual sleep is handled by PM framework */

  pm_activity(CONFIG_PM_IDLE_DOMAIN, 0);

  return 0;
}

/****************************************************************************
 * Name: power_module_wake_up
 ****************************************************************************/

int power_module_wake_up(void)
{
  if (!g_in_sleep)
    {
      return 0;
    }

  g_in_sleep = false;

  /* Signal PM activity to wake up */

  pm_activity(CONFIG_PM_IDLE_DOMAIN, 0);

  syslog(LOG_INFO, "Waking up from low power mode\n");
  return 0;
}
