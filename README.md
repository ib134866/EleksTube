# EleksTube - Wifi NTP clock update


The EleksMaker EleksTube bamboo LED clock was originally a kickstart project that was produced to highlight EleksMaker's laser cutting technology, but is now being sold on Banggood.

I have noticed that this clock has a poor time keeping ability and will gain minutes every day.  EleksMaker has updated the clock controller and has greatly improved the internal clock time keeping.  However, you still need to use the PC software to set the clock time, control the colour of the clock display and adjust the time for DST offsets periodically.  Fortunately EleksMaker has published an API to interface with the serial port available on the clock controller.  The API details are located on EleksMakers forum here:  http://forum.eleksmaker.com/topic/1941/elekstube-api-control-protocol

As a result, I developed this Arduino program to provide NTP or GPS updates to the EleksTube using an inexpensive ESP8266 development board.

The program has the following features:

1. Utilizes WiFiManager to setup a Access Point the first time the program in run.
2. Displays the Access Point IP address on the clock display and waits for the user to connect to the AP and add a SSID to the configuration.
3. Allows user to add timezonedb token, OTA password and Blynk token to wifi configuraton and subsequently saved in SPIF filesystem to be used in the program.
4. Automatically determines your time zone based on your internet IP address.
5. Uses the time zone information to determine your offset from UTC.
6. Uses the time zone information to determine DST details - DST start, DST end and current DST status - either on or off.  Uitilzes this information to automatically switch DST offset from UTC when DST changes occur.
7. Utilizes OTA (Over The Air) update capabilities of the ESP 8266 module to allow sketch updates after the module is installed in the belly of the clock.
8. Uses Blynk https://www.blynk.cc mobile application software to control the clock.  The Blynk application allows for selection of clock source (NTP,GPS or internal RTC), update internal RTC with NTP/GPS data, display current date information once a minute for five seconds at a configurable start second.  Change from 24hr to 12 hr time display, change the calculated timezone to a user selectable timezone.  Configure a time range when the clock display should be off - ie. when sleeping if clock is in a bedroom. Each digit can have an individual colour set and stored and restored during a reboot.  The date display can have a different colour than the time display.

To get started:

Install a local Blynk server:

I would recommend that a local Blynk App be configured to support the EleksTube.  The following link describes the installation procedure:

https://github.com/blynkkk/blynk-server#getting-started

The Blynk server can be installed easily on a Raspberry PI, Windows PC, Docker or Linux PC. Once the Blynk server is installed, you will need to login and create a new user with sufficient credit to support the EleksTube Blynk App - approximately $3200 in credit.  Details on logging and and setting up a user are detailed in the installation instructions

Install the Blynk Android/IOS App:

Install the Android or IOS app on your smart phone and scan the following image in the app to install the EleksTube Blynk App.

To start using it:
1. Download Blynk App: http://j.mp/blynk_Android or http://j.mp/blynk_iOS
2. Login to the Blynk App with the username and password created during the installation of your local Blynk Server.  Specify the local Blynk Server and port.
3. Touch the QR-code icon and point the camera to the code below

<img src="https://github.com/ib134866/EleksTube/blob/master/EleksTube_Wifi/EleksTube_Blynk_App.png" width="200" height="200">

Copy the blynk auth token from the Blynk app located in the "Nut" menu of the EleksTube app - under Devices, My Devices.  The Auth Token is pushed to the Blynk server, but will be required for the EleksTube connection to the Blynk Server.

You will need Auth Token when configuring the Wifi setting of the EleksTube.  Press the "Email" key to email the auth token to yourself.

Install compile and install the program:<br>

Download and install the Arduino IDE - https://www.arduino.cc/en/Main/Software<br>
The software was created using IDE version 1.8.6.<br>
Download the Arduino sketch for the EleksTube - EleksTube_Wifi.ino<br>
Load the EleksTube_Wifi.ino in the Arduino IDE.<br>
Download and install the ESP8266 board manager into the Arduino IDE - https://github.com/esp8266/Arduino<br>
Download and install the libraries detailed in the Arduino sketch into the Arduino IDE.<br>
Connect your ESP8266 board to your PC and select the boards serial port in the Arduino IDE.<br>
Select your ESP8266 Board in the "Tools" menu of the Arduino IDE.<br>

