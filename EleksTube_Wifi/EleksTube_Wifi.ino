/* 
 *******************************************************************************************************************
 * Developed for the EleksMaker http://eleksmaker.com EleksTube clock to to enhance the accuracy of timekeeping by 
 * by providing NTP functionality.
 * This sketch uses the ESP-12F 8266 Arduino board.
 * tzapus WiFiManager https://github.com/tzapu/WiFiManager is used to initiate Wifi connectivity.
 * The first time connecting, the ElecksTube will display the IP address of the WiFiManager access point
 * (default 192.168.4.1) breifly and wait for you to navigate via a web browser to the AP address and enter
 * an available network SSID.  All wifi settings are saved between reboots.
 * Automatic time zone recognitiom and DST functionality is performed.
 * http://ip-api.com is utilized to determine your geographic location based on your IP address.
 * http://timezonedb.com API is called to determine your time zone and DST capabilities.
 * You will require your own free timezonedb.com API key available at http://timezonedb.com
 * The timezone API is only called during setup and when a DST change has been detected. 
 * OTA update capability (Arduino IDE) has been included to allow updates to the board while installed in the EleksTube.
 * Instructions for using OTA updates in located here: http://esp8266.github.io/Arduino/versions/2.0.0/doc/ota_updates/ota_updates.html
 * You will need to either add your own MD5 hash password, set a ASCII password or have no password to the sketch
 *
 *  Development environment specifics:
 *  Arduino 1.8.6
 *  GreeKit ESP-F DevKit V4 - http://www.doit.am
 *  EleksMaker ElecksTube - https://www.banggood.com/EleksMaker-EleksTube-Bamboo-6-Bit-Kit-Time-Electronic-Glow-Tube-Clock-Time-Flies-Lapse-p-1297292.html?rmmds=search&cur_warehouse=CN
 * 
 * Distributed as-is; no warranty implied or given.
 */
//#define DEBUG                           // Turn on DEBUG to serial port
#define BLYNK_APP                       // use the Blynk app to control the clock 
//#define WIFIMANAGER_RESET               // Define to reset WiFi settings for testing 
//#define FORMAT_SPIFFS                   // format SPIF FS to store peristent variables - use to initialise FS
#define WIFI_STORE_PARAMS          // use to store extra WiFiManager parameters
#include <FS.h>                         //this needs to be first, or it all crashes and burns...
#include <NTPClient.h>                  // NTP time client - https://github.com/arduino-libraries/NTPClient
#include <ESP8266WiFi.h>                // ESP8266 WiFi library - https://github.com/esp8266/Arduino
#include <WiFiManager.h>                // WiFi Manager to handle networking - https://github.com/tzapu/WiFiManager
#include <WiFiUdp.h>                    // UDP library to transport NTP data - https://www.arduino.cc/en/Reference/WiFi
#include <ArduinoJson.h>                // json library for parsing http results - https://arduinojson.org/?utm_source=meta&utm_medium=library.properties
#include <ESP8266HTTPClient.h>          // ESP8266 http library - https://github.com/esp8266/Arduino/blob/master/libraries/ESP8266HTTPClient/keywords.txt
#include <TimeLib.h>                    // Advanced time functions. - https://github.com/PaulStoffregen/Time
#include <ArduinoOTA.h>                 // OTA library to support Over The Air upgrade of sketch - https://github.com/esp8266/Arduino/tree/master/libraries/ArduinoOTA 
#include <MD5Builder.h>                 // MD5 hash library included in the esp8266/Arduino libraries - https://github.com/esp8266/Arduino

#ifdef WIFI_STORE_PARAMS
char blynk_token[34] = "Your_Blynk_Token";
char timezonedb_key[20] = "Your_TimeZoneDB_Key";
char OTA_password[20] = "Your_OTA_password";
//flag for saving data
bool shouldSaveConfig = false;

#endif
#ifdef BLYNK_APP
/* Comment this out to disable prints and save space */
#define BLYNK_PRINT Serial
#include <BlynkSimpleEsp8266.h>
// You should get Auth Token in the Blynk App.
// Go to the Project Settings (nut icon).
char *auth = (char *) blynk_token; // convert blynk auth token entered in WiFiManager to char *
#endif

