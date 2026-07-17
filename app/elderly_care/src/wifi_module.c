/****************************************************************************
 * elderly_care/src/wifi_module.c
 *
 * WiFi module using MQTT protocol to publish alert messages
 * to a remote broker in JSON format.
 ****************************************************************************/

#include "elderly_care.h"
#include <arpa/inet.h>
#include <sys/socket.h>

/* Module state */

static int g_mqtt_sock = -1;
static bool g_connected = false;

/****************************************************************************
 * Name: mqtt_connect
 *
 * Description: Simple MQTT CONNECT packet send and CONNACK receive.
 *              Returns 0 on success, negative errno on failure.
 ****************************************************************************/

static int mqtt_connect(const char *client_id)
{
  uint8_t packet[128];
  int pos = 0;
  uint16_t client_id_len;
  uint16_t remain_len;
  uint8_t connack[4];
  ssize_t n;

  client_id_len = strlen(client_id);
  remain_len = 10 + 2 + client_id_len;

  /* Fixed header: CONNECT */

  packet[pos++] = 0x10;
  packet[pos++] = (uint8_t)remain_len;

  /* Variable header */

  packet[pos++] = 0x00; /* Protocol name MSB */
  packet[pos++] = 0x04; /* Protocol name LSB */
  packet[pos++] = 'M';
  packet[pos++] = 'Q';
  packet[pos++] = 'T';
  packet[pos++] = 'T';
  packet[pos++] = 0x04; /* Protocol level */
  packet[pos++] = 0x02; /* Connect flags: clean session */
  packet[pos++] = 0x00; /* Keep-alive MSB */
  packet[pos++] = 0x3C; /* Keep-alive LSB: 60s */

  /* Payload: client ID */

  packet[pos++] = (uint8_t)(client_id_len >> 8);
  packet[pos++] = (uint8_t)(client_id_len & 0xff);
  memcpy(&packet[pos], client_id, client_id_len);
  pos += client_id_len;

  /* Send CONNECT packet */

  n = send(g_mqtt_sock, packet, pos, 0);
  if (n < 0)
    {
      return -errno;
    }

  /* Receive CONNACK */

  n = recv(g_mqtt_sock, connack, sizeof(connack), 0);
  if (n < 4)
    {
      return -EIO;
    }

  /* Check CONNACK return code */

  if (connack[3] != 0x00)
    {
      syslog(LOG_ERR, "MQTT CONNACK error: %d\n", connack[3]);
      return -ECONNREFUSED;
    }

  return 0;
}

/****************************************************************************
 * Name: mqtt_publish
 *
 * Description: Publish a message to an MQTT topic.
 ****************************************************************************/

static int mqtt_publish(const char *topic, const char *message)
{
  uint8_t *packet;
  uint16_t topic_len;
  uint32_t message_len;
  uint32_t remain_len;
  uint32_t pos;
  ssize_t n;

  topic_len = strlen(topic);
  message_len = strlen(message);
  remain_len = 2 + topic_len + message_len;

  packet = (uint8_t *)malloc(2 + remain_len);
  if (packet == NULL)
    {
      return -ENOMEM;
    }

  pos = 0;

  /* Fixed header: PUBLISH, QoS 0 */

  packet[pos++] = 0x30;

  /* Remaining length encoding */

  if (remain_len < 128)
    {
      packet[pos++] = (uint8_t)remain_len;
    }
  else
    {
      packet[pos++] = (uint8_t)(remain_len % 128 + 128);
      packet[pos++] = (uint8_t)(remain_len / 128);
    }

  /* Topic */

  packet[pos++] = (uint8_t)(topic_len >> 8);
  packet[pos++] = (uint8_t)(topic_len & 0xff);
  memcpy(&packet[pos], topic, topic_len);
  pos += topic_len;

  /* Message payload */

  memcpy(&packet[pos], message, message_len);
  pos += message_len;

  n = send(g_mqtt_sock, packet, pos, 0);
  free(packet);

  if (n < 0)
    {
      return -errno;
    }

  return 0;
}

