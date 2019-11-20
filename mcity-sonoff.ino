/*
  Mcity Wifi Switch

  HTTP Endpoints:
  ---
  / - GET - HUMAN ReadableInformation about device
  /device - GET - Text state of relay
  /device - POST - Use WWWUSERNAME/WWWPASSWORD HTTP Auth and arg state to change relay.
  /firmware - POST - Use HTTP Auth with OTAUSER OTAPASSWORD to write bin file with new firmware.

  Button / LED Information:
  ---
  |---------------|-----------------|-------------------|
  |  Button       |LED              | Action            |
  |---------------|-----------------|-------------------|
  |short press    |relay state      |change relay state |
  |long press > 3s|change           |restart ESP8266    |
  |      --       |flash - slow     |connecting to wifi |
  |      --       |flash - fast 10x |server ready       |
  |      --       |flash - short    |client got page    | 
  |before power on|    --           |flash formware mode|
  |---------------|-----------------|-------------------|

  Firmware Update:
  ---
  - In Arduino Interface: Sketch->Export Compiled Library
  - Navigate to http://ip:port/firmware and enter the OTAUSER and OTAPASSWORD
  - Choose exported .bin file for firmware and press upload
  - Device will load and reboot.
  - Firmware cannot take up more than 50% of space on device or OTA upload will fail.
  
*/

#include <Ticker.h>
#include <ESP8266WiFi.h>
#include <WiFiClient.h>
#include <ESP8266WebServer.h>
#include <ESP8266mDNS.h>
#include <ESP8266HTTPUpdateServer.h>

#define APSSID          "MSetup"   //AP SSID
#define APPASSWORD      ""         //AP password
#define SERVERPORT      80         //Web server port
#define WWWUSERNAME     "admin"    // Set www user name
#define WWWPASSWORD     "admin"    // Set www user password
#define OTAUSER         "admin"    // Set OTA user
#define OTAPASSWORD     "admin"    // Set OTA password
#define OTAPATH         "/firmware"// Set path for update
#define RELAYPIN        12         // GPIO12 relay pin
#define LEDPIN          13         // GPIO13 GREEN LED (active low)
#define BUTTONPIN       0          // GPIO0 button pin
#define BUTTONTIME      0.25       // [s] time between button read
#define BUTTONON        "color: green; border: 3px #fff outset;"
#define BUTTONOFF       "color: red; border: 3px #fff outset;"
#define BUTTONNOACT     "color: black; border: 7px #fff outset;"
#define BUTTONDEBOUNCE  1 //Minimum number of seconds between a valid button press or relay switch.

bool    LEDState        = true;    // Green LED off
bool    RelayState      = false;   // Relay off
bool    ButtonFlag      = false;   // Does the button need to be handled on this loop
char    ButtonCount     = 0;       // How many cycles/checks the button has been pressed for.
String  OnButt;
String  OffButt;

//Setup classes needed from libraries.
MDNSResponder mdns;
Ticker buttonTick;
ESP8266WebServer server(SERVERPORT);
ESP8266HTTPUpdateServer httpUpdater;

void setup(void){  
  //  Init
  pinMode(BUTTONPIN, INPUT);
  pinMode(LEDPIN, OUTPUT);
  pinMode(RELAYPIN, OUTPUT);
  
  Serial.begin(115200); 
  delay(5000);

  //Start wifi connection
  Serial.println("Connecting to wifi..");
  WiFi.begin(APSSID, APPASSWORD);
  
  //Print MAC to serial so we can use the address for auth if needed.
  printMAC();

  // Wait for connection - Slow flash
  Serial.print("Waiting on Connection ...");
  while (WiFi.status() != WL_CONNECTED) {
    digitalWrite(LEDPIN, LOW);
    delay(500);
    Serial.print(".");
    //Serial.println(WiFi.status());
    digitalWrite(LEDPIN, HIGH);
    delay(500);
  }

  //Print startup status and network information
  Serial.println("");
  Serial.print("Connected to: ");
  Serial.println(APSSID);
  Serial.print("IP: ");
  Serial.println(WiFi.localIP());
  Serial.print("Gateway: ");
  Serial.println(WiFi.gatewayIP());
  Serial.print("Subnet: ");  
  Serial.println(WiFi.subnetMask());
  Serial.print("Device ID: ");
  Serial.println(ESP.getChipId());
  if (mdns.begin("esp8266", WiFi.localIP())) {
    Serial.println("MDNS: Responder Started");
  }

  //Setup HTTP Server Endpoints
  server.on("/", HTTP_GET, handleGET);
  server.on("/device", HTTP_POST, handleStatePOST);
  server.on("/device", HTTP_GET, handleStateGET);
  server.onNotFound(handleNotFound);
  httpUpdater.setup(&server, OTAPATH, OTAUSER, OTAPASSWORD); //OTA Update endpoint
  
  //Start the web server
  server.begin();
  
  //Start up blink of LED signaling everything is ready. Fast Flash
  for (int i = 0; i < 10; i++) {
    setLED(!LEDState);
    delay(100);
  }
  Serial.println("Server is up.");

  //Enable periodic watcher for button event handling
  buttonTick.attach(BUTTONTIME, buttonFlagSet);
}

