#ifndef FAKE_CONTROL_VOLTAGE_H
#define FAKE_CONTROL_VOLTAGE_H

#include <stdlib.h>
#include <stdio.h>
#include <errno.h>
#include <string.h>
#include <jack/jack.h>
#include <jack/midiport.h>
#include <jack/ringbuffer.h>
#include <pthread.h>
#include <stdbool.h>

#define JackPortIsControlVoltage 0x100

enum Ports {
    PORT_CAPTURE1,
    PORT_CAPTURE2,
    PORT_PLAYBACK1,
    PORT_PLAYBACK2,
    PORT_ARRAY_SIZE // this is not used as a port index
};

static const size_t queue_size = 64*sizeof(jack_port_id_t);

typedef struct FAKE_CONTROL_VOLTAGE_T {
  jack_client_t *client;
  jack_port_t *ports[PORT_ARRAY_SIZE];

  float sample_rate;
  float frequency;
  float x[2];
  float y[2];
  bool index;
} fake_control_voltage_t;

/**
 * For use as a Jack-internal client, `jack_initialize()` and
 * `jack_finish()` have to be exported in the shared library.
 */
__attribute__ ((visibility("default")))
int jack_initialize(jack_client_t* client, const char* load_init);

__attribute__ ((visibility("default")))
void jack_finish(void* arg);

#endif
