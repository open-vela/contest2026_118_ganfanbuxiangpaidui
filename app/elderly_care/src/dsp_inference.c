/****************************************************************************
 * elderly_care/src/dsp_inference.c
 *
 * DSP inference module for human/fall detection.
 * Falls back to frame-difference motion detection when no model is loaded.
 ****************************************************************************/

#include "elderly_care.h"

/* DSP device state */

static int g_dsp_fd = -1;
static bool g_model_loaded = false;

/* Previous frame buffer for motion detection fallback */

static uint8_t g_prev_frame[AI_MODEL_INPUT_W * AI_MODEL_INPUT_H * 3];
static bool g_prev_frame_valid = false;

/****************************************************************************
 * Name: yuv422_to_rgb
 *
 * Description: Convert YUV422 (YUYV) pixel data to RGB888.
 ****************************************************************************/

static void yuv422_to_rgb(const uint8_t *yuv, uint8_t *rgb,
                          int width, int height)
{
  int i;
  int total = width * height;

  for (i = 0; i < total / 2; i++)
    {
      int y0 = yuv[0];
      int u  = yuv[1];
      int y1 = yuv[2];
      int v  = yuv[3];
      int r;
      int g;
      int b;

      /* Y0 -> RGB */

      r = y0 + (int)(1.402f * (v - 128));
      g = y0 - (int)(0.344f * (u - 128)) - (int)(0.714f * (v - 128));
      b = y0 + (int)(1.772f * (u - 128));

      rgb[0] = (uint8_t)(r < 0 ? 0 : (r > 255 ? 255 : r));
      rgb[1] = (uint8_t)(g < 0 ? 0 : (g > 255 ? 255 : g));
      rgb[2] = (uint8_t)(b < 0 ? 0 : (b > 255 ? 255 : b));

      /* Y1 -> RGB */

      r = y1 + (int)(1.402f * (v - 128));
      g = y1 - (int)(0.344f * (u - 128)) - (int)(0.714f * (v - 128));
      b = y1 + (int)(1.772f * (u - 128));

      rgb[3] = (uint8_t)(r < 0 ? 0 : (r > 255 ? 255 : r));
      rgb[4] = (uint8_t)(g < 0 ? 0 : (g > 255 ? 255 : g));
      rgb[5] = (uint8_t)(b < 0 ? 0 : (b > 255 ? 255 : b));

      yuv += 4;
      rgb += 6;
    }
}

/****************************************************************************
 * Name: resize_nearest
 *
 * Description: Nearest-neighbor resize to target dimensions.
 ****************************************************************************/

static void resize_nearest(const uint8_t *src, int src_w, int src_h,
                           uint8_t *dst, int dst_w, int dst_h)
{
  int x;
  int y;

  for (y = 0; y < dst_h; y++)
    {
      int src_y = y * src_h / dst_h;

      for (x = 0; x < dst_w; x++)
        {
          int src_x = x * src_w / dst_w;
          int src_idx = (src_y * src_w + src_x) * 3;
          int dst_idx = (y * dst_w + x) * 3;

          dst[dst_idx]     = src[src_idx];
          dst[dst_idx + 1] = src[src_idx + 1];
          dst[dst_idx + 2] = src[src_idx + 2];
        }
    }
}

/****************************************************************************
 * Name: motion_detect_fallback
 *
 * Description: Frame-difference based motion detection (no DSP model).
 ****************************************************************************/

static int motion_detect_fallback(const uint8_t *rgb_resized,
                                  struct ai_result_s *result)
{
  int pixel_count = AI_MODEL_INPUT_W * AI_MODEL_INPUT_H * 3;
  int diff_sum = 0;
  int i;
  float avg_diff;
  float motion_ratio;

  if (!g_prev_frame_valid)
    {
      memcpy(g_prev_frame, rgb_resized, pixel_count);
      g_prev_frame_valid = true;
      result->human_detected = false;
      result->human_confidence = 0.0f;
      result->fall_detected = false;
      result->fall_confidence = 0.0f;
      return 0;
    }

