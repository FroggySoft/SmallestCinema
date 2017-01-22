/*
 *  This sketch simulates a movie using a RGB led
 *  
 *    
 *    NodeMCU 1.0 (ESP-12E module)
 */
 
//#define FASTLED_ESP8266_RAW_PIN_ORDER
#define FASTLED_ESP8266_D1_PIN_ORDER

#include <ESP8266WiFi.h>
#include <FastLED.h>
#include <WiFiClient.h> 
#include <ESP8266WebServer.h>
#include <DNSServer.h>

#define ESP8266_LED   2

const char* ssid1 = <your ssid here>
const char* password = <your code here>
IPAddress ip(192,168,1,2);  //Node static IP
IPAddress gateway(192,168,1,1);
IPAddress subnet(255,255,255,0);

const char *ssid2 = "SmallestCinema";
const byte DNS_PORT = 53;
const byte WEB_PORT = 80;

DNSServer        mDnsServer;
ESP8266WebServer mWebServer(WEB_PORT);

// Create an instance of the server
// specify the port to listen on as an argument
WiFiServer server(80);

#define NUM_LEDS 8    // How many leds in your strip?
#define DATA_PIN D5   // is actually pin D2 on the board
#define MODE_PIN D1

#define SPEED_SLOW    2000
#define SPEED_MEDIUM  1000
#define SPEED_FAST    500

// Define the array of leds
CRGB leds[NUM_LEDS];

unsigned int mCurrentMovie = 0;
unsigned int mPlayingScene = 0;
bool mVideoOn = true;
String mLedColor = "Off";
unsigned int mTimeNextFrame = 0;
unsigned int mFrameDelay = SPEED_MEDIUM;

unsigned int mNoClientFoundCounter = 100; //wait 10 seconds before creating own wifi AP
bool mIsConnectedMode = false;

// include the 'movies':
#include "SmallestCinema.h"

void setup()
{
  Serial.begin(9600);
  delay(10);

  // prepare GPIO's
  pinMode(ESP8266_LED, OUTPUT);
  digitalWrite(ESP8266_LED, HIGH); // = led off

  pinMode(MODE_PIN, INPUT_PULLUP);
  delay(10);
  if( digitalRead(MODE_PIN) )     // low -> make AP
  {
    CreateNetwork();
    mIsConnectedMode = false;
  }
  else      // join the existing network
  {
    ConnectToNetwork();
    mIsConnectedMode = true;
  }
    
  FastLED.addLeds<WS2811, DATA_PIN, GRB>(leds, NUM_LEDS);

  mCurrentMovie = random(NrOfMovies);
}

void loop()
{
  
  if(mVideoOn)
  {
    if( mTimeNextFrame > 100)
    {
      mTimeNextFrame -= 100;
    }
    else
    {
      setLed( mMovies[mCurrentMovie][mPlayingScene]);
    
      mPlayingScene++;
      if(mPlayingScene>mMovieLength[mCurrentMovie])
      {
        mPlayingScene = 0;
      }
      mTimeNextFrame = mFrameDelay;
    }
  }

  if( mIsConnectedMode)
  {
    // Check if a client has connected
    WiFiClient client = server.available();
    if (client)
    {
      // Wait until the client sends some data
      int counter=10;
      while(!client.available() && counter>0)
      {
        delay(10);
        counter--;  // don't wait too long
      }
      delay(10*counter);   // to get a stable timing
      
      // Read the first line of the request
      String req = client.readStringUntil('\r');
      client.flush();

      if( req.length() != 0)
      {
        String lReply = HandleRequest(req);
        client.print(lReply);
      }
    }
    else
    {
      delay(100);
    }
  }
  else    // StandAlone
  {
    mDnsServer.processNextRequest();
    mWebServer.handleClient();
    delay(100);
  }
}

