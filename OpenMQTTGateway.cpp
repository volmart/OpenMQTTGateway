/*
  OpenMQTTGateway  - ESP8266 or Arduino program for home automation

   Act as a wifi or ethernet gateway between your 433mhz/infrared IR signal  and a MQTT broker
   Send and receiving command by MQTT

  This program enables to:
 - receive MQTT data from a topic and send RF 433Mhz signal corresponding to the received MQTT data
 - publish MQTT data to a different topic related to received 433Mhz signal
 - receive MQTT data from a topic and send IR signal corresponding to the received MQTT data
 - publish MQTT data to a different topic related to received IR signal
 - publish MQTT data to a different topic related to BLE devices rssi signal

  Copyright: (c)Florian ROBERT

  Contributors:
  - 1technophile
  - crankyoldgit
  - glasruit
  - HannesDi
  - Landrash
  - larsenglund
  - ChiefZ
  - PatteWi
  - jumpalottahigh
  - zerinrc
  - philfifi
  - Spudtater
  - prahjister
  - rickybrent
  - petricaM
  - ekim from Home assistant forum
  - ronvl from Home assistant forum
  - Chris Broekema
  - Lars Englund
  - Fredrik Lindqvist
  - Philippe LUC
  - Patrick Wilhelm
  - Georgi Yanev
  - zerinrc
  - 8666
  - animavitis
  - belidzs
  - alibahba
  - HosfordDotMe
  - torwag
  - intractve
  - QuagmireMan
  - f-reiling
  - McGr3g0r
  - steadramon

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

#ifdef ESP32
  #include <WiFi.h>
//  #include <ArduinoOTA.h>
  #include <WiFiUdp.h>
  //WiFiClient eClient;
  #ifdef MDNS_SD
    #include <ESPmDNS.h>
  #endif
#elif defined(ESP8266)
  #include <FS.h> 
  #include <ESP8266WiFi.h>
//  #include <ArduinoOTA.h>
//  #include <DNSServer.h>
//  #include <ESP8266WebServer.h>
  #include <WiFiManager.h>  
  WiFiClient eClient;

#else
  #include <Ethernet.h>
  EthernetClient eClient;
#endif

#include <PubSubClient.h>
#include <ArduinoJson.h>
#include <ESPiLight.h>

// Modules config inclusion
#include "config_RF.h"

#include "User_config.h"

// array to store previous received RFs, IRs codes and their timestamps
#if defined(ESP8266) || defined(ESP32) || defined(__AVR_ATmega2560__) || defined(__AVR_ATmega1280__)
  #define array_size 12
  unsigned long ReceivedSignal[array_size][2] ={{0,0},{0,0},{0,0},{0,0},{0,0},{0,0},{0,0},{0,0},{0,0},{0,0},{0,0},{0,0}};
  //Time used to wait for an interval before checking system measures
  unsigned long timer_sys_measures = 0;
#else // boards with smaller memory
  #define array_size 4
  unsigned long ReceivedSignal[array_size][2] ={{0,0},{0,0},{0,0},{0,0}};
#endif

/*-------------DEFINE YOUR MQTT PARAMETERS BELOW----------------*/
//MQTT Parameters definition
//#define mqtt_server_name "www.mqtt_broker.com" // instead of defining the server by its IP you can define it by its name, uncomment this line and set the correct MQTT server host name
char mqtt_user[20] = "your_username"; // not compulsory only if your broker needs authentication
char mqtt_pass[30] = "your_password"; // not compulsory only if your broker needs authentication
char mqtt_server[40] = "192.168.8.199";
char mqtt_port[6] = "1883";

// these values are only used if no dhcp configuration is available
const uint8_t ip[] = { 192, 168, 8, 195 }; //ip adress
const uint8_t gateway[] = { 0, 0, 0, 0 }; //ip adress, if first value is different from 0 advanced config network will be used and you should fill gateway & dns
const uint8_t Dns[] = { 0, 0, 0, 0 }; //ip adress, if first value is different from 0 advanced config network will be used and you should fill gateway & dns
const uint8_t subnet[] = { 255, 255, 255, 0 }; //ip adress

