/**
 * Control my "robot butler"
 * Accepts commands over http and passes them on to devices.
 * Features experiments in temperature and lightwaverf
 * 
 * @author PC <paulypopex+arduino@gmail.com>
 * @date    Mon Sun  6 22:50:43 GMT 2013 
 */ 

#include <SPI.h>
#include <Ethernet.h>
#include <EthernetUdp.h>
// put your textlocal.com credentials in TextLocal.h - copy TextLocal.h.sample
#include "TextLocal.h"

#define BLUE_PIN 7
#define GREEN_PIN 6
#define YELLOW_PIN 5
#define RED_PIN 4
#define BUTTON 6
#define THERMOMETER_PIN A0
#define DOOR_PIN A1
#define TOO_LONG 60
#define TIME_BETWEEN_NAGS 60

// how many pins have things on them?
#define ANALOG_PINS 2

// Up to 1023 + a null
#define PIN_VAL_MAX_LEN 5
#define HTTP_PORT 666

unsigned int doorOpen = 0;
unsigned int localPort = 9761; // local port to listen on
unsigned int lwrfPort = 9760; // lightwaverf port to send on
IPAddress lwrfServer( 192, 168, 0, 14 ); // lightwaverf wifi link ip address. Or just 255,255,255,255 to broadcast
EthernetUDP Udp; // An EthernetUDP instance to let us send and receive packets over UDP

#define STRING_BUFFER_SIZE 256
typedef char BUFFER[STRING_BUFFER_SIZE];

byte mac[] = { 0x64, 0xA7, 0x69, 0x0D, 0x21, 0x21 }; // mac address of this arduino
byte ip[] = { 192, 168, 1, 101 }; // requested ip address of this arduino
byte router[] = { 192, 168, 1, 254 }; // my router
EthernetServer server( HTTP_PORT ); // Initialize the json server on this port
// EthernetServer webserver( 80 ); // Initialize the web server on this port
EthernetClient client; // to make outbound connections

unsigned long time;
unsigned long openFor;
unsigned long lastAlerted;

#ifndef TEXTLOCAL_H
char textLocalUser[] = "user";
char textLocalPassword[] = "pass";
#endif

void setup( ) {
  pinMode( RED_PIN, OUTPUT );
  pinMode( YELLOW_PIN, OUTPUT );
  pinMode( GREEN_PIN, OUTPUT );
  pinMode( BLUE_PIN, OUTPUT );
  Ethernet.begin( mac ); // dhcp
  // Ethernet.begin( mac, ip, router, router ); // can't get this working
  Serial.begin( 9600 );
  delay( 1000 ); // give it a second to initialise
  Udp.begin( localPort );
  server.begin( );
  Serial.print( "server is at " );
  Serial.print( Ethernet.localIP( ));
  Serial.print( ":" );
  Serial.print( HTTP_PORT );
  // webserver.begin( );
}

void loop( ) {
  thermoLight( );
  jsonServer( );
  // webServer( );
  door( DOOR_PIN );
  if ( client.available( )) {
    char c = client.read( );
    Serial.print( c );
  }
  delay( 1 );
}

void test ( int pin ) {
  Serial.print( "Testing pin " ); 
  Serial.println( pin ); 
  for ( int i = 0; i < 3; i ++ ) {
    digitalWrite( pin, HIGH );
    delay( 100 );
    digitalWrite( pin, LOW );
    delay( 100 );
  }
}

void jsonServer ( ) {
  EthernetClient client = server.available();  // listen for incoming clients
  if ( client ) {
    Serial.println( "new client" );
    if ( client.connected( ) && client.available( )) {
      char *method = "";
      char *path = "";
      char *response;
      char *data = "";
      if ( getRequest( client, method, path, data )) {
        response = callRoute( method, path, data );
        Serial.println( response );
      }
      client.print( httpHeader( "200 OK", "" ));
      client.print( jsonResponse( response ));
    }
    delay( 1 ); // give the web browser time to receive the data
    client.stop(); // close the connection:
  }
}

/* void webServer ( ) {
  EthernetClient client = webserver.available();  // listen for incoming clients
  if ( client ) {
    if ( client.connected( ) && client.available( )) {
      BUFFER html, header;
      sprintf( html, "<html><title>Arduino!</title><body><h1>Arduino!</h1><p>Server is on port %d remember...</p></body></html>", HTTP_PORT, HTTP_PORT );
      sprintf( header, "HTTP/1.1 200 OK\nws: arduino\nContent-Type: text/html\nContent-length: %d\nConnection: close\n\n", strlen( html ));
      client.print( header );
      client.print( html );
    }
    delay( 1 ); // give the web browser time to receive the data
    client.stop( ); // close the connection:
  }
} */

