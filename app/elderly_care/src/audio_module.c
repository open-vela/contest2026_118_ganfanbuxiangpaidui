/****************************************************************************
 * elderly_care/src/audio_module.c
 *
 * Audio module for voice event detection.
 * Reads PCM data from /dev/audio/pcm0c (16kHz/2ch/16bit).
 * Detects help calls using RMS energy and burst counting.
 ****************************************************************************/

#include "elderly_care.h"
#include <nuttx/audio/audio.h>

/* Module state */

static int g_audio_fd = -1;

/* Detection state */

static int g_burst_count = 0;

/****************************************************************************
 * Name: audio_module_init
 ****************************************************************************/

int audio_module_init(void)
{
  g_audio_fd = open("/dev/audio/pcm0c", O_RDONLY);
  if (g_audio_fd < 0)
    {
      syslog(LOG_ERR, "open /dev/audio/pcm0c failed: %d\n", errno);
      return -errno;
    }

  /* Configure audio: 16kHz, 2 channels, 16-bit */

  struct audio_caps_desc_s caps;
  memset(&caps, 0, sizeof(caps));
  caps.caps.ac_len = sizeof(caps);
  caps.caps.ac_type.audio_type = AUDIO_TYPE_INPUT;
  caps.caps.ac_channels = AUDIO_CHANNELS;
  caps.caps.ac_chmap = AUDIO_CHANNEL_MAP_STEREO;
  caps.caps.ac_controls.b[0] = AUDIO_SUBFMT_END;
  caps.caps.ac_sample_rate = AUDIO_SAMPLE_RATE;
  caps.caps.ac_bitrate = AUDIO_SAMPLE_RATE * AUDIO_CHANNELS * 16;
  caps.caps.ac_bitwidth = 16;

  ioctl(g_audio_fd, AUDIOIOC_CONFIGURE, (unsigned long)&caps);

  g_burst_count = 0;

  syslog(LOG_INFO, "Audio module initialized (%dHz %dch %dbit)\n",
         AUDIO_SAMPLE_RATE, AUDIO_CHANNELS, 16);
  return 0;
}

/****************************************************************************
 * Name: audio_module_deinit
 ****************************************************************************/

int audio_module_deinit(void)
{
  if (g_audio_fd >= 0)
    {
      close(g_audio_fd);
      g_audio_fd = -1;
    }

  g_burst_count = 0;
  syslog(LOG_INFO, "Audio module deinitialized\n");
  return 0;
}

/****************************************************************************
 * Name: audio_module_capture
 ****************************************************************************/

int audio_module_capture(int16_t *buffer, size_t samples)
{
  ssize_t bytes_read;
  size_t target_bytes;

  if (g_audio_fd < 0 || buffer == NULL)
    {
      return -EINVAL;
    }

  target_bytes = samples * sizeof(int16_t);
  bytes_read = read(g_audio_fd, buffer, target_bytes);

  if (bytes_read < 0)
    {
      return -errno;
    }

  if ((size_t)bytes_read < target_bytes)
    {
      /* Partial read, zero-fill remainder */

      memset((uint8_t *)buffer + bytes_read, 0,
             target_bytes - bytes_read);
    }

  return 0;
}

/****************************************************************************
 * Name: audio_module_detect
 ****************************************************************************/

int audio_module_detect(int16_t *buffer, struct audio_result_s *result)
{
  size_t i;
  size_t count;
  double sum_sq = 0.0;
  double rms;
  double frame_energy;

  if (buffer == NULL || result == NULL)
    {
      return -EINVAL;
    }

  /* Calculate RMS and frame energy */

  count = AUDIO_BUFFER_SIZE / 2;
  for (i = 0; i < count; i++)
    {
      double sample = (double)buffer[i] / 32768.0;

      sum_sq += sample * sample;
    }

  rms = sqrt(sum_sq / count);
  frame_energy = sum_sq / count;

  /* Update noise level */

  result->noise_level = (float)rms;

  /* Burst detection: consecutive high-energy frames indicate help call */

  if (rms > 0.2 && frame_energy > 0.04)
    {
      g_burst_count++;
    }
  else
    {
      if (g_burst_count > 0)
        {
          g_burst_count--;
        }
    }

  /* Detect help call: burst_count > 3 means sustained loud vocalization */

  if (g_burst_count > 3 && rms > 0.2)
    {
      result->help_detected = true;
      result->help_confidence = (float)(rms * 2.0);
      if (result->help_confidence > 1.0f)
        {
          result->help_confidence = 1.0f;
        }
      result->moan_detected = false;
      result->moan_confidence = 0.0f;
    }
  else
    {
      result->help_detected = false;
      result->help_confidence = 0.0f;
      result->moan_detected = false;
      result->moan_confidence = 0.0f;
    }

  return 0;
}
