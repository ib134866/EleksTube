/* 
 *******************************************************************************************************************
 * Developed for the EleksMaker http://eleksmaker.com EleksTube clock to to enhance the accuracy of timekeeping by 
 * by providing NTP/GPS functionality.
 * This sketch has been tested with the ESP8266 ESP-12For the WEMOS D1 mini V3.0.0 Arduino board.
 * tzapus WiFiManager https://github.com/tzapu/WiFiManager is used to initiate Wifi connectivity.
 * The first time connecting, the ElecksTube will display the IP address of the WiFiManager access point
 * (default 192.168.4.1) on the EleksTube clock and wait for you to navigate via a web browser to the AP address and enter
 * an available network SSID.  All wifi settings are saved between reboots. Other parameters are entered during the initial
 * WiFi setup:
 * timezoneDB API key - available from http://timezonedb.com
 * OTA password - required to upload new code to the ESP 8266 after installation into the EleksTube clock
 * Blynk key - generated from your own Blynk app on your smart phone - check the nut icon
 * Blynk host - either your own local Blynk server(ip address or host name) or blynk-cloud.com for hosted server
 * Blynk port - default port for local Blynk server is 8080.
 * All parameters are saved to a SPIF FS located on the ESP 8266 board
 * Automatic time zone recognitiom and DST functionality is performed.
 * http://ip-api.com is utilized to determine your geographic location based on your IP address.
 * http://timezonedb.com API is called to determine your time zone and DST capabilities.
 * You will require your own free timezonedb.com API key available at http://timezonedb.com
 * The timezone API is only called during setup and when a DST change has been detected. 
 * OTA update capability (Arduino IDE) has been included to allow updates to the board while installed in the EleksTube.
 * Instructions for using OTA updates in located here: http://esp8266.github.io/Arduino/versions/2.0.0/doc/ota_updates/ota_updates.html
 * 
 *
 *  Development environment specifics:
 *  Arduino 1.8.6
 *  GreeKit ESP-F DevKit V4 - http://www.doit.am , https://www.banggood.com/NodeMcu-Lua-WIFI-Internet-Things-Development-Board-Based-ESP8266-CP2102-Wireless-Module-p-1097112.html?rmmds=myorder&cur_warehouse=CN
 *  or
 *  WEMOS D1 mini V3.0.0 - https://www.banggood.com/Wemos-D1-Mini-V3_0_0-WIFI-Internet-Of-Things-Development-Board-Based-ESP8266-4MB-p-1264245.html?rmmds=myorder&cur_warehouse=CN
 *  EleksMaker ElecksTube - https://www.banggood.com/EleksMaker-EleksTube-Bamboo-6-Bit-Kit-Time-Electronic-Glow-Tube-Clock-Time-Flies-Lapse-p-1297292.html?rmmds=search&cur_warehouse=CN
 * 
 * Distributed as-is; no warranty implied or given.
 */
//#define F_Reset                       // Force Wifi Reset
            
#include <FS.h>                         // this needs to be first, or it all crashes and burns...
#include <NTPClient.h>                  // NTP time client - https://github.com/arduino-libraries/NTPClient
#include <ESP8266WiFi.h>                // ESP8266 WiFi library - https://github.com/esp8266/Arduino
#include <WiFiManager.h>                // WiFi Manager to handle networking - https://github.com/tzapu/WiFiManager
#include <WiFiUdp.h>                    // UDP library to transport NTP data - https://www.arduino.cc/en/Reference/WiFi
#include <ArduinoJson.h>                // json library for parsing http results - https://arduinojson.org/?utm_source=meta&utm_medium=library.properties
#include <ESP8266HTTPClient.h>          // ESP8266 http library - https://github.com/esp8266/Arduino/blob/master/libraries/ESP8266HTTPClient/keywords.txt
#include <TinyGPS++.h>                  // TinyGPS++ library - http://arduiniana.org/libraries/tinygpsplus/
#include <TimeLib.h>                    // Advanced time functions. - https://github.com/PaulStoffregen/Time
#include <ArduinoOTA.h>                 // OTA library to support Over The Air upgrade of sketch - https://github.com/esp8266/Arduino/tree/master/libraries/ArduinoOTA 
#include <MD5Builder.h>                 // MD5 hash library included in the esp8266/Arduino libraries - https://github.com/esp8266/Arduino

// Wifi setup additional parameters saved to SPIF FS
char blynk_token[34]; //Blynk server token 
char timezonedb_key[20];  // timezoneDB API key
char OTA_password[20];    // Over The Air (OTA) password to update software
char blynk_host[34];  //Blynk server name or IP address
char blynk_port[8];   // Blynk server port default is 8080 for local Blynk server
char LED_Colour[][36] = {"402000", "402000", "402000", "402000", "402000", "402000"}; // Colour of LEDs - 042000 represents an orange colour similar to a Neon Nixie Tube
char Date_LED_Colour[][36] = {"402000", "402000", "402000", "402000", "402000", "402000"}; // Colour of LEDs - 042000 represents an orange colour similar to a Neon Nixie Tube
char LED_Colour_Orig[][36] = {"402000", "402000", "402000", "402000", "402000", "402000"}; // Colour of LEDs - 042000 represents an orange colour similar to a Neon Nixie Tube
char All_LED_Colour[] = "402000"; //default colour for all leds
char All_LED_Colour_Orig[] = "402000";  // default colour for all leds
//flag for saving data
bool shouldSaveConfig = false;

/* Comment this out to disable prints and save space */
//#define BLYNK_PRINT Serial
#include <BlynkSimpleEsp8266.h>     // Blynk Library - https://github.com/blynkkk/blynk-library
// You should get Auth Token in the Blynk App.
// Go to the Project Settings (nut icon).
char *auth = (char *) blynk_token; // convert blynk auth token entered in WiFiManager to char *