#if defined(ESP32) || defined(ESPWifiManualSetup) // for nodemcu, weemos and esp8266
  #define wifi_ssid "wifi ssid"
  #define wifi_password "wifi password"
#else // for arduino + W5100
  const uint8_t mac[] = {  0xDE, 0xED, 0xBA, 0xFE, 0x54, 0x95 }; //W5100 ethernet shield mac adress
#endif

/*------------------------------------------------------------------------*/

//adding this to bypass the problem of the arduino builder issue 50
void callback(char*topic, byte* payload,unsigned int length);

boolean connectedOnce = false; //indicate if we have been connected once to MQTT

int failure_number = 0; // number of failure connecting to MQTT

// client link to pubsub mqtt
PubSubClient client(eClient);

//MQTT last attemps reconnection date
unsigned long lastReconnectAttempt = 0;

//timers for LED indicators
unsigned long timer_led_receive = 0;
unsigned long timer_led_send = 0;
unsigned long timer_led_error = 0;

void revert_hex_data(char * in, char * out, int l){
  //reverting array 2 by 2 to get the data in good order
  int i = l-2 , j = 0; 
  while ( i != -2 ) {
    if (i%2 == 0) out[j] = in[i+1];
    else  out[j] = in[i-1];
    j++;
    i--;
  }
  out[l-1] = '\0';
}

int strpos(char *haystack, char *needle) //from @miere https://stackoverflow.com/users/548685/miere
{
   char *p = strstr(haystack, needle);
   if (p)
      return p - haystack;
   return -1;
}

bool to_bool(String const& s) { // thanks Chris Jester-Young from stackoverflow
     return s != "0";
}

void extract_char(char * token_char, char * subset, int start ,int l, boolean reverse, boolean isNumber){
    char tmp_subset[l+1];
    memcpy( tmp_subset, &token_char[start], l );
    tmp_subset[l] = '\0';
    if (isNumber){
      char tmp_subset2[l+1];
      if (reverse) revert_hex_data(tmp_subset, tmp_subset2, l+1);
      else strncpy( tmp_subset2, tmp_subset , l+1);
      long long_value = strtoul(tmp_subset2, NULL, 16);
      sprintf(tmp_subset2, "%ld", long_value);
      strncpy( subset, tmp_subset2 , l+1);
    }else{
      if (reverse) revert_hex_data(tmp_subset, subset, l+1);
      else strncpy( subset, tmp_subset , l+1);
    }
    subset[l] = '\0';
}

//trace
void trc(String msg){
  #ifdef TRACE
  Serial.println(msg);
  #endif
}

void trc(int msg){
  #ifdef TRACE
  Serial.println(msg);
  #endif
}

void trc(unsigned int msg){
  #ifdef TRACE
  Serial.println(msg);
  #endif
}

void trc(long msg){
  #ifdef TRACE
  Serial.println(msg);
  #endif
}

void trc(unsigned long msg){
  #ifdef TRACE
  Serial.println(msg);
  #endif
}

void trc(double msg){
  #ifdef TRACE
  Serial.println(msg);
  #endif
}

void trc(float msg){
  #ifdef TRACE
  Serial.println(msg);
  #endif
}

void pub(char * topic, char * payload, boolean retainFlag){
    client.publish(topic, payload, retainFlag);
}

void pub(char * topic, char * payload){
    client.publish(topic, payload);
}

void pub(char * topic, String payload){
    client.publish(topic,(char *)payload.c_str());
}

void pub(String topic, String payload){
    client.publish((char *)topic.c_str(),(char *)payload.c_str());
}

void pub(String topic, char *  payload){
    client.publish((char *)topic.c_str(),payload);
}

void pub(String topic, int payload){
    char val[12];
    sprintf(val, "%d", payload);
    client.publish((char *)topic.c_str(),val);
}

void pub(String topic, float payload){
    char val[12];
    dtostrf(payload,3,1,val);
    client.publish((char *)topic.c_str(),val);
}

