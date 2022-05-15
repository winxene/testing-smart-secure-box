#include <Arduino.h>
/*import libraries*/
#include <Ticker.h>
#include <PubSubClient.h>
#if defined(ESP32)  
  #include <WiFi.h>
#endif  
#if defined(ESP8266)
  #include <ESP8266WiFi.h>
#endif
#include "DHTesp.h"
#include <Wire.h>
#include "BH1750.h"
#include "device.h"

/*wifi ssid & pass used for the device*/ 
const char* ssid = "POCOX3GT";
const char* password = "ayamgoreng";

/*define MQTT constant value*/
#define MQTT_BROKER  "broker.emqx.io"
#define MQTT_TOPIC_PUBLISH   "esp32_test/data"

WiFiClient wifiClient; //define wifi client
PubSubClient mqtt(wifiClient); //define mqtt 

Ticker timerPublish; //define timer
DHTesp dht; //define the dht
BH1750 lightMeter; //define the lux meter

/*declare all the functions*/
void taskTemperature(void *arg); 
void taskLight(void *arg);
void WifiConnect();
boolean mqttConnect();
void onPublishMessage();

/*define global variables*/
char g_szDeviceId[30];
float lux;
float temperature;
float humidity;

void setup() {
  Serial.begin(115200);
  delay(100);
  
  xTaskCreatePinnedToCore(taskTemperature, "Temperature", 2048, NULL, 1, NULL, 0); //pin a specific task to a specific core
  xTaskCreatePinnedToCore(taskLight, "Light", 2048, NULL, 2, NULL, 1);
  
  /*setup the dht*/
  dht.setup(PIN_DHT, DHTesp::DHT11); 
  Serial.printf("Board: %s\n", ARDUINO_BOARD);
  Serial.printf("DHT Sensor ready, sampling period: %d ms\n", dht.getMinimumSamplingPeriod());

  /*setup the lux meter*/
  Wire.begin(PIN_SDA, PIN_SCL); 
  lightMeter.begin(BH1750::CONTINUOUS_HIGH_RES_MODE, 0x23, &Wire);

  WifiConnect(); //call WifiConnect to connect to Wifi
  mqttConnect();//call mqttConnect to connect to MQTT
}

void loop() {
  mqtt.loop(); //to allow the client to process incoming messages to send publish data and makes a refresh of the connection.
}

void onPublishMessage() //to publish message
{
  char szMsg[50];
  sprintf(szMsg, "lux:  %.2f", lux);
  mqtt.publish(MQTT_TOPIC_PUBLISH, szMsg);
  sprintf(szMsg, "temp:  %.2f", temperature);
  mqtt.publish(MQTT_TOPIC_PUBLISH, szMsg);
  sprintf(szMsg, "humidity:  %.2f", humidity);
  mqtt.publish(MQTT_TOPIC_PUBLISH, szMsg);
}

boolean mqttConnect() {
#if defined(ESP32)  
  sprintf(g_szDeviceId, "esp32_%08X",(uint32_t)ESP.getEfuseMac());
#endif  
#if defined(ESP8266)  
  sprintf(g_szDeviceId, "esp8266_%08X",(uint32_t)ESP.getChipId());
#endif  

  mqtt.setServer(MQTT_BROKER, 1883); // Connect to MQTT Broker
  Serial.printf("Connecting to %s clientId: %s\n", MQTT_BROKER, g_szDeviceId);
  
  //to authenticate MQTT:
  boolean status = mqtt.connect(g_szDeviceId);
  if (status == false) {
    Serial.print(" fail, rc=");
    Serial.print(mqtt.state());
    return false;
  }
  Serial.println(" success");

  onPublishMessage();
  return mqtt.connected();
}


void WifiConnect() //to connect to Wifi
{
  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid, password);
  while (WiFi.waitForConnectResult() != WL_CONNECTED) {
    Serial.println("Connection Failed! Rebooting...");
    delay(5000);
    ESP.restart();
  }  
  Serial.print("System connected with IP address: ");
  Serial.println(WiFi.localIP());
  Serial.printf("RSSI: %d\n", WiFi.RSSI());
}


void taskTemperature(void *arg) {
  for (;;)
  {
    TickType_t nTickStart = xTaskGetTickCount(); //store the count of starting tick
    vTaskDelay(5000 / portTICK_PERIOD_MS); //to give delay
    TickType_t nElapaseTick = xTaskGetTickCount() - nTickStart; //to store the time interval
    temperature = dht.getTemperature();
    humidity = dht.getHumidity();
    if (dht.getStatus() == DHTesp::ERROR_NONE) //check if there's an error
    {
    Serial.printf("[%u] Temperature: %.2f C\n", nElapaseTick, temperature); //print the temperature to terminal
    Serial.printf("[%u] Humidity: %.2f RH\n", nElapaseTick, humidity); //print the humidity to terminal
    }
    else {
      Serial.printf("error disini\n");
    }
  }
}

void taskLight(void *arg)
{
  for (;;)
  {
    TickType_t nTickStart = xTaskGetTickCount();//store the count of starting tick
    vTaskDelay(5000 / portTICK_PERIOD_MS);
    lux = lightMeter.readLightLevel();
    TickType_t nElapaseTick = xTaskGetTickCount() - nTickStart; //to store the time interval
    Serial.printf("[%u] Light: %.2f lux\n", nElapaseTick, lux); //print the lux to terminal
    onPublishMessage(); //to publish lux value
  }
}



