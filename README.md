# EleksTube - Wifi NTP clock update


The EleksMaker EleksTube bamboo LED clock was originally a kickstart project that was produced to highlight EleksMaker's laser cutting technology, but is now being sold on Banggood.

I have noticed that this clock has a poor time keeping ability and will gain minutes every day.  Fortunately EleksMaker has published an API to interface with the serial port available on the clock controller.  The API details are located on EleksMakers forum here:  http://forum.eleksmaker.com/topic/1941/elekstube-api-control-protocol

As a result, I developed this Arduino program to provide NTP updates to the EleksTube using an inexpensive ESP8266 development board.

The program has the following features:

1. Utilizes WiFiManager to setup a Access Point the first time the program in run.
2. Displays the Access Point IP address on the clock display and waits for the user to connect to the AP and add a SSID to the configuration.
3. Allows user to add timezonedb token, OTA password and Blynk token to wifi configuraton and subsequently saved in SPIF filesystem to be used in the program.
4. Automatically determines your time zone based on your internet IP address.
5. Uses the time zone information to determine your offset from UTC.
6. Uses the time zone information to determine DST details - DST start, DST end and current DST status - either on or off.  Uitilzes this information to automatically switch DST offset from UTC when DST changes occur.
7. Utilizes OTA (Over The Air) update capabilities of the ESP 8266 module to allow sketch updates after the module is installed in the belly of the clock.
8. Uses Blynk https://www.blynk.cc mobile application software to control the clock.  Blynk application allows for selection of clock source (NTP or internal RTC), update internal RTC with NTP data, display current date information once a minute for five seconds at a configurable start second.  Change from 24hr to 12 hr time display.  Configure a time range when the clock display should be off - ie. when sleeping if clock is in a bedroom. Each digit can have an individual colour set and stored and restored during a reboot.  The date display can have a different colour than the time display.

The first time the program is installed on a new ESP8266 board, you must initialize the SPIF files system.  Uncomment the following define statement:

//#define FORMAT_SPIFFS                   // format SPIF FS to store peristent variables - use to initialise FS

Once you have completed the Wifi setup and entered your timezonedb and blynk tokens, then comment the define statement.  If you do not, it will initialze the filesystem everytime you reboot the device and you will need to re-enter your tokens again.

I've made EleksTube app. You are welcome to try it out!

To start using it:
1. Download Blynk App: http://j.mp/blynk_Android or http://j.mp/blynk_iOS
2. Touch the QR-code icon and point the camera to the code below
3. Enjoy my app!

<img src="https://github.com/ib134866/EleksTube/blob/master/EleksTube_Wifi/EleksTube_Blynk_App.png" width="200" height="200">

Screenshots of the application and configuration.
<br>
<img src="https://github.com/ib134866/EleksTube/blob/master/EleksTube_Wifi/EleksTube_Wifi_Screenshot.jpg" width="400" height="600">
<img src="https://github.com/ib134866/EleksTube/blob/master/EleksTube_Wifi/EleksTube_Wifi_Configuration_Screenshot.jpg" width=400 height="600">
<img src="https://github.com/ib134866/EleksTube/blob/master/EleksTube_Wifi/EleksTube_Blynk_App_Screenshot.jpg" width="400" height="600">



