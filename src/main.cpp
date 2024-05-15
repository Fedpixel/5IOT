#include <Arduino.h>
#include <Wire.h>
#include <SPI.h>
#include <WiFi.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <Adafruit_NeoPixel.h>
#include <Adafruit_AHTX0.h>
#include <esp_now.h>
#include <esp_wifi.h>


#define WIRE Wire
#define PIN 8
#define NUMPIXELS 16
#define BUTTON_PIN 9

Adafruit_AHTX0 aht;
Adafruit_SSD1306 display = Adafruit_SSD1306(128, 32, &WIRE);
Adafruit_NeoPixel pixels(NUMPIXELS, PIN, NEO_GRB + NEO_KHZ800);

float temperaturefromnetwork;
float humidityfromnetwork;
char receivedfrom[18];
uint8_t redfromnetwork;
uint8_t greenfromnetwork;
uint8_t bluefromnetwork;
uint8_t broadcastAddress[] = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};
int rssi_display;

// Structure example to send data
// Must match the receiver structure
typedef struct struct_message {
  float temperature;
  float humidity;
} struct_message_t;

typedef struct struct_message_light {
  uint8_t red;
  uint8_t green;
  uint8_t blue;
} struct_message_light_t;

typedef struct {
  unsigned frame_ctrl: 16;
  unsigned duration_id: 16;
  uint8_t addr1[6]; /* receiver address */
  uint8_t addr2[6]; /* sender address */
  uint8_t addr3[6]; /* filtering address */
  unsigned sequence_ctrl: 16;
  uint8_t addr4[6]; /* optional */
} wifi_ieee80211_mac_hdr_t;

typedef struct {
  wifi_ieee80211_mac_hdr_t hdr;
  uint8_t payload[0]; /* network data ended with 4 bytes csum (CRC32) */
} wifi_ieee80211_packet_t;

// Create a struct_message called myData
struct_message_t myData;
struct_message_light_t myDataLight;

esp_now_peer_info_t peerInfo;

// callback when data is sent
void OnDataSent(const uint8_t *mac_addr, esp_now_send_status_t status) {
  Serial.print("\r\nLast Packet Send Status:\t");
  Serial.println(status == ESP_NOW_SEND_SUCCESS ? "Delivery Success" : "Delivery Fail");
}

