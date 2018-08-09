#include <math.h>
#include "fake-control-voltage.h"

/**
 * Add a port_id to the waiting queue.
 * It is a producer in the realtime context.
 */
size_t push_back(jack_ringbuffer_t *queue, jack_port_id_t port_id) {
  size_t written = 0;
  
  if (queue == NULL) {
    fprintf(stderr, "Could not schedule port connection.\n");
  } else {

    // Check if there is space to write
    size_t write_space = jack_ringbuffer_write_space(queue);
    if (write_space >= sizeof(jack_port_id_t)) {
      written = jack_ringbuffer_write(queue,
				      &port_id,
				      sizeof(jack_port_id_t));
      if (written < sizeof(jack_port_id_t)) {
	fprintf(stderr, "Could not schedule port connection.\n");
      }
    }    
  }
  return written;
}


/**
 * Return the next port_id or NULL if empty.
 */
jack_port_id_t next(jack_ringbuffer_t *queue) {
  size_t read = 0;
  jack_port_id_t id = NULL;
  
  if (queue == NULL) {
    fprintf(stderr, "Queue memory problem.\n");
  } else {
    char buffer[sizeof(jack_port_id_t)];
    read = jack_ringbuffer_read(queue, buffer, sizeof(jack_port_id_t));
    if (read == sizeof(jack_port_id_t)) {
      id = (jack_port_id_t) *buffer;      
    }
  }
  return id;
}


/**
 * Connect any outstanding Jack ports.
 * It is a consumer in the non-realtime context.
 */
void handle_scheduled_connections(fake_control_voltage_t *const mm) {
  // Check if there are connections scheduled.
  jack_port_id_t source = next(mm->ports_to_connect);

  if (source != NULL) {
    int result;
    result = jack_connect(mm->client,
			  jack_port_name(jack_port_by_id(mm->client, source)),
			  jack_port_name(mm->ports[PORT_CAPTURE]));
    switch(result) {
    case 0:
      // Fine.
      break;
    case EEXIST:
      fprintf(stderr, "Connection exists.\n");
      break;
    default:
      fprintf(stderr, "Could not connect port.\n");
      break;
    }
  }
}


static int process_callback(jack_nframes_t nframes, void *arg)
{
  fake_control_voltage_t *const fcv = (fake_control_voltage_t *const) arg;

  // Get the output buffer once per cycle.
  float *sine_buffer = (float *) jack_port_get_buffer(fcv->ports[PORT_PLAYBACK], nframes);

  // See Smith & Cook "The Second-Order Digital Waveguide Oscillator" 1992,
  // https://ccrma.stanford.edu/~jos/wgo/wgo.pdf, p. 2 
  const float pit = M_PI / fcv->sample_rate;
  for (jack_nframes_t pos = 0; pos < nframes; ++pos) {
    bool i = fcv->index;
    
    // "Magic Circle" algorithm
    const float e = 2*sin(pit * fcv->frequency);
    fcv->x[i] = fcv->x[!i] + e*fcv->y[!i];
    fcv->y[i] = -e*fcv->x[i] + fcv->y[!i];
    
    sine_buffer[pos] = fcv->x[i];
    // cosine: y[i]

    // Flip indexes every frame
    fcv->index = !fcv->index;
  }
  
  // Copy events from the input to the output.
  void *input_port_buffer = jack_port_get_buffer(fcv->ports[PORT_CAPTURE], nframes);

  return 0;
}


int jack_initialize(jack_client_t* client, const char* load_init)
{
  fake_control_voltage_t *const fcv = malloc(sizeof(fake_control_voltage_t));
  if (!fcv) {
    fprintf(stderr, "Out of memory\n");
    return EXIT_FAILURE;
  }

  fcv->client = client;
 
  // Register ports.
  fcv->ports[PORT_CAPTURE] = jack_port_register(client, "capture",
					  JACK_DEFAULT_AUDIO_TYPE,
					  JackPortIsPhysical | JackPortIsInput, 0);
  fcv->ports[PORT_PLAYBACK] = jack_port_register(client, "playback",
					   JACK_DEFAULT_AUDIO_TYPE,
					   JackPortIsPhysical | JackPortIsOutput, 0);
  for (int i = 0; i < PORT_ARRAY_SIZE; ++i) {
    if (!fcv->ports[i]) {
      fprintf(stderr, "Can't register jack port\n");
      free(fcv);
      return EXIT_FAILURE;
    }
  }

  fcv->sample_rate = jack_get_sample_rate(fcv->client);

  // Initial values for the oscillator
  fcv->frequency = 440.0;
  fcv->x[0] = 0.0;
  fcv->x[1] = 0.0;
  fcv->y[0] = 0.0;
  fcv->y[0] = 1.0;
  
  // Set port aliases
  jack_port_set_alias(fcv->ports[PORT_CAPTURE], "CV playback");
  jack_port_set_alias(fcv->ports[PORT_PLAYBACK], "CV capture");

  // Create the ringbuffer (single-producer/single-consumer) for
  // scheduled port connections. It contains elements of type
  // `jack_port_id_t`.
  fcv->ports_to_connect = jack_ringbuffer_create(queue_size);
  
  // Set callbacks
  jack_set_process_callback(client, process_callback, fcv);
 
  /* Activate the jack client */
  if (jack_activate(client) != 0) {
    fprintf(stderr, "can't activate jack client\n");
    free(fcv);
    return EXIT_FAILURE;
  }

  // TODO: Schedule already existing ports for connection.
  
  return 0;
}


void jack_finish(void* arg)
{
  fake_control_voltage_t *const fcv = (fake_control_voltage_t *const) arg;

  jack_deactivate(fcv->client);
  
  for (int i = 0; i < PORT_ARRAY_SIZE; ++i) {
    jack_port_unregister(fcv->client, fcv->ports[i]);
  }

  jack_ringbuffer_free(fcv->ports_to_connect);
  
  free(fcv);
}
