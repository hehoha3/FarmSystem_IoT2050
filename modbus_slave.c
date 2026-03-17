/*
 * modbus_slave.c
 * Simple Modbus RTU slave using libmodbus.
 * - Listens on a serial port (default /dev/ttyS2)
 * - Slave/unit id = 1 (configurable)
 * - Exposes holding registers; holding index 1 corresponds to Modbus 40002
 *
 * Compile:
 *   gcc -o modbus_slave modbus_slave.c -lmodbus
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

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <signal.h>
#include <errno.h>
#include <unistd.h>
#include <termios.h>
#include <sys/ioctl.h>
#include <modbus/modbus.h>
#include <mosquitto.h>

#define MQTT_HOST "127.0.0.1"
#define MQTT_PORT 1883
#define MQTT_TOPIC "hmi/button"

static volatile int run = 1;

void sigint_handler(int sig) {
    (void)sig;
    run = 0;
}

/* Optional helper to toggle RTS (for DE/RE control on some RS485 transceivers).
 * If your adapter handles direction automatically, you don't need to use this.
 * This function sets or clears the RTS line on the serial fd.
 */
int set_rts(int fd, int level) {
    int status;
    if (ioctl(fd, TIOCMGET, &status) == -1) {
        perror("ioctl TIOCMGET");
        return -1;
    }
    if (level)
        status |= TIOCM_RTS;
    else
        status &= ~TIOCM_RTS;
    if (ioctl(fd, TIOCMSET, &status) == -1) {
        perror("ioctl TIOCMSET");
        return -1;
    }
    return 0;
}

int main(int argc, char *argv[])
{
    if (argc < 7) {
        fprintf(stderr, "Usage: %s <device> <baud> <parity> <data_bits> <stop_bits> <slave_id> [initial_40002]\n", argv[0]);
        return 1;
    }

    mosquitto_lib_init();
    struct mosquitto *mosq = mosquitto_new("iot2050_c_client", true, NULL);
    if(mosquitto_connect(mosq, MQTT_HOST, MQTT_PORT, 60)){
        fprintf(stderr, "Can not connected to MQTT Broker\n");
        return -1;
    }
        
    // Maintain MQTT connectivity and process background packets.
    mosquitto_loop_start(mosq);

    const char *device = argv[1];
    int baud = atoi(argv[2]);
    char parity = argv[3][0];
    int data_bits = atoi(argv[4]);
    int stop_bits = atoi(argv[5]);
    int slave_id = atoi(argv[6]);
    uint16_t initial_value = 1;
    if (argc >= 8) initial_value = (uint16_t)atoi(argv[7]);

    /* Signal handling for clean shutdown */
    signal(SIGINT, sigint_handler);
    signal(SIGTERM, sigint_handler);

    /* Create new RTU context */
    modbus_t *ctx = modbus_new_rtu(device, baud, parity, data_bits, stop_bits);
    if (ctx == NULL) {
        fprintf(stderr, "Unable to create the libmodbus context\n");
        return -1;
    }

    /* Set slave id so that only requests for this id are processed */
    if (modbus_set_slave(ctx, slave_id) == -1) {
        fprintf(stderr, "Invalid slave id\n");
        modbus_free(ctx);
        return -1;
    }

    /* Optional: set response timeout (s, us) */
    modbus_set_response_timeout(ctx, 1, 0);

    /* Connect to the serial port */
    if (modbus_connect(ctx) == -1) {
        fprintf(stderr, "Connection failed: %s\n", modbus_strerror(errno));
        modbus_free(ctx);
        return -1;
    }

    /* If you need direct control of RTS/DE, you can access ctx->s to get fd and toggle RTS.
       Example: set RTS high before sending, low after sending. We'll assume auto-direction adapters
       are used; toggle example provided below commented.
    */

    /* Create mapping: number of coils/discrete/holding/input regs.
       We need at least 2 holding registers (so that index 1 exists for 40002).
       modbus_mapping_new(start_coils, start_discrete_inputs, start_holding_registers, start_input_registers)
       Here we allocate 0 coils/di, 100 holding regs, 0 input regs.
    */
    const int HOLDING_REG_COUNT = 100;
    modbus_mapping_t *mb_mapping = modbus_mapping_new(0, 0, HOLDING_REG_COUNT, 0);
    if (mb_mapping == NULL) {
        fprintf(stderr, "Failed to allocate the mapping: %s\n", modbus_strerror(errno));
        modbus_close(ctx);
        modbus_free(ctx);
        return -1;
    }

//    modbus_set_debug(ctx, TRUE);

    /* Initialize register 40002 -> index 1 */
    /* Note: Modbus convention: 40001 -> index 0, so 40002 -> index 1 */
    if (HOLDING_REG_COUNT <= 1) {
        fprintf(stderr, "HOLDING_REG_COUNT too small\n");
        modbus_mapping_free(mb_mapping);
        modbus_close(ctx);
        modbus_free(ctx);
        return -1;
    }
    mb_mapping->tab_registers[1] = 333;

    /* Buffer to receive requests */
    uint8_t query[MODBUS_RTU_MAX_ADU_LENGTH];

    printf("Modbus RTU slave running on %s, baud %d, parity %c, data %d, stop %d, unit %d\n",
           device, baud, parity, data_bits, stop_bits, slave_id);
    printf("Listening for requests. Holding register 40002 (index 1) initial value: %u\n", initial_value);

    uint16_t old_val = 0;

    while (run) {
        int rc;

        /* Receive a request from master.
         * rc = length of request (positive) on success,
         * rc = -1 on error or timeout (depending on libmodbus behavior).
         */
        rc = modbus_receive(ctx, query);
        if (rc > 0) {
            /* If you must control DE/RE manually, set RTS here before reply (example):
             * int fd = modbus_get_socket(ctx);
             * set_rts(fd, 1);  // enable driver
             * usleep(1000);    // small delay if needed
             *
             * modbus_reply(ctx, query, rc, mb_mapping);
             *
             * usleep(1000);
             * set_rts(fd, 0);  // disable driver
             */

            rc = modbus_reply(ctx, query, rc, mb_mapping);
	    uint16_t current_val = mb_mapping->tab_registers[2];

	    if (current_val != old_val)
	    {
		printf("HMI Button Pressed! Publishing to MQTT ...\n");
		char msg[10];
		sprintf(msg, "%d", current_val);
		mosquitto_publish(mosq, NULL, MQTT_TOPIC, strlen(msg), msg, 0, false);
	    }

	    old_val = current_val;
            //if (rc == -1) {
            //    fprintf(stderr, "modbus_reply failed: %s\n", modbus_strerror(errno));
            //    /* continue, don't exit immediately */
	    //}
        } else if (rc == -1) {
            /* Error or timeout. If error is critical, break loop; otherwise continue */
            int err = errno;
            if (err == EBADF || err == EIO) {
                fprintf(stderr, "modbus_receive critical error: %s\n", modbus_strerror(err));
                break;
            }
            /* else non-fatal, continue listening */
        }
        /* small sleep to avoid busy loop */
        usleep(1000);
    }

    printf("Shutting down...\n");
    mosquitto_destroy(mosq);
    mosquitto_lib_cleanup();
    modbus_mapping_free(mb_mapping);
    modbus_close(ctx);
    modbus_free(ctx);
    return 0;
}