// callback function that will be executed when data is received
void OnDataRecv(const uint8_t * mac, const uint8_t *incomingData, int len) {
  sprintf(receivedfrom, "%02X:%02X:%02X:%02X:%02X:%02X", mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
  if (len == sizeof(myData)){
    memcpy(&myData, incomingData, sizeof(myData));
    temperaturefromnetwork = myData.temperature;
    humidityfromnetwork = myData.humidity;

    //Display data on serial monitor
    // Serial.print("Bytes received: ");
    // Serial.println(len);
    // Serial.print("Temperature: ");
    // Serial.println(myData.temperature);
    // Serial.print("Humidity: ");
    // Serial.println(myData.humidity);
    // Serial.print("From: ");
    // Serial.println(receivedfrom);
    // Serial.println();
  }
  else if (len == sizeof(myDataLight))
  {
    memcpy(&myDataLight, incomingData, sizeof(myDataLight));
    pixels.clear();
    pixels.setPixelColor(0, myDataLight.red, myDataLight.green, myDataLight.blue);
    pixels.show();
    // Serial.print("Bytes received: ");
    // Serial.println(len);
    // Serial.print("Red: ");
    // Serial.println(myDataLight.red);
    // Serial.print("Green: ");
    // Serial.println(myDataLight.green);
    // Serial.print("Blue: ");
    // Serial.println(myDataLight.blue);
    // Serial.println();
    // Serial.print("From: ");
    // Serial.println(receivedfrom);
    // Serial.println();
  }
  else {
    Serial.print("Invalid packet received from : ");
    Serial.println(receivedfrom);
  }
}

void promiscuous_rx_cb(void *buf, wifi_promiscuous_pkt_type_t type) {

    // All espnow traffic uses action frames which are a subtype of the mgmnt frames so filter out everything else.
    if (type != WIFI_PKT_MGMT)
        return;

    static const uint8_t ACTION_SUBTYPE = 0xd0;
    static const uint8_t ESPRESSIF_OUI[] = {0x18, 0xfe, 0x34};

    const wifi_promiscuous_pkt_t *ppkt = (wifi_promiscuous_pkt_t *)buf;
    const wifi_ieee80211_packet_t *ipkt = (wifi_ieee80211_packet_t *)ppkt->payload;
    const wifi_ieee80211_mac_hdr_t *hdr = &ipkt->hdr;


   int rssi = ppkt->rx_ctrl.rssi;
   rssi_display = rssi;
}

void IRAM_ATTR handleButtonPress()
{
  Serial.println("Button pressed");
  // Set values to send
  myDataLight.red = 0;
  myDataLight.green = 150;
  myDataLight.blue = 0;

  // Send message via ESP-NOW
  esp_err_t result = esp_now_send(broadcastAddress, (uint8_t *) &myDataLight, sizeof(myDataLight)); 

  if (result == ESP_OK) {
    Serial.println("Sent with success");
  }
  else {
    Serial.println("Error sending the data");
  }

}


void setup() {
  Serial.begin(115200);
  Wire.setPins(3,2);

  //Setup WiFi
  WiFi.mode(WIFI_MODE_STA);
  WiFi.begin();
  esp_wifi_set_promiscuous(true);
  esp_wifi_set_channel(1, WIFI_SECOND_CHAN_NONE);
  esp_wifi_set_promiscuous_rx_cb(&promiscuous_rx_cb);

  //Setup OLED
  display.begin(SSD1306_SWITCHCAPVCC, 0x3C);
  Serial.println("OLED begun");
  display.display();
  display.clearDisplay();

  //Setup AHT
  if (! aht.begin()) {
    Serial.println("Could not find AHT? Check wiring");
    while (1) delay(10);
  }
  Serial.println("AHT10 or AHT20 found");

  //Setup ESP-Now
  if (esp_now_init() != ESP_OK) {
    Serial.println("Error initializing ESP-NOW");
    return;
  }

  // Once ESPNow is successfully Init, we will register for Send CB to
  // get the status of Transmitted packet
  esp_now_register_send_cb(OnDataSent);

  // Register peer
  memcpy(peerInfo.peer_addr, broadcastAddress, 6);
  peerInfo.channel = 0;  
  peerInfo.encrypt = false;

  // Add peer        
  if (esp_now_add_peer(&peerInfo) != ESP_OK){
    Serial.println("Failed to add peer");
    return;
  }

  // Once ESPNow is successfully Init, we will register for recv CB to
  // get recv packer info
  esp_now_register_recv_cb(OnDataRecv);

  //Setup button
  pinMode(BUTTON_PIN, INPUT_PULLUP);
  attachInterrupt(digitalPinToInterrupt(BUTTON_PIN), handleButtonPress, FALLING);
}

void displayonscreen() {
  display.setTextSize(1);
  display.setTextColor(SSD1306_WHITE);
  display.setCursor(0,0);
  display.print("Me:");
  display.println(WiFi.macAddress());
  display.print("From:");
  display.println(receivedfrom);
  display.print(temperaturefromnetwork);
  display.print("C ");
  display.print(humidityfromnetwork);
  display.print("% ");
  display.print(humidityfromnetwork);
  display.print("dBm");

  display.display();
  display.clearDisplay();
  delay(500);
}

float gettemperature() {
  sensors_event_t humidity, temp;
  aht.getEvent(&humidity, &temp);
  return temp.temperature;
}

float gethumidity() {
  sensors_event_t humidity, temp;
  aht.getEvent(&humidity, &temp);
  return humidity.relative_humidity;
}

void sendmessage(){
  delay(5000);

  // Set values to send
  myData.temperature = gettemperature();
  myData.humidity = gethumidity();

  // Send message via ESP-NOW
  esp_err_t result = esp_now_send(broadcastAddress, (uint8_t *) &myData, sizeof(myData)); 

  if (result == ESP_OK) {
    Serial.println("Sent with success");
  }
  else {
    Serial.println("Error sending the data");
  }
}

void loop() {
  displayonscreen();
  sendmessage();
}