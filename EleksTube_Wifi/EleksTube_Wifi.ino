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

#include <NTPClient.h>                  // NTP time client - https://github.com/arduino-libraries/NTPClient
#include <ESP8266WiFi.h>                // ESP8266 WiFi library - https://github.com/esp8266/Arduino
#include <WiFiManager.h>                // WiFi Manager to handle networking - https://github.com/tzapu/WiFiManager
#include <WiFiUdp.h>                    // UDP library to transport NTP data - https://www.arduino.cc/en/Reference/WiFi
#include <ArduinoJson.h>                // json library for parsing http results - https://arduinojson.org/?utm_source=meta&utm_medium=library.properties
#include <ESP8266HTTPClient.h>          // ESP8266 http library - https://github.com/esp8266/Arduino/blob/master/libraries/ESP8266HTTPClient/keywords.txt
#include <TimeLib.h>                    // Advanced time functions. - https://github.com/PaulStoffregen/Time
#include <SoftwareSerial.h>             // SoftwareSerial to add another serial port for clock output - https://www.arduino.cc/en/Reference/SoftwareSerial
#include <ArduinoOTA.h>                 // OTA library to support Over The Air upgrade of sketch - https://github.com/esp8266/Arduino/tree/master/libraries/ArduinoOTA 
#define KEY "XXXXXXXXXXXX"              // timezonedb.com API Key - Get your API key at http://timezonedb.com


char urlData[180];                      // array for holding complied query URL
int  offset;                            // local UTC offset in seconds
int  DST;                               // What is the current DST status, if 1 then DST is enabled if 0 DST is disabled - returned from TIMEZONEDB
int  DST_Start;                         // epoch when DST starts for given timezone - returned from timezonedb
int  DST_End;                           // epoch when DST ends for given timezond - returned from timezonedb
int  Cur_time;                          // current time - epoch
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

const char LED_Colour[] = "402000";     // Colour of LEDs - 042000 represents an orange colour similar to a Neon Nixie Tube
const char LED_black[] = "000000";      // All black
const char EleksTube_realtime[2] = "/";       // EleksTube API - realtime header value
const char EleksTube_update[2] = "*";         // EleksTube API - update time header value
const char EleksTube_reset[6] = "$00";        // EleksTube API - mode change 
const char EleksTube_alldigit[2] = "#";        // EleksTube API - flash through all digits

WiFiUDP ntpUDP;                                 //initialise UDP NTP
NTPClient ntpClient(ntpUDP, "time.google.com"); // initialist NTP client with server name 

HTTPClient http;                                // Initialise HTTP Client

SoftwareSerial EleksTube(5, 16);      // Second serial port for transmitting clock commands
                                      //receive pin 5 - D1, transmit 16 - D0

void setup()
{
  Serial.begin(115200);               // USB serial port
  EleksTube.begin(115200);            // EleksTube clock serial port
  delay(5000);                        // delay 5 seconds for the EleksTube to boot
  WiFiManager wifiManager;            // initialze wifiManager
  wifiManager.setAPCallback(configModeCallback);  // if WiFi fails display AP IP address on clock
  wifiManager.autoConnect("EleksTube");   // configuration for the access point - this is the SSID that is displayed
  
  Serial.println("WiFi Client connected!");

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
  ArduinoOTA.setPasswordHash("Your_Hashed_Password_Here");
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
  Serial.println("Ready");
  Serial.print("IP address: ");
  Serial.println(WiFi.localIP());
  // ESP OTA Setup end
    
  ntpClient.begin();            // Start NTP client
  delay(5000);                  // wait 5 seconds to start ntpClient
  
  getIPtz();                    // get ip-api.com Timezone data for your location
  getOffset();                  // get timezonedb.com UTC offset, DST start and end time and DST status
  
  setSyncProvider(getNTPTime);  // Specify function to sync time_t with NTP
  setSyncInterval(600);         // Set time_t sync interval - 10 minutes
  
  time_t getNTPTime();          // get current NTP time
  if(timeStatus()==timeNotSet) // test for time_t set
     Serial.println("Unable to sync with NTP");
  else
     Serial.print("NTP has set the system time to: "); Serial.print(hour()); Serial.print(":"); Serial.print(minute()); Serial.print(":"); Serial.print(second()); Serial.print(" UTC Offset:"), Serial.print(offset), Serial.print(" DST:"); Serial.print(DST); Serial.print(" Epoch:"); Serial.println(now());;
     Serial.println("Updated EleksTube onboard RTC time to NTP time");
     splitTime();                           // split the hour, minutes and seconds into individual digits for EleksTube
     EleksTube.print(EleksTube_reset);    // Reset display
     delay(1000);
     EleksSerialOut(EleksTube_update, h_10, LED_Colour, h_1, LED_Colour, m_10, LED_Colour, m_1, LED_Colour, s_10, LED_Colour, s_1, LED_Colour);     // Update RTC clock on EleksTude clock
     delay(2000);                         // wait for EleksTube to update RTC
}

