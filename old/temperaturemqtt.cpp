#include <Arduino.h>
#include <Adafruit_AHTX0.h>
#include <Wire.h>
#include <SPI.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <WiFi.h>
#include <MQTT.h>
#define WIRE Wire

Adafruit_AHTX0 aht;
Adafruit_SSD1306 display = Adafruit_SSD1306(128, 32, &WIRE);
WiFiClient wifiClient;
MQTTClient client;

//Wifi
const char* ssid = "Hesias";
const char* pass = "bienvenuechezHesias";
//MQTT
unsigned long lastMillis = 0;
int count = 0;

void connect() {
  Serial.print("checking wifi...");
  while (WiFi.status() != WL_CONNECTED) {
    Serial.print(".");
    delay(1000);
  }

  Serial.print("\nconnecting...");
  while (!client.connect("arduinoquentin")) {
    Serial.print(".");
    delay(1000);
  }

  Serial.println("\nconnected!");

  client.subscribe("loris/co2");
  // client.unsubscribe("/hello");
}

void messageReceived(String &topic, String &payload) {
  Serial.println("incoming: " + topic + " - " + payload);

  // Note: Do not use the client in the callback to publish, subscribe or
  // unsubscribe as it may cause deadlocks when other things arrive while
  // sending and receiving acknowledgments. Instead, change a global variable,
  // or push to a queue and handle it in the loop after calling `client.loop()`.
}

void setup() {
  Serial.begin(115200);

  delay(1000);

  //Setup of the WiFi network
  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid, pass);
  Serial.println("\nConnecting");

  while(WiFi.status() != WL_CONNECTED){
      Serial.print(".");
      delay(100);
  }

  Serial.println("\nConnected to the WiFi network");
  Serial.print("Local ESP32 IP: ");
  Serial.println(WiFi.localIP());

  //Setup of the AHT20 sensor
  Wire.setPins(3,2);
  if (! aht.begin()) {
    Serial.println("Could not find AHT? Check wiring");
    while (1) delay(10);
  }
  Serial.println("AHT10 or AHT20 found");

  //Setup of the mqtt client
  client.begin("172.20.63.156", 1883, wifiClient);
  client.onMessage(messageReceived);

  connect();

  //Setup of the screen
  display.begin(SSD1306_SWITCHCAPVCC, 0x3C);
}

void loop() {
  //Gathering temperature and humidity data
  sensors_event_t humidity, temp;
  aht.getEvent(&humidity, &temp);

  //Displaying temperature and humidity to the screen
  //Serial.print("Temperature: "); Serial.print(temp.temperature); Serial.println(" degrees C");
  //Serial.print("Humidity: "); Serial.print(humidity.relative_humidity); Serial.println("% rH");
  
  //Displaying data to the screen
  display.setTextSize(1);
  display.setTextColor(SSD1306_WHITE);
  display.setCursor(0,0);

  display.print(temp.temperature);
  display.println("C");
  display.print(humidity.relative_humidity);
  display.println("%");
  display.print(WiFi.localIP());
  
  display.display();
  display.clearDisplay();

  client.loop();

  if (!client.connected()) {
    connect();
  }

  char temperature[20];
  sprintf(temperature, "%f", temp.temperature);
  // publish a message roughly every second.
  if (millis() - lastMillis > 1000) {
    lastMillis = millis();
    client.publish("quenting/esp32/temperature", temperature);
  }

  delay(1000);
}