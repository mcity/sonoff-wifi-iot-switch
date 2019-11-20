# Mcity Sonoff Wifi IoT Switch
Firmware for the ESP8266 featured in the Sonoff Basic wifi switch.
This firmware removes the need to connect to ITEAD/Sonoff cloud.
A simple HTTP server with Basic auth is provided, along with OTA firmware updates.

## Thanks
We borrowed heavily from the following tutorials for development of our desired feature set:
* https://medium.com/@jeffreyroshan/flashing-a-custom-firmware-to-sonoff-wifi-switch-with-arduino-ide-402e5a2f77b
* https://www.instructables.com/id/SONOFF-ESP8266-Update-Firmware-With-Aduino-IDE/
* http://esp8266.fancon.cz/esp8266-sonoff-relay-ap-arduino/esp8266-sonoff-basic-arduino-ap.html

## Why Sonoff?
We were in search of a cheap, easy to deploy solution to control power to 120v devices. The Sonoff devices runs about $6 dollars each and support the wi-fi network we have deployed at our test facility.

We required the ability to directly control the device without a cloud or 3rd party provider in between. The ability to re-flash the firmware on the Sonoff devices made implementing support for them in our Track API extremely easy. The licensable version of Mcity OCTANE has built in configuration support for this firmware and these devices.

Additional IO pins are available on the Sonoff board, but we do not implement support for them in this firmware as we have not yet needed to use them in our production environment.

## Setup Arduino IDE
In the Arduino IDE go to File>Preferences

Enter the following URL in the Additional Boards Manager URLs field.
http://arduino.esp8266.com/stable/package_esp8266com_index.json

Open the Boards Manager, go to Tools>Board>BoardsManager

Search for ESP8266 and press install on the ESP8266 community package when found.

## Connecting the Board for programming

### Modifying the Sonoff

Open the Sonoff and solder in 4 male header pins to the center of the board next to the button.

These pins are labeled on the back of the board. From the button over they are VCC 3v3, RX, TX, GND

### Connecting the Sonoff ot the PC
We used this USB->TTL Adapter 
https://www.amazon.com/Adapter-Serial-Converter-Development-Projects/dp/B075N82CDL/ref=cm_cr_srp_d_product_top?ie=UTF8

Ensure the 120v or any other power is not connected. Do not plug the adapter into the PC at this time.

Our USB->TTL board supports both 5v and 3v3 TTL levels. Set it to 3v3 to match the Sonoff

Make the following connections:

  |  TTL Device (3v3) | Sonoff          |
  |-------------------|-----------------|
  | VCC               | VCC             |
  | TX                | RX              |
  | RX                | TX              |
  | GND               | GND             |

Hold down the button on the Sonoff and plug the USB->TTL into the computer. After 2 seconds, release the button on the Sonoff. This procedure puts the device into flash mode.

## Configuration
Open the mcity-sonoff.ino sketch in the Arduino IDE and modify the script #define lines after the #include to set:
* Wireless credentials 
* Basic authentication credentials
* Over the air (OTA) firmware credentials 

Default credentials for OTA and HTTP Basic authentication are admin/admin.

Compile the sketch.

## Flashing

### USB Flash
With the Sonoff in flash mode, choose compile and upload sketch from the Arduino IDE. The Sonoff will restart once the firmware has been flashed. 

Open the Serial Monitor from the Arduino IDE to see the status as the device starts. Be sure the serial monitor bit rate is set to 115200. The startup will show the devices MAC address, in case of need for MAC filtering on WI-FI. Once authenticated IP, Gateway, NetMask will be displayed.

### Over The Air (OTA) Flash
If the Sonoff is connected to 120v power, authenticated to the network after the initial flash, it can be updated via OTA firmware updates. 

In Arduino IDE, under the sketch menu, choose "Export Compiled Binary". Locate the binary on your file system.

With the initial firmware flash complete, the device will have an http endpoint on /firmware. Navigate to http://ip:port/firmware and using the form displayed, pick the newly exported .BIN file for upload. Press upload.

The device will load the new firmware and then restart when complete.

If the firmware exceeds 50% of the capacity of the ESP8266 chip, then the firmware OTA update will not succeed. The device requires enough room to load both current and new firmware at the same time for the OTA update to succeed.

## LED Status Light / Button Support

  |  Button       |LED              | Action            |
  |---------------|-----------------|-------------------|
  |short press    |relay state      |change relay state |
  |long press > 3s|change           |restart ESP8266    |
  |      --       |flash - slow     |connecting to wifi |
  |      --       |flash - fast 10x |server ready       |
  |      --       |flash - short    |client got page    | 
  |before power on|    --           |flash firmware mode|

## Connecting to your device for control
Unplug the Sonoff from our USB->TTL Programmer. With all wires not connected to power, attach required wires to the Sonoff inside the case. Seal the Sonoff case. 

Apply power to the Sonoff from a wall outlet. It should authenticate to wireless and after a flash sequence, be ready to use.

## Usage
### API Endpoints
* / - GET - HUMAN ReadableInformation about device
* /device - GET - Text state of relay
* /device - POST - Use WWWUSERNAME/WWWPASSWORD HTTP Auth and arg state to change relay.
* /firmware - POST - Use HTTP Auth with OTAUSER OTAPASSWORD to write bin file with new firmware.

### Web
Navigate to http://ip:port/ in a web browser to access a basic web interface. You'll be prompted for credentials when trying to make changes, but status of the relay can be seen without authentication.

### cURL
Fetch device statsu: curl http://ip/device

Change state: curl -u admin:admin -d 'state=OFF' http://ip/device
Change state (option 2): curl -u admin:admin -X POST http://ip/device?state=OFF

### Development
This application is maintained by Tyler Worman (tworman@umich.edu) if you'd like to contribute, please contact us or create a pull request for features on this repository.




