/****************************************************************************
 * elderly_care/src/camera_module.c
 *
 * Camera module using V4L2 API to capture frames from /dev/video0.
 ****************************************************************************/

#include "elderly_care.h"
#include <sys/videoio.h>

/* Module state */

static int g_cam_fd = -1;
static uint8_t g_cam_buffer[CAMERA_FRAME_SIZE];

/****************************************************************************
 * Name: camera_module_init
 ****************************************************************************/

int camera_module_init(void)
{
  struct v4l2_format fmt;
  struct v4l2_requestbuffers req;
  int ret;

  g_cam_fd = open("/dev/video0", O_RDONLY);
  if (g_cam_fd < 0)
    {
      syslog(LOG_ERR, "open /dev/video0 failed: %d\n", errno);
      return -errno;
    }

  /* Set format: 240x320 YUYV */

  memset(&fmt, 0, sizeof(fmt));
  fmt.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
  fmt.fmt.pix.width = CAMERA_WIDTH;
  fmt.fmt.pix.height = CAMERA_HEIGHT;
  fmt.fmt.pix.pixelformat = V4L2_PIX_FMT_YUYV;
  fmt.fmt.pix.field = V4L2_FIELD_NONE;

  ret = ioctl(g_cam_fd, VIDIOC_S_FMT, &fmt);
  if (ret < 0)
    {
      syslog(LOG_ERR, "VIDIOC_S_FMT failed: %d\n", errno);
      close(g_cam_fd);
      g_cam_fd = -1;
      return -errno;
    }

  /* Request buffers */

  memset(&req, 0, sizeof(req));
  req.count = 1;
  req.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
  req.memory = V4L2_MEMORY_USERPTR;

  ret = ioctl(g_cam_fd, VIDIOC_REQBUFS, &req);
  if (ret < 0)
    {
      syslog(LOG_ERR, "VIDIOC_REQBUFS failed: %d\n", errno);
      close(g_cam_fd);
      g_cam_fd = -1;
      return -errno;
    }

  /* Start streaming */

  enum v4l2_buf_type type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
  ret = ioctl(g_cam_fd, VIDIOC_STREAMON, &type);
  if (ret < 0)
    {
      syslog(LOG_ERR, "VIDIOC_STREAMON failed: %d\n", errno);
      close(g_cam_fd);
      g_cam_fd = -1;
      return -errno;
    }

  syslog(LOG_INFO, "Camera module initialized (%dx%d YUYV)\n",
         CAMERA_WIDTH, CAMERA_HEIGHT);
  return 0;
}

/****************************************************************************
 * Name: camera_module_deinit
 ****************************************************************************/

int camera_module_deinit(void)
{
  if (g_cam_fd >= 0)
    {
      enum v4l2_buf_type type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
      ioctl(g_cam_fd, VIDIOC_STREAMOFF, &type);
      close(g_cam_fd);
      g_cam_fd = -1;
    }

  syslog(LOG_INFO, "Camera module deinitialized\n");
  return 0;
}

/****************************************************************************
 * Name: camera_module_capture
 ****************************************************************************/

int camera_module_capture(uint8_t *buffer, size_t *size)
{
  struct v4l2_buffer buf;
  int ret;

  if (g_cam_fd < 0 || buffer == NULL || size == NULL)
    {
      return -EINVAL;
    }

  /* Dequeue a buffer */

  memset(&buf, 0, sizeof(buf));
  buf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
  buf.memory = V4L2_MEMORY_USERPTR;
  buf.m.userptr = (unsigned long)g_cam_buffer;
  buf.length = CAMERA_FRAME_SIZE;

  ret = ioctl(g_cam_fd, VIDIOC_DQBUF, &buf);
  if (ret < 0)
    {
      return -errno;
    }

  /* Copy frame data */

  memcpy(buffer, g_cam_buffer, buf.bytesused);
  *size = buf.bytesused;

  /* Re-queue the buffer */

  ret = ioctl(g_cam_fd, VIDIOC_QBUF, &buf);
  if (ret < 0)
    {
      return -errno;
    }

  return 0;
}
