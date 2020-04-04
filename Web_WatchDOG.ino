/*
*   WEB_WtachDOG - use for watching the Internet BOX. Can ba adapt for Serveur ( need to connect the reset swicth)
*   Ping a remote host on the WEB (twice) if no result cut the power supply for x times
*   the ESP8266 is monitor by a DS1232 via D7, if there's not Pulse the ds1232 Reset the ESP8266
*   
*   some data are store in the EEPROM ( ssid+password+remotehost_1+remotehost_2 )
*   
*   BLUE LED  -> Wifi searching : blinking every second  
*   RED LED   ->  
*   GREEN LED ->
*   
*   BIP       ->
*   
*   version 1.0 by SHM
*   date : 2020-04-05
*/


#include <Wire.h>                                                     // Only needed for Arduino 1.6.5 and earlier
#include <ESP8266WiFi.h>                                              // Library WIFI for ESP8266
#include <ESP8266Ping.h>                                              // Library PING For ESP 8266 https://github.com/dancol90/ESP8266Ping
#include <EEPROM.h>                                                   // Library for EEPROM ( store data )
#include "SSD1306Wire.h"                                              // Use for OLED SSD1306
#include "credentials.h"                                              // credentials by default ( my LAB credentails for this dev )

struct config{
  char magic_key[6];                                                  // magic_key is use for the valide struct memory MUST BE EGAL to WWCHD
  char ssid[25];                                                      // ssid limitation 20 caracteres
  char password[25];                                                  // password limitation 20 caracteres
  char remote_1[25];                                                  // 1st remote host
  char remote_2[25];                                                  // 2nd remote host
}myConfig;  

int eepromAdress=0;                                                   // pointer of the EEPROM Memory
char ReadData[125];                                                   // use for Serial Communication

#define LED_RED D6                                                    // define LED RED to D6 pin
#define LED_GREEN D3                                                  // define LED GREEN to D3 pin
#define LED_BLUE D4                                                   // define LED BLUE to D4 pin  
#define DS1232_STPulse D7                                             // define Pulse to D7  
#define RELAY D5                                                      // define Relay to D5 
#define BIP D0                                                        // define Buzzer to D0

const char* remote_host_1 = "google.com";                             // define 1st Remote host by default  
const char* remote_host_2 = "lwspanel.fr";                            // define 2nd remote host by default

unsigned long previousMillis=0 ;
unsigned long interval = 1500000L;                                    // 15 minutes Delay


SSD1306Wire display(0x3c, SDA, SCL);                                  // ADDRESS, SDA, SCL  -  SDA and SCL usually populate automatically based on your board's pins_arduino.h

void CheckWIFI(){                                                     // Check Wifi, if not reconnect , trying 120 connection, if NOT RESET ESP8266.
  uint8_t Nombre_Tentative;
  Nombre_Tentative=0;
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);                                                       // just a timing 
    Nombre_Tentative++;                                               // increase the attempt
    Blink(1,500);                                                     // Blink BLUE LED
    if (Nombre_Tentative>120) {ESP.restart();}                        // attempt > 120 -> RESET ESP8266
    Serial.print(".");
    }
}

void Blink(int Number, int Timing)                                    // Blinking Function MUST be CHANGE for Multiple LED
{
  for (int i=0; i<=Number; i++)
  {
    digitalWrite(LED_RED, !digitalRead(LED_RED));                     //switch led
    delay(Timing);                                                    // Delay
  }
  digitalWrite(LED_RED,LOW);                                          // Put Down to the PIN ( Swich OFF the LED )    
}