/****************************************************************************
 * Name: wifi_module_init
 ****************************************************************************/

int wifi_module_init(void)
{
  g_mqtt_sock = -1;
  g_connected = false;
  syslog(LOG_INFO, "WiFi module initialized\n");
  return 0;
}

/****************************************************************************
 * Name: wifi_module_deinit
 ****************************************************************************/

int wifi_module_deinit(void)
{
  if (g_mqtt_sock >= 0)
    {
      close(g_mqtt_sock);
      g_mqtt_sock = -1;
    }

  g_connected = false;
  syslog(LOG_INFO, "WiFi module deinitialized\n");
  return 0;
}

/****************************************************************************
 * Name: wifi_module_connect
 ****************************************************************************/

int wifi_module_connect(const char *ssid, const char *password)
{
  struct sockaddr_in broker_addr;
  int ret;

  if (ssid == NULL || password == NULL)
    {
      return -EINVAL;
    }

  /* Create TCP socket */

  g_mqtt_sock = socket(AF_INET, SOCK_STREAM, 0);
  if (g_mqtt_sock < 0)
    {
      syslog(LOG_ERR, "socket create failed: %d\n", errno);
      return -errno;
    }

  /* Connect to MQTT broker */

  memset(&broker_addr, 0, sizeof(broker_addr));
  broker_addr.sin_family = AF_INET;
  broker_addr.sin_port = htons(MQTT_BROKER_PORT);
  inet_pton(AF_INET, MQTT_BROKER_HOST, &broker_addr.sin_addr);

  ret = connect(g_mqtt_sock,
                (struct sockaddr *)&broker_addr,
                sizeof(broker_addr));
  if (ret < 0)
    {
      syslog(LOG_ERR, "MQTT broker connect failed: %d\n", errno);
      close(g_mqtt_sock);
      g_mqtt_sock = -1;
      return -errno;
    }

  /* Send MQTT CONNECT */

  ret = mqtt_connect(MQTT_CLIENT_ID);
  if (ret < 0)
    {
      syslog(LOG_ERR, "MQTT CONNECT failed: %d\n", ret);
      close(g_mqtt_sock);
      g_mqtt_sock = -1;
      return ret;
    }

  g_connected = true;
  syslog(LOG_INFO, "WiFi connected to MQTT broker\n");
  return 0;
}

/****************************************************************************
 * Name: wifi_module_send_alert
 ****************************************************************************/

int wifi_module_send_alert(struct alert_record_s *record)
{
  char topic[64];
  char message[512];
  struct tm tm_info;
  int ret;

  if (record == NULL)
    {
      return -EINVAL;
    }

  /* Try to connect if not connected */

  if (!g_connected)
    {
      ret = wifi_module_connect("", "");
      if (ret < 0)
        {
          return ret;
        }
    }

  /* Build topic */

  snprintf(topic, sizeof(topic), "%salerts", MQTT_TOPIC_PREFIX);

  /* Build JSON message */

  localtime_r(&record->timestamp.tv_sec, &tm_info);
  snprintf(message, sizeof(message),
           "{\"type\":%d,\"level\":%d,"
           "\"time\":\"%04d-%02d-%02dT%02d:%02d:%02d\","
           "\"message\":\"%s\"}",
           record->type,
           record->level,
           tm_info.tm_year + 1900,
           tm_info.tm_mon + 1,
           tm_info.tm_mday,
           tm_info.tm_hour,
           tm_info.tm_min,
           tm_info.tm_sec,
           record->message);

  /* Publish */

  ret = mqtt_publish(topic, message);
  if (ret < 0)
    {
      syslog(LOG_ERR, "MQTT publish failed: %d\n", ret);
      g_connected = false;
      return ret;
    }

  return 0;
}