/*
 * printMAC
 * Print the device MAC address to the serial port.
 */
void printMAC(void) {
  byte mac[6];
  WiFi.macAddress(mac);
  Serial.print("MAC: ");
  Serial.print(mac[0],HEX);
  Serial.print(":");
  Serial.print(mac[1],HEX);
  Serial.print(":");
  Serial.print(mac[2],HEX);
  Serial.print(":");
  Serial.print(mac[3],HEX);
  Serial.print(":");
  Serial.print(mac[4],HEX);
  Serial.print(":");
  Serial.println(mac[5],HEX);
}

/* 
 *  handleNotFound
 *  Return a 404 error on not found page.
 */
void handleNotFound() {
  server.send(404, "text/plain", "404: Not found");
}

/* 
 *  handleMainPage - GET
 *  Return Text for main page on GET
 */
void handleGET() {
  //Quick LED Flash
  setLED(!LEDState);

  //Serve Page
  Serial.println("Serviced Page Request");
  String  buff;
  buff  = "<!DOCTYPE html PUBLIC \"-//W3C//DTD HTML 4.01 Transitional//EN\">\n";
  buff += "<html><head>\n";
  buff += "<style type=\"text/css\">\n";
  buff += "html {font-family: sans-serif;background:#f0f5f5}\n";
  buff += ".submit {width: 10%; height:5vw; font-size: 100%; font-weight: bold; border-radius: 4vw;}\n";
  buff += "@media (max-width: 1281px) {\n";
  buff += "html {font-size: 3vw; font-family: sans-serif;background:#ffe4c4}\n";
  buff += ".submit {width: 40%; height:20vw; font-size: 100%; font-weight: bold; border-radius: 15vw;}}\n";
  buff += "</style>\n";
  buff += "<meta content=\"text/html; charset=utf-8\">\n";
  buff += "<title>Mcity - Wifi Power Switch</title></head><body>\n";
  buff += "<pre>\n";
  buff += "MMMMMMMM               MMMMMMMM                      iiii          tttt                               \n";
  buff += "M:::::::M             M:::::::M                     i::::i      ttt:::t                               \n";
  buff += "M::::::::M           M::::::::M                      iiii       t:::::t                               \n";
  buff += "M:::::::::M         M:::::::::M                                 t:::::t                               \n";
  buff += "M::::::::::M       M::::::::::M    cccccccccccccccciiiiiiittttttt:::::tttttttyyyyyyy           yyyyyyy\n";
  buff += "M:::::::::::M     M:::::::::::M  cc:::::::::::::::ci:::::it:::::::::::::::::t y:::::y         y:::::y \n";
  buff += "M:::::::M::::M   M::::M:::::::M c:::::::::::::::::c i::::it:::::::::::::::::t  y:::::y       y:::::y  \n";
  buff += "M::::::M M::::M M::::M M::::::Mc:::::::cccccc:::::c i::::itttttt:::::::tttttt   y:::::y     y:::::y   \n";
  buff += "M::::::M  M::::M::::M  M::::::Mc::::::c     ccccccc i::::i      t:::::t          y:::::y   y:::::y    \n";
  buff += "M::::::M   M:::::::M   M::::::Mc:::::c              i::::i      t:::::t           y:::::y y:::::y     \n";
  buff += "M::::::M    M:::::M    M::::::Mc:::::c              i::::i      t:::::t            y:::::y:::::y      \n";
  buff += "M::::::M     MMMMM     M::::::Mc::::::c     ccccccc i::::i      t:::::t    tttttt   y:::::::::y       \n";
  buff += "M::::::M               M::::::Mc:::::::cccccc:::::ci::::::i     t::::::tttt:::::t    y:::::::y        \n";
  buff += "M::::::M               M::::::M c:::::::::::::::::ci::::::i     tt::::::::::::::t     y:::::y         \n";
  buff += "M::::::M               M::::::M  cc:::::::::::::::ci::::::i       tt:::::::::::tt    y:::::y          \n";
  buff += "MMMMMMMM               MMMMMMMM    cccccccccccccccciiiiiiii         ttttttttttt     y:::::y           \n";
  buff += "                                                                                   y:::::y            \n";
  buff += "                                                                                  y:::::y             \n";
  buff += "           Greetings from Mcity Engineering!                                     y:::::y              \n";
  buff += "                                                                                y:::::y               \n";
  buff += "                                                                               yyyyyyy                \n"; 
  buff += "</pre>\n";
  buff += "Wifi-enabled IIoT Power Switch\n";
  buff += "<form action=\"/device\" method=\"POST\">\n";
  buff += "<h2>Device ID: " + String(ESP.getChipId()) + "</h2>\n";
  buff += "<h2>Relay State: ";
  if (RelayState) {
    buff += "ON</h2>\n";
  } else {
    buff += "OFF</h2>\n";
  }
  buff += "<input type=\"submit\" name=\"state\" class=\"submit\" value=\"ON\" style=\"" + OnButt + "\">\n";
  buff += "<input type=\"submit\" name=\"state\" class=\"submit\" value=\"OFF\" style=\"" + OffButt + "\">\n";
  buff += "</form></body></html>\n";
  server.send(200, "text/html", buff);

  //Quick LED Flash
  delay(20);
  setLED(!LEDState);
}