char urlData[180];                      // array for holding complied query URL
int  offset;                            // local UTC offset in seconds
int  DST;                               // What is the current DST status, if 1 then DST is enabled if 0 DST is disabled - returned from TIMEZONEDB
int  DST_Start;                         // epoch when DST starts for given timezone - returned from timezonedb
int  DST_End;                           // epoch when DST ends for given timezond - returned from timezonedb
int hr_1224 = 1;    // display time as 12hr or 24hr, 1=24hr
int clk_hour;   // current hour
int h_10;       // 10s of hours
int h_1;        // 1s of hours
int m_10;       // 10s of minutes
int m_1;        // 1s of minutes
int s_10;       // 10s of seconds
int s_1;        // 1s of seconds
int mnth_1;     // 1s of months
int mnth_10;    // 10s of months
int day_1;      // 1s of day
int day_10;     // 10s of day
int year_1;     // 1s of year
int year_10;    // 10s of year
int S_NTP;      // sync clock with NTP
int S_GPS;       // sync clock with GPS
int W_RTC;      // write NTP time to RTC
int TZ_Blynk;   // use Blynk provided TZ instead of IP calculated TZ
int D_Date;      // Display Date
int D_Date_second; // Seconds to display date
int D_Date_second_end;  // Seconds date display stops
int D_Date_offset = 5; // number of seconds to display date
int DisplayOn;    // Control to turn display on and off
long D_on;      // Display on time - seconds
long D_off;     // Display off time - seconds
int Digit_Offset = 6; // LED digit to update colour - initial 6 will update all digits
int LED_bright; // LED brightness variable
bool n_param = true; // flag to reset WiFi params if a new json field is introduced
bool one_press = false; // flag to detect if update RTC is pressed twice with 1/2 second
bool reset_wifi;    // flag to save to SPIF FS if Wifi settings should be reset
static uint32_t b_pressTime; // millis() memory for button press time
char LED_black[] = "000000";      // All black
const char EleksTube_realtime[2] = "/";       // EleksTube API - realtime header value
const char EleksTube_update[2] = "*";         // EleksTube API - update time header value
bool Date_colour_change = false;              // Date colour change flag
static const uint32_t GPSBaud = 9600;         // GPS baud rate
int NTP_Update = 600;                         // number of seconds between NTP time updates
int GPS_Update = 4;                           // number of seconds between GPS time updates
const char* NTP_Server = "time.google.com";   // NTP Time server - adjust as desired
// The TinyGPS++ object
TinyGPSPlus gps;                              // define the TinyGPSPlus object

// The serial connection to the GPS device
HardwareSerial gps_Serial(Serial);            // hardware serial is preferred for GPS.  Make sure to disconnect GPS if using the USB serial to upload otherwise upload will fail

// define structure for RGB
typedef struct rgb {
    double r;       // a fraction between 0 and 1
    double g;       // a fraction between 0 and 1
    double b;       // a fraction between 0 and 1
} rgb;
// define structure for HSV - Hue, Saturation, Value
typedef struct hsv {
    double h;       // angle in degrees
    double s;       // a fraction between 0 and 1
    double v;       // a fraction between 0 and 1
} hsv;

static hsv   rgb2hsv(rgb in);
static rgb   hsv2rgb(hsv in);

WiFiUDP ntpUDP;                                 //initialise UDP NTP
NTPClient ntpClient(ntpUDP, NTP_Server);        //initialise NTP client with server name 

HTTPClient http;                                // Initialise HTTP Client