void pub(char * topic, float payload){
    char val[12];
    dtostrf(payload,3,1,val);
    client.publish(topic,val);
}

void pub(char * topic, int payload){
    char val[6];
    sprintf(val, "%d", payload);
    client.publish(topic,val);
}

void pub(char * topic, unsigned int payload){
    char val[6];
    sprintf(val, "%u", payload);
    client.publish(topic,val);
}

void pub(char * topic, unsigned long payload){
    char val[11];
    sprintf(val, "%lu", payload);
    client.publish(topic,val);
}

void pub(String topic, unsigned long payload){
    char val[11];
    sprintf(val, "%lu", payload);
    client.publish((char *)topic.c_str(),val);
}

void pub(char * topicori, JsonObject& data){

    String topic = topicori;
    
    #ifdef jsonPublishing
      char JSONmessageBuffer[JSON_MSG_BUFFER];
      trc(F("Pub json into:"));
      trc(topic);
      data.printTo(JSONmessageBuffer, sizeof(JSONmessageBuffer));
      trc(JSONmessageBuffer);
      pub(topic, JSONmessageBuffer);
    #endif
}

int getMin(){
  unsigned int minimum = ReceivedSignal[0][1];
  int minindex=0;
  for (int i = 0; i < array_size; i++)
  {
    if (ReceivedSignal[i][1] < minimum) {
      minimum = ReceivedSignal[i][1];
      minindex = i;
    }
  }
  return minindex;
}


void storeValue(unsigned long MQTTvalue){
    unsigned long now = millis();
    // find oldest value of the buffer
    int o = getMin();
    trc(F("Min ind: "));
    trc(o);
    // replace it by the new one
    ReceivedSignal[o][0] = MQTTvalue;
    ReceivedSignal[o][1] = now;
    trc(F("store code :"));
    trc(String(ReceivedSignal[o][0])+"/"+String(ReceivedSignal[o][1]));
    trc(F("Col: val/timestamp"));
    for (int i = 0; i < array_size; i++)
    {
      trc(String(i) + ":" + String(ReceivedSignal[i][0])+"/"+String(ReceivedSignal[i][1]));
    }
}

boolean isAduplicate(unsigned long value){
trc(F("isAduplicate?"));
// check if the value has been already sent during the last time_avoid_duplicate
for (int i = 0; i < array_size;i++){
 if (ReceivedSignal[i][0] == value){
      unsigned long now = millis();
      if (now - ReceivedSignal[i][1] < time_avoid_duplicate){ // change
      trc(F("--don't pub. duplicate--"));
      return true;
    }
  }
}
return false;
}

ESPiLight rf(-1);  // use -1 to disable transmitter

void PilighttoMQTT(){
  rf.loop();
}

void pilightCallback(const String &protocol, const String &message, int status, size_t repeats, const String &deviceID) {
   if (status == VALID) {
    trc(F("Creating RF PiLight buffer"));
    StaticJsonBuffer<JSON_MSG_BUFFER> jsonBuffer;
    JsonObject& RFPiLightdata = jsonBuffer.createObject();
    trc(F("Rcv. Pilight"));
    RFPiLightdata.set("message", (char *)message.c_str());
    RFPiLightdata.set("protocol",(char *)protocol.c_str());
    RFPiLightdata.set("length", (char *)deviceID.c_str());
    RFPiLightdata.set("repeats", (int)repeats);
    RFPiLightdata.set("status", (int)status);
    pub(subjectPilighttoMQTT,RFPiLightdata);      
   }
}

void setupPilight(){
    #ifndef ZgatewayRF && ZgatewayRF2 && ZgatewayRF315 //receiving with Pilight is not compatible with ZgatewayRF or RF2 or RF315 as far as I can tell 
        rf.setCallback(pilightCallback);
        rf.initReceiver(RF_RECEIVER_PIN);
        trc(F("RF_EMITTER_PIN "));
        trc(String(RF_EMITTER_PIN));
        trc(F("RF_RECEIVER_PIN "));
        trc(String(RF_RECEIVER_PIN));
        trc(F("ZgatewayPilight setup done "));
    #else
        trc(F("ZgatewayPilight setup cannot be done, comment first ZgatewayRF && ZgatewayRF2 && ZgatewayRF315"));
    #endif
}

