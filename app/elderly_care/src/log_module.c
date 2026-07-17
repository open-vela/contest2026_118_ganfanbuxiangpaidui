/****************************************************************************
 * elderly_care/src/log_module.c
 *
 * Log module with in-memory circular buffer and Flash write-back.
 * Caches log entries when network is unavailable, flushes via WiFi
 * when connectivity is restored.
 ****************************************************************************/

#include "elderly_care.h"

/* Circular buffer for log entries */

struct log_entry_s
{
  bool valid;
  struct alert_record_s record;
};

static struct log_entry_s g_log_buffer[LOG_MAX_ENTRIES];
static int g_log_head = 0;
static int g_log_tail = 0;
static int g_log_count = 0;
static int g_flash_fd = -1;

/****************************************************************************
 * Name: log_module_init
 ****************************************************************************/

int log_module_init(void)
{
  memset(g_log_buffer, 0, sizeof(g_log_buffer));
  g_log_head = 0;
  g_log_tail = 0;
  g_log_count = 0;

  /* Open Flash partition for persistent log storage */

  g_flash_fd = open(LOG_FLASH_PARTITION, O_WRONLY | O_CREAT | O_APPEND);
  if (g_flash_fd < 0)
    {
      syslog(LOG_WARNING,
             "Flash log partition not available, using memory only\n");
    }

  syslog(LOG_INFO, "Log module initialized (max %d entries)\n",
         LOG_MAX_ENTRIES);
  return 0;
}

/****************************************************************************
 * Name: log_module_deinit
 ****************************************************************************/

int log_module_deinit(void)
{
  /* Flush pending entries before shutdown */

  log_module_flush_pending();

  if (g_flash_fd >= 0)
    {
      close(g_flash_fd);
      g_flash_fd = -1;
    }

  g_log_head = 0;
  g_log_tail = 0;
  g_log_count = 0;

  syslog(LOG_INFO, "Log module deinitialized\n");
  return 0;
}

/****************************************************************************
 * Name: log_module_write
 ****************************************************************************/

int log_module_write(struct alert_record_s *record)
{
  if (record == NULL)
    {
      return -EINVAL;
    }

  /* Add to circular buffer */

  memcpy(&g_log_buffer[g_log_head].record, record,
         sizeof(struct alert_record_s));
  g_log_buffer[g_log_head].valid = true;

  g_log_head = (g_log_head + 1) % LOG_MAX_ENTRIES;

  if (g_log_count < LOG_MAX_ENTRIES)
    {
      g_log_count++;
    }
  else
    {
      /* Buffer full, advance tail (oldest entry overwritten) */

      g_log_tail = (g_log_tail + 1) % LOG_MAX_ENTRIES;
    }

  /* Write to Flash if available */

  if (g_flash_fd >= 0)
    {
      char flash_entry[LOG_ENTRY_MAX_LEN];
      struct tm tm_info;
      int len;

      localtime_r(&record->timestamp.tv_sec, &tm_info);
      len = snprintf(flash_entry, sizeof(flash_entry),
                     "[%04d-%02d-%02d %02d:%02d:%02d] type=%d level=%d %s\n",
                     tm_info.tm_year + 1900,
                     tm_info.tm_mon + 1,
                     tm_info.tm_mday,
                     tm_info.tm_hour,
                     tm_info.tm_min,
                     tm_info.tm_sec,
                     record->type,
                     record->level,
                     record->message);

      write(g_flash_fd, flash_entry, len);
    }

  return 0;
}

/****************************************************************************
 * Name: log_module_flush_pending
 ****************************************************************************/

int log_module_flush_pending(void)
{
  int count = 0;

  /* Flush all pending entries from circular buffer */

  while (g_log_count > 0)
    {
      struct log_entry_s *entry = &g_log_buffer[g_log_tail];

      if (entry->valid)
        {
          /* Try to send via WiFi if connected */

#ifdef CONFIG_ELDERLY_CARE_WIFI_ENABLE
          wifi_module_send_alert(&entry->record);
#endif
          entry->valid = false;
          count++;
        }

      g_log_tail = (g_log_tail + 1) % LOG_MAX_ENTRIES;
      g_log_count--;
    }

  if (count > 0)
    {
      syslog(LOG_INFO, "Flushed %d pending log entries\n", count);
    }

  return count;
}