char urlData[180];                      // array for holding complied query URL
int  offset;                            // local UTC offset in seconds
int  DST;                               // What is the current DST status, if 1 then DST is enabled if 0 DST is disabled - returned from TIMEZONEDB
int  DST_Start;                         // epoch when DST starts for given timezone - returned from timezonedb
int  DST_End;                           // epoch when DST ends for given timezond - returned from timezonedb
int  Cur_time;                          // current time - epoch
int x;                                  // temporary loop variable
int hr_1224 = 1;    // display time as 12hr or 24hr, 1=24hr
int clk_hour;   // current hour
int h_10;       // 10s of hours
int h_1;        // 1s of hours
int m_10;       // 10s of minutes
int m_1;        // 1s of minutes
int s_10;       // 10s of seconds
int s_1;        // 1s of seconds
int mnth_1;     // 1s of months
int mnth_10;    // 10s of monthd
int day_1;      // 1s of day
int day_10;     // 10s of day
int year_1;     // 1s of year
int year_10;    // 10s of year
int S_NTP;      // synch clock with NTP
int W_RTC;      // write NTP time to RTC
int D_Date;      // Display Date
int D_Date_second; // Seconds to display date
int D_Date_second_end;  // Seconds date display stops
int D_Date_offset = 5; // number of seconds to display date
int DisplayOn;    // Control to turn display on and off
long D_on;      // Display on time - seconds
long D_off;     // Display off time - seconds
int Digit_Offset = 6; // LED digit to update colour - initial 6 will update all digits
char LED_Colour[][36] = {"402000", "402000", "402000", "402000", "402000", "402000"}; // Colour of LEDs - 042000 represents an orange colour similar to a Neon Nixie Tube
char Date_LED_Colour[][36] = {"402000", "402000", "402000", "402000", "402000", "402000"}; // Colour of LEDs - 042000 represents an orange colour similar to a Neon Nixie Tube
//char LED_Colour[]= "402000";     // Colour of LEDs - 042000 represents an orange colour similar to a Neon Nixie Tube
char LED_black[] = "000000";      // All black
const char EleksTube_realtime[2] = "/";       // EleksTube API - realtime header value
const char EleksTube_update[2] = "*";         // EleksTube API - update time header value

#ifdef BLYNK_APP_APP
const String BLYNK_BLACK =   "#000000";
#endif
WiFiUDP ntpUDP;                                 //initialise UDP NTP
NTPClient ntpClient(ntpUDP, "time.google.com"); // initialist NTP client with server name 

HTTPClient http;                                // Initialise HTTP Client

HardwareSerial EleksTube(Serial1);        // D4 - TX only