void MQTTtoPilight(char * topicOri, JsonObject& Pilightdata) 
{

  int result = 0;

   if (strcmp(topicOri,subjectMQTTtoPilight) == 0){
    trc(F("MQTTtoPilight json data analysis"));
    const char * message = Pilightdata["message"];
    const char * protocol = Pilightdata["protocol"];
    if (message && protocol) {
      trc(F("MQTTtoPilight msg & protocol ok"));
      result = rf.send(protocol, message);
    }else{
    trc(F("MQTTtoPilight fail reading from json"));
    }
    int msgLength = 0;
    uint16_t codes[MAXPULSESTREAMLENGTH];
    msgLength = rf.stringToPulseTrain(
        message,
        codes, MAXPULSESTREAMLENGTH);
    if (msgLength > 0) {
      trc(F("MQTTtoPilight raw ok"));
      rf.sendPulseTrain(codes, msgLength);
      result = msgLength;
    }else{
      trc(F("MQTTtoPilight raw KO"));
      result = -9999;
    }
    
   String MQTTmessage;
   if (result > 0) {
      trc(F("Adv data MQTTtoPilight push state via PilighttoMQTT"));
      pub(subjectGTWPilighttoMQTT, Pilightdata);
   } else {
    switch (result) {
      case ESPiLight::ERROR_UNAVAILABLE_PROTOCOL:
        MQTTmessage = "protocol is not avaiable";
        break;
      case ESPiLight::ERROR_INVALID_PILIGHT_MSG:
        MQTTmessage = "message is invalid";
        break;
      case ESPiLight::ERROR_INVALID_JSON:
        MQTTmessage = "message is not a proper json object";
        break;
      case ESPiLight::ERROR_NO_OUTPUT_PIN:
        MQTTmessage = "no transmitter pin";
        break;
      case -9999:
        MQTTmessage = "invalid pulse train message";
        break;
    }
    trc(F("ESPiLight Error: "));
    trc(String(MQTTmessage));
    pub(subjectGTWPilighttoMQTT, MQTTmessage);
   }
 }
}

void receivingMQTT(char * topicOri, char * datacallback) {
  if (strstr(topicOri, subjectMultiGTWKey) != NULL) // storing received value so as to avoid publishing this value if it has been already sent by this or another OpenMQTTGateway
  {
    trc(F("Storing signal"));
    unsigned long data = 0;
    #ifdef jsonPublishing
      trc(F("Creating Json buffer"));
      StaticJsonBuffer<JSON_MSG_BUFFER> jsonBuffer;
      JsonObject& jsondata = jsonBuffer.parseObject(datacallback);
      if (jsondata.success())  data =  jsondata["value"];
    #endif

    #ifdef simplePublishing
      data = strtoul(datacallback, NULL, 10); // we will not be able to pass values > 4294967295
    #endif
    
    if (data != 0) {
      storeValue(data);
      trc(F("Data from JSON stored"));
    }
  }
  //YELLOW ON
  digitalWrite(led_send, LOW);

  StaticJsonBuffer<JSON_MSG_BUFFER> jsonBuffer;
  JsonObject& jsondata = jsonBuffer.parseObject(datacallback);

  if (jsondata.success()) { // json object ok -> json decoding
   #ifdef ZgatewayPilight // ZgatewayPilight is only defined with json publishing
     MQTTtoPilight(topicOri, jsondata);
   #endif
  } else { // not a json object --> simple decoding

  }
//YELLOW OFF
digitalWrite(led_send, HIGH);
}


