/*
    WEB_WtachDOG - use for watching the Internet BOX. Can ba adapt for Serveur ( need to connect the reset swicth)
    Ping a remote host on the WEB (twice) if no result cut the power supply for x times
    the ESP8266 is monitor by a DS1232 via D7, if there's not Pulse the ds1232 Reset the ESP8266

    some data are store in the EEPROM (Key+ssid+password+remotehost_1+remotehost_2 )

    BLUE LED  -> Wifi searching : blinking every second
    RED LED   -> not use
    GREEN LED -> not use
    RELAY     -> D8
    BIP       -> D0
    D5        -> Mode Config  No WIFI Connection / No RESTART if Wifi failed )

    version  by SHM (see variable )
    date : 2020-04-05
*/

// ===============================================================================================================================================
//                                                                   INCLUDE
// ===============================================================================================================================================
#include <Wire.h>                                                     // Only needed for Arduino 1.6.5 and earlier
#include <ESP8266WiFi.h>                                              // Library WIFI for ESP8266
#include <ESP8266Ping.h>                                              // Library PING For ESP 8266 https://github.com/dancol90/ESP8266Ping
#include <EEPROM.h>                                                   // Library for EEPROM ( store data )
#include <NTPClient.h>                                                // Library for NTP Time (https://github.com/arduino-libraries/NTPClient)
#include <WiFiUdp.h>                                                  // Need for NTP

#include "SSD1306Wire.h"                                              // Use for OLED SSD1306
#include "credentials.h"                                              // credentials by default ( my LAB credentails for this dev )
// ===============================================================================================================================================
//                                                                   DEFINE
// ===============================================================================================================================================
#define Version  "1.5c"
#define LED_RED D6                                                    // define LED RED to D6 pin
#define LED_GREEN D3                                                  // define LED GREEN to D3 pin
#define LED_BLUE D4                                                   // define LED BLUE to D4 pin  
#define DS1232_STPulse D7                                             // define Pulse to D7  
#define RELAY D8                                                      // define Relay to D8 - GPIO15 
#define BIP D0                                                        // define Buzzer to D0
#define PIN_CONFIG D5                                                 // define INTER to D5
// ===============================================================================================================================================
//                                                                   VARIABLE
// ===============================================================================================================================================
struct config {
  char magic_key[6];                                                  // magic_key is use for the valide struct memory MUST BE EGAL to WWCHD
  char ssid[25];                                                      // ssid limitation 20 caracteres
  char password[25];                                                  // password limitation 20 caracteres
  char remote_1[25];                                                  // 1st remote host
  char remote_2[25];                                                  // 2nd remote host
  char ntpServer[30];                                                 // NTP Serveur or Pool ex: europe.pool.ntp.org
  unsigned long interval;                                             // Interval for scanning
} myConfig;

int eepromAdress = 0;                                                 // pointer of the EEPROM Memory
char ReadData[125];                                                   // use for Serial Communication
char* remote_host_1 = "google.com";                                   // define 1st Remote host by default
char* remote_host_2 = "lwspanel.fr";                                  // define 2nd remote host by default
unsigned long previousMillis = 0 ;
unsigned long interval = 500000L;                                     // 5 minutes Delay
boolean ConfigMode;                                                   // detection for Config or Production

SSD1306Wire display(0x3c, SDA, SCL);                                  // ADDRESS, SDA, SCL  -  SDA and SCL usually populate automatically based on your board's pins_arduino.h
WiFiUDP ntpUDP;                                                       // Need for NTPClient
NTPClient timeClient(ntpUDP, "europe.pool.ntp.org", 7200, 60000);     // NTP Object set the Offset (summer:7200/winter:3600)
String sTime;                                                         // Time String ( use in CMD_TIME )
int Jour;                                                             // use to store the day ( NTPClient )
char sDate[][10] = {                                                  // Stucture of Date + JOUR for Index
  "Dimanche",
  "Lundi",
  "Mardi",
  "Mercredi",
  "Jeudi",
  "Vendredi",
  "Samedi"
};
// ===============================================================================================================================================
//                                                                   CHECK WIFI
// ===============================================================================================================================================
void CheckWIFI() {                                                    // Check Wifi, if not reconnect , trying 120 connection, if NOT RESET ESP8266.
  if (ConfigMode == 1) {return;}                                      // Check if CONFIG_MODE if Config mode return don't check wifi ( use for parameter )
  uint8_t Nombre_Tentative;
  Nombre_Tentative = 0;
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);                                                       // just a timing
    Nombre_Tentative++;                                               // increase the attempt
    Blink(1, 500);                                                    // Blink BLUE LED
    if (Nombre_Tentative > 120) {
      ESP.restart(); // attempt > 120 -> RESET ESP8266
    }
    Serial.print(".");
  }
}
// ===============================================================================================================================================
//                                                                   BLINK
// ===============================================================================================================================================
void Blink(int Number, int Timing)                                    // Blinking Function MUST be CHANGE for Multiple LED
{
  for (int i = 0; i <= Number; i++)
  {
    digitalWrite(LED_RED, !digitalRead(LED_RED));                     //switch led
    delay(Timing);                                                    // Delay
  }
  digitalWrite(LED_RED, LOW);                                         // Put Down to the PIN ( Swich OFF the LED )
}
// ===============================================================================================================================================
//                                                                   ALARM
// ===============================================================================================================================================
void Alarm() {                                                       // use for Reboot ALARM
  for (int i = 0; i <= 7; i++)                                       // must be odd ( impair )
  {
    digitalWrite(BIP, !digitalRead(BIP));                            // switch BIP
    delay(150);                                                      // timing
  }
}