String HandleRequest(String aReq)
{
  aReq.toLowerCase();
  aReq.replace(String("%20")," ");

  // Match the request
  String lReply;
  
  if (aReq.indexOf("help") != -1)
  {
    lReply = getHelpPage();
  }
  else if (aReq.indexOf("/on") != -1)
  {
    doOn(aReq);
    lReply = getJsonStatus();
  }
  else if (aReq.indexOf("/off") != -1)
  {
    doOff();
    lReply = getJsonStatus();
  }
  else if (aReq.indexOf("/red") != -1)
  {
    doRed();
    lReply = getJsonStatus();
  }
  else if (aReq.indexOf("/green") != -1)
  {
    doGreen();
    lReply = getJsonStatus();
  }
  else if (aReq.indexOf("/blue") != -1)
  {
    doBlue();
    lReply = getJsonStatus();
  }
  else if (aReq.indexOf("/get") != -1)
  {
    lReply = getJsonStatus();
  }
  else if (aReq.indexOf("/speed") != -1)
  {
    unsigned int pos = aReq.indexOf("?delay=");
    String speed = aReq.substring(pos+7);
    unsigned int lSpeed = speed.toInt();
    doSpeed(lSpeed);
    lReply = getJsonStatus();
  }
  else    // general index page
  {
    lReply = "HTTP/1.1 200 OK\r\nContent-Type: text/html\r\n\r\n<!DOCTYPE HTML>\r\n";
    lReply += getIndexPage();
  }

  return lReply;
}


void ConnectToNetwork()
{  
  // Connect to WiFi network

  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid1, password);
  WiFi.config(ip, gateway, subnet);
  
  while (WiFi.status() != WL_CONNECTED)
  {
    delay(500);
  }
 
  // Start the server
  server.begin();
  Serial.print("Connected to network ");
  Serial.println(ssid1);
}

void CreateNetwork()
{
  WiFi.mode(WIFI_AP);
  WiFi.softAP(ssid2);

  IPAddress mIPAddress = WiFi.softAPIP();

  mDnsServer.start(DNS_PORT, "*", mIPAddress);

  // replay to all requests with same HTML
  mWebServer.onNotFound([]()
  {
    handleRoot();
  });
 
  mWebServer.on("/", handleRoot);
  mWebServer.on("/index", handleRoot);
  mWebServer.on("/on", handleOn);
  mWebServer.on("/off", handleOff);
  mWebServer.on("/red", handleRed);
  mWebServer.on("/green", handleGreen);
  mWebServer.on("/blue", handleBlue);
  mWebServer.on("/speed", handleSpeed);
  mWebServer.begin();

  Serial.print("Created network ");
  Serial.println(ssid2);
}

void handleRoot()
{
  mWebServer.send(200, "text/html", getIndexPage());
}

void handleOn()
{
  String lMovie = mWebServer.arg(0);
  Serial.print("Select movie ");
  Serial.println(lMovie);
  doOn(lMovie);  
  mWebServer.send(200, "text/html", getIndexPage());
}

void handleOff()
{
  doOff();
  mWebServer.send(200, "text/html", getIndexPage());
}

void handleRed()
{
  doRed();
  mWebServer.send(200, "text/html", getIndexPage());
}

void handleGreen()
{
  doGreen();
  mWebServer.send(200, "text/html", getIndexPage());
}

void handleBlue()
{
  doBlue();
  mWebServer.send(200, "text/html", getIndexPage());
}

void handleSpeed()
{
  String lSpeedStr = mWebServer.arg(0);
  unsigned int lSpeed = lSpeedStr.toInt();
  Serial.print("Select speed ");
  Serial.println(lSpeed);
  doSpeed(lSpeed);
  mWebServer.send(200, "text/html", getIndexPage());
}

String getIndexPage()
{
  String lReply;
  //lReply = "HTTP/1.1 200 OK\r\nContent-Type: text/html\r\n\r\n<!DOCTYPE HTML>\r\n";
  lReply = "<html>Select one of the available movies:";
  lReply += "<table>";
  lReply += "<tr>";
  for( int i=0; i<NrOfMovies; i++)
  {
    lReply += "<td align=\"center\"><button type=\"button\" onclick=\"location.href='/on?movie=" + mMovieNames[i] + "'\">" + mMovieNames[i] + "</button></td>";
    if( (i%3)==2)
    {
      lReply += "</tr><tr>";
    }
  }
  lReply += "</tr></table><br><br><br>";
  lReply += "Adjust the movie playing speed:";
  lReply += "<table>";
  lReply += "<tr>";
  lReply += "<td><button type=\"button\" onclick=\"location.href='/speed?delay=";
  lReply += SPEED_FAST;
  lReply += "'\">fast</button></td>";
  lReply += "<td><button type=\"button\" onclick=\"location.href='/speed?delay=";
  lReply += SPEED_MEDIUM;
  lReply += "'\">normal</button></td>";
  lReply += "<td><button type=\"button\" onclick=\"location.href='/speed?delay=";
  lReply += SPEED_SLOW;
  lReply += "'\">slow</button></td></tr>";
  lReply += "<tr><td></td><td><button type=\"button\" onclick=\"location.href='/off'\">Stop</button></td></tr>";
  lReply += "</table><br><br>";
  lReply += "Test the leds:";
  lReply += "<table>";
  lReply += "<tr>";
  lReply += "<td><button type=\"button\" onclick=\"location.href='/red'\">red</button></td>";
  lReply += "<td><button type=\"button\" onclick=\"location.href='/green'\">green</button></td>";
  lReply += "<td><button type=\"button\" onclick=\"location.href='/blue'\">blue</button></td></tr>";
  lReply += "</table><br><br>";
  if( mVideoOn )
  {
    lReply += "Now playing: ";
    lReply += mMovieNames[mCurrentMovie];
    lReply += " (";
    lReply += mFrameDelay;
    lReply += " ms/frame)";
  } 
  else
  {
    lReply += mLedColor;
  }
  lReply += "<br><br><br>";
  lReply += "<button type=\"button\" onclick=\"location.href='/help'\">help</button>";
  lReply += "</html>";
  
  return lReply;
}

