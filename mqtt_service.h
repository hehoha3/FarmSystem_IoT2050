#ifndef MQTT_SERVICE_H
#define MQTT_SERVICE_H

#include <mosquitto.h>
#include <stdint.h>
#include <stdbool.h>

/**
 * Initializes the MQTT client and connects to the broker.
 * @param host MQTT broker host
 * @param port MQTT broker port
 * @param client_id MQTT client ID
 * @return Pointer to the mosquitto instance, or NULL on failure.
 */
struct mosquitto* mqtt_service_init(const char *host, int port, const char *client_id);

/**
 * Publishes a value to a given MQTT topic.
 * @param mosq Mosquitto instance
 * @param topic MQTT topic
 * @param value Value to publish
 */
void mqtt_service_publish(struct mosquitto *mosq, const char *topic, uint16_t value);

/**
 * Cleans up and destroys the MQTT client.
 * @param mosq Mosquitto instance
 */
void mqtt_service_cleanup(struct mosquitto *mosq);

#endif /* MQTT_SERVICE_H */