HardwareSerial EleksTube(Serial1);        // D4 - TX only , Use hardware serial to communicate with EleksTube
// Blynk App Controls
// Read the following Blynk app button status on boot up
BLYNK_CONNECTED() {
    Blynk.syncVirtual(V0);              // TZ source - Blynk App or IP address location
    Blynk.syncVirtual(V1);              // Write current time (NTP/GPS) to RTC chip on EleksTube controller
    Blynk.syncVirtual(V3);              // Display date once a minute
    Blynk.syncVirtual(V4);              // Set the second that the date will be displayed - displayed for 5 seconds
    Blynk.syncVirtual(V5);              // Set the display on and off time.  This is used to blank the display during the night time if desired.
    Blynk.syncVirtual(V7);              // Select the digit to update the colour
    Blynk.syncVirtual(V8);              // Display 12 or 24 hour time mode
    Blynk.syncVirtual(V10);             // set the time source - RTC(internal), NTP(Wifi), GPS(external GPS receiver)
}                                      
BLYNK_WRITE(V0)         // Use the Blynk TZ offset instead of IP address calculate TZ
{
  TZ_Blynk = param.asInt(); // assigning incoming value from pin V0 to TZ_Blynk
  if(TZ_Blynk == 1){
    Blynk.syncVirtual(V5);    // set TZ offset
  }else{
    getIPtz();                // get the TZ based on IP address
    getOffset();              // get the offset based on TZ name
    Blynk.syncVirtual(V10);    // ensure time offset is displayed
  }
}
BLYNK_WRITE(V1)     // Write current time to EleksTube internal RTC chip
{
  W_RTC = param.asInt(); // assigning incoming value from pin V1 to W_RTC - write current time and LED_Colour to clock and RTC
  if(W_RTC == 1 && !one_press){   // first button press write to RTC and set a flag and current second variable
      one_press = true;
      b_pressTime = elapsedSecsToday(now()); // get the current cout of seconds today.
  }
  if(W_RTC == 1 && one_press && ((elapsedSecsToday(now()) - b_pressTime) >= 3)){  // if button is pressed again at 3 seconds - reset flag, set wifi reset flag, write flag to SPIF FS and restart system
    one_press = false;
    reset_wifi = true;    // reset Wifi Settings to allow user to adjust variables.  This resets the ESP8266 Wifi settings.
    w_SPIFFS();     // write reset_wifi variable to SPIF FS to survive reset
    system_restart();   // reset the system
  }
}
BLYNK_WRITE(V2)       // Zebra colour selector
{
  int r = param[0].asInt(); // get a RED channel value
  int g = param[1].asInt(); // get a GREEN channel value
  int b = param[2].asInt(); // get a BLUE channel value
  hsv myhsv;        // define hsv structure
  myhsv = rgb2hsv({r,g,b}); // convert current colour to hsv to adjust brightness, but maintain colour
  Blynk.virtualWrite(V9, myhsv.v); // update the brightness slider with converted colour
  Blynk.syncVirtual(V7); // determine witch digit we are updating by getting the value from the Blynk App
  if(Digit_Offset == 6){    // All digits
    sprintf(All_LED_Colour,"%02X%02X%02X",r,g,b);   // combine the RGB colour values into a six character string, zero pad single character values    
    sprintf(All_LED_Colour_Orig,"%02X%02X%02X",r,g,b);   // combine the RGB colour values into a six character string, zero pad single character values    
    for (int thisPin = 0; thisPin < Digit_Offset; thisPin++) {
      sprintf(LED_Colour[thisPin],"%02X%02X%02X",r,g,b);   // combine the RGB colour values into a six character string, zero pad single character values    
      sprintf(LED_Colour_Orig[thisPin],"%02X%02X%02X",r,g,b);   // combine the RGB colour values into a six character string, zero pad single character values    
    }
  }else if (Digit_Offset == 7){   // Date display
    for (int thisPin = 0; thisPin < Digit_Offset - 1; thisPin++){
      sprintf(Date_LED_Colour[thisPin],"%02X%02X%02X",r,g,b);   // combine the RGB colour values into a six character string, zero pad single character values    
    }
  }else{    // otherwise single digit to update
  sprintf(LED_Colour[Digit_Offset],"%02X%02X%02X",r,g,b);   // combine the RGB colour values into a six character string, zero pad single character values    
  sprintf(LED_Colour_Orig[Digit_Offset],"%02X%02X%02X",r,g,b);   // combine the RGB colour values into a six character string, zero pad single character values    
  }
  w_SPIFFS();     // save variables in SPIF FS.  Save colour selection during reboots
}
BLYNK_WRITE(V3)   // Date display
{
  D_Date = param.asInt(); // assigning incoming value from pin V3 to D_Date - display the Date on the clock at a set second
}
BLYNK_WRITE(V4)   // Second to begin displaying date
{
  D_Date_second = param.asInt(); // assigning incoming value from pin V4 to D_Date_second - set the second that the date will be displayed
}
BLYNK_WRITE(V5) // display on and off time - will also provide TZ and offset from UTC
{
  D_off = param[0].asLong(); // assigning incoming value from pin V5 to D_off, used to turn display off - time in seconds since midnight
  D_on = param[1].asLong();  // assigning incoming value from pin V5 to D_on, used to turn display on - time in seconds since midnight
  if(TZ_Blynk == 1){    // check if timezone defined in Blynk app should be used instead of calculated TZ
    const char* tz = param[2].asStr();  // assign TZ offset from Blynk
    urlData[0] = (char)0;                                       // clear array
    // build query url for timezonedb.com
    strcpy (urlData, "http://api.timezonedb.com/v2.1/get-time-zone?key=");
    strcat (urlData, timezonedb_key);                                      // api key
    strcat (urlData, "&format=json&by=zone&zone=");
    strcat (urlData, tz);                                       // timezone data from ip-api.com
    getOffset();                // get the TZ offset from UTC
    Blynk.syncVirtual(V10);     // update time with new offset
  }
}
BLYNK_WRITE(V6) {                   // EleksTube special modes - only available with RTC as time source
  switch (param.asInt()){           // returns menu selection
    case 1:{
      EleksTube.print("$00");       // Normal mode - as per EleksTube API
      // Serial.print("Mode selected:");
      // Serial.println("normal mode");
      break;
    }
    case 2:{
      EleksTube.print("$01");     // Rainbow mode - as per EleksTube API
      // Serial.print("Mode selected:");
      // Serial.println("rainbow mode");
      break;
    }
    case 3:{
      EleksTube.print("$02");   // Waterfall mode - as per ElecksTube API
      // Serial.print("Mode selected:");
      // Serial.println("waterfall mode");
      break;
    } 
    case 4:{
      EleksTube.print("$04");     // Random mode - as per ElecksTube API
      // Serial.print("Mode selected:");
      // Serial.println("random mode");
      break;
    } 
  } 
}
BLYNK_WRITE(V7) {                 // digit selection for colour change
  switch (param.asInt()){           // returns menu selection
    case 1:{
      Digit_Offset = 6;       // All digits colour mode
      Date_colour_change = false;
      update_zeRGBa();
      break;
    }
    case 2:{
      Digit_Offset = 0;     // select the 10h to update colour
      Date_colour_change = false;
      update_zeRGBa();
      break;
    }
    case 3:{
      Digit_Offset = 1;     // select the 1h to update colour
      Date_colour_change = false;
      update_zeRGBa();
      break;
    }
    case 4:{
      Digit_Offset = 2;     // select the 10m to update colour
      Date_colour_change = false;
      update_zeRGBa();
      break;
    }
    case 5:{
      Digit_Offset = 3;     // select the 1m to update colour
      Date_colour_change = false;
      update_zeRGBa();
      break;
    }
    case 6:{
      Digit_Offset = 4;     // select the 10s to update colour
      Date_colour_change = false;
      update_zeRGBa();
      break;
    }
    case 7:{
      Digit_Offset = 5;     // select the 1s to update colour
      Date_colour_change = false;
      update_zeRGBa();
      break;
    }
    case 8:{
      Digit_Offset = 7;     // select the date to update colour
      Date_colour_change = true;
      update_zeRGBa();
      break;
    }
  } 
}
BLYNK_WRITE(V8)         // 12 or 24 hour time display
{
  hr_1224 = param.asInt(); // assigning incoming value from pin V8 to display 12 or 24 hour time
}
BLYNK_WRITE(V9)   // Slider for brightness
{
  LED_bright = param.asInt();
  Blynk.syncVirtual(V7);      // determine which digit we are adjusting the brightness
  if(Digit_Offset == 6){      // All digits
    int r, g, b;
    sscanf(All_LED_Colour_Orig, "%02x%02x%02x", &r, &g, &b);
    hsv myhsv;
    myhsv = rgb2hsv({r,g,b});   // convert current colour to hsv format
    myhsv.v = LED_bright;       // adjust the brightness of the colour to the slider value
    rgb myrgb;                  
    myrgb = hsv2rgb({myhsv.h,myhsv.s,myhsv.v});   // convert back to RGB
    r = myrgb.r;
    g = myrgb.g;
    b = myrgb.b;
    sprintf(All_LED_Colour,"%02X%02X%02X",r,g,b);   // combine the RGB colour values into a six character string, zero pad single character values    
    for (int thisPin = 0; thisPin < Digit_Offset; thisPin++) {
      sprintf(LED_Colour[thisPin],"%02X%02X%02X",r,g,b);   // combine the RGB colour values into a six character string, zero pad single character values    
    }
  }else if (Digit_Offset == 7){ // Date LED colour
    int r, g, b;
    sscanf(Date_LED_Colour[0], "%02x%02x%02x", &r, &g, &b);
    hsv myhsv;
    myhsv = rgb2hsv({r,g,b});     // convert current colour to hsv
    myhsv.v = LED_bright;         // adjust brightness of the colour to the slider value
    rgb myrgb;
    myrgb = hsv2rgb({myhsv.h,myhsv.s,myhsv.v});   // convert back to RGB
    r = myrgb.r;
    g = myrgb.g;
    b = myrgb.b;
    for (int thisPin = 0; thisPin < Digit_Offset - 1; thisPin++){
      sprintf(Date_LED_Colour[thisPin],"%02X%02X%02X",r,g,b);   // combine the RGB colour values into a six character string, zero pad single character values    
    }
  }else if(Digit_Offset < 6){
    int r, g, b;
    sscanf(LED_Colour_Orig[Digit_Offset], "%02x%02x%02x", &r, &g, &b);
    hsv myhsv;
    myhsv = rgb2hsv({r,g,b});   // convert current colour to hsv
    myhsv.v = LED_bright;       // adjust brightness of the colour to the slider value
    rgb myrgb;
    myrgb = hsv2rgb({myhsv.h,myhsv.s,myhsv.v});   // convert back to RGB
    r = myrgb.r;
    g = myrgb.g;
    b = myrgb.b;
    sprintf(LED_Colour[Digit_Offset],"%02X%02X%02X",r,g,b);   // combine the RGB colour values into a six character string, zero pad single character values    
  }
  w_SPIFFS();   // save all RGB setting in SPIF FS
}
BLYNK_WRITE(V10) {                  // set the time source 
  switch (param.asInt()){           // returns menu selection
    case 1:{
      S_NTP = 0;          // internal RTC
      S_GPS = 0;          // internal RTC
      Blynk.syncVirtual(V6);      // determine which special colour mode to display for intenal RTC clock
      // ensure that the time variables are set to turn off display etc. 
      setSyncProvider(getNTPTime);  // Specify function to sync time_t with NTP
      time_t getNTPTime();          // get current NTP time
      break;
    }
    case 2:{
      S_NTP = 1;      // Clock source NTP
      S_GPS = 0;
      setSyncProvider(getNTPTime);  // Specify function to sync time_t with NTP
      time_t getNTPTime();          // get current NTP time
      if(timeStatus()==timeNotSet) // test for time_t set
      {
        //  Serial.println("Unable to sync with NTP");
      } else
      {
        //  Serial.print("NTP has set the system time to: "); 
        //  Serial.print(hour()); Serial.print(":"); Serial.print(minute()); Serial.print(":"); Serial.print(second()); Serial.print(" UTC Offset:"), Serial.print(offset), Serial.print(" DST:"); Serial.print(DST); Serial.print(" Epoch:"); Serial.println(now());;
      }
      break;
    }
    case 3:{
      S_NTP = 0;
      S_GPS = 1;    // Clock source GPS
      setSyncProvider(getGPSTime);
      time_t getGPSTime();          // get current GPS time
      if(timeStatus()==timeNotSet) // test for time_t set
      {
      // Serial.println("Unable to sync with GPS");
      } else{
      // Serial.print("GPS has set the system time to: "); 
      // Serial.print(hour()); Serial.print(":"); Serial.print(minute()); Serial.print(":"); Serial.print(second()); Serial.print(" UTC Offset:"), Serial.print(offset), Serial.print(" DST:"); Serial.print(DST); Serial.print(" Epoch:"); Serial.println(now());;
      }
      break;
    }
  }
}