String getHelpPage(void)
{
  String lReply = "HTTP/1.1 200 OK\r\nContent-Type: text/html\r\n\r\n<!DOCTYPE HTML>\r\n<html>";
  lReply += "Available commands:<br>";
  lReply += "<table>";
  lReply += "<tr><td>/on</td><td>will play a random file</td></tr>";
  lReply += "<tr><td>/on?movie=&lt;name&gt;</td><td>will play the indicated movie*</td></tr>";
  lReply += "<tr><td>/off</td><td>switch leds off</td></tr>";
  lReply += "<tr><td>/get</td><td>return current status</td></tr>";
  lReply += "<tr><td>/red</td><td>Set the red leds on</td></tr>";
  lReply += "<tr><td>/green</td><td>Set the green leds on</td></tr>";
  lReply += "<tr><td>/blue</td><td>Set the blue leds on</td></tr>";
  lReply += "<tr><td>/speed?delay=n</td><td>will determine the delay between each frame (in ms)</td></tr>";
  lReply += "</table><br><br>";
  lReply += "<br><br>";
  lReply += "*available movies:<br>";
  lReply += "<ul>";
  for( int i=0; i<NrOfMovies; i++)
  {
    lReply += "<li>";
    lReply += mMovieNames[i];
    lReply += "</li>";
  }
  lReply += "</ul><br><br>";
  lReply += "Reply is a JSON string with state of the leds<br><br>";
  lReply += "</html>";      
  return lReply;
}

String getJsonStatus(void)
{
  String lReply = "HTTP/1.1 200 OK\r\nContent-type: application/json\r\n\r\n{\n";
  lReply += "\"status\" : \"OK\",\n";
  lReply += "\"leds\" : \"";
  if( mVideoOn )
  {
    lReply += "now playing ";
    lReply += mMovieNames[mCurrentMovie];
    lReply += "\"\n\"speed\" : \"";
    lReply += mFrameDelay;
  }
  else
  {
    lReply += mLedColor;
  }
  lReply += "\"\n}\n";

  return lReply;
}

void doOn(String aReq)
{
  mVideoOn = true;
  mPlayingScene = 0;

  aReq.toLowerCase();
  mCurrentMovie = random(NrOfMovies);  // just pick on
  // maybe overwrite with a user choise
  for( int i=0; i<NrOfMovies; i++)
  {
    String lName = mMovieNames[i];
    lName.toLowerCase();
    if( aReq.indexOf(lName)!=-1 )
    {
      mCurrentMovie = i;
    }
  }
  Serial.print("Starting movie ");
  Serial.println(mMovieNames[mCurrentMovie]);
}

void doOff(void)
{
  mVideoOn = false;
  mLedColor = "Off";
  setLed( CRGB::Black );
}

void doRed(void)
{
  mVideoOn = false;
  mLedColor = "Red";
  setLed( CRGB::Red );
}

void doGreen(void)
{
  mVideoOn = false;
  mLedColor = "Green";
  setLed( CRGB::Green );
}

void doBlue(void)
{
  mVideoOn = false;
  mLedColor = "Blue";
  setLed( CRGB::Blue );
}

void doSpeed(unsigned int aSpeed)
{
  mFrameDelay = aSpeed;
  Serial.print("New delay ");
  Serial.println(mFrameDelay);
}

void setLed(CRGB aColor)
{
  for(int i=0; i<NUM_LEDS; i++)
  {
    leds[i] = aColor;
  }
  FastLED.show();
}