#ifdef BLYNK_APP
BLYNK_CONNECTED() {
    Blynk.syncVirtual(V0);
    Blynk.syncVirtual(V3);
    Blynk.syncVirtual(V4);
    Blynk.syncVirtual(V5);
    Blynk.syncVirtual(V7); 
    Blynk.syncVirtual(V8); 
    // Blynk.syncAll();                // get all of the control settings from the Blynk server
}                                      
// in Blynk app writes values to the Virtual Pin 0
BLYNK_WRITE(V0)
{
  S_NTP = param.asInt(); // assigning incoming value from pin V0 to S_NTP - sync clock with NTP server
  // You can also use:
  // String i = param.asStr();
  // double d = param.asDouble();
  //Serial.print("V0 Button value is: ");
  //Serial.println(S_NTP);
}
// in Blynk app writes values to the Virtual Pin 1
BLYNK_WRITE(V1)
{
  W_RTC = param.asInt(); // assigning incoming value from pin V1 to W_RTC - write current time and LED_Colour to clock and RTC
  // You can also use:
  // String i = param.asStr();
  // double d = param.asDouble();
  //Serial.print("V1 Button value is: ");
  //Serial.println(W_RTC);
  UpdateRTC();          // Update clock's RTC
}
// in Blynk app writes values to the Virtual Pin 2
BLYNK_WRITE(V2)
{
  int r = param[0].asInt(); // get a RED channel value
  int g = param[1].asInt(); // get a GREEN channel value
  int b = param[2].asInt(); // get a BLUE channel value
  // You can also use:
  // String i = param.asStr();
  // double d = param.asDouble();
  Blynk.syncVirtual(V7);
  if(Digit_Offset == 6){
    for (int thisPin = 0; thisPin < Digit_Offset; thisPin++) {
    sprintf(LED_Colour[thisPin],"%02X%02X%02X",r,g,b);   // combine the RGB colour values into a six character string, zero pad single character values    
    }
  }else if (Digit_Offset == 7){
    for (int thisPin = 0; thisPin < Digit_Offset - 1; thisPin++){
      sprintf(Date_LED_Colour[thisPin],"%02X%02X%02X",r,g,b);   // combine the RGB colour values into a six character string, zero pad single character values    
    }
  }else{
  sprintf(LED_Colour[Digit_Offset],"%02X%02X%02X",r,g,b);   // combine the RGB colour values into a six character string, zero pad single character values    
  }
  Serial.println("saving config");
  DynamicJsonBuffer jsonBuffer;
  JsonObject& json = jsonBuffer.createObject();
  json["timezonedb_key"] = timezonedb_key;    // save the timezonedb key
  json["OTA_password"] = OTA_password;        // save the OTA_password
  json["blynk_token"] = blynk_token;          // save the unique Blynk Token
  for (int thisPin = 0; thisPin < 6; thisPin++) {
    json["LED_Colour_" + String(thisPin)] = LED_Colour[thisPin];
    json["Date_LED_Colour_" + String(thisPin)] = Date_LED_Colour[thisPin];
  }
  File configFile = SPIFFS.open("/config.json", "w");
    if (!configFile) {
      Serial.println("failed to open config file for writing");
    }

  #ifdef DEBUG
  json.printTo(Serial);
  #endif
  json.printTo(configFile);
  configFile.close();
  #ifdef DEBUG
  Serial.print("V2 Button value is: ");
  Serial.println(LED_Colour);
  Serial.println(Date_LED_Colour);
  #endif
}
// in Blynk app writes values to the Virtual Pin 3
BLYNK_WRITE(V3)
{
  D_Date = param.asInt(); // assigning incoming value from pin V3 to D_Date - display the Date on the clock at a set second
  // You can also use:
  // String i = param.asStr();
  // double d = param.asDouble();
  //Serial.print("V3 Button value is: ");
  //Serial.println(D_Date);
}
// in Blynk app writes values to the Virtual Pin 4
BLYNK_WRITE(V4)
{
  D_Date_second = param.asInt(); // assigning incoming value from pin V4 to D_Date_second - set the second that the date will be displayed
  // You can also use:
  // String i = param.asStr();
  // double d = param.asDouble();
  //Serial.print("V4 Button value is: ");
  //Serial.println(D_Date_second);
}
// in Blynk app writes values to the Virtual Pin 5
BLYNK_WRITE(V5)
{
  D_off = param[0].asLong(); // assigning incoming value from pin V5 to D_off, used to turn display off - time in seconds since midnight
  D_on = param[1].asLong();  // assigning incoming value from pin V5 to D_on, used to turn display on - time in seconds since midnight
  // You can also use:
  // String i = param.asStr();
  // double d = param.asDouble();
  //Serial.print("Display on value is: ");
  //Serial.println(D_on);
  //Serial.print("Display off value is: ");
  //Serial.println(D_off);
}
// in Blynk app writes values to the Virtual Pin 6
BLYNK_WRITE(V6) {
  switch (param.asInt()){           // returns menu selection
    case 1:{
      EleksTube.print("$00");       // Normal mode - as per EleksTube API
      #ifdef DEBUG
      Serial.print("Mode selected:");
      Serial.println("normal mode");
      #endif;
      break;
    }
    case 2:{
      EleksTube.print("$01");     // Rainbow mode - as per EleksTube API
      #ifdef DEBUG
      Serial.print("Mode selected:");
      Serial.println("rainbow mode");
      #endif;
      //Blynk.setProperty(V0, "color", BLYNK_BLACK);
      //Blynk.virtualWrite(V0, LOW);
      //Blynk.setProperty(V0, "onLabel", "disabled");
      //Blynk.setProperty(V0, "offLabel", "disabled");
      //Serial.println("Button V0 disable"); 
      break;
    }
    case 3:{
      EleksTube.print("$02");   // Waterfall mode - as per ElecksTube API
      #ifdef DEBUG
      Serial.print("Mode selected:");
      Serial.println("waterfall mode");
      #endif;
      //Blynk.setProperty(V0, "color", BLYNK_BLACK);
      //Blynk.virtualWrite(V0, LOW);
      //Blynk.setProperty(V0, "onLabel", "disabled");
      //Blynk.setProperty(V0, "offLabel", "disabled");
      //Serial.println("Button V0 disable");
      break;
    } 
    case 4:{
      EleksTube.print("$04");     // Random mode - as per ElecksTube API
      #ifdef DEBUG
      Serial.print("Mode selected:");
      Serial.println("random mode");
      #endif;
      //Blynk.setProperty(V0, "color", BLYNK_BLACK);
      //Blynk.virtualWrite(V0, LOW);
      //Blynk.setProperty(V0, "onLabel", "disabled");
      //Blynk.setProperty(V0, "offLabel", "disabled");
      //Serial.println("Button V0 disable");
      break;
    } 
  } 
}
// in Blynk app writes values to the Virtual Pin 6
BLYNK_WRITE(V7) {
  switch (param.asInt()){           // returns menu selection
    case 1:{
      Digit_Offset = 6;       // All digits clour mode
      break;
    }
    case 2:{
      Digit_Offset = 0;     // select the 10h to update colour
      break;
    }
    case 3:{
      Digit_Offset = 1;     // select the 1h to update colour
      break;
    }
    case 4:{
      Digit_Offset = 2;     // select the 10m to update colour
      break;
    }
    case 5:{
      Digit_Offset = 3;     // select the 1m to update colour
      break;
    }
    case 6:{
      Digit_Offset = 4;     // select the 10s to update colour
      break;
    }
    case 7:{
      Digit_Offset = 5;     // select the 1s to update colour
      break;
    }
    case 8:{
      Digit_Offset = 7;     // select the date to update colour
      break;
    }
  } 
}
BLYNK_WRITE(V8)
{
  hr_1224 = param.asInt(); // assigning incoming value from pin V0 to S_NTP - sync clock with NTP server
  // You can also use:
  // String i = param.asStr();
  // double d = param.asDouble();
  //Serial.print("V8 Button value is: ");
  //Serial.println(hr_1224);
}
#endif

