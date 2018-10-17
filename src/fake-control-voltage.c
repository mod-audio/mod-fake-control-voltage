#include <math.h>
#include "fake-control-voltage.h"


static int process_callback(jack_nframes_t nframes, void *arg)
{
  fake_control_voltage_t *const fcv = (fake_control_voltage_t *const) arg;

  // Get the output buffer once per cycle.
  float *sine_buffer = (float *) jack_port_get_buffer(fcv->ports[PORT_PLAYBACK1], nframes);  

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
    fcv->index = !(fcv->index);
  }
  
  // Copy events from the input to the output.
  void *input_port_buffer1 = jack_port_get_buffer(fcv->ports[PORT_CAPTURE1], nframes);
  void *input_port_buffer2 = jack_port_get_buffer(fcv->ports[PORT_CAPTURE2], nframes);

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
 
  // Register ports. Since this simulates hardware the output is called "capture"!
  fcv->ports[PORT_CAPTURE1] = jack_port_register(client, "cv_playback_1",
					  JACK_DEFAULT_AUDIO_TYPE,
					  JackPortIsPhysical | JackPortIsInput | JackPortIsTerminal | JackPortIsControlVoltage, 0);
  fcv->ports[PORT_CAPTURE2] = jack_port_register(client, "cv_playback_2",
					  JACK_DEFAULT_AUDIO_TYPE,
					  JackPortIsPhysical | JackPortIsInput | JackPortIsTerminal | JackPortIsControlVoltage, 0);
  fcv->ports[PORT_PLAYBACK1] = jack_port_register(client, "cv_capture_1",
					   JACK_DEFAULT_AUDIO_TYPE,
					   JackPortIsPhysical | JackPortIsOutput | JackPortIsTerminal | JackPortIsControlVoltage, 0);
  fcv->ports[PORT_PLAYBACK2] = jack_port_register(client, "cv_capture_2",
					   JACK_DEFAULT_AUDIO_TYPE,
					   JackPortIsPhysical | JackPortIsOutput | JackPortIsTerminal | JackPortIsControlVoltage, 0);
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
  fcv->y[1] = 1.0;
  
  // Set port aliases
  jack_port_set_alias(fcv->ports[PORT_CAPTURE1], "CV playback 1");
  jack_port_set_alias(fcv->ports[PORT_CAPTURE2], "CV playback 2");  
  jack_port_set_alias(fcv->ports[PORT_PLAYBACK1], "CV capture 1");
  jack_port_set_alias(fcv->ports[PORT_PLAYBACK2], "CV capture 2");
  
  // Set callbacks
  jack_set_process_callback(client, process_callback, fcv);
 
  /* Activate the jack client */
  if (jack_activate(client) != 0) {
    fprintf(stderr, "can't activate jack client\n");
    free(fcv);
    return EXIT_FAILURE;
  }
  
  return 0;
}


void jack_finish(void* arg)
{
  fake_control_voltage_t *const fcv = (fake_control_voltage_t *const) arg;

  jack_deactivate(fcv->client);
  
  for (int i = 0; i < PORT_ARRAY_SIZE; ++i) {
    jack_port_unregister(fcv->client, fcv->ports[i]);
  }
  
  free(fcv);
}