Verify the sketch in the Arduino IDE to esure you are not missing any libraries.<br>
Upload the compiled sketch to your ES8266 board.<br>

Configure the Wifi prior to connecting to the EleksTube clock if desired.  The ESP8266 board will become a Wifi Access Point (AP) the first time connected with no valid wifi configuration.  The default IP address of the AP is 192.168.4.1.  Use a Wifi connected PC or smartphone to connect to the open AP.

If you are connecting the ESP8266 12-F board to the EleksTube, be aware that there is an issue the power on reset if the power is connected to the Vin pin on the board.  I experienced CPU lock up if power was connected to the Vin on the board.  However if you connect the power to the USB port, there is no issue with power on reset.  As a result you will have to make a cable with a male micro usb connector to power the ESP8266 12-F baord from the EleksTube clock.  See the pictures below.

ESP8266 12-F pinouts:<br>
3v3 pin to the red wire on GPS module<br>
GND pin to the black wire on GPS module<br>
RX pin to the white wire on GPS module.  You must remove this wire if you are uploading new code via the EPS8266 USB interface as this port is shared with the USB port.<br>
D4 ping to the RX pin on the EleksTube clock serial interface<br>
Micro USB port - plus(+) and minus (-) from the EleskTube clock serial interface - Micro USB wiring - https://never-stop-building-blog-production.s3.amazonaws.com/pictures/wiring-micro-usb-pinout/micro-usb-power-connector-wiring.png<br>

ESP8266 WEMOS D1 mini V3.0.0 pinouts:
<br>
+5V pin to EleksTube + pin on serial interface<br>
GND pin (solder an extra pin to this connection) to negative (-) pin on EleksTube clock serial interface and black wire on the GPS module<br>
D4 pin to the RX pin on the EleksTube clock serial interface<br>
RX pin to the white wire on the GPS module. You must remove this wire if you are uploading new code via the EPS8266 USB interface as this port is shared with the USB port.<br>
3V3 pin to the red wire on the GPS module.<br>
<br>
The first time the program is installed on a new ESP8266 board, the SPIF files system is initialized.  The SPIF filesystem is used to store configuration parameters during the WiFi setup.

Once you have completed the Wifi setup and entered your timezonedb, blynk token, blynk host (local or remote) and blynk port (8080 for local), the clock will begin displaying the corrected time.



Screenshots of the application and configuration.
<br>
<img src="https://github.com/ib134866/EleksTube/blob/master/EleksTube_Wifi/EleksTube_Wifi_Screenshot.jpg" width="400" height="600">
<img src="https://github.com/ib134866/EleksTube/blob/master/EleksTube_Wifi/EleksTube_Blynk_App_Screenshot.jpg" width="400" height="600">

Images of connectivity to the EleksTube controller, ESP8266 or Wemos D1 Arduino boards and BN-220 GPS.

<br>
Wemos D1 mini Connectivity
<img src="https://github.com/ib134866/EleksTube/blob/master/EleksTube_Wifi/Wemos_D1_mini_V3.jpg" width="600" height="400">
ESP8266 12-F Connectivity
<img src="https://github.com/ib134866/EleksTube/blob/master/EleksTube_Wifi/ESP8266_12-F.jpg" width="600" height="400">
EleksTube connectivity to Wemos D1 mini
<img src="https://github.com/ib134866/EleksTube/blob/master/EleksTube_Wifi/EleksTube_Controller.jpg" width="600" height="400">
EleksTube connectivity to ESP8266 12-F
<img src="https://github.com/ib134866/EleksTube/blob/master/EleksTube_Wifi/EleksTube_Controller_ESP8266.jpg" width="600" height="400">
BN-220 GPS connectivity to Wemos D1 mini
<img src="https://github.com/ib134866/EleksTube/blob/master/EleksTube_Wifi/BN-220_GPS.jpg" width="600" height="400">
BN-220 GPS connectivity to ESP8266 12-F
<img src="https://github.com/ib134866/EleksTube/blob/master/EleksTube_Wifi/BN-220_GPS-ESP8266.jpg" width="600" height="400">
