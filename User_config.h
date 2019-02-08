#ifndef _USERCONFIG_H_
#define _USERCONFIG_H_

/*  
  OpenMQTTGateway  - ESP8266 or Arduino program for home automation 

   Act as a wifi or ethernet gateway between your 433mhz/infrared IR signal  and a MQTT broker 
   Send and receiving command by MQTT
 
  This program enables to:
 - receive MQTT data from a topic and send signals corresponding to the received MQTT data
 - publish MQTT data to a different topic related to received signals
  
    Copyright: (c)Florian ROBERT
  
    This file is part of OpenMQTTGateway.
    
    OpenMQTTGateway is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.

    OpenMQTTGateway is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/
/*-------------------VERSION----------------------*/
#define OMG_VERSION "0.9"

/*-------------CONFIGURE WIFIMANAGER-------------(only ESP8266 & SONOFF RFBridge)*/
/*
 * The following parameters are set during the WifiManager setup process:
 * - wifi_ssid
 * - wifi_password
 * - mqtt_user
 * - mqtt_pass
 * - mqtt_server
 * - mqtt_port
 * 
 * To completely disable WifiManager, define ESPWifiManualSetup.
 * If you do so, please don't forget to set these variables before compiling
 * 
 * Otherwise you can provide these credentials on the web interface after connecting 
 * to the access point with your password (SSID: WifiManager_ssid, password: WifiManager_password)
 */

/*-------------DEFINE YOUR  NETWORK PARAMETERS BELOW----------------*/
#if defined(ESP8266)  // for nodemcu, weemos and esp8266
  //#define ESPWifiManualSetup true //uncomment you don't want to use wifimanager for your credential settings on ESP
#endif

#define WifiManager_password "19821985" //this is going to be the WPA2-PSK password for the initial setup access point 
#define WifiManager_ssid "mtn-volmart" //this is the network name of the initial setup access point
#define WifiManager_ConfigPortalTimeOut 120


#define Gateway_Name "OpenMQTTGateway"

//uncomment the line below to integrate msg value into the subject when receiving
//#define valueAsASubject true

/*-------------DEFINE THE MODULES YOU WANT BELOW----------------*/
//Addons and module management, comment the Z line

//#define ZgatewayRF     "RF"       //ESP8266, Arduino, ESP32
//#define ZgatewayRF315  "RF315"    //ESP8266, Arduino, ESP32
//#define ZgatewayIR     "IR"       //ESP8266, Arduino,         Sonoff RF Bridge
#define ZgatewayPilight "Pilight" //ESP8266, Arduino, ESP32
//#define ZgatewayBT     "BT"       //ESP8266, Arduino, ESP32
//#define ZgatewayRF2    "RF2"        //ESP8266, Arduino, ESP32
//#define ZgatewaySRFB   "SRFB"     //                          Sonoff RF Bridge
//#define Zgateway2G     "2G"       //ESP8266, Arduino, ESP32
//#define ZgatewayRFM69  "RFM69"    //ESP8266, Arduino, ESP32
//#define ZactuatorONOFF "ONOFF"    //ESP8266, Arduino, ESP32,  Sonoff RF Bridge
//#define ZsensorINA226  "INA226"   //ESP8266, Arduino, ESP32
//#define ZsensorHCSR501 "HCSR501"  //ESP8266, Arduino, ESP32,  Sonoff RF Bridge
//#define ZsensorADC     "ADC"      //ESP8266, Arduino, ESP32
//#define ZsensorBH1750  "BH1750"   //ESP8266, Arduino, ESP32
//#define ZsensorTSL2561 "TSL2561"  //ESP8266, Arduino, ESP32
//#define ZsensorBME280  "BME280"   //ESP8266, Arduino, ESP32
//#define ZsensorDHT     "DHT"      //ESP8266, Arduino, ESP32,  Sonoff RF Bridge
//#define ZsensorGPIOKeyCode "GPIOKeyCode" //ESP8266, Arduino, ESP32
//#define ZsensorGPIOInput "GPIOInput" //ESP8266, Arduino, ESP32
//#define ZmqttDiscovery "HADiscovery"//ESP8266, Arduino, ESP32, Sonoff RF Bridge

