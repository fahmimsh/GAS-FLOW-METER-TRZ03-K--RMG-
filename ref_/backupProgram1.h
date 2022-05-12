#include <Arduino.h>
#include <ArduinoOTA.h>
#include <ESP_EEPROM.h>
#include <stdio.h>
#include <Wire.h>
#include <LiquidCrystal_I2C.h>
#include <PubSubClient.h>
#include <ArduinoJson.h>

#define pin_LF 2

WiFiClient espClient;
PubSubClient client(espClient);
LiquidCrystal_I2C lcd(0x27,16,2);

uint8_t EEPROM_address = 0;
char topiq1[16], topiq2[16], topiq3[16];
const char* id_dev = "1";
String clientId = "GAS_METER";
// const char* ssid = "AVIAN SSO";
// const char* password = "avian2021";
const char* ssid = "AIC-Lab";
const char* password = "LabAIC2020";
// const char* mqtt_server = "test.mosquitto.org";
const char* mqtt_server = "192.168.110.126";

float count_gas = 0.0, count_gas_prev = 0.0, kwh_gas = 0.0;
int scan_rssi;
String mac_address, local_IP;
unsigned long prev_time_send = 0;
IRAM_ATTR void handleInterrupt() {
   count_gas += 0.01;
}
void eepromWriteFloat(float data_input);
void inisialisasi_wifi();
void reconnectwifi();
String getValue(String data, char separator, int index);
void callback(char* topic, byte* payload, unsigned int length);
void reconnect();
void lcd_clear();
void send_mqqtt(bool flag_send);
void setup() {
  // put your setup code here, to run once:
  Serial.begin(9600);
  lcd.init();
  lcd.backlight();
  pinMode(pin_LF, INPUT_PULLUP);
  attachInterrupt(digitalPinToInterrupt(pin_LF), handleInterrupt, FALLING);
  inisialisasi_wifi();
  client.setServer(mqtt_server, 1883);
  client.setCallback(callback);
  snprintf(topiq1, 32, "gas/%s/log", id_dev);
  snprintf(topiq2, 32, "gas/%s/status", id_dev);
  snprintf(topiq3, 32, "gas/%s/set", id_dev);
  Serial.println("inisialisasi done");
  EEPROM.begin(32);
  EEPROM.get(EEPROM_address, count_gas);
  ArduinoOTA.begin();
}

void loop() {
  ArduinoOTA.handle();
  reconnectwifi();
  lcd.setCursor(0, 0); lcd.print(count_gas);
  lcd.setCursor(0, 1); lcd.print(kwh_gas);
  count_gas += 0.01;
  kwh_gas = count_gas * 10.55;
  eepromWriteFloat(count_gas);
  if (count_gas != count_gas_prev){
    count_gas_prev = count_gas;
    if(count_gas > 999999.99){
      count_gas = 0.0;
    }else {
      Serial.println(count_gas);
      send_mqqtt(false);
    }
  }
  if(millis() - prev_time_send >= 10000){
    prev_time_send = millis();
    send_mqqtt(true);
  }
}
void eepromWriteFloat(float data_input){
  EEPROM.put(EEPROM_address, data_input);
  EEPROM.commit();
}
void inisialisasi_wifi(){
    lcd.setCursor(0, 0); lcd.print("AIR METER AVIAN");
    lcd.setCursor(0, 1); lcd.print("Connecting WiFi");
    WiFi.begin(ssid, password);
    uint32_t notConnectedCounter = 0;
    while (WiFi.status() != WL_CONNECTED)
    {
     delay(100);
     Serial.print("*");
     notConnectedCounter++;
      if(notConnectedCounter > 150) { // Reset board if not connected after 5s
          Serial.println("Resetting due to Wifi not connecting...");
          ESP.restart();
      }
    }
    local_IP = WiFi.localIP().toString();
    mac_address = WiFi.macAddress();
    Serial.println((String) "Connected to WiFi Local IP:" + local_IP);
    lcd.setCursor(0, 1); lcd.printf("Connected OK");
}
void reconnectwifi(){
  if (!client.connected()) {
    reconnect();
  }
  client.loop();
  if(WiFi.status() != WL_CONNECTED)
  {
    inisialisasi_wifi();
  } else {
    scan_rssi = WiFi.RSSI();
  }
}
String getValue(String data, char separator, int index){
  int found = 0;
  int strIndex[] = { 0, -1 };
  int maxIndex = data.length() - 1;
  for (int i = 0; i <= maxIndex && found <= index; i++) {
      if (data.charAt(i) == separator || i == maxIndex) {
          found++;
          strIndex[0] = strIndex[1] + 1;
          strIndex[1] = (i == maxIndex) ? i+1 : i;
      }
    }
  return found > index ? data.substring(strIndex[0], strIndex[1]) : "";
}
void callback(char* topic, byte* payload, unsigned int length) {
  String topic_set = (String)topic;
  String topic_kind = getValue(topic_set, '/', 0);
  String topic_id = getValue(topic_set, '/', 1);
  String topic_task = getValue(topic_set, '/', 2);
  char message[length + 1];
  strncpy (message, (char*)payload, length);
  message[length] = '\0';
  if(topic_kind == "gas"){
    if(topic_id == id_dev){
      if(topic_task == "set"){
        count_gas = atof(message);
      }
    }
  }
}
void reconnect(){
  while (!client.connected()) {
    String clientId = "ESP32Client-";
    clientId += String(random(0xffff), HEX);
    if (client.connect(clientId.c_str())) {
      client.subscribe(topiq3);
      client.subscribe(topiq2);
      client.subscribe(topiq1);
    } else {
      delay(5000);
    }
  }
}
void lcd_clear(){
  lcd.clear();
  lcd.setCursor(7, 0); lcd.print(",");
  lcd.setCursor(13, 0); lcd.print("");
  lcd.setCursor(13, 1); lcd.print("m3");
}
void send_mqqtt(bool flag_send){
  StaticJsonDocument<256> doc;
  if(flag_send == false){
    doc["device_id"] = id_dev;
    doc["gas"] = count_gas;
    doc["kwh"] = kwh_gas;
    doc["signal"] = scan_rssi;
    doc["ip"] = local_IP;
    doc["mac"] = mac_address;
    doc["note"] = " ";
    char msg[256];
    serializeJson(doc, msg);
    client.publish(topiq2, msg);
  }else if(flag_send == true){
    doc["device_id"] = id_dev;
    doc["gas"] = count_gas;
    doc["kwh"] = kwh_gas;
    char msg1[256];
    serializeJson(doc, msg1);
    client.publish(topiq1, msg1);
  }
}