#if defined(ESP8266) || defined(ESP32) || defined(__AVR_ATmega2560__) || defined(__AVR_ATmega1280__)
void stateMeasures(){
    unsigned long now = millis();
    if (now > (timer_sys_measures + TimeBetweenReadingSYS)) {//retriving value of memory ram every TimeBetweenReadingSYS
      timer_sys_measures = millis();
      StaticJsonBuffer<JSON_MSG_BUFFER> jsonBuffer;
      JsonObject& SYSdata = jsonBuffer.createObject();
      trc("Uptime (s)");    
      unsigned long uptime = millis()/1000;
      trc(uptime);
      SYSdata["uptime"] = uptime;
      #if defined(ESP8266) || defined(ESP32)
        trc("Remaining memory");
        uint32_t freeMem;
        freeMem = ESP.getFreeHeap();
        SYSdata["freeMem"] = freeMem;
        trc(freeMem);
        trc("RSSI");
        long rssi = WiFi.RSSI();
        SYSdata["rssi"] = rssi;
        trc(rssi);
        trc("SSID");
        String SSID = WiFi.SSID();
        SYSdata["SSID"] = SSID;
        trc(SSID);
      #endif
      trc("Activated modules");
      String modules = "";

      #ifdef ZgatewayPilight
          modules = modules + ZgatewayPilight;
      #endif

      SYSdata["modules"] = modules;
      trc(modules);
      char JSONmessageBuffer[100];
      SYSdata.printTo(JSONmessageBuffer, sizeof(JSONmessageBuffer));
      pub(subjectSYStoMQTT,JSONmessageBuffer);
    }
}
#endif

// Callback function, when the gateway receive an MQTT value on the topics subscribed this function is called
void callback(char* topic, byte* payload, unsigned int length) {
  // In order to republish this payload, a copy must be made
  // as the orignal payload buffer will be overwritten whilst
  // constructing the PUBLISH packet.
  trc(F("Hey I got a callback "));
  // Allocate the correct amount of memory for the payload copy
  byte* p = (byte*)malloc(length + 1);
  // Copy the payload to the new buffer
  memcpy(p,payload,length);
  // Conversion to a printable string
  p[length] = '\0';
  //launch the function to treat received data
  receivingMQTT(topic,(char *) p);
  // Free the memory
  free(p);
}

#if defined(ESP32) || defined(ESPWifiManualSetup)
#elif defined(ESP8266) && !defined(ESPWifiManualSetup)
//Wifi manager parameters
//flag for saving data
bool shouldSaveConfig = true;
//do we have been connected once to mqtt

//callback notifying us of the need to save config
void saveConfigCallback () {
  trc("Should save config");
  shouldSaveConfig = true;
}