/*-------------DEFINE YOUR ADVANCED NETWORK PARAMETERS BELOW----------------*/
//#define MDNS_SD //uncomment if you  want to use mdns for discovering automatically your ip server, please note that MDNS with ESP32 can cause the BLE to not work
#define maxMQTTretry 4 //maximum MQTT connection attempts before going to wifi setup

//set minimum quality of signal so it ignores AP's under that quality
#define MinimumWifiSignalQuality 8

/*-------------DEFINE YOUR MQTT ADVANCED PARAMETERS BELOW----------------*/
#define Base_Topic "home/"
#define version_Topic  Base_Topic Gateway_Name "/version"
#define will_Topic  Base_Topic Gateway_Name "/LWT"
#define will_QoS 0
#define will_Retain true
#define will_Message "offline"
#define Gateway_AnnouncementMsg "online"

#define jsonPublishing true //comment if you don't want to use Json  publishing  (one topic for all the parameters)
//example home/OpenMQTTGateway_ESP32_DEVKIT/BTtoMQTT/4XXXXXXXXXX4 {"rssi":-63,"servicedata":"fe0000000000000000000000000000000000000000"}
#define jsonReceiving true //comment if you don't want to use Json  reception analysis

//#define simplePublishing true //comment if you don't want to use simple publishing (one topic for one parameter)
//example 
// home/OpenMQTTGateway_ESP32_DEVKIT/BTtoMQTT/4XXXXXXXXXX4/rssi -63.0
// home/OpenMQTTGateway_ESP32_DEVKIT/BTtoMQTT/4XXXXXXXXXX4/servicedata fe0000000000000000000000000000000000000000
#define simpleReceiving true //comment if you don't want to use old way reception analysis

/*-------------DEFINE YOUR OTA PARAMETERS BELOW----------------*/
#define ota_hostname Gateway_Name
#define ota_password "OTAPASSWORD"
#define ota_port 8266

/*-------------DEFINE PINs FOR STATUS LEDs----------------*/
#define led_receive 40
#define led_send 42
#define led_error 44

//      VCC   ------------D|-----------/\/\/\/\ -----------------  Arduino PIN
//                        LED       Resistor 270-510R

/*----------------------------OTHER PARAMETERS-----------------------------*/
/*-------------------CHANGING THEM IS NOT COMPULSORY-----------------------*/
/*----------------------------USER PARAMETERS-----------------------------*/
#ifdef ZgatewaySRFB
  #define SERIAL_BAUD 19200
#else
  #define SERIAL_BAUD 115200
#endif
/*--------------MQTT general topics-----------------*/
// global MQTT subject listened by the gateway to execute commands (send RF, IR or others)
#define subjectMQTTtoX  Base_Topic Gateway_Name "/commands/#"
#define subjectMultiGTWKey "toMQTT"
#define subjectGTWSendKey "MQTTto"

//variables to avoid duplicates
#define time_avoid_duplicate 3000 // if you want to avoid duplicate mqtt message received set this to > 0, the value is the time in milliseconds during which we don't publish duplicates

//uncomment to use multicore function of ESP32 for BLE
#ifdef ESP32
  #define multiCore //comment to don't use multicore function of ESP32 for BLE
#endif

#define JSON_MSG_BUFFER 512 // Json message max buffer size, don't put 1024 or higher it is causing unexpected behaviour on ESP8266

#define TimeBetweenReadingSYS 120000 // time between system readings (like memory)
#define subjectSYStoMQTT  Base_Topic Gateway_Name "/SYStoMQTT"

/*-------------------ACTIVATE TRACES----------------------*/
#define TRACE 1  // commented =  trace off, uncommented = trace on

#endif