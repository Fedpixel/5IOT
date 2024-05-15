#include <Arduino.h>
#include <Adafruit_AHTX0.h>
#include <Wire.h>
#include <SPI.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <WiFi.h>
#include <MQTT.h>
#define WIRE Wire
#include <AsyncTCP.h>
#include <ESPAsyncWebServer.h>

Adafruit_AHTX0 aht;
Adafruit_SSD1306 display = Adafruit_SSD1306(128, 32, &WIRE);
WiFiClient wifiClient;
MQTTClient client;
AsyncWebServer server(80);

//Wifi
const char* ssid = "Hesias";
const char* pass = "bienvenuechezHesias";

//MQTT
unsigned long lastMillis = 0;

//WEB
const char* PARAM_MESSAGE = "message";

String co2meter;
String tempmeter;
String humidmeter;

void notFound(AsyncWebServerRequest *request) {
    request->send(404, "text/plain", "Not found");
}


const char index_html[] PROGMEM = R"rawliteral(
<!DOCTYPE HTML><html>
<head>
  <meta name="viewport" content="width=device-width, initial-scale=1">
  <link rel="stylesheet" href="https://use.fontawesome.com/releases/v5.7.2/css/all.css" integrity="sha384-fnmOCqbTlWIlj8LyTjo7mOUStjsKC4pOpQbqyi7RrhN7udi9RwhKkMHpvLbHG9Sr" crossorigin="anonymous">
  <style>
    html {
     font-family: Arial;
     display: inline-block;
     margin: 0px auto;
     text-align: center;
    }
    h2 { font-size: 3.0rem; }
    p { font-size: 3.0rem; }
    .units { font-size: 1.2rem; }
    .dht-labels{
      font-size: 1.5rem;
      vertical-align:middle;
      padding-bottom: 15px;
    }
  </style>
</head>
<body>
  <h2>ESP32 Server</h2>
  <p>
    <i class="fas fa-thermometer-half" style="color:#059e8a;"></i> 
    <span class="dht-labels">Temperature</span> 
    <span id="temperature">%TEMPERATURE%</span>
    <sup class="units">&deg;C</sup>
  </p>
  <p>
    <i class="fas fa-tint" style="color:#00add6;"></i> 
    <span class="dht-labels">Humidity</span>
    <span id="humidity">%HUMIDITY%</span>
    <sup class="units">&percnt;</sup>
  </p>
  <p>
    <i class="fas fa-wind" style="color:#00add6;"></i> 
    <span class="dht-labels">CO2</span>
    <span id="humidity">%CO2%</span>
    <sup class="units">ppm</sup>
  </p>
</body>
</html>)rawliteral";

// Replaces placeholder with DHT and MQTT values
String processor(const String& var){
  
  //Gather data from sensor
  sensors_event_t humidity, temp;
  aht.getEvent(&humidity, &temp);

  //Gather data from MQTT

  //Replace placeholder with data
  if(var == "TEMPERATURE"){
    return tempmeter;
  }
  else if(var == "HUMIDITY"){
    return humidmeter;
  }
    else if(var == "CO2"){
    return co2meter;
  }
  return String();
}

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
  client.subscribe("loris/temperature");
  client.subscribe("loris/humidity");
  // client.unsubscribe("/hello");
}

void messageReceived(String &topic, String &payload) {
  Serial.println("incoming: " + topic + " - " + payload);

  if (topic == "loris/co2") {
    co2meter = payload;
  }
  else if (topic == "loris/temperature") {
    tempmeter = payload;
  }
  else if (topic == "loris/humidity") {
    humidmeter = payload;
  }
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

  //Setup of the web server
  // Route for root / web page
  server.on("/", HTTP_GET, [](AsyncWebServerRequest *request){
    request->send_P(200, "text/html", index_html, processor);
  });

  // Start server
  server.begin();
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
    client.publish("quenting/temperature", temperature);
  }

  delay(1000);
}