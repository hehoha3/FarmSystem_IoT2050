#ifndef MODBUS_SERVICE_H
#define MODBUS_SERVICE_H

#include <modbus/modbus.h>
#include <stdint.h>
#include <stdbool.h>

/**
 * Optional helper to toggle RTS (for DE/RE control on some RS485 transceivers).
 * @param fd Serial file descriptor
 * @param level Toggle level (1 to set, 0 to clear)
 * @return 0 on success, -1 on failure
 */
int set_rts(int fd, int level);

/**
 * Initializes the MODBUS context and connects to the serial port.
 * @return Valid modbus_t pointer, or NULL on failure.
 */
modbus_t* modbus_service_init(const char *device, int baud, char parity, int data_bits, int stop_bits, int slave_id);

/**
 * Initializes the modbus mapping and sets initial register value.
 * @return Valid mapping pointer, or NULL on failure.
 */
modbus_mapping_t* modbus_service_mapping_init(int holding_reg_count, uint16_t initial_40002_value);

/**
 * Cleans up modbus structures.
 */
void modbus_service_cleanup(modbus_t *ctx, modbus_mapping_t *mb_mapping);

#endif /* MODBUS_SERVICE_H */