void loop()
{
  ArduinoOTA.handle();   // Required to handle OTA request via network
  // handle DST time changes by getting new offset 
  if ((DST == 1 && now() >= DST_End) || (DST == 0 && now() >= DST_Start)) // DST has changed - get new UTC offset
  {
    Serial.println("Resetting timezone offset due to DST change");
    getIPtz();                    // get ip-api.com Timezone data for your location
    getOffset();                  // get timezonedb.com UTC offset
    getNTPTime();                 // update NTP time
    splitTime();                  // split the hour, minutes and seconds into individual digits for EleksTube
    EleksTube.print(EleksTube_reset);    // Reset display
    EleksSerialOut(EleksTube_update, h_10, LED_Colour, h_1, LED_Colour, m_10, LED_Colour, m_1, LED_Colour, s_10, LED_Colour, s_1, LED_Colour);     // Update RTC clock on EleksTude clock
    delay(2000);                          // wait for EleksTube to update RTC
  }
  Cur_time = now();   // set current time
  while( Cur_time == now()){ }    // only update clock on second change - wait here until seconds change
  if (second() >= 50 && second() <= 55)
  {
    splitDate();
    if(second() == 50)
    {
      //EleksSerialOut(EleksTube_realtime, 0, LED_black, 0, LED_black, 0, LED_black, 0, LED_black, 0, LED_black, 0, LED_black);
      EleksTube.print(EleksTube_alldigit); // Flash all digits
      EleksTube.print(EleksTube_reset);
      //delay(200);
    } else
    {
      EleksSerialOut(EleksTube_realtime, mnth_10, LED_Colour, mnth_1, LED_Colour, day_10, LED_Colour, day_1, LED_Colour, year_10, LED_Colour, year_1, LED_Colour);
    }
  } else
  {
    splitTime();                      // split the time into separate digits to display
    // output time to EleksTube via EleksTube serial output
    EleksSerialOut(EleksTube_realtime, h_10, LED_Colour, h_1, LED_Colour, m_10, LED_Colour, m_1, LED_Colour, s_10, LED_Colour, s_1, LED_Colour);     // Update RTC clock on EleksTude clock
  }
  // Print time on USB serial for reference
  String serial_out = (String(h_10) + String(h_1) + ":" + String(m_10) + String(m_1) + ":" + String(s_10) + String(s_1) + " DST:" + String(DST) + " Epoch:" + String(now()));
  Serial.println(serial_out);
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
  strcat (urlData, KEY);                                      // api key
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
  EleksTube.print(EleksTube_reset);       // reset display
  delay(1000);                            // wait for EleksTube to process
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
  EleksTube.print(EleksTube_reset); // put the EleksTube to a known state
  Serial.println(WiFi.softAPIP());    // display the IP address on the serial port as well
  Serial.println(myWiFiManager->getConfigPortalSSID());   // display the AP SSID on the serial port
}
void splitTime(){
// Correct time -  split time into individual digits and add leading zeros to hours, minutes and seconds as required
  if(hour() <= 9)   // hour is lss then 9 add zero
  {
    h_10 = 0;       // set leading digit to zero
    h_1 = hour();   // set least significant digit to variable
  } else {
    h_1 = hour() % 10;    // get modulus and set least significant digit to result
    h_10 = hour() / 10;  // divide by 10 to get the most significant digit
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
void EleksSerialOut(const char header[2], int d_1, const char d_1_color[], int d_2, const char d_2_color[], int d_3, const char d_3_color[], int d_4, const char d_4_color[], int d_5, const char d_5_color[], int d_6, const char d_6_color[])
{
  // Output time to EleksTube according to ElekMaker published API
  // /(header)1(first tube number)66CCFF(Standard Sixteen Binary RGB color)166CCFF(second tube number and color).....
  String clock_out = (String(header) + String(d_1) + String(d_1_color) + String(d_2) + String(d_2_color) + String(d_3) + String(d_3_color) + String(d_4) + String(d_4_color) + String(d_5) + String(d_5_color) + String(d_6) + String(d_6_color));
  EleksTube.print(clock_out);
  }