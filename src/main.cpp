#include <Arduino.h>
#include <Adafruit_AHTX0.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <Adafruit_NeoPixel.h>
#include <Wire.h>
#include <SPI.h>
#include <WiFi.h>
#include <MQTT.h>
#include <AsyncTCP.h>
#include <ESPAsyncWebServer.h>

#define WIRE Wire
#define PIN 8
#define NUMPIXELS 16

Adafruit_AHTX0 aht;
Adafruit_SSD1306 display = Adafruit_SSD1306(128, 32, &WIRE);
Adafruit_NeoPixel pixels(NUMPIXELS, PIN, NEO_GRB + NEO_KHZ800);
WiFiClient wifiClient;
MQTTClient client;
AsyncWebServer server(80);

const char* ssid = "Hesias";
const char* pass = "bienvenuechezHesias";

String co2fromprof;
String tempfromprof;
String humidityfromprof;

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
  <p>
  <form method="get" action="/ledon">
    <input type="submit" value="LED ON">
  </form>
    <form method="get" action="/ledoff">
    <input type="submit" value="LED OFF">
  </form>
  </p>
</body>
</html>)rawliteral";

String processor(const String& var){
  
  //Gather data from sensor
  sensors_event_t humidity, temp;
  aht.getEvent(&humidity, &temp);

  //Replace placeholder with data
  if(var == "TEMPERATURE"){
    return String(temp.temperature);
  }
  else if(var == "HUMIDITY"){
    return String(humidity.relative_humidity);
  }
    else if(var == "CO2"){
    return co2fromprof;
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
  while (!client.connect("quenting")) {
    Serial.print(".");
    delay(1000);
  }

  Serial.println("\nconnected!");

  client.subscribe("loris/co2");
  client.subscribe("loris/temperature");
  client.subscribe("loris/humidity");
  client.subscribe("quenting/led");
}

void messageReceived(String &topic, String &payload) {
  Serial.println("incoming: " + topic + " - " + payload);

  if (topic == "loris/co2") {
    co2fromprof = payload;
  }
  else if (topic == "loris/temperature") {
    tempfromprof = payload;
  }
  else if (topic == "loris/humidity") {
    humidityfromprof = payload;
  }
  else if (topic == "quenting/led" && payload == "1") {
    pixels.clear();
    pixels.setPixelColor(0, 150, 150, 150);
    pixels.show();
  }
  else if (topic == "quenting/led" && payload == "0"){
    pixels.clear();
    pixels.setPixelColor(0, 0, 0, 0);
    pixels.show();
  }
    else if (topic == "quenting/led" && payload.charAt(0) == '#'){
    uint32_t color = (uint32_t)strtol(&payload[1], NULL, 16);
    pixels.clear();
    pixels.setPixelColor(0, color);
    pixels.show();
  }
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

  //Setup of the MQTT client
  client.begin("172.20.63.156", 1883, wifiClient);
  client.onMessage(messageReceived);

  connect();

  //Setup of the screen
  display.begin(SSD1306_SWITCHCAPVCC, 0x3C);

  //Setup of the web server
  server.on("/", HTTP_GET, [](AsyncWebServerRequest *request){
    request->send_P(200, "text/html", index_html, processor);
  });

  server.on("/ledon", HTTP_GET, [](AsyncWebServerRequest *request){
    pixels.clear();
    pixels.setPixelColor(0, 150, 150, 150);
    pixels.show(); 
    request->redirect("/");
  });

  server.on("/ledoff", HTTP_GET, [](AsyncWebServerRequest *request){
    pixels.clear(); 
    pixels.setPixelColor(0, 0, 0, 0);
    pixels.show(); 
    request->redirect("/");
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
  char humidityvalue[20];
  sprintf(temperature, "%f", temp.temperature);
  sprintf(humidityvalue, "%f", humidity.relative_humidity);

  unsigned long lastMillis = 0;
  if (millis() - lastMillis > 1000) {
    lastMillis = millis();
    client.publish("quenting/temperature", temperature);
    client.publish("quenting/humidity", humidityvalue);
  }

  delay(1000);
}