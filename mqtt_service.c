#include "mqtt_service.h"
#include <stdio.h>
#include <string.h>

struct mosquitto* mqtt_service_init(const char *host, int port, const char *client_id) {
    mosquitto_lib_init();
    struct mosquitto *mosq = mosquitto_new(client_id, true, NULL);
    if (!mosq) {
        fprintf(stderr, "Failed to initialize mosquitto client.\n");
        return NULL;
    }

    if (mosquitto_connect(mosq, host, port, 60)) {
        fprintf(stderr, "Can not be connected to MQTT Broker\n");
        mosquitto_destroy(mosq);
        return NULL;
    }
    
    // Maintain MQTT connectivity and process background packets.
    mosquitto_loop_start(mosq);

    return mosq;
}

void mqtt_service_publish(struct mosquitto *mosq, const char *topic, uint16_t value) {
    if (!mosq) return;
    char msg[16];
    snprintf(msg, sizeof(msg), "%u", value);
    mosquitto_publish(mosq, NULL, topic, strlen(msg), msg, 0, false);
}

void mqtt_service_cleanup(struct mosquitto *mosq) {
    if (mosq) {
        mosquitto_destroy(mosq);
    }
    mosquitto_lib_cleanup();
}