  /* Compute average pixel difference */

  for (i = 0; i < pixel_count; i++)
    {
      int d = (int)rgb_resized[i] - (int)g_prev_frame[i];

      diff_sum += (d < 0) ? -d : d;
    }

  avg_diff = (float)diff_sum / pixel_count;
  motion_ratio = avg_diff / 255.0f;

  /* Update previous frame */

  memcpy(g_prev_frame, rgb_resized, pixel_count);

  /* Determine detection result based on motion threshold */

  if (motion_ratio > 0.05f)
    {
      result->human_detected = true;
      result->human_confidence = motion_ratio * 5.0f;
      if (result->human_confidence > 1.0f)
        {
          result->human_confidence = 1.0f;
        }
    }
  else
    {
      result->human_detected = false;
      result->human_confidence = 0.0f;
    }

  result->fall_detected = false;
  result->fall_confidence = 0.0f;
  result->human_x = 0;
  result->human_y = 0;
  result->human_w = 0;
  result->human_h = 0;

  return 0;
}

/****************************************************************************
 * Name: dsp_inference_init
 ****************************************************************************/

int dsp_inference_init(void)
{
  g_dsp_fd = open("/dev/dsp0", O_RDWR);
  if (g_dsp_fd < 0)
    {
      syslog(LOG_WARNING,
             "DSP device not available, using fallback detection\n");
      g_model_loaded = false;
      return 0;
    }

  /* Try to load model file */

  struct stat st;
  if (stat("/data/models/elderly_care_model.bin", &st) == 0)
    {
      g_model_loaded = true;
      syslog(LOG_INFO, "DSP model loaded successfully\n");
    }
  else
    {
      g_model_loaded = false;
      syslog(LOG_WARNING,
             "DSP model not found, using fallback detection\n");
    }

  g_prev_frame_valid = false;
  return 0;
}

/****************************************************************************
 * Name: dsp_inference_deinit
 ****************************************************************************/

int dsp_inference_deinit(void)
{
  if (g_dsp_fd >= 0)
    {
      close(g_dsp_fd);
      g_dsp_fd = -1;
    }

  g_model_loaded = false;
  g_prev_frame_valid = false;
  syslog(LOG_INFO, "DSP inference module deinitialized\n");
  return 0;
}

/****************************************************************************
 * Name: dsp_inference_detect
 ****************************************************************************/

int dsp_inference_detect(uint8_t *frame, struct ai_result_s *result)
{
  static uint8_t rgb_full[CAMERA_WIDTH * CAMERA_HEIGHT * 3];
  static uint8_t rgb_resized[AI_MODEL_INPUT_SIZE];

  if (frame == NULL || result == NULL)
    {
      return -EINVAL;
    }

  /* Convert YUV422 to RGB */

  yuv422_to_rgb(frame, rgb_full, CAMERA_WIDTH, CAMERA_HEIGHT);

  /* Resize to model input size (96x96) */

  resize_nearest(rgb_full, CAMERA_WIDTH, CAMERA_HEIGHT,
                 rgb_resized, AI_MODEL_INPUT_W, AI_MODEL_INPUT_H);

  if (g_model_loaded && g_dsp_fd >= 0)
    {
      /* Use DSP for inference */

      int ret = write(g_dsp_fd, rgb_resized, AI_MODEL_INPUT_SIZE);
      if (ret < 0)
        {
          syslog(LOG_ERR, "DSP write failed: %d\n", errno);
          return motion_detect_fallback(rgb_resized, result);
        }

      ret = read(g_dsp_fd, result, sizeof(struct ai_result_s));
      if (ret < (int)sizeof(struct ai_result_s))
        {
          syslog(LOG_ERR, "DSP read failed: %d\n", errno);
          return motion_detect_fallback(rgb_resized, result);
        }

      return 0;
    }

  /* Fallback: frame-difference motion detection */

  return motion_detect_fallback(rgb_resized, result);
}