// define MD5 hash generator used to store the OTA password
MD5Builder _md5;
String md5(String str) {
  _md5.begin();
  _md5.add(String(str));
  _md5.calculate();
  return _md5.toString();
}

//callback notifying us of the need to save config
void saveConfigCallback () {
  // Serial.println("Should save config");
  shouldSaveConfig = true;
}      

void setup()
{
  gps_Serial.begin(9600);               // USB serial port baud rate
  EleksTube.begin(115200);            // EleksTube clock serial port baud rate
  delay(5000);                        // delay 5 seconds for the EleksTube to boot
  
  if (SPIFFS.begin()) {     // set up SPIF FS to store user entered data and LED colour between reboots
    n_param = false;      // set the new parameter flag to false
    // Serial.println("mounted file system");
    if (SPIFFS.exists("/config.json")) {
      //file exists, reading and loading
      // Serial.println("reading config file");
      File configFile = SPIFFS.open("/config.json", "r");
      if (configFile) {
        // Serial.println("opened config file");
        size_t size = configFile.size();
        // Allocate a buffer to store contents of the file.
        std::unique_ptr<char[]> buf(new char[size]);

        configFile.readBytes(buf.get(), size);
        DynamicJsonBuffer jsonBuffer;
        JsonObject& json = jsonBuffer.parseObject(buf.get());
        if (json.success()) {
          // Serial.println("\nparsed json");
          if(json.containsKey("timezonedb_key")){   // check if file contains timezonedb key
          strcpy(timezonedb_key, json["timezonedb_key"]);   // populate variable with saved value
          }else{n_param = true;}  // if value is missing from file, then proceed to Access Point mode to get variable from user
          if(json.containsKey("OTA_password")){ // check if file contains OTA_password
          strcpy(OTA_password, json["OTA_password"]); // populate variable with saved value
          }else{n_param = true;}  // if value is missing from file, then proceed to Access Point mode to get variable from user
          if(json.containsKey("blynk_token")){    // check if file contains blynk_token
          strcpy(blynk_token, json["blynk_token"]); // populate variable with saved value
          }else{n_param = true;}  // if value is missing from file, then proceed to Access Point mode to get variable from user
          for (int thisPin = 0; thisPin < 6; thisPin++) {
            if(json.containsKey("LED_Colour_" + String(thisPin))){  // check if file contains LED_Colour_ variables
            strcpy(LED_Colour[thisPin], json["LED_Colour_" + String(thisPin)]); // populate variable with saved value
            }else{n_param = true;}  // if value is missing from file, then proceed to Access Point mode to get variable from user
            if(json.containsKey("Date_LED_Colour_" + String(thisPin))){   // check if file contains Date_LED_Colour_ variables
            strcpy(Date_LED_Colour[thisPin], json["Date_LED_Colour_" + String(thisPin)]); // populate variable with saved value
            }else{n_param = true;}  // if value is missing from file, then proceed to Access Point mode to get variable from user
          }
          if(json.containsKey("blynk_host")){   //check if file conatins blynk_host variable
          strcpy(blynk_host, json["blynk_host"]); // populate variable with saved value
          }else{n_param = true;}  // if value is missing from file, then proceed to Access Point mode to get variable from user
          if(json.containsKey("blynk_port")){   //check if file contains blynk_port
          strcpy(blynk_port, json["blynk_port"]); // populate variable with saved value
          }else{n_param = true;}  // if value is missing from file, then proceed to Access Point mode to get variable from user
          if(json.containsKey("reset_wifi")){   //check if file contains reset_wifi variable
            if(json["reset_wifi"] == "true"){
              reset_wifi = true;  // set wifi variable to bool
            }else{
              reset_wifi = false; // set wifi variable to bool
            }
          }else{n_param = true;}  // if value is missing from file, then proceed to Access Point mode to get variable from user
        } else {
          // Serial.println("failed to load json config");
        }
        configFile.close();   // close the file 
      }
    }
  } else {
    n_param = true;   // SPIF FS does not exist, must proceed to Wifi AP for configuration
    SPIFFS.format();  // format the SPIF FS - assume new ESP8266
  }
  //end read
  // The extra parameters to be configured (can be either global or just in the setup)
  // After connecting, parameter.getValue() will get you the configured value
  // id/name placeholder/prompt default length
  WiFiManagerParameter custom_timezonedb_key("timezonedb_key", "Timezone DB key",timezonedb_key, 20); // set the defaults for the variables
  WiFiManagerParameter custom_OTA_password("OTA_password", "OTA Password",OTA_password, 20);
  WiFiManagerParameter custom_blynk_token("blynk_token", "Blynk Token",blynk_token, 32);
  WiFiManagerParameter custom_blynk_host("blynk_host", "Blynk Hostname",blynk_host, 32);
  WiFiManagerParameter custom_blynk_port("blynk_port", "Blynk IP Port", "8080", 8);
  
  WiFiManager wifiManager;            // initialze wifiManager
  //set config save notify callback
  wifiManager.setSaveConfigCallback(saveConfigCallback);
  //add all your parameters here
  wifiManager.addParameter(&custom_timezonedb_key); // parameters to be filled in during Wifi configuration
  wifiManager.addParameter(&custom_OTA_password);
  wifiManager.addParameter(&custom_blynk_token);
  wifiManager.addParameter(&custom_blynk_host);
  wifiManager.addParameter(&custom_blynk_port);
  
  #ifdef F_Reset
  n_param =1;
  #endif
  if(n_param || reset_wifi){
    wifiManager.resetSettings();    // reset wifi to get new parameters if missing from config.json file
    reset_wifi = false;     // reset wifi flag
  }
  wifiManager.setAPCallback(configModeCallback);  // if WiFi fails display AP IP address on clock
  wifiManager.autoConnect("EleksTube");   // configuration for the access point - this is the SSID that is displayed
  // Serial.println("WiFi Client connected!");
  
  //read updated parameters from Wifi configuration
  strcpy(timezonedb_key, custom_timezonedb_key.getValue());
  strcpy(OTA_password, custom_OTA_password.getValue());
  strcpy(blynk_token, custom_blynk_token.getValue());
  strcpy(blynk_host, custom_blynk_host.getValue());
  strcpy(blynk_port, custom_blynk_port.getValue());
  //save the custom parameters to FS
  if (shouldSaveConfig) {
    w_SPIFFS();     // save RGB settings, OTA, blynk, reset_wifi and timezonedb settings
  }
  // OTA Setup begin - Over The Air software upgrade
  // Setup ESP OTA upgrade capability
  WiFi.mode(WIFI_STA);
  // Port defaults to 8266
  ArduinoOTA.setPort(8266);
  // Hostname defaults to esp8266-[ChipID]
  ArduinoOTA.setHostname("EleksTube");
  // No authentication by default
  // Password can be set with it's md5 value as well
  String OTA_Hash_Password = md5(OTA_password);       // convert OTA_password to MD5 hash to be used by the OTA update
  char *s2;
  s2 = new char[33];                                  // convert string to char for ArduinoOTA
  memcpy(s2, OTA_Hash_Password.c_str(), 33);
  ArduinoOTA.setPasswordHash(s2);
  delete [] s2;              
  ArduinoOTA.onStart([]() {
    String type;
    if (ArduinoOTA.getCommand() == U_FLASH) {
      type = "sketch";
    } else { // U_SPIFFS
      type = "filesystem";
    }
    // NOTE: if updating SPIFFS this would be the place to unmount SPIFFS using SPIFFS.end()
    Serial.println("Start updating " + type);
  });
  ArduinoOTA.onEnd([]() {
    Serial.println("\nEnd");
  });
  ArduinoOTA.onProgress([](unsigned int progress, unsigned int total) {
    Serial.printf("Progress: %u%%\r", (progress / (total / 100)));
  });
  ArduinoOTA.onError([](ota_error_t error) {
    Serial.printf("Error[%u]: ", error);
    if (error == OTA_AUTH_ERROR) {
      Serial.println("Auth Failed");
    } else if (error == OTA_BEGIN_ERROR) {
      Serial.println("Begin Failed");
    } else if (error == OTA_CONNECT_ERROR) {
      Serial.println("Connect Failed");
    } else if (error == OTA_RECEIVE_ERROR) {
      Serial.println("Receive Failed");
    } else if (error == OTA_END_ERROR) {
      Serial.println("End Failed");
    }
  });
  ArduinoOTA.begin();

  Blynk.config(auth, blynk_host, atoi(blynk_port)); // connect to Blynk server
 
  ntpClient.begin();            // Start NTP client
  delay(5000);                  // wait 5 seconds to start ntpClient
  if(TZ_Blynk == 0){
    getIPtz();  // get ip-api.com Timezone data for your location
  }else{
    Blynk.syncVirtual(V5);    // get the current TZ as set in the Blynk app
  }                    
  getOffset();                  // get timezonedb.com UTC offset, DST start and end time and DST status
}