boolean RemotePing(){                                                 // Function : ping remote host 1 & 2
  Serial.print("Pinging host : ");
  Serial.println(remote_host_1);
  if(Ping.ping(remote_host_1)) {                                      // ping Remote Host 1
    Serial.println("remote_host_1 - Success!!");
    return true;                                                      // If Succes OK the connection is GOOD Return OK    
  } else {                                                            // If Not
    Serial.println("remote_host_1 - Error :(");         
    Serial.print("Pinging host : ");
    Serial.println(remote_host_2);                                    // Ping Remote Host 2 for confirmation
    if(Ping.ping(remote_host_2)) {    
        Serial.println("remote_host_2 - Success!!");
        return true;                                                  // OK return TRUE
    } else { 
      Serial.println("remote_host_2 - Error :(");                     // else NO Connection Return FALSE 
      return false;}
  }
}

void ReadEEPROM(){
   Serial.println("key      : "+String(myConfig.magic_key));
   Serial.println("ssid     : "+String(myConfig.ssid));
   Serial.println("password : "+String(myConfig.password));
   Serial.println("Remote 1 : "+String(myConfig.remote_1));
   Serial.println("Remote 2 : "+String(myConfig.remote_2));
//   WIFI_SSID=myConfig.ssid;
//   WIFI_PASS=myConfig.password;
//   remote_host_1=myConfig.remote_1;
//   remote_host_2=myConfig.remote_2;  
}

void DisplayMessage(int x,int y, String Message){                     // Function to display text at x,y
   display.clear();
   display.setFont(ArialMT_Plain_16);                                 // Set the police
   display.drawString(0, 0, "    WATCHDOG  ");
   display.setFont(ArialMT_Plain_16);
   display.drawString(x, y, Message);                                 // display message
   display.display();                                                 // Write Memory to Screen
   delay(1000);
}

void setup() {
   analogWrite(DS1232_STPulse,100);                                   // Start the Pulse for DS1232 (WatchDOG)
   Serial.begin(115200);                                              // Start the Serial port of the ESP8266
   Serial.println();   
   Serial.println("-------------- START --------------------");
   Serial.println(" Version 1.0 - Date : 2020-04-05 by SHM  ");
   Serial.println(); 
   Serial.println("Init Screen");
   display.init();                                                    // Init Display OLED 0.96 SSD1306
   display.clear();                                                   // Clear Display
   display.flipScreenVertically();                                    // Rotate Screen See hardware
   display.setTextAlignment(TEXT_ALIGN_LEFT);                         // Format TEXT          
   EEPROM.begin(128);                                                 // define the size of the memory in EEPROM
   Serial.println("EEPROM Size : "+String(EEPROM.length()));
   DisplayMessage(0,30,"Init EEPROM");
   EEPROM.get(eepromAdress,myConfig);                                 // Read EEPROM
   if (strcmp(myConfig.magic_key,"WWCHD")==0){ReadEEPROM();}          // if the struct is valid, ReadEEPROM 
   Serial.println("Initialise the PIN");                              // init Pin OUT
   DisplayMessage(0,30,"Init Pin OUT");
   pinMode(LED_RED,OUTPUT);
   pinMode(LED_GREEN,OUTPUT);
   pinMode(LED_BLUE,OUTPUT);
   pinMode(RELAY,OUTPUT);
   pinMode(BIP,OUTPUT);
   digitalWrite(LED_RED,LOW);                                         // Put LOW Signal
   digitalWrite(LED_GREEN,LOW);
   digitalWrite(LED_BLUE,LOW);
   digitalWrite(RELAY,LOW);
   digitalWrite(BIP,LOW);
   Serial.println("Init the Wifi Connection.");    
   DisplayMessage(0,30,"Init WIFI");
   WiFi.begin(WIFI_SSID, WIFI_PASS);                                  // Init Wifi connection
   CheckWIFI();
   Serial.println("");
   Serial.println("WiFi connected");
   Serial.print("IP address: ");
   Serial.println(WiFi.localIP());
   DisplayMessage(12,30,WiFi.localIP().toString());                    // display the IP
   digitalWrite(BIP, HIGH);                                           // BEEP After the END od Setup
   delay(2000);                                                       // Waiting Time for the BEEP
   digitalWrite(BIP, LOW);                                            // Stop BEEP
   Serial.println("- End of SETUP - ");
}