void setup_wifimanager(boolean reset_settings){
    if(reset_settings)  SPIFFS.format();

    //read configuration from FS json
    trc("mounting FS...");
  
    if (SPIFFS.begin()) {
      trc("mounted file system");
      if (SPIFFS.exists("/config.json")) {
        //file exists, reading and loading
        trc("reading config file");
        File configFile = SPIFFS.open("/config.json", "r");
        if (configFile) {
          trc("opened config file");
          size_t size = configFile.size();
          // Allocate a buffer to store contents of the file.
          std::unique_ptr<char[]> buf(new char[size]);
  
          configFile.readBytes(buf.get(), size);
          DynamicJsonBuffer jsonBuffer;
          JsonObject& json = jsonBuffer.parseObject(buf.get());
          json.printTo(Serial);
          if (json.success()) {
            trc("\nparsed json");
  
            strcpy(mqtt_server, json["mqtt_server"]);
            strcpy(mqtt_port, json["mqtt_port"]);
            strcpy(mqtt_user, json["mqtt_user"]);
            strcpy(mqtt_pass, json["mqtt_pass"]);
  
          } else {
            trc("failed to load json config");
          }
        }
      }
    } else {
      trc("failed to mount FS");
    }
  
    // The extra parameters to be configured (can be either global or just in the setup)
    // After connecting, parameter.getValue() will get you the configured value
    // id/name placeholder/prompt default length
    WiFiManagerParameter custom_mqtt_server("server", "mqtt server", mqtt_server, 40);
    WiFiManagerParameter custom_mqtt_port("port", "mqtt port", mqtt_port, 6);
    WiFiManagerParameter custom_mqtt_user("user", "mqtt user", mqtt_user, 20);
    WiFiManagerParameter custom_mqtt_pass("pass", "mqtt pass", mqtt_pass, 30);
  
   //WiFiManager
    //Local intialization. Once its business is done, there is no need to keep it around
    WiFiManager wifiManager;
    //Set timeout before going to portal
    wifiManager.setConfigPortalTimeout(WifiManager_ConfigPortalTimeOut);
  
    //set config save notify callback
    wifiManager.setSaveConfigCallback(saveConfigCallback);
  
    //set static ip
    //wifiManager.setSTAStaticIPConfig(IPAddress(10,0,1,99), IPAddress(10,0,1,1), IPAddress(255,255,255,0));
    
    //add all your parameters here
    wifiManager.addParameter(&custom_mqtt_server);
    wifiManager.addParameter(&custom_mqtt_port);
    wifiManager.addParameter(&custom_mqtt_user);
    wifiManager.addParameter(&custom_mqtt_pass);

    if(reset_settings)  wifiManager.resetSettings();
    //set minimu quality of signal so it ignores AP's under that quality
    wifiManager.setMinimumSignalQuality(MinimumWifiSignalQuality);
  
    //fetches ssid and pass and tries to connect
    //if it does not connect it starts an access point with the specified name
    //and goes into a blocking loop awaiting configuration
    if (!wifiManager.autoConnect(WifiManager_ssid, WifiManager_password)) {
      trc("failed to connect and hit timeout");
      delay(3000);
      //reset and try again, or maybe put it to deep sleep
      ESP.reset();
      delay(5000);
    }
  
    //if you get here you have connected to the WiFi
    trc("connected...yeey :)");
  
    //read updated parameters
    strcpy(mqtt_server, custom_mqtt_server.getValue());
    strcpy(mqtt_port, custom_mqtt_port.getValue());
    strcpy(mqtt_user, custom_mqtt_user.getValue());
    strcpy(mqtt_pass, custom_mqtt_pass.getValue());
  
    //save the custom parameters to FS
    if (shouldSaveConfig) {
      trc("saving config");
      DynamicJsonBuffer jsonBuffer;
      JsonObject& json = jsonBuffer.createObject();
      json["mqtt_server"] = mqtt_server;
      json["mqtt_port"] = mqtt_port;
      json["mqtt_user"] = mqtt_user;
      json["mqtt_pass"] = mqtt_pass;
    
      File configFile = SPIFFS.open("/config.json", "w");
      if (!configFile) {
        trc("failed to open config file for writing");
      }
  
      json.printTo(Serial);
      json.printTo(configFile);
      configFile.close();
      //end save
    }
}

#else // Arduino case

#endif

boolean reconnect() {

  // Loop until we're reconnected
  while (!client.connected()) {
      trc(F("MQTT connection...")); //F function enable to decrease sram usage
      if (client.connect(Gateway_Name, mqtt_user, mqtt_pass, will_Topic, will_QoS, will_Retain, will_Message)) {
      trc(F("Connected to broker"));
      failure_number = 0;
      // Once connected, publish an announcement...
      pub(will_Topic,Gateway_AnnouncementMsg,will_Retain);
      // publish version
      pub(version_Topic,OMG_VERSION,will_Retain);

      //Subscribing to topic
      if (client.subscribe(subjectMQTTtoX)) {
        #ifdef ZgatewayRF
          client.subscribe(subjectMultiGTWRF); // subject on which other OMG will publish, this OMG will store these msg and by the way don't republish them if they have been already published
        #endif
        #ifdef ZgatewayRF315
          client.subscribe(subjectMultiGTWRF315);// subject on which other OMG will publish, this OMG will store these msg and by the way don't republish them if they have been already published
        #endif
        #ifdef ZgatewayIR
          client.subscribe(subjectMultiGTWIR);// subject on which other OMG will publish, this OMG will store these msg and by the way don't republish them if they have been already published
        #endif
        trc(F("Subscription OK to the subjects"));
      }
      } else {
      failure_number ++; // we count the failure
      trc(F("failure_number"));
      trc(failure_number);
      trc(F("failed, rc="));
      trc(client.state());
      trc(F("try again in 5s"));
      // Wait 5 seconds before retrying
      delay(5000);

      if (failure_number > maxMQTTretry){
        trc(F("failed connecting to mqtt"));
        #if defined(ESP8266) && !defined(ESPWifiManualSetup)
          if (!connectedOnce) {
            trc(F("fail connecting to mqtt, reseting wifi manager"));
            setup_wifimanager(true); // if we didn't connected once to mqtt we reset and start in AP mode again to have a chance to change the parameters
          }
        #elif defined(ESP32) || defined(ESPWifiManualSetup)// ESP32 case we don't use Wifi manager yet
          setup_wifi();
        #endif
      }
    }
  }
  return client.connected();
}

