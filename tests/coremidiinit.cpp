// Exercise the real CoreMIDI constructors without a MIDI service or hardware.
// The SDK declarations precede the substitutions, which affect only this
// executable's copy of RtMidi.cpp, never the production library.
#include <CoreMIDI/CoreMIDI.h>
#include <CoreFoundation/CoreFoundation.h>
#include <cstdio>
#include <cstdlib>
#include <memory>
#include <set>
#include <string>

namespace {
bool failClientCreation = true;
unsigned int clientAttempts = 0;
unsigned int portDisposals = 0;
std::set<CFTypeRef> names;

void check( bool condition, const char *message )
{
  if ( !condition ) {
    std::fprintf( stderr, "FAIL: %s\n", message );
    std::exit( 1 );
  }
}

CFStringRef makeName( CFAllocatorRef allocator, const char *text, CFStringEncoding encoding )
{
  CFStringRef name = CFStringCreateWithCString( allocator, text, encoding );
  check( name != NULL, "allocate client name" );
  names.insert( name );
  return name;
}

void releaseName( CFTypeRef value )
{
  names.erase( value );
  CFRelease( value );
}

OSStatus createClient( CFStringRef name, MIDINotifyProc, void *, MIDIClientRef *client )
{
  check( names.count( name ) == 1, "client name remains alive during creation" );
  ++clientAttempts;
  if ( failClientCreation ) return -10844;
  *client = 123;
  return noErr;
}

OSStatus disposePort( MIDIPortRef )
{
  ++portDisposals;
  return noErr;
}
}

#undef __UNIX_JACK__
#undef __LINUX_ALSA__
#define __MACOSX_CORE__
#define MIDIClientCreate createClient
#define CFStringCreateWithCString makeName
#define CFRelease releaseName
#define MIDIPortDispose disposePort
#include "../RtMidi.cpp"
#undef MIDIPortDispose
#undef CFRelease
#undef CFStringCreateWithCString
#undef MIDIClientCreate

template<class Midi>
void exercise()
{
  for ( unsigned int attempt = 0; attempt < 3; ++attempt ) {
    bool caught = false;
    try {
      // Exercise both default API selection and explicit CoreMIDI selection.
      Midi midi( attempt == 0 ? RtMidi::UNSPECIFIED : RtMidi::MACOSX_CORE );
    }
    catch ( const RtMidiError &error ) {
      caught = true;
      check( error.getType() == RtMidiError::DRIVER_ERROR, "preserve error type" );
      check( error.getMessage().find( "-10844" ) != std::string::npos, "preserve OS status" );
    }
    check( caught, "failed construction reaches caller's catch" );
    check( names.empty(), "release client name on failure" );
    check( CoreMidiClientSingleton == 0, "do not publish a failed client" );
    check( clientAttempts == attempt + 1, "retry client creation after failure" );
  }

  failClientCreation = false;
  {
    Midi midi( RtMidi::MACOSX_CORE );
    check( midi.getCurrentApi() == RtMidi::MACOSX_CORE, "construct after failure" );
    check( names.empty(), "release client name on success" );
    check( CoreMidiClientSingleton == 123, "publish successful client" );
    Midi second( RtMidi::MACOSX_CORE );
    check( clientAttempts == 4, "reuse successful shared client" );
  }
  check( portDisposals == 0, "do not dispose an unopened port" );
  // This fixture owns a synthetic client, not a CoreMIDI service object.
  CoreMidiClientSingleton = 0;
}

int main( int argc, char **argv )
{
  check( argc == 2, "specify input or output" );
  if ( std::string( argv[1] ) == "input" ) exercise<RtMidiIn>();
  else if ( std::string( argv[1] ) == "output" ) exercise<RtMidiOut>();
  else check( false, "unknown test mode" );
  std::printf( "CoreMIDI %s initialization: passed\n", argv[1] );
}