void loop() {

      
 if( millis() - previousMillis >= interval) {                          // timing is > interval (15 minutes) -> action
    DisplayMessage(5,30,"Action !"); 
    DisplayMessage(5,30,"         ");
    if (RemotePing()==true ){                                         // start the Remote Ping 
        DisplayMessage(5,30,"CNX : OK  ");                            // If True CNX OK
       } else {                                                       // If NOT
        DisplayMessage(5,30,"CNX : NOK  ");                           // Need to RESTART put D5 DOWN
        digitalWrite(RELAY, HIGH);                                    // Relay Actif for 2 seconds
        delay(2000);                                                  // Waiting Time for the Relay
        digitalWrite(RELAY, LOW);
        DisplayMessage(5,30,"Reboot BOX...");
       }
    previousMillis = millis();   
    }

// scan for Serial Communication
// WM-ssid,password,remote_host_1,remote_host_2
// RM
// help
// Version
 

while (Serial.available())
  {
    size_t byteCount=Serial.readBytesUntil('\n',ReadData,sizeof(ReadData)-1); // Store data in ReadData until a \n appear and linmit to size of ReadData
    ReadData[byteCount]=NULL;                                         // Store a NULL at the end of ReadData
    Serial.print("ReadData:");
    Serial.println(ReadData);
    char *keywordPointer=strstr(ReadData,"-");
    if (keywordPointer!=NULL) {                                                       // with Parameter ( detection of "-" )
      // Avec parametre
      Serial.println("Avec parametres");
      int dataPos=(keywordPointer-ReadData+1);                                        // pointer after "-"
      char Parameter[4][25];
      int CountParameter=0;
      char *token=strtok(&ReadData[dataPos],",");                                     // Pointer to the First Parameter
      if (token!=NULL && strlen(token)< sizeof(Parameter[0])) {                       // Verify Token and Size
        strncpy(Parameter[0],token,sizeof(Parameter[0]));                             // Copy 1st Parameter to Parameter[0]
      }
      for (int i=1;i<4;i++) {                                                         // next 3 parameters
        token=strtok(NULL,",");
        if (token!=NULL && strlen(token)< sizeof(Parameter[i])) {
          strncpy(Parameter[i],token,sizeof(Parameter[i]));
        }
      }                                                                               // All parameter are store in PARAMETER[] 
      strcpy(myConfig.magic_key,"WWCHD");                                             // Store Value in MyConfig    
      strcpy(myConfig.ssid,Parameter[0]);
      strcpy(myConfig.password,Parameter[1]);
      strcpy(myConfig.remote_1,Parameter[2]);
      strcpy(myConfig.remote_2,Parameter[3]);
      EEPROM.put(eepromAdress,myConfig);                                              // Flash EEPROM at eepromAdress
      EEPROM.commit();                                                                // Commit ! 
      ReadEEPROM();                                                                   // Read the EEPROM and affect the value to variable ! NEED to restart for WIFI Connection
      
    }else{                                                                            // No Parameter ( no detection of "-" )
      if (strcmp(ReadData,"Version")==0) { Serial.println("Version = 1.0.0");}        // return the version of the software
      else if (strcmp(ReadData,"Help")==0) { Serial.println("Help - ?");}             // return the help Command 
      else if (strcmp(ReadData,"RM")==0) { Serial.println("Read Memory !");ReadEEPROM();}          // return the MEMORY
      else if (strcmp(ReadData,"Reset")==0) { Serial.println("Reset !");ESP.restart();}          // return the MEMORY
    }



    
  }

//stringRead.replace("\n"," ");
//stringRead.replace("\r"," ");


}


 
void HELP(){
  Serial.println();
  Serial.println("--- HELP ---");
  Serial.println("- Command  -");
  Serial.println();
      
}