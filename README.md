# mod-fake-control-voltage

A small Jack-internal client to simulate the physical CV-ports. It has
the same Jack port option value like mod-host expects. Note that this is a
hack, since Jack does not support Control Voltage ports.

`fake-control-voltage.so` is a Jack-internal client,
`fake-control-voltage-test` is a normal standalone client. The ports:

* `fake-control-voltage:capture` sends a 440 Hz sine wave

* `fake-control-voltage:playback` is just a sink.


## Build
```bash
$ mkdir build && cd build
$ cmake ..
$ cmake --build .
```

to install the shared library in `/usr/lib/jack/mod-fake-contol-voltage.so` run

```bash
$ sudo make install
```

## Run

```bash
$ mod-fake-control-voltage &
$ jack_lsp -p
...
mod-fake-control-voltage:cv_playback_1
	properties: input,physical,terminal,
mod-fake-control-voltage:cv_capture_1
	properties: output,physical,terminal,
```