char *callRoute ( char * method, char * path, char * data ) {
  Serial.print( "Path was " );
  Serial.println( path );
  char * room = strtok( path, "/" );
  char * device = strtok( NULL, "/" ); // strotok is mental
  char * f = strtok( NULL, "/" );
  char cmd[32]; // enough room for whole instruction
  cmd[0] = 0; // null string
  strcat( cmd, "666,!" );
  strcat( cmd, "R" );
  strcat( cmd, room );
  strcat( cmd, "D" );
  strcat( cmd, device );
  strcat( cmd, "F" );
  strcat( cmd, f );
  strcat( cmd, "|testing|arduino!" );
  Udp.beginPacket( lwrfServer, lwrfPort );
  Udp.write( cmd );
  Udp.endPacket( );
  return cmd;
}

char *httpHeader ( char *status, char *contentType ) {
  BUFFER s = "HTTP/1.1 ";
  strcat( s, status );
  strcat( s, "\nws:arduino\nContent-Type: application/json\n\n" );
  return s;
}

char *jsonResponse ( char * response ) {
  BUFFER s = "{\"a\":{";
  for ( int analogPin = 0; analogPin < ANALOG_PINS; analogPin ++ ) {
    if ( analogPin > 0 ) strcat( s, "," );
    strcat( s, jsonPair( analogPin, analogRead( analogPin )));
  }
  // strcat( s, "},\"d\":{" );
  // strcat( s, jsonPair( BUTTON, digitalRead( BUTTON )));
  strcat( s, "}" );
  
  strcat( s, ",\"r\":" );
  strcat( s, "\"" );
  strcat( s, response );
  strcat( s, "\"" );
  int v = analogRead( THERMOMETER_PIN );
  char vs[4] = "";
  itoa( vtoc( v ), vs, 10 );
  strcat( s, ",\"temperature\":" );
  strcat( s, "\"" );
  strcat( s, vs );
  strcat( s, "\"" );

  strcat( s, ",\"door\":" );
  strcat( s, "\"" );
  strcat( s, analogRead( DOOR_PIN ) ? "open" : "closed" );
  strcat( s, "\"" );
   
  strcat( s, "}" );
  return s;
}

char *jsonPair ( int k, int v ) {
  BUFFER s = "";
  char vs[PIN_VAL_MAX_LEN];
  sprintf( vs, "\"%d\"", k );
  strcat( s, vs );
  strcat( s, ":" );
  sprintf( vs, "%d", v );
  strcat( s, vs );
  return s;
}

boolean getRequest ( EthernetClient client, char * & method, char * & path, char * & data ) {
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

int vtoc ( int v ) {
  return ( 5 * v * 100.0 ) / 1024.0;
}

int thermoLight( ) {
  int v = vtoc( analogRead( THERMOMETER_PIN ));
  digitalWrite( BLUE_PIN, ( v <= 16 ));
  digitalWrite( GREEN_PIN, ( v >= 16 && v <= 20 ));
  digitalWrite( YELLOW_PIN, ( v >= 20 && v <= 23 ));
  digitalWrite( RED_PIN, ( v >= 23 ));
  // delay( 100 );
  return v;
}

/**
 * Is the door open or what?
 * Door has one of these on it http://www.toolstation.com/shop/Magnetic+Contact/p33648
 * doorOpen is a global
 * TIME_BETWEEN_NAGS is a number of seconds
 * TOO_LONG is a number of seconds
 */
int door( int pin ) {
  unsigned int v = analogRead( pin ) ? 1 : 0;
  // Serial.println( v );
  if ( v != doorOpen ) {
    Serial.print( v );
    Serial.print( " != " );
    Serial.println( doorOpen );
    doorOpen = v ? 1 : 0;
    Serial.print( "now doorOpen = " );
    Serial.println( doorOpen );
    delay( 1000 );
    // alert( doorOpen );
  }
  if ( doorOpen ) {
    openFor = ( millis( ) - time ) / 1000;
    if ( openFor > TOO_LONG ) {
      if (( millis( ) - lastAlerted ) / 1000 > TIME_BETWEEN_NAGS ) {
        lastAlerted = millis( );
        Serial.print( "door open for " );
        Serial.print( openFor );
        Serial.println( " secs" );
      }
    }
  }
  else {
    time = millis( );
  }
  return v;
}
/**
 * To send a text or a tweet or something.
 * Needs work, needs a password stored out of the repo, in a header maybe.
 */
int alert( int status ) {
  // Serial.print( "door is " );
  // Serial.println( status );
  // Serial.print( "username is " );
  // Serial.println( textLocalUser );
  if ( client.connect( "www.txtlocal.com", 80 )) {
    Serial.println( "connected to server ok" );
    client.print( "GET /getcredits.php?uname=" );
    client.print( textLocalUser );
    client.print( "&pword=" );
    client.print( textLocalPassword );
    client.println( " HTTP/1.1" );
    client.print( "Host: " );
    client.println( "www.txtlocal.com" );
    client.println( );
    client.println( "User-Agent: Arduino/1.0" );
    client.println( "Connection: close" );
  }
  else {
    // Serial.println( "not connected" );
  }
}
