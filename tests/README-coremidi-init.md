# CoreMIDI initialization failure regression

`coremidiinit.cpp` compiles an isolated copy of the real CoreMIDI backend with
client creation and client-name ownership instrumented. It never creates an OS
MIDI client or opens a port. Production builds do not contain the substitutions.

The input and output cases exercise default and explicit API selection, repeated
creation failure, propagation of `DRIVER_ERROR` and its OS status, name cleanup,
successful construction after failure, shared-client reuse, and destruction of
an unopened object. They run separately so each begins without a shared client.

On macOS:

```sh
cmake -S . -B build -DRTMIDI_API_JACK=OFF -DRTMIDI_BUILD_TESTING=ON
cmake --build build
ctest --test-dir build --output-on-failure
```

The same tests run under Autotools `make check` when CoreMIDI is enabled. For an
isolated sanitizer run, without either build system:

```sh
c++ -std=c++11 -Wall -Wextra -fsanitize=address,undefined \
  -fno-omit-frame-pointer -g tests/coremidiinit.cpp \
  -framework CoreMIDI -framework CoreFoundation -framework CoreAudio \
  -o /tmp/coremidiinit
/tmp/coremidiinit input
/tmp/coremidiinit output
```

Both cases abort with an uncaught `RtMidiError` against the unmodified
`23b8cd5fa6aae239c3468ebb3b76d06310c9a3ca` source: the client helper's `throw()`
specification prevents propagation to the test's catch. With the fix, both pass,
including under ASan/UBSan. This establishes the exception/cleanup behavior; it
does not reproduce the device conditions responsible for an actual CoreMIDI
service failure.