// define MD5 hash generator
MD5Builder _md5;
String md5(String str) {
  _md5.begin();
  _md5.add(String(str));
  _md5.calculate();
  return _md5.toString();
}

#ifdef WIFI_STORE_PARAMS
//callback notifying us of the need to save config
void saveConfigCallback () {
  Serial.println("Should save config");
  shouldSaveConfig = true;
}      
#endif

void setup()
{
  Serial.begin(115200);               // USB serial port
  EleksTube.begin(115200);            // EleksTube clock serial port
  delay(5000);                        // delay 5 seconds for the EleksTube to boot
  #ifdef WIFI_STORE_PARAMS
  //clean FS, for testing
  #ifdef FORMAT_SPIFFS
  SPIFFS.format();
  #endif

  //read configuration from FS json
  Serial.println("mounting FS...");

  if (SPIFFS.begin()) {
    Serial.println("mounted file system");
    if (SPIFFS.exists("/config.json")) {
      //file exists, reading and loading
      Serial.println("reading config file");
      File configFile = SPIFFS.open("/config.json", "r");
      if (configFile) {
        Serial.println("opened config file");
        size_t size = configFile.size();
        // Allocate a buffer to store contents of the file.
        std::unique_ptr<char[]> buf(new char[size]);

        configFile.readBytes(buf.get(), size);
        DynamicJsonBuffer jsonBuffer;
        JsonObject& json = jsonBuffer.parseObject(buf.get());
        #ifdef DEBUG
        json.printTo(Serial);
        #endif
        if (json.success()) {
          Serial.println("\nparsed json");

          strcpy(timezonedb_key, json["timezonedb_key"]);
          strcpy(OTA_password, json["OTA_password"]);
          strcpy(blynk_token, json["blynk_token"]);
          for (int thisPin = 0; thisPin < 6; thisPin++) {
            strcpy(LED_Colour[thisPin], json["LED_Colour_" + String(thisPin)]);
            strcpy(Date_LED_Colour[thisPin], json["Date_LED_Colour_" + String(thisPin)]);
          }
        } else {
          Serial.println("failed to load json config");
        }
        configFile.close();
      }
    }
  } else {
    Serial.println("failed to mount FS");
  }
  //end read
  // The extra parameters to be configured (can be either global or just in the setup)
  // After connecting, parameter.getValue() will get you the configured value
  // id/name placeholder/prompt default length
  WiFiManagerParameter custom_timezonedb_key("timezonedb_key", "Timezone DB key", timezonedb_key, 20);
  WiFiManagerParameter custom_OTA_password("OTA_password", "OTA Password", OTA_password, 20);
  WiFiManagerParameter custom_blynk_token("blynk_token", "Blynk Token", blynk_token, 32);
  #endif
  WiFiManager wifiManager;            // initialze wifiManager
  #ifdef WIFI_STORE_PARAMS
  //set config save notify callback
  wifiManager.setSaveConfigCallback(saveConfigCallback);
  //add all your parameters here
  wifiManager.addParameter(&custom_timezonedb_key);
  wifiManager.addParameter(&custom_OTA_password);
  wifiManager.addParameter(&custom_blynk_token);
  #endif
  //reset settings - for testing
  #ifdef WIFIMANAGER_RESET
  wifiManager.resetSettings();      // reset Wifi Settings for testing
  #endif
  wifiManager.setAPCallback(configModeCallback);  // if WiFi fails display AP IP address on clock
  wifiManager.autoConnect("EleksTube");   // configuration for the access point - this is the SSID that is displayed
  #ifdef DEBUG
  Serial.println("WiFi Client connected!");
  #endif;
  #ifdef WIFI_STORE_PARAMS
  //read updated parameters
  strcpy(timezonedb_key, custom_timezonedb_key.getValue());
  strcpy(OTA_password, custom_OTA_password.getValue());
  strcpy(blynk_token, custom_blynk_token.getValue());
  #ifdef DEBUG
  Serial.println(timezonedb_key);
  Serial.println(OTA_password);
  Serial.println(blynk_token);
  #endif
  //save the custom parameters to FS
  if (shouldSaveConfig) {
    Serial.println("saving config");
    DynamicJsonBuffer jsonBuffer;
    JsonObject& json = jsonBuffer.createObject();
    json["timezonedb_key"] = timezonedb_key;    // save the timezonedb key
    json["OTA_password"] = OTA_password;        // save the OTA_password
    json["blynk_token"] = blynk_token;          // save the unique Blynk Token
    for (int thisPin = 0; thisPin < 6; thisPin++) {
      json["LED_Colour_" + String(thisPin)] = LED_Colour[thisPin];
      json["Date_LED_Colour_" + String(thisPin)] = Date_LED_Colour[thisPin];
    }
  
    File configFile = SPIFFS.open("/config.json", "w");
    if (!configFile) {
      Serial.println("failed to open config file for writing");
    }
    #ifdef DEBUG
    json.printTo(Serial);
    #endif
    json.printTo(configFile);
    configFile.close();
    //end save
  }
  #endif
  // OTA Setup begin
  // Setup ESP OTA upgrade capaility
  WiFi.mode(WIFI_STA);
  // Port defaults to 8266
  ArduinoOTA.setPort(8266);
  // Hostname defaults to esp8266-[ChipID]
  ArduinoOTA.setHostname("EleksTube");
  // No authentication by default
  // ArduinoOTA.setPassword("admin");
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
  #ifdef DEBUG
  Serial.println("Ready");
  Serial.print("IP address: ");
  Serial.println(WiFi.localIP());
  #endif;
  // ESP OTA Setup end
  #ifdef BLYNK_APP
  Blynk.config(auth);
  #endif
  ntpClient.begin();            // Start NTP client
  delay(5000);                  // wait 5 seconds to start ntpClient
  
  getIPtz();                    // get ip-api.com Timezone data for your location
  getOffset();                  // get timezonedb.com UTC offset, DST start and end time and DST status
  
  setSyncProvider(getNTPTime);  // Specify function to sync time_t with NTP
  setSyncInterval(600);         // Set time_t sync interval - 10 minutes
  
  time_t getNTPTime();          // get current NTP time
  if(timeStatus()==timeNotSet) // test for time_t set
     {
     #ifdef DEBUG
     Serial.println("Unable to sync with NTP");
     #endif;
     } else
     {
     #ifdef DEBUG
     Serial.print("NTP has set the system time to: "); 
     Serial.print(hour()); Serial.print(":"); Serial.print(minute()); Serial.print(":"); Serial.print(second()); Serial.print(" UTC Offset:"), Serial.print(offset), Serial.print(" DST:"); Serial.print(DST); Serial.print(" Epoch:"); Serial.println(now());;
     #endif;
     }
}

