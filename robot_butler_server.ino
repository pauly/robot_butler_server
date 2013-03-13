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

#define BLUE_PIN 7
#define GREEN_PIN 6
#define YELLOW_PIN 5
#define RED_PIN 4
#define BUTTON 6
#define THERMOMETER_PIN A0
#define DOOR_PIN A1

// how many pins have things on them?
#define ANALOG_PINS 2

// Up to 1023 + a null
#define PIN_VAL_MAX_LEN 5
#define HTTP_PORT 666

unsigned int status = 0;
unsigned int v = 0;
unsigned int previous = 0;
unsigned int localPort = 9761; // local port to listen on
unsigned int lwrfPort = 9760; // lightwaverf port to send on
IPAddress lwrfServer( 192, 168, 0, 14 ); // lightwaverf wifi link ip address. Or just 255,255,255,255 to broadcast
EthernetUDP Udp; // An EthernetUDP instance to let us send and receive packets over UDP

#define STRING_BUFFER_SIZE 256
typedef char BUFFER[STRING_BUFFER_SIZE];

byte mac[] = { 0x64, 0xA7, 0x69, 0x0D, 0x21, 0x21 }; // mac address of this arduino
IPAddress ip( 192, 168, 1, 101 ); // requested ip address of this arduino

EthernetServer server( HTTP_PORT ); // Initialize the json server on this port
EthernetServer webserver( 80 ); // Initialize the web server on this port

unsigned long time;
unsigned long open_for;
unsigned long last_alerted;

void setup( ) {
  Serial.begin( 9600 );
  pinMode( RED_PIN, OUTPUT );
  pinMode( YELLOW_PIN, OUTPUT );
  pinMode( GREEN_PIN, OUTPUT );
  pinMode( BLUE_PIN, OUTPUT );
  test( RED_PIN );
  test( GREEN_PIN );
  test( YELLOW_PIN );
  test( BLUE_PIN );
  Ethernet.begin( mac, ip );
  Udp.begin( localPort );
  server.begin( );
  Serial.print( "server is at " );
  Serial.print( Ethernet.localIP( ));
  Serial.print( ":" );
  Serial.print( HTTP_PORT );
  webserver.begin( );
  Serial.print( "web server is also at " );
  Serial.println( Ethernet.localIP( ));
}

void loop( ) {
  thermo_light( );
  my_server( );
  my_web_server( );
  door( );
}

void test ( int pin ) {
  Serial.print( "Testing pin " ); 
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
      char *response;
      char *data = "";
      if ( get_request( client, method, path, data )) {
        response = call_route( method, path, data );
        Serial.println( response );
      }
      client.print( http_header( "200 OK", "" ));
      client.print( json_response( response ));
    }
    delay( 1 ); // give the web browser time to receive the data
    client.stop(); // close the connection:
  }
}

void my_web_server ( ) {
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
}

char *call_route ( char * method, char * path, char * data ) {
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

char *http_header ( char *status, char *content_type ) {
  BUFFER s = "HTTP/1.1 ";
  strcat( s, status );
  strcat( s, "\nws:arduino\nContent-Type: application/json\n\n" );
  return s;
}

char *json_response ( char * response ) {
  BUFFER s = "{\"a\":{";
  for ( int analog_pin = 0; analog_pin < ANALOG_PINS; analog_pin ++ ) {
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

int vtoc ( int v ) {
  return ( 5 * v * 100.0 ) / 1024.0;
}

int thermo_light( ) {
  int v = vtoc( analogRead( THERMOMETER_PIN ));
  digitalWrite( BLUE_PIN, ( v <= 16 ));
  digitalWrite( GREEN_PIN, ( v >= 16 && v <= 20 ));
  digitalWrite( YELLOW_PIN, ( v >= 20 && v <= 22 ));
  digitalWrite( RED_PIN, ( v >= 22 ));
  delay( 100 );
  return v;
}

/**
 * Is the door open or what?
 * Door has one of these on it http://www.toolstation.com/shop/Magnetic+Contact/p33648
 */
int door( ) {
  int v = analogRead( DOOR_PIN );
  if ( v == 0 ) {
    time = millis( );
  }
  else {
    unsigned long k = 1000;
    unsigned long one_minute = k * 60;
     open_for = ( millis( ) - time ) / k;
     if ( open_for > 60 ) {
       unsigned long since_alert = ( millis( ) - last_alerted ) / k;
       if ( since_alert > 60 ) {
         last_alerted = millis( );
         Serial.print( "door! " );
         Serial.print( "time is " );
         Serial.print( time );
         Serial.print( ", millis( ) is " );
         Serial.print( millis( ));
         Serial.print( ", open for " );
         Serial.print(( millis( ) - time ) / one_minute );
         Serial.println( " secs" );
       }
    }
  }
  return v;
}