void setup()
{
  //Launch serial for debugging purposes
  Serial.begin(SERIAL_BAUD);
  
  #if defined(ESP8266) || defined(ESP32)
  
    #ifdef ESP8266
      #ifndef ZgatewaySRFB // if we are not in sonoff rf bridge case we apply the ESP8266 pin optimization
        Serial.end();
        Serial.begin(SERIAL_BAUD, SERIAL_8N1, SERIAL_TX_ONLY);// enable on ESP8266 to free some pin
      #endif
    #endif
    
    #if defined(ESP8266) && !defined(ESPWifiManualSetup)
      setup_wifimanager(false);
    #else // ESP32 case we don't use Wifi manager yet
      setup_wifi();
    #endif

    trc(F("OpenMQTTGateway mac: "));
    trc(WiFi.macAddress()); 

    trc(F("OpenMQTTGateway ip: "));
    trc(WiFi.localIP().toString());

    // Port defaults to 8266
    /*ArduinoOTA.setPort(ota_port);

    // Hostname defaults to esp8266-[ChipID]
    ArduinoOTA.setHostname(ota_hostname);

    // No authentication by default
    ArduinoOTA.setPassword(ota_password);

    ArduinoOTA.onStart([]() {
      trc(F("Start"));
    });
    ArduinoOTA.onEnd([]() {
      trc(F("\nEnd"));
    });
    ArduinoOTA.onProgress([](unsigned int progress, unsigned int total) {
      Serial.printf("Progress: %u%%\r", (progress / (total / 100)));
    });
    ArduinoOTA.onError([](ota_error_t error) {
      Serial.printf("Error[%u]: ", error);
      if (error == OTA_AUTH_ERROR) trc(F("Auth Failed"));
      else if (error == OTA_BEGIN_ERROR) trc(F("Begin Failed"));
      else if (error == OTA_CONNECT_ERROR) trc(F("Connect Failed"));
      else if (error == OTA_RECEIVE_ERROR) trc(F("Receive Failed"));
      else if (error == OTA_END_ERROR) trc(F("End Failed"));
    });
    ArduinoOTA.begin();
    */

  #else // In case of arduino platform

    //Launch serial for debugging purposes
    Serial.begin(SERIAL_BAUD);
    //Begining ethernet connection in case of Arduino + W5100
    setup_ethernet();
    //setup LED status, turn all ON for short amount then leave only the RED LED ON
    pinMode(led_receive, OUTPUT);
    pinMode(led_send, OUTPUT);
    pinMode(led_error, OUTPUT);
    digitalWrite(led_receive, LOW);
    digitalWrite(led_send, LOW);
    digitalWrite(led_error, LOW);
    delay(500);
    digitalWrite(led_receive, HIGH);
    digitalWrite(led_send, HIGH);
    
  #endif

  #if defined(MDNS_SD) && defined(ESP8266)
     trc(F("Connecting to MQTT by mDNS without mqtt hostname"));
     connectMQTTmdns();
  #else
     long port;
     port = strtol(mqtt_port,NULL,10);
     trc(port);
   #ifdef mqtt_server_name // if name is defined we define the mqtt server by its name
     trc(F("Connecting to MQTT with mqtt hostname"));
     IPAddress mqtt_server_ip;
     WiFi.hostByName(mqtt_server_name, mqtt_server_ip);
     client.setServer(mqtt_server_ip, port);
     trc(mqtt_server_ip.toString());
   #else // if not by its IP adress
     trc(F("Connecting to MQTT by IP adress"));
     client.setServer(mqtt_server, port);
     trc(mqtt_server);
   #endif
  #endif

  client.setCallback(callback);
  
  delay(1500);

  lastReconnectAttempt = 0;

  #ifdef ZsensorBME280
    setupZsensorBME280();
  #endif
  #ifdef ZsensorBH1750
    setupZsensorBH1750();
  #endif
  #ifdef ZsensorTSL2561
    setupZsensorTSL2561();
  #endif
  #ifdef ZactuatorONOFF
    setupONOFF();
  #endif
  #ifdef Zgateway2G
    setup2G();
  #endif
  #ifdef ZgatewayIR
    setupIR();
  #endif
  #ifdef ZgatewayRF
    setupRF();
  #endif
  #ifdef ZgatewayRF315
    setupRF315();
  #endif
  #ifdef ZgatewayRF2
    setupRF2();
  #endif
  #ifdef ZgatewayPilight
    setupPilight();
  #endif
  #ifdef ZgatewaySRFB
    setupSRFB();
  #endif
  #ifdef ZgatewayBT
    setupBT();
  #endif
  #ifdef ZgatewayRFM69
    setupRFM69();
  #endif
  #ifdef ZsensorINA226
    setupINA226();
  #endif
  #ifdef ZsensorHCSR501
    setupHCSR501();
  #endif
  #ifdef ZsensorGPIOInput
    setupGPIOInput();
  #endif
  #ifdef ZsensorGPIOKeyCode
   setupGPIOKeyCode();
  #endif
  
  trc(F("MQTT_MAX_PACKET_SIZE"));
  trc(MQTT_MAX_PACKET_SIZE);
  trc(F("JSON_MSG_BUFFER"));
  trc(JSON_MSG_BUFFER);
  trc(F("Setup OpenMQTTGateway end"));
}