void loop()
{
  ArduinoOTA.handle();   // Required to handle OTA request via network
  // handle DST time changes by getting new offset 
  if (((DST == 1 && now() >= DST_Start) || (DST == 0 && now() >= DST_End)) && (DST_End != 0 && DST_Start != 0)) // DST has changed - get new UTC offset
  {
    // Serial.println("Resetting timezone offset due to DST change");
    if(TZ_Blynk == 0){
      getIPtz();                    // get ip-api.com Timezone data for your location
    }else{
      Blynk.syncVirtual(V5);      // get the current TZ as set in the Blynk app
    }
    getOffset();                  // get timezonedb.com UTC offset
    UpdateRTC();                  // update clock RTC
  }
  static uint32_t lastTime = millis(); // millis() memory
  DisplayOn = 0;          // initialise display off 
  while (millis() - lastTime < 500)   // wait for 1/2 Second
  {
    Blynk.run();    // Blynk check command
    // If the user wants to re-configure parameters, then pressing Write to RTC button twice 3 seconds apart will reset the clock to a WiFi AP for reconfiguration
    if(elapsedSecsToday(now()) - b_pressTime > 3 ){ //if the W_RTC button is pressed and 3 seconds have elapsed, reset one press flag
      one_press = false;    // reset flag
    }
  }    // only update clock on 1/2 second change - wait here until seconds change
  lastTime = millis();    // reset 1/2 second delay
  if ( D_off > D_on && (D_off >= elapsedSecsToday(now()) && elapsedSecsToday(now()) >= D_on)) //set display on if current time is within display on time
  {
    // Serial.println(String(D_off) + " > " + String(D_on) + " , " +  String(D_off) + " >= " + elapsedSecsToday(now()) + " ," + elapsedSecsToday(now()) + " >= " + String(D_on));
    DisplayOn = 1;
  }
  if ( D_off < D_on && (D_on <= elapsedSecsToday(now()) || elapsedSecsToday(now()) <= D_off))
  {
    // Serial.println(String(D_off) + " < " + String(D_on) + " , " +  String(D_on) + " <= " + elapsedSecsToday(now()) + " ," + elapsedSecsToday(now()) + " <= " + String(D_off)); 
    DisplayOn = 1;
  }  
    if ( DisplayOn == 1 )    // if the display on (D_on) is less than elapsed seconds then display time
  {
    if ((S_NTP == 1 || S_GPS == 1) && W_RTC == 0)        // display time from NTP or GPS if selected
    {
      if(D_Date_second == second())       // Display date second reached as defined in Blynk App?
      {
        D_Date_second_end = (elapsedSecsToday(now()) + D_Date_offset);    // set the display date second end time
      }
      if ((elapsedSecsToday(now()) <= D_Date_second_end && D_Date == 1) || Date_colour_change)    // display date if seconds are between D_Date_second and D_Date_second_end
      {
        splitDate();    // splite the date in separate digits for display
        //display the date on the clock via serial output
        EleksSerialOut(EleksTube_realtime, mnth_10, Date_LED_Colour[0], mnth_1, Date_LED_Colour[1], day_10, Date_LED_Colour[2], day_1, Date_LED_Colour[3], year_10, Date_LED_Colour[4], year_1, Date_LED_Colour[5]);
      } else
      {
        splitTime();    // split the time into separate digits to display
        // output time to EleksTube via EleksTube serial output
        EleksSerialOut(EleksTube_realtime, h_10, LED_Colour[0], h_1, LED_Colour[1], m_10, LED_Colour[2], m_1, LED_Colour[3], s_10, LED_Colour[4], s_1, LED_Colour[5]); 
      }
    }
  } else
  {   // black out tube if between D_off and D_on
    // send LEB_Black to all digits
    EleksSerialOut(EleksTube_realtime, h_10, LED_black, h_1, LED_black, m_10, LED_black, m_1, LED_black, s_10, LED_black, s_1, LED_black);  
  }
  // Print time on serial for debugging
  // String serial_out = (String(h_10) + String(h_1) + ":" + String(m_10) + String(m_1) + ":" + String(s_10) + String(s_1) + " DST:" + String(DST) + " Epoch:" + String(now()));
  // EleksTube.println(serial_out + " offset: "+ String(offset) + " V0: " + String(TZ_Blynk) + " ElepasedSeconds: " + elapsedSecsToday(now()) + " D_on: " + String(D_on) + " D_off: " + String(D_off) + " DisplayOn: " + DisplayOn);
}

void getIPtz() { // pull timezone data from ip-api.com and create URL string to query http://api.timezonedb.com

  http.begin("http://ip-api.com/json/?fields=timezone");      //get location timezone data from ip-api based on IP address
  int httpCode = http.GET();                                  // Send the request
  String ipapi = http.getString();                            // Return to the string
  http.end();   //Close connection                            // Close http client
  DynamicJsonBuffer jsonBuffer;                               // Set up dynamic json buffer
  JsonObject& root = jsonBuffer.parseObject(ipapi);           // Parse ip-api data
  if (!root.success()) {
    Serial.println(F("Parsing failed!"));
    return;
}

  char* tz = strdup(root["timezone"]);                        // copy timezone data from json parse

  urlData[0] = (char)0;                                       // clear array
  // build query url for timezonedb.com
  strcpy (urlData, "http://api.timezonedb.com/v2.1/get-time-zone?key=");
  //strcat (urlData, KEY);                                      // api key
  strcat (urlData, timezonedb_key);                                      // api key
  strcat (urlData, "&format=json&by=zone&zone=");
  strcat (urlData, tz);                                       // timezone data from ip-api.com
}

void getOffset() {              // get the timezone offset, state of dst, dst end and dst start
 
  http.begin(urlData);
  int httpCode = http.GET();        //Send the request
  String timezonedb = http.getString();
  http.end();                       //Close connection
  DynamicJsonBuffer jsonBuffer;     // allocate json buffer

  JsonObject& root = jsonBuffer.parseObject(timezonedb); // parse return from timzonedb.com

  offset = (root["gmtOffset"]);     // get offset from UTC/GMT
  DST = (root["dst"]);              // get current DST status
  DST_Start = (root["zoneStart"]);  // get epoch time when DST starts
  DST_End = (root["zoneEnd"]);      // get epoch time when DST stops

}

time_t getNTPTime(){                // Return current UTC time to time_t
  ntpClient.update();
  setSyncInterval(NTP_Update);         // Set time_t sync interval
  return (ntpClient.getEpochTime() + offset);
  // Serial.println( ntpClient.getEpochTime() + offset);
}

time_t getGPSTime(){              // Return current UTC time to time_t
  setSyncInterval(GPS_Update);    // Set time_t sync interval
  tmElements_t tm;
  time_t GPSTime;
  while (gps_Serial.available() > 0)    // serial data available from GPS - could be multiple messages waiting in serial buffer
  {
    if (gps.encode(gps_Serial.read())){ // read serial data from GPS and encode the NMEA message
      if(gps.time.isUpdated()){         // update the tmElements array
        tm.Second = gps.time.second();
        tm.Minute = gps.time.minute();
        tm.Hour = gps.time.hour();
        tm.Day = gps.date.day();
        tm.Month = gps.date.month();
        tm.Year = gps.date.year() - 1970;   // convert to 1970 Epoch
        GPSTime =  makeTime(tm);      // convert array to UTC
      }
    }
  }
  if ((millis() > 5000 && gps.charsProcessed() < 10) || (gps.time.age() > 1500))  // assume no GPS if over 5 seconds, less than 10 characters recevied or the GPS time age in over 1.5 seconds
  {     // reset the clock source back to NTP
    S_GPS = 0;  // no GPS sync
    S_NTP = 1;  // return to NTP sync
    Blynk.virtualWrite(V10,2);   // update Blynk app to reflect NTP
    setSyncProvider(getNTPTime);  // Specify function to sync time_t with NTP
    time_t getNTPTime();
    return (ntpClient.getEpochTime() + offset); // return the NTP time
  }else{
    return (GPSTime + offset);    // return the GPS time
  }
}

void configModeCallback (WiFiManager *myWiFiManager) {
  IPAddress APIP = WiFi.softAPIP();       // get the AP IP address
  int x = 0;
  while(x < 4)                            // Display the octects of the AP IP address on the EleksTube
  {
  // Serial.println(APIP[x]);
  int result;
  // Display the octects of the AP IP address on the left most 3 digits of the EleksTube display
  String clock_out = (String(EleksTube_realtime) + String((APIP[x] / 100)) + "FFFFFF" + String(((APIP[x] / 10) % 10)) + "FFFFFF" + String((APIP[x] % 10)) + "FFFFFF000000000000000000000");
  for(int y = 0; y <= 10; y++){ // loop 10 times to display the octect
    EleksTube.print(clock_out);   // display the octect on the display
    delay(400);   // delay 400 ms
  }
  x++;
  }
  // Serial.println(WiFi.softAPIP());    // display the IP address on the serial port as well
  // Serial.println(myWiFiManager->getConfigPortalSSID());   // display the AP SSID on the serial port
}

void splitTime(){
// Correct time -  split time into individual digits and add leading zeros to hours, minutes and seconds as required
  if(hr_1224){
    clk_hour = hour();  // defines the display to either 12 or 24 hour
  }else{
    clk_hour = hourFormat12();
  }
  if(clk_hour <= 9)   // hour is less then 9 add zero
  {
    h_10 = 0;       // set leading digit to zero
    h_1 = clk_hour;   // set least significant digit to variable
  } else {
    h_1 = clk_hour % 10;    // get modulus and set least significant digit to result
    h_10 = clk_hour / 10;  // divide by 10 to get the most significant digit
  }
  if(minute() <= 9)   // minute is less than 9 add leading zero
  {
    m_10 = 0;       // set leading digit to zero
    m_1 = minute(); // set least significant digit to variable
  } else {
    m_1 = minute() % 10;    // get modulus and set least significant digit to result
    m_10 = minute() / 10;  // divide by 10 to get the most significant digit
  }
  if(second() <= 9)   // second is less than 9, add leading zero
  {
    s_10 = 0;             // set leading digit to zero
    s_1 = second();       // set least significant digit to variable
  } else {
    s_1 = second() % 10;    // get modulus and set least significant digit to result
    s_10 = second() / 10;  // divide by 10 to get the most significant digit
  }
}
void splitDate(){
// Correct time -  split time into individual digits and add leading zeros to hours, minutes and seconds as required
  if(month() <= 9)   // hour is lss then 9 add zero
  {
    mnth_10 = 0;       // set leading digit to zero
    mnth_1 = month();   // set least significant digit to variable
  } else {
    mnth_1 = month() % 10;    // get modulus and set least significant digit to result
    mnth_10 = month() / 10;  // divide by 10 to get the most significant digit
  }
  if(day() <= 9)   // minute is less than 9 add leading zero
  {
    day_10 = 0;       // set leading digit to zero
    day_1 = day(); // set least significant digit to variable
  } else {
    day_1 = day() % 10;    // get modulus and set least significant digit to result
    day_10 = day() / 10;  // divide by 10 to get the most significant digit
  }
  if((year() % 100) <= 9)   // year is less than 9, add leading zero
  {
    year_10 = 0;             // set leading digit to zero
    year_1 = year() % 100 ;       // set least significant digit to variable
  } else {
    year_1 = (year() % 100) % 10;    // get modulus and set least significant digit to result
    year_10 = (year() % 100) / 10;  // divide by 10 to get the most significant digit
  }
}
void UpdateRTC()    // update the ElekTube RTC chip on the controller
{
    if(S_NTP ==1){
      getNTPTime();                 // update NTP time
    }
    if(S_GPS ==1){
      getGPSTime();                 // update GPS time
    }
    splitTime();                  // split the hour, minutes and seconds into individual digits for EleksTube
    // Write the time to the RTC chip in the EleksTube clock
    EleksSerialOut(EleksTube_update, h_10, LED_Colour[0], h_1, LED_Colour[1], m_10, LED_Colour[2], m_1, LED_Colour[3], s_10, LED_Colour[4], s_1, LED_Colour[5]);     // Update RTC clock on EleksTude clock
}

void EleksSerialOut(const char header[2], int d_1, const char d_1_color[], int d_2, const char d_2_color[], int d_3, const char d_3_color[], int d_4, const char d_4_color[], int d_5, const char d_5_color[], int d_6, const char d_6_color[])
{
  // Output time to EleksTube according to ElekMaker published API
  // /(header)1(first tube number)66CCFF(Standard Sixteen Binary RGB color)166CCFF(second tube number and color).....
  String clock_out = (String(header) + String(d_1) + String(d_1_color) + String(d_2) + String(d_2_color) + String(d_3) + String(d_3_color) + String(d_4) + String(d_4_color) + String(d_5) + String(d_5_color) + String(d_6) + String(d_6_color));
  EleksTube.print(clock_out);
}

void w_SPIFFS()
{
  // write variables to a JSON file on the SPIF FS
  // Serial.println("saving config");
  DynamicJsonBuffer jsonBuffer;
  JsonObject& json = jsonBuffer.createObject();
  json["timezonedb_key"] = timezonedb_key;    // save the timezonedb key
  json["OTA_password"] = OTA_password;        // save the OTA_password
  json["blynk_token"] = blynk_token;          // save the unique Blynk Token
  json["All_LED_Colour"] = All_LED_Colour;    // Colour for all LEDs
  json["All_LED_Colour_Orig"] = All_LED_Colour_Orig;  // Original colour for all LEDs
  for (int thisPin = 0; thisPin < 6; thisPin++) {
    json["LED_Colour_" + String(thisPin)] = LED_Colour[thisPin];
    json["LED_Colour_Orig_" + String(thisPin)] = LED_Colour_Orig[thisPin];
    json["Date_LED_Colour_" + String(thisPin)] = Date_LED_Colour[thisPin];
  }
  json["blynk_host"] = blynk_host;          // save the unique Blynk Hostname
  json["blynk_port"] = blynk_port;          // save the unique Blynk port
  json["reset_wifi"] = reset_wifi;          // save the reset wifi flag
  File configFile = SPIFFS.open("/config.json", "w");
    if (!configFile) {
      // Serial.println("failed to open config file for writing");
    }

  // json.printTo(Serial);
  json.printTo(configFile);
  configFile.close();
}

hsv rgb2hsv(rgb in)     // convert from RGB to HSV
{
    hsv         out;
    double      min, max, delta;

    min = in.r < in.g ? in.r : in.g;
    min = min  < in.b ? min  : in.b;

    max = in.r > in.g ? in.r : in.g;
    max = max  > in.b ? max  : in.b;

    out.v = max;                                // v
    delta = max - min;
    if (delta < 0.00001)
    {
        out.s = 0;
        out.h = 0; // undefined, maybe nan?
        return out;
    }
    if( max > 0.0 ) { // NOTE: if Max is == 0, this divide would cause a crash
        out.s = (delta / max);                  // s
    } else {
        // if max is 0, then r = g = b = 0              
        // s = 0, h is undefined
        out.s = 0.0;
        out.h = NAN;                            // its now undefined
        return out;
    }
    if( in.r >= max )                           // > is bogus, just keeps compilor happy
        out.h = ( in.g - in.b ) / delta;        // between yellow & magenta
    else
    if( in.g >= max )
        out.h = 2.0 + ( in.b - in.r ) / delta;  // between cyan & yellow
    else
        out.h = 4.0 + ( in.r - in.g ) / delta;  // between magenta & cyan

    out.h *= 60.0;                              // degrees

    if( out.h < 0.0 )
        out.h += 360.0;

    return out;
}

rgb hsv2rgb(hsv in)   // covert HSV to RGB
{
    double      hh, p, q, t, ff;
    long        i;
    rgb         out;

    if(in.s <= 0.0) {       // < is bogus, just shuts up warnings
        out.r = in.v;
        out.g = in.v;
        out.b = in.v;
        return out;
    }
    hh = in.h;
    if(hh >= 360.0) hh = 0.0;
    hh /= 60.0;
    i = (long)hh;
    ff = hh - i;
    p = in.v * (1.0 - in.s);
    q = in.v * (1.0 - (in.s * ff));
    t = in.v * (1.0 - (in.s * (1.0 - ff)));

    switch(i) {
    case 0:
        out.r = in.v;
        out.g = t;
        out.b = p;
        break;
    case 1:
        out.r = q;
        out.g = in.v;
        out.b = p;
        break;
    case 2:
        out.r = p;
        out.g = in.v;
        out.b = t;
        break;

    case 3:
        out.r = p;
        out.g = q;
        out.b = in.v;
        break;
    case 4:
        out.r = t;
        out.g = p;
        out.b = in.v;
        break;
    case 5:
    default:
        out.r = in.v;
        out.g = p;
        out.b = q;
        break;
    }
    return out;     
}
void update_zeRGBa()    // update the Zebra on the Blynk app
{
  int r, g, b;
  if(Digit_Offset == 6 )    // use the All_LED_Colour
  {
    sscanf(All_LED_Colour_Orig, "%02x%02x%02x", &r, &g, &b);
    Blynk.virtualWrite(V2, r, g, b);
  }else if(Digit_Offset == 7) // use the date display colour
  {
    sscanf(Date_LED_Colour[0], "%02x%02x%02x", &r, &g, &b);
  }else if(Digit_Offset < 6)    // use the individual digit colour
  {
    sscanf(LED_Colour_Orig[Digit_Offset], "%02x%02x%02x", &r, &g, &b);
  }
  Blynk.virtualWrite(V2, r, g, b);    // update the Zebra with the colour
}
