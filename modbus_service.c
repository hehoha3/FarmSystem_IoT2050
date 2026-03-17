#include "modbus_service.h"
#include <stdio.h>
#include <errno.h>
#include <termios.h>
#include <sys/ioctl.h>
#include <unistd.h>

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

modbus_t* modbus_service_init(const char *device, int baud, char parity, int data_bits, int stop_bits, int slave_id) {
    modbus_t *ctx = modbus_new_rtu(device, baud, parity, data_bits, stop_bits);
    if (ctx == NULL) {
        fprintf(stderr, "Unable to create the libmodbus context\n");
        return NULL;
    }

    if (modbus_set_slave(ctx, slave_id) == -1) {
        fprintf(stderr, "Invalid slave id\n");
        modbus_free(ctx);
        return NULL;
    }

    modbus_set_response_timeout(ctx, 1, 0);

    if (modbus_connect(ctx) == -1) {
        fprintf(stderr, "Connection failed: %s\n", modbus_strerror(errno));
        modbus_free(ctx);
        return NULL;
    }

    return ctx;
}

modbus_mapping_t* modbus_service_mapping_init(int holding_reg_count, uint16_t initial_40002_value) {
    if (holding_reg_count <= 1) {
        fprintf(stderr, "HOLDING_REG_COUNT too small\n");
        return NULL;
    }

    modbus_mapping_t *mb_mapping = modbus_mapping_new(0, 0, holding_reg_count, 0);
    if (mb_mapping == NULL) {
        fprintf(stderr, "Failed to allocate the mapping: %s\n", modbus_strerror(errno));
        return NULL;
    }

    /* Initialize register 40002 -> index 1 */
    mb_mapping->tab_registers[1] = initial_40002_value;

    return mb_mapping;
}

void modbus_service_cleanup(modbus_t *ctx, modbus_mapping_t *mb_mapping) {
    if (mb_mapping) {
        modbus_mapping_free(mb_mapping);
    }
    if (ctx) {
        modbus_close(ctx);
        modbus_free(ctx);
    }
}
