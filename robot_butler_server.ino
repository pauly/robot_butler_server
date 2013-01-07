/**
 * Control my "robot butler"
 * Accepts commands over http and passes them on to devices.
 * LOTS of unused code in here, a work in progress!
 * Features experiments in temperature and lightwaverf
 * 
 * @author PC <paulypopex+arduino@gmail.com>
 * @date    Mon Sun  6 22:50:43 GMT 2013 
 */ 

#include <SPI.h>
#include <Ethernet.h>
#include <EthernetUdp.h>

#define BLUE 9
#define GREEN 10
#define WHITE 11
#define YELLOW 12
#define RED 3
#define BUTTON 7
#define THERMOMETER A0

// Up to 1023 + a null
#define PIN_VAL_MAX_LEN 5

unsigned int status = 0;
unsigned int v = 0;
unsigned int previous = 0;
unsigned int localPort = 9761; // local port to listen on
unsigned int lwrfPort = 9760; // local port to listen on
IPAddress lwrfServer( 192, 168, 0, 14 ); // lightwaverf wifi link ip address
EthernetUDP Udp; // An EthernetUDP instance to let us send and receive packets over UDP

#define STRING_BUFFER_SIZE 256
typedef char BUFFER[STRING_BUFFER_SIZE];

byte mac[] = { 0x64, 0xA7, 0x69, 0x0D, 0x21, 0x21 }; // mac address of this arduino
IPAddress ip( 192, 168, 0, 101 ); // requested ip address of this arduino

EthernetServer server( 666 ); // Initialize the Ethernet server library

void setup( ) {
  Serial.begin( 9600 );
  pinMode( RED, OUTPUT );
  // test( RED );
  pinMode( YELLOW, OUTPUT );
  pinMode( WHITE, OUTPUT );
  pinMode( GREEN, OUTPUT );
  pinMode( BLUE, OUTPUT );
  pinMode( BUTTON, INPUT );
  Ethernet.begin( mac, ip );
  Udp.begin( localPort );
  server.begin();
  Serial.print( "server is at " );
  Serial.println( Ethernet.localIP( ));
}

void loop( ) {
  // thermo_light(  );
  my_server( );
}

void test ( int pin ) {
  Serial.print( "Testing pin" ); 
  Serial.println( pin ); 
  for ( int i = 0; i < 3; i ++ ) {
    digitalWrite( pin, HIGH );
    delay( 100 );
    digitalWrite( pin, LOW );
  }
}

void my_server ( ) {
  EthernetClient client = server.available();  // listen for incoming clients
  if ( client ) {
    Serial.println( "new client" );
    if ( client.connected( ) && client.available( )) {
      char *method = "";
      char *path = "";
      char *response = "ok";
      char *data = "";
      if ( get_request( client, method, path, data )) {
        response = call_route( method, path, data );
      }
      client.print( json_header( "200 OK" ));
      client.print( json_response( response ));
    }
    delay( 1 ); // give the web browser time to receive the data
    client.stop(); // close the connection:
  }
}

char *call_route ( char *method, char *path, char *data ) {
  Serial.print( "Path was " );
  Serial.println( path );
  char *room = strtok( path, "/" );
  Serial.print( "room " );
  Serial.println( room );
  char *device;
  device = strtok( path, "/" );
  char *status = strtok( path, "/" );
  Serial.print( "status " );
  Serial.println( status );
  // char *s = "666,!R2D1F1|dining on|arduino!";
  char *s = "666,!R";
  strcat( s, room );
  Serial.print( "device " );
  Serial.println( device );
  strcat( s, "D" );
  strcat( s, device );
  // strcat( s, "F" );
  // strcat( s, status );
  // strcat( s, "|dining on|arduino!" );
  // strcat( s, "{\"resource\":\"" );
  // strcat( s, "\"}" );
  // sprintf( response, "{\"method\": \"%s\", \"path\": \"%s\", \"pin_a\": \"%s\"}", method, path, pin );
  // Serial.print( "<response>" );
  // Serial.print( response );
  // Serial.print( "</response>" );
  
  Udp.beginPacket( lwrfServer, lwrfPort );
  Udp.write( s );
  Udp.endPacket( );

  return s;
}

char *json_header ( char *status ) {
  BUFFER s = "HTTP/1.1 ";
  strcat( s, status );
  strcat( s, "\nContent-Type: application/json\nConnnection: close\n\n" );
  return s;
}

char *json_response ( char *response ) {
  BUFFER s = "{\"a\":{";
  for ( int analog_pin = 0; analog_pin < 6; analog_pin ++ ) {
    if ( analog_pin > 0 ) strcat( s, "," );
    strcat( s, json_pair( analog_pin, analogRead( analog_pin )));
  }
  strcat( s, "},\"d\":{" );
  strcat( s, json_pair( BUTTON, digitalRead( BUTTON )));
  strcat( s, "}" );
  
  strcat( s, ",\"r\":" );
  strcat( s, "\"" );
  strcat( s, response );
  strcat( s, "\"" );

  // strcat( s, ",\"about\":\"x\"" );
  
  strcat( s, "}" );
  return s;
}

char *json_pair ( int k, int v ) {
  BUFFER s = "";
  char vs[PIN_VAL_MAX_LEN];
  sprintf( vs, "\"%d\"", k );
  strcat( s, vs );
  strcat( s, ":" );
  sprintf( vs, "%d", v );
  strcat( s, vs );
  return s;
}

boolean get_request ( EthernetClient client, char * & method, char * & path, char * & data ) {
  char s[STRING_BUFFER_SIZE];
  s[0] = client.read();
  s[1] = client.read();
  int i = 2;
  while ( s[i-2] != '\r' && s[i-1] != '\n' && i < STRING_BUFFER_SIZE ) {
    s[i ++] = client.read();
  }
  method = strtok( s, " \n\r" );
  path = strtok( NULL, " \n\r?" );
  data = strtok( NULL, " ?" );
  return true;
}

void thermo_light( ) {
  int v = analogRead( THERMOMETER );
  Serial.print( v );
  Serial.print( ':' );
  v = ( 5 * v * 100.0 ) / 1024.0;
  Serial.println( v );
  digitalWrite( BLUE, ( v <= 5 ));
  digitalWrite( GREEN, ( v >= 5 && v <= 14 ));
  digitalWrite( WHITE, ( v >= 13 && v <= 18 ));
  digitalWrite( YELLOW, ( v >= 17 && v <= 21 ));
  digitalWrite( RED, ( v >= 21 ));
  delay( 1000 );
}