// ===============================================================================================================================================
//                                                                   REMOTE PING
// ===============================================================================================================================================
boolean RemotePing() {                                                // Function : ping remote host 1 & 2
  Serial.print("Pinging host : ");
  Serial.println(remote_host_1);
  if (Ping.ping(remote_host_1)) {                                     // ping Remote Host 1
    Serial.println("remote_host_1 - Success!!");
    return true;                                                      // If Succes OK the connection is GOOD Return OK
  } else {                                                            // If Not
    Serial.println("remote_host_1 - Error :(");
    Serial.print("Pinging host : ");
    Serial.println(remote_host_2);                                    // Ping Remote Host 2 for confirmation
    if (Ping.ping(remote_host_2)) {
      Serial.println("remote_host_2 - Success!!");
      return true;                                                    // OK return TRUE
    } else {
      Serial.println("remote_host_2 - Error :(");                     // else NO Connection Return FALSE
      return false;
    }
  }
}
// ===============================================================================================================================================
//                                                                   READ EEPROM
// ===============================================================================================================================================
void ReadEEPROM() {
  Serial.println("key      : " + String(myConfig.magic_key));        // Key is use to control valid data
  Serial.println("ssid     : " + String(myConfig.ssid));             // ssid Wifi routeur
  Serial.println("password : " + String(myConfig.password));         // password of ssid
  Serial.println("Remote 1 : " + String(myConfig.remote_1));         // remote host N°1
  Serial.println("Remote 2 : " + String(myConfig.remote_2));         // remote host N°2
  Serial.println("ntpServer: " + String(myConfig.ntpServer));        // ntpServer
  Serial.println("interval : " + String(myConfig.interval));         // interval
   if (strcmp(myConfig.magic_key, "WWCHD") == 0) {                   // if Magickey valid fix memory->data 
        WIFI_SSID = myConfig.ssid;                                   
        WIFI_PASS = myConfig.password;
        remote_host_1 = myConfig.remote_1;
        remote_host_2 = myConfig.remote_2;
        timeClient.end();                                            // stop the NTP
        timeClient.setPoolServerName(myConfig.ntpServer);            // fix the ntp server if defined in memory
        timeClient.begin();                                          // restart the NTP 
        interval=myConfig.interval;
      }
}
// ===============================================================================================================================================
//                                                                   DISPLAY MESSAGE
// ===============================================================================================================================================
void DisplayMessage(int x, int y, String Message) {                  // Function to display text at x,y (x=64 always with TEXT ALIGN_CENTER)
  display.clear();
  display.setTextAlignment(TEXT_ALIGN_CENTER);   
  display.setFont(ArialMT_Plain_16);                                 // Set the police
  display.drawString(64, 0, "    WATCHDOG  ");
  display.setFont(ArialMT_Plain_16);
  display.drawString(64, y, Message);                                 // display message
  display.setFont(ArialMT_Plain_10);
  display.setTextAlignment(TEXT_ALIGN_LEFT);  
  display.drawString(0, 50, sDate[Jour]);                            // display date
  display.setTextAlignment(TEXT_ALIGN_RIGHT);  
  display.drawString(128, 50, sTime);                                // display time
  display.display();                                                 // Write Memory to Screen
  delay(1000);
}
// ===============================================================================================================================================
//                                                                   SETUP
// ===============================================================================================================================================
void setup() {
  pinMode(DS1232_STPulse, OUTPUT);                                   // define the pin mode for PWM 
  analogWrite(DS1232_STPulse, 100);                                  // Start the Pulse for DS1232 (WatchDOG)
  Serial.begin(115200);                                              // Start the Serial port of the ESP8266
  Serial.println();
  Serial.println("-------------- START --------------------");
  Serial.print(" Version "); Serial.print(Version); Serial.println(" - Date : 2020-04-05 by SHM  ");
  Serial.println();
  Serial.println("Init Screen");
  display.init();                                                    // Init Display OLED 0.96 SSD1306
  display.clear();                                                   // Clear Display
  display.flipScreenVertically();                                    // Rotate Screen See hardware
  display.setTextAlignment(TEXT_ALIGN_CENTER);                       // Format TEXT
  EEPROM.begin(255);                                                 // define the size of the memory in EEPROM
  Serial.println("EEPROM Size : " + String(EEPROM.length()));
  DisplayMessage(64, 25, "Init EEPROM");
  EEPROM.get(eepromAdress, myConfig);                                // Read EEPROM
  if (strcmp(myConfig.magic_key, "WWCHD") == 0) {
    ReadEEPROM();                                                    // if the struct is valid, ReadEEPROM
  }
  Serial.println("Initialise the PIN");                              // init Pin OUT
  DisplayMessage(64, 25, "Init Pin OUT");
  pinMode(LED_RED, OUTPUT);
  pinMode(LED_GREEN, OUTPUT);
  pinMode(LED_BLUE, OUTPUT);
  pinMode(RELAY, OUTPUT);
  pinMode(BIP, OUTPUT);
  pinMode(PIN_CONFIG, INPUT_PULLUP);
  digitalWrite(LED_RED, LOW);                                        // Put LOW Signal
  digitalWrite(LED_GREEN, LOW);
  digitalWrite(LED_BLUE, LOW);
  digitalWrite(RELAY, LOW);
  digitalWrite(BIP, LOW);
  ConfigMode = digitalRead(PIN_CONFIG);
  if (ConfigMode == 1) {
    Serial.print("Config: ");
    Serial.println(ConfigMode);
    DisplayMessage(64, 25, "MODE Config");
  }
  Serial.println("Init the Wifi Connection.");
  DisplayMessage(64, 25, "Init WIFI");
  WiFi.begin(WIFI_SSID, WIFI_PASS);                                  // Init Wifi connection
  CheckWIFI();                                                       // Check Wifi
  Serial.println("");                                                // Orint IP Adress
  Serial.println("WiFi connected");
  Serial.print("IP address: ");
  Serial.println(WiFi.localIP());
  DisplayMessage(64, 25, WiFi.localIP().toString());                 // display the IP
  digitalWrite(BIP, HIGH);                                           // BEEP After the END od Setup
  delay(2000);                                                       // Waiting Time for the BEEP
  digitalWrite(BIP, LOW);                                            // Stop BEEP
  timeClient.begin();                                                // Start NTP
  CMD_Time();                                                        // Read Date/time
  Serial.println("- End of SETUP - ");
}
// ===============================================================================================================================================
//                                                                   LOOP
// ===============================================================================================================================================
void loop() {
  if ( millis() - previousMillis >= interval) {                       // timing is > interval -> action
    CheckWIFI();                                                      // Check Wifi Connection before calling NTP
    if ( timeClient.update() == true) {                               // time is valid and connection is valid 
      sTime = timeClient.getFormattedTime().substring(0, 5);          // fix TIME
      DisplayMessage(64, 25, "Last Query : "+sTime);
      Serial.println(timeClient.getFormattedTime());
    } else {                                                          // Time is NOT Valid 2eme chech for connection : Ping Remote twice
      if (RemotePing() == true ) {                                    // start the Remote Ping
        DisplayMessage(64, 25, "PING : OK  ");                        // If True CNX OK
      } else {                                                        // If NOT
        DisplayMessage(64, 25, "PING : NOK  ");                       // Need to RESTART put RELAY ON
        DisplayMessage(64, 25, "Rebooting...");                         
        Alarm();                                                      // start ALARM  
        Serial.println("Rebooting ....");
        digitalWrite(RELAY, HIGH);                                    // Relay Actif for 2 seconds
        delay(5000);                                                  // Waiting Time for the Relay
        digitalWrite(RELAY, LOW);
        Serial.println("Wainiting until the Box Restart .... And Get Connection....");
        for (int boucle = 0; boucle < 120; boucle++) {                // Start waiting for 120s ( time is use for reboot box )
          delay(1000);
          Serial.println(String(boucle));
          DisplayMessage(64, 25, String(boucle));
        }
      }
    }
    previousMillis = millis();                                        // reset previousmillis
  }
  //
  // --------- Serial COMMAND to communicate to module BAUD 115200,8,N,1 see schematic for connection
  //
  while (Serial.available())
  {
    size_t byteCount = Serial.readBytesUntil('\n', ReadData, sizeof(ReadData) - 1); // Store data in ReadData until a \n appear and linmit to size of ReadData
    ReadData[byteCount] = NULL;                                       // Store a NULL at the end of ReadData
    //    Serial.print("ReadData:");
    //    Serial.println(ReadData);
    char *keywordPointer = strstr(ReadData, "-");
    if (keywordPointer != NULL) {                                     // Command with Parameter ( detection of "-" )
      Serial.println("Avec parametres");
      int dataPos = (keywordPointer - ReadData + 1);                  // pointer after "-"
      char Parameter[6][25];
      int CountParameter = 0;
      char *token = strtok(&ReadData[dataPos], ",");                  // Pointer to the First Parameter
      if (token != NULL && strlen(token) < sizeof(Parameter[0])) {    // Verify Token and Size
        strncpy(Parameter[0], token, sizeof(Parameter[0]));           // Copy 1st Parameter to Parameter[0]
      }
      for (int i = 1; i < 6; i++) {                                   // next 3 parameters
        token = strtok(NULL, ",");
        if (token != NULL && strlen(token) < sizeof(Parameter[i])) {
          strncpy(Parameter[i], token, sizeof(Parameter[i]));
        }
      }                                                               // All parameter are store in PARAMETER[]
      strcpy(myConfig.magic_key, "WWCHD");                            // Store Value in MyConfig
      strcpy(myConfig.ssid, Parameter[0]);
      strcpy(myConfig.password, Parameter[1]);
      strcpy(myConfig.remote_1, Parameter[2]);
      strcpy(myConfig.remote_2, Parameter[3]);
      strcpy(myConfig.ntpServer, Parameter[4]);
      myConfig.interval=atol(Parameter[5]);
      EEPROM.put(eepromAdress, myConfig);                             // Flash EEPROM at eepromAdress
      EEPROM.commit();                                                // Commit !
      ReadEEPROM();                                                   // Read the EEPROM and affect the value to variable ! NEED to restart for WIFI Connection
    } else {                                                                                                // No Parameter ( no detection of "-" )
      if (strcmp(ReadData, "Version") == 0) {CMD_Version();}                                                // return the version of the software
      else if (strcmp(ReadData, "Help") == 0) {CMD_Help();}                                                 // return the help Command
      else if (strcmp(ReadData, "RM") == 0) {ReadEEPROM();}                                                 // return the MEMORY
      else if (strcmp(ReadData, "Reset") == 0) {Serial.println("Reset !");ESP.restart();}                   // return the MEMORY
      else if (strcmp(ReadData, "Relay") == 0) {CMD_Relay();}                                               // Toggle the relay
      else if (strcmp(ReadData, "Beep") == 0) {CMD_Beep();}                                                 // Beep for 2s
      else if (strcmp(ReadData, "PulseOFF") == 0) {Serial.println("OK");analogWrite(DS1232_STPulse, 0);}    // turn Off Pulse , DS1232 MUST Reboot
      else if (strcmp(ReadData, "Time") == 0) {Serial.println("OK"); CMD_Time();}                           // Request Time NTP
      else if (strcmp(ReadData, "SimulReboot") == 0) {Serial.println("OK");SimulReboot();}                  // simul bad Remote Host to generate a reboot
      else if (strcmp(ReadData, "Config") == 0) {CMD_Config();}                                             // print Config
      else if (strcmp(ReadData, "SetInterval_1") == 0) {Serial.println("OK"); interval = 100000L;}          // set interval to 1 min
      else if (strcmp(ReadData, "RAZ")==0) {Serial.println("OK");RAZ();}                                     // RAZ Memory
    }
  }
}
// ===============================================================================================================================================
//                                                                   CMD_RAZ
// ===============================================================================================================================================
void RAZ(){
      strcpy(myConfig.magic_key, "#####");                           // Store Value in MyConfig
      strcpy(myConfig.ssid, "N/A");
      strcpy(myConfig.password,"N/A");
      strcpy(myConfig.remote_1,"N/A");
      strcpy(myConfig.remote_2,"N/A");
      strcpy(myConfig.ntpServer,"N/A");
      myConfig.interval=500000L;
      EEPROM.put(eepromAdress, myConfig);                            // Flash EEPROM at eepromAdress
      EEPROM.commit();                  
}