#if defined(MDNS_SD) && defined(ESP8266)
  void connectMQTTmdns(){
    if (!MDNS.begin("ESP_MQTT")) {
        trc(F("Error setting up MDNS responder!"));
        while(1){
            delay(1000);
        }
    }
    trc(F("Browsing for service MQTT "));
    int n = MDNS.queryService("mqtt", "tcp");
    if (n == 0) {
        trc(F("no services found"));
    }else {
        trc(n);
        trc(" service(s) found");
        for (int i=0; i < n; ++i) {
            // Print details for each service found
            trc(i + 1);
            trc(MDNS.hostname(i));
            trc(MDNS.IP(i).toString());
            trc(MDNS.port(i));
        }
        if (n==1) {
          trc(F("One MQTT server found setting parameters"));
          client.setServer(MDNS.IP(0), int(MDNS.port(0)));
        }else{
          trc(F("Several MQTT servers found, please deactivate mDNS and set your default server"));
        }
    }
  }
#endif

void loop()
{
    unsigned long now = millis();
  //MQTT client connexion management
  if (!client.connected()) { // not connected

    //RED ON
    digitalWrite(led_error, LOW);

    if (now - lastReconnectAttempt > 5000) {
      lastReconnectAttempt = now;
      // Attempt to reconnect
      if (reconnect()) {
        lastReconnectAttempt = 0;
      }
    }
  } else { //connected
    // MQTT loop
    //RED OFF
    if (now - timer_led_receive > 300) {
      timer_led_receive = now;
      digitalWrite(led_receive, HIGH);
    }
    digitalWrite(led_error, HIGH);
    connectedOnce = true;
    client.loop();

    /*#if defined(ESP8266) || defined(ESP32)
      ArduinoOTA.handle();
    #endif
*/
    #ifdef ZgatewayPilight
      PilighttoMQTT();
    #endif

    #if defined(ESP8266) || defined(ESP32) || defined(__AVR_ATmega2560__) || defined(__AVR_ATmega1280__)
      stateMeasures();
    #endif
  }
}