/* 
 *  handleStatePOST
 *  Modify state on POST
 */
void handleStatePOST() {
  /* request for www user/password from client */
  if (!server.authenticate(WWWUSERNAME, WWWPASSWORD))
    return server.requestAuthentication();
  if (server.arg("state") == "ON") setRelay(true);
  if (server.arg("state") == "OFF") setRelay(false);
  handleStateGET();
}

/* 
 *  handleStateGET
 *  Print state on GET
 */
void handleStateGET() {    
  //Serve Page
  Serial.println("Serviced API Request");

  //Print Relay state
  String  buff;
  if (RelayState) {
    buff = "ON\n";
  } else {
    buff = "OFF\n";
  }

  server.send(200, "text/html", buff);
  
  //Quick LED Flash
  delay(20);
  setLED(!LEDState);
}

/* 
 *  setRelay
 *  Sets the state of the Relay
*/
void setRelay(bool SetRelayState) {
  //Switch the HTML for the display page
  if (SetRelayState == true) {
    OnButt  = BUTTONON;
    OffButt = BUTTONNOACT;
  }
  if (SetRelayState == false) {
    OnButt = BUTTONNOACT;
    OffButt  = BUTTONOFF;
  }

  //Set the relay state
  RelayState = SetRelayState;
  digitalWrite(RELAYPIN, RelayState);

  //Set the LED to opposite of the button.
  setLED(!SetRelayState);
}

/*
 * setLED
 * Sets the state of the LED
 */
void setLED(bool SetLEDState) {
  LEDState = SetLEDState;     // set green LED
  digitalWrite(LEDPIN, LEDState);
}

/*
 * ButtonFlagSet
 * Sets a variable so that on next loop, the button state is handled.
 */
void buttonFlagSet(void) {
  ButtonFlag = true;
}

/* Read and handle button Press*/
void getButton(void) {
  // short press butoon to change state of relay
  if (digitalRead(BUTTONPIN) == false ) ++ButtonCount;
  if (digitalRead(BUTTONPIN) == true && ButtonCount > 1 && ButtonCount < 12 ) {
    setRelay(!RelayState); // change relay
  }
  /* long press button restart */
  if (ButtonCount > 12) {
    setLED(!LEDState);
    buttonTick.detach();    // Stop Tickers
    /* Wait for release button */
    while (!digitalRead(BUTTONPIN)) yield();
    delay(100);
    ESP.restart();
  }
  if (digitalRead(BUTTONPIN) == true) ButtonCount = 0;
  ButtonFlag = false;
}
 
/*
 * loop
 * System Loop
 */
void loop(void){
  server.handleClient();           // Listen for HTTP request
  if (ButtonFlag) getButton();     // Handle the button press 
} 