// ===============================================================================================================================================
//                                                                   CMD_SimulReboot
// ===============================================================================================================================================
void SimulReboot(){
  timeClient.end();                                                   // stop NTPClient  
  timeClient.setPoolServerName("");                                   // Fix NTP server to NULL
  timeClient.begin();                                                 // Start NTPClient
  remote_host_1 = "";                                                 // Fix remote_1 to NULL    
  remote_host_2 = "";                                                 // Fix remote_2 to NULL  
}
// ===============================================================================================================================================
//                                                                   CMD_Config
// ===============================================================================================================================================
void CMD_Config() {

  Serial.println("Config");                                           // print Config  
  Serial.print("SSID : ");  Serial.println(WIFI_SSID);
  Serial.print("PASS : "); Serial.println(WIFI_PASS);
  Serial.print("Host 1 : "); Serial.println(remote_host_1);
  Serial.print("Host 2 : "); Serial.println(remote_host_2);
  Serial.print("Interval : "); Serial.println(interval);
  Serial.print("Date : ");Serial.println(sDate[Jour]);
  Serial.print("Time : "); Serial.println(sTime);
}
// ===============================================================================================================================================
//                                                                   CMD_TIME
// ===============================================================================================================================================
void CMD_Time() {                                                   // fix time
  timeClient.forceUpdate();
  Serial.println(timeClient.getFormattedTime());
  sTime = timeClient.getFormattedTime().substring(0, 5);
  Jour=timeClient.getDay();                                         // get day ( number) 
  Serial.println(Jour);
  Serial.print(sDate[Jour]);                                        // use sDate of Jour  
  Serial.print(" - TIME : ");
  Serial.println(sTime);
  DisplayMessage(64,25,sTime);
}
// ===============================================================================================================================================
//                                                                   CMD_VERSION
// ===============================================================================================================================================
void CMD_Version() {
  Serial.print("Version : ");
  Serial.println(Version);
  DisplayMessage(64, 25, "Version :" + String(Version));
}
// ===============================================================================================================================================
//                                                                   CMD_BEEP
// ===============================================================================================================================================
void CMD_Beep() {
  DisplayMessage(64, 25, "Beep ON");
  digitalWrite(BIP, !digitalRead(BIP));
  delay(2000);
  DisplayMessage(64, 25, "Beep OFF");
  digitalWrite(BIP, !digitalRead(BIP));
}
// ===============================================================================================================================================
//                                                                   CMD_RELAY
// ===============================================================================================================================================
void CMD_Relay() {
  Serial.println("Relay !");
  DisplayMessage(64, 25, "Relay ON");
  digitalWrite(RELAY, !digitalRead(RELAY));
  delay(5000);
  DisplayMessage(64, 25, "Relay OFF");
  digitalWrite(RELAY, !digitalRead(RELAY));
}
// ===============================================================================================================================================
//                                                                   CMD_HELP
// ===============================================================================================================================================
void CMD_Help() {
  Serial.println();
  Serial.println("--- HELP ---");
  Serial.println(" ---------------------------  HELP  ---------------------------------");
  Serial.println("                        -*- Command  -*-");
  Serial.println(" Reset   -> Reset the ESP8266");
  Serial.println(" RM      -> Read MEMORY");
  Serial.println(" Relay   -> Toggle the Relay for 5s");
  Serial.println(" Version -> Print the software version");
  Serial.println(" WM-ssid,password,remote1,remote2,ntpServer,500000L      -> Write MEMORY with parameter (500000L=5min)");
  Serial.println(" Beep    -> Beep for 2s");
  Serial.println(" PulseOFF -> turn off the Pulse for DS1232");
  Serial.println(" Time    -> Request NTP Time");
  Serial.println(" SimulReboot -> Simul a bad Host name to generate a reboot");
  Serial.println(" Config    -> Print Config");
  Serial.println(" SetInterval_1 -> set interval to 1 minute");
  Serial.println(" RAZ -> RAZ Memory ");
  Serial.println(" --------------------------------------------------------------------");
}
