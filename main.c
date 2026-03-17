/*
 * modbus_slave.c
 * Simple Modbus RTU slave using libmodbus.
 * - Listens on a serial port (default /dev/ttyS2)
 * - Slave/unit id = 1 (configurable)
 * - Exposes holding registers; holding index 1 corresponds to Modbus 40002
 *
 * Compile:
 *   gcc -o modbus_slave modbus_slave.c mqtt_service.c modbus_service.c -lmodbus
 * -lmosquitto
 *
 * Run:
 *   sudo ./modbus_slave /dev/ttyS2 9600 N 8 1 1
 * args:
 *   argv[1] = serial device
 *   argv[2] = baud
 *   argv[3] = parity (N/E/O)
 *   argv[4] = data bits (usually 8)
 *   argv[5] = stop bits (1 or 2)
 *   argv[6] = slave id (1)
 *   argv[7] = initial value for register 40002 (optional, default 0)
 */

#include <errno.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#include "modbus_service.h"
#include "mqtt_service.h"

#define MQTT_HOST "127.0.0.1"
#define MQTT_PORT 1883
#define MQTT_TOPIC "hmi/button"

static volatile int run = 1;

void sigint_handler(int sig) {
  (void)sig;
  run = 0;
}

int main(int argc, char *argv[]) {
  if (argc < 7) {
    fprintf(stderr,
            "Usage: %s <device> <baud> <parity> <data_bits> <stop_bits> "
            "<slave_id> [initial_40002]\n",
            argv[0]);
    return 1;
  }

  struct mosquitto *mosq =
      mqtt_service_init(MQTT_HOST, MQTT_PORT, "iot2050_c_client");
  if (!mosq) {
    return -1;
  }

  const char *device = argv[1];
  int baud = atoi(argv[2]);
  char parity = argv[3][0];
  int data_bits = atoi(argv[4]);
  int stop_bits = atoi(argv[5]);
  int slave_id = atoi(argv[6]);
  uint16_t initial_value = 1;
  if (argc >= 8)
    initial_value = (uint16_t)atoi(argv[7]);

  /* Signal handling for clean shutdown */
  signal(SIGINT, sigint_handler);
  signal(SIGTERM, sigint_handler);

  /* Create new RTU context */
  modbus_t *ctx =
      modbus_service_init(device, baud, parity, data_bits, stop_bits, slave_id);
  if (!ctx) {
    mqtt_service_cleanup(mosq);
    return -1;
  }

  /* Create mapping: number of coils/discrete/holding/input regs.
     We need at least 2 holding registers (so that index 1 exists for 40002).
     modbus_mapping_new(start_coils, start_discrete_inputs,
     start_holding_registers, start_input_registers) Here we allocate 0
     coils/di, 100 holding regs, 0 input regs.
  */
  const int HOLDING_REG_COUNT = 100;

  /* Original code unconditionally mapped 333 to register index 1 despite
   * reading initial_value from args */
  modbus_mapping_t *mb_mapping =
      modbus_service_mapping_init(HOLDING_REG_COUNT, 333);
  if (!mb_mapping) {
    modbus_service_cleanup(ctx, NULL);
    mqtt_service_cleanup(mosq);
    return -1;
  }

  /* Buffer to receive requests */
  uint8_t query[MODBUS_RTU_MAX_ADU_LENGTH];

  printf("Modbus RTU slave running on %s, baud %d, parity %c, data %d, stop "
         "%d, unit %d\n",
         device, baud, parity, data_bits, stop_bits, slave_id);
  printf("Listening for requests. Holding register 40002 (index 1) initial "
         "value: %u\n",
         initial_value);

  uint16_t old_val = 0;

  while (run) {
    int rc;

    /* Receive a request from master.
     * rc = length of request (positive) on success,
     * rc = -1 on error or timeout (depending on libmodbus behavior).
     */
    rc = modbus_receive(ctx, query);
    if (rc > 0) {
      rc = modbus_reply(ctx, query, rc, mb_mapping);
      uint16_t current_val = mb_mapping->tab_registers[2];

      if (current_val != old_val) {
        printf("HMI Button Pressed! Publishing to MQTT ...\n");
        mqtt_service_publish(mosq, MQTT_TOPIC, current_val);
      }

      old_val = current_val;
    } else if (rc == -1) {
      /* Error or timeout. If error is critical, break loop; otherwise continue
       */
      int err = errno;
      if (err == EBADF || err == EIO) {
        fprintf(stderr, "modbus_receive critical error: %s\n",
                modbus_strerror(err));
        break;
      }
      /* else non-fatal, continue listening */
    }
    /* small sleep to avoid busy loop */
    usleep(1000);
  }

  printf("Shutting down...\n");
  mqtt_service_cleanup(mosq);
  modbus_service_cleanup(ctx, mb_mapping);
  return 0;
}