void loop()
{
  ArduinoOTA.handle();   // Required to handle OTA request via network
  // handle DST time changes by getting new offset 
  if ((DST == 1 && now() >= DST_Start) || (DST == 0 && now() >= DST_End)) // DST has changed - get new UTC offset
  {
    #ifdef DEBUG
    Serial.println("Resetting timezone offset due to DST change");
    #endif;
    getIPtz();                    // get ip-api.com Timezone data for your location
    getOffset();                  // get timezonedb.com UTC offset
    UpdateRTC();                  // update clock RTC
    delay(2000);                  // wait for EleksTube to update RTC
  }
  Cur_time = now();   // set current time
  DisplayOn = 0;
  while( Cur_time == now())
  {
    #ifdef BLYNK_APP
    Blynk.run();    // Blynk check command
    #endif
  }    // only update clock on second change - wait here until seconds change
  if ( D_off > D_on && (D_off >= elapsedSecsToday(now()) && elapsedSecsToday(now()) >= D_on))
  {
    #ifdef DEBUG
    Serial.println(String(D_off) + " > " + String(D_on) + " , " +  String(D_off) + " >= " + elapsedSecsToday(now()) + " ," + elapsedSecsToday(now()) + " >= " + String(D_on));
    #endif;
    DisplayOn = 1;
  }
  if ( D_off < D_on && (D_on <= elapsedSecsToday(now()) || elapsedSecsToday(now()) <= D_off))
  {
    #ifdef DEBUG
    Serial.println(String(D_off) + " < " + String(D_on) + " , " +  String(D_on) + " <= " + elapsedSecsToday(now()) + " ," + elapsedSecsToday(now()) + " <= " + String(D_off)); 
    #endif;
    DisplayOn = 1;
  }  
    if ( DisplayOn == 1 )    // if the display on (D_on) is less than elapsed seconds or display on (D_on) is less than elepased seconds then display time
  {
    if ( S_NTP == 1 && W_RTC == 0)        // display time from NTP if NTP is selected
    {
      if(D_Date_second == second())
      {
        D_Date_second_end = elapsedSecsToday(now() + D_Date_offset);
      }
      if (elapsedSecsToday(now()) <= D_Date_second_end && D_Date == 1)    // display date if seconds are between D_Date_second and D_Date_second_end
      {
        splitDate();
        EleksSerialOut(EleksTube_realtime, mnth_10, Date_LED_Colour[0], mnth_1, Date_LED_Colour[1], day_10, Date_LED_Colour[2], day_1, Date_LED_Colour[3], year_10, Date_LED_Colour[4], year_1, Date_LED_Colour[5]);
      } else
      {
        splitTime();                      // split the time into separate digits to display
        // output time to EleksTube via EleksTube serial output
        EleksSerialOut(EleksTube_realtime, h_10, LED_Colour[0], h_1, LED_Colour[1], m_10, LED_Colour[2], m_1, LED_Colour[3], s_10, LED_Colour[4], s_1, LED_Colour[5]);     // Update RTC clock on EleksTude clock
      }
    }
  } else
  {   // black out tube if between D_off and D_on
    EleksSerialOut(EleksTube_realtime, h_10, LED_black, h_1, LED_black, m_10, LED_black, m_1, LED_black, s_10, LED_black, s_1, LED_black);     // Update RTC clock on EleksTude clock
  }
  // Print time on USB serial for reference
  #ifdef DEBUG
  String serial_out = (String(h_10) + String(h_1) + ":" + String(m_10) + String(m_1) + ":" + String(s_10) + String(s_1) + " DST:" + String(DST) + " Epoch:" + String(now()));
  Serial.println(serial_out + " V0: " + String(S_NTP) + " ElepasedSeconds: " + elapsedSecsToday(now()) + " D_on: " + String(D_on) + " D_off: " + String(D_off) + " DisplayOn: " + DisplayOn);
  #endif;
  //delay(100);   // delay for the epoch time to advance otherwise we miss seconds
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

void getOffset() {
 
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
  return ntpClient.getEpochTime() + offset;
  Serial.println( ntpClient.getEpochTime() + offset);
}

void configModeCallback (WiFiManager *myWiFiManager) {
  IPAddress APIP = WiFi.softAPIP();       // get the AP IP address
  int x = 0;
  while(x < 4)                            // Display the octects of the AP IP address on the EleksTube
  {
  Serial.println(APIP[x]);
  int result;
  // Display the octects of the AP IP address on the left most 3 digits of the EleksTube display
  String clock_out = (String(EleksTube_realtime) + String((APIP[x] / 100)) + "FFFFFF" + String(((APIP[x] / 10) % 10)) + "FFFFFF" + String((APIP[x] % 10)) + "FFFFFF000000000000000000000");
  EleksTube.print(clock_out);
  delay(4000);                      // delay for four seconds so the user can note the IP address
  x++;
  }
  Serial.println(WiFi.softAPIP());    // display the IP address on the serial port as well
  Serial.println(myWiFiManager->getConfigPortalSSID());   // display the AP SSID on the serial port
}
void splitTime(){
// Correct time -  split time into individual digits and add leading zeros to hours, minutes and seconds as required
  if(hr_1224){
    clk_hour = hour();
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
  if((year() % 100) <= 9)   // second is less than 9, add leading zero
  {
    year_10 = 0;             // set leading digit to zero
    year_1 = year() % 100 ;       // set least significant digit to variable
  } else {
    year_1 = (year() % 100) % 10;    // get modulus and set least significant digit to result
    year_10 = (year() % 100) / 10;  // divide by 10 to get the most significant digit
  }
}
void UpdateRTC()
{
    getNTPTime();                 // update NTP time
    splitTime();                  // split the hour, minutes and seconds into individual digits for EleksTube
    EleksSerialOut(EleksTube_update, h_10, LED_Colour[0], h_1, LED_Colour[1], m_10, LED_Colour[2], m_1, LED_Colour[3], s_10, LED_Colour[4], s_1, LED_Colour[5]);     // Update RTC clock on EleksTude clock
}
void EleksSerialOut(const char header[2], int d_1, const char d_1_color[], int d_2, const char d_2_color[], int d_3, const char d_3_color[], int d_4, const char d_4_color[], int d_5, const char d_5_color[], int d_6, const char d_6_color[])
{
  // Output time to EleksTube according to ElekMaker published API
  // /(header)1(first tube number)66CCFF(Standard Sixteen Binary RGB color)166CCFF(second tube number and color).....
  String clock_out = (String(header) + String(d_1) + String(d_1_color) + String(d_2) + String(d_2_color) + String(d_3) + String(d_3_color) + String(d_4) + String(d_4_color) + String(d_5) + String(d_5_color) + String(d_6) + String(d_6_color));
  EleksTube.print(clock_out);
  }
