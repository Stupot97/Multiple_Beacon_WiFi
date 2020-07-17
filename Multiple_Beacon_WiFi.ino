#include <espnow.h>
#include <ESP8266WiFi.h>
#include <string.h>
#include <map>
#define FASTLED_ESP8266_NODEMCU_PIN_ORDER
#include <FastLED.h>

//Macros
#define RETRY_INTERVAL 5000
#define ESP_OK 0

#define HOST_INPUT1 D1
#define HOST_INPUT2 D2
#define HOST_INPUT3 D5
#define HOST_INPUT4 D6
#define HOST_INPUT5 D7
#define HOST_OUTPUT1 D3
#define HOST_OUTPUT2 D4

#define NODE_INPUT1 D6
#define NODE_INPUT2 D7
#define NODE_OUTPUT1 D2
#define NODE_OUTPUT2 D3
#define NODE_OUTPUT3 D4

#define NUM_LEDS 60 //macro of number of LEDs
#define LED_TYPE    WS2812
#define COLOR_ORDER GRB

#define NO_PATTERN 0x00
#define PATTERN1 0x01
#define PATTERN2 0x02
#define PATTERN3 0x03
#define PATTERN4 0x04
#define PATTERN5 0x05

//-----Multiple Beacon WiFi Project----//
//-----Author: Stuart D'Amico----------//
//
// Last Date Modified: 7/17/20
//
// Description: This project is designed to have several node arduinos connected to a host arduino. Whenever an
// event is triggered on a node arduino, the host reacts to this and activates the corresponding LEDs.
// The host arduino acts as a server while the node arduinos act as clients. Both the hosts and clients send data
// back and forth to each other to update on events that may have occured at nodes or whether the host wants to change LED patterns.
// All the software is the same on host and client arduinos, so whether an arduino is a host
// or client is determined by its MAC Address.
//
// Progress: At the moment, this project successfully establishes two-way connections between the host and the nodes, lights up
// the corresponding LEDs on the host side depending on when events A and B occur, and selects patterns from the host side. 
// Now I'm working on being able to connect 400 nodes to a host. I have written code for displaying patterns, but 
// I cannot test it since I don't have access to WS2812 LED strips.

//-----Global Variables----//
char hostMACAdd [18] = "8C:AA:B5:0D:FB:A4"; //change this MAC address to change the host node
const uint8_t channel = 14;
CRGB leds[NUM_LEDS];


//-----Data Structures----//
struct __attribute__((packed)) NodeInfo {
  bool isHost; //flag that controls whether host or node code executes
  uint8_t pattern;
  bool A; //event A has occured or is occuring
  bool B; //event B has occured or is occuring
};

char * hostList [21] = { //list of 
  hostMACAdd,
  "02:00:00:00:00:00",
  "12:11:11:11:11:11",
  "22:22:22:22:22:22",
  "32:33:33:33:33:33",
  "42:44:44:44:44:44",
  "52:55:55:55:55:55",
  "62:66:66:66:66:66",
  "72:77:77:77:77:77",
  "82:88:88:88:88:88",
  "92:99:99:99:99:99",
  "A2:AA:AA:AA:AA:AA",
  "B2:BB:BB:BB:BB:BB",
  "C2:CC:CC:CC:CC:CC",
  "D2:DD:DD:DD:DD:DD",
  "E2:EE:EE:EE:EE:EE",
  "F2:FF:FF:FF:FF:FF",
  "04:00:00:00:00:00",
  "14:11:11:11:11:11",
  "24:22:22:22:22:22",
  "34:33:33:33:33:33"
};

std::map<std::string, std::vector<bool>> eventMap; //list of local MAC addresses corresponding to events A and B

NodeInfo myNodeInfo; //info for each node
NodeInfo sentInfo; //info received from another node

//-----Function Prototypes----//
void initESPNow();
void sendData();
void sendCallBackFunction(uint8_t* mac, uint8_t sendStatus);
void receiveCallBackFunction(uint8_t *senderMAC, uint8_t *incomingData, uint8_t len);
void processEvents();
void processPatterns();

void noPattern();
void pattern1();
void pattern2();
void pattern3();
void pattern4();
void pattern5();

void setup() {
  Serial.begin(74880);
  Serial.println();
  Serial.println("MAC Address: " + WiFi.macAddress());
  
  //is it a host or node?
  if (strcmp(WiFi.macAddress().c_str(), hostMACAdd)){
    //node
    myNodeInfo.isHost = false;
    myNodeInfo.pattern = NO_PATTERN;
    
    pinMode(NODE_INPUT1, INPUT);
    pinMode(NODE_INPUT2, INPUT);

    //add NUM_LEDS amount of LEDS to each NODE_OUTPUT
    FastLED.addLeds<LED_TYPE,NODE_OUTPUT1,COLOR_ORDER>(leds, NUM_LEDS).setCorrection(TypicalSMD5050);
    FastLED.addLeds<LED_TYPE,NODE_OUTPUT2,COLOR_ORDER>(leds, NUM_LEDS).setCorrection(TypicalSMD5050);
    FastLED.addLeds<LED_TYPE,NODE_OUTPUT3,COLOR_ORDER>(leds, NUM_LEDS).setCorrection(TypicalSMD5050);

    //Enable ESP Now
    initESPNow();

    esp_now_set_self_role(ESP_NOW_ROLE_COMBO);
    //convert MAC address to int
    int hostIntMAC [6];
    uint8_t hostUintMAC [6];
    
    
    sscanf(hostMACAdd, "%x:%x:%x:%x:%x:%x%*c",
    &hostIntMAC[0], &hostIntMAC[1], &hostIntMAC[2],
    &hostIntMAC[3], &hostIntMAC[4], &hostIntMAC[5]); 

    for(int i=0; i<6; ++i){
      hostUintMAC[i] = (uint8_t) hostIntMAC[i];
    }
    
    if (esp_now_add_peer(hostUintMAC, ESP_NOW_ROLE_COMBO, channel, NULL, 0) != ESP_OK) {
      Serial.println("Error connecting to host.");
    }
    
    
    int sendUnsuccessfullyRegistered = esp_now_register_send_cb(sendCallBackFunction);
    int receiveUnsuccessfullyRegistered = esp_now_register_recv_cb(receiveCallBackFunction);
    
    if (!sendUnsuccessfullyRegistered) {
      Serial.println("Send callback function registered!");
    }
    else {
      Serial.println("Send callback function not registered.");
    }
    if (!receiveUnsuccessfullyRegistered) {
      Serial.println("Receive callback function registered!");
    }
    else {
      Serial.println("Receive callback function not registered.");
    }
  }
  else {
    //host
    myNodeInfo.isHost = true;
    myNodeInfo.pattern = NO_PATTERN;
    
    pinMode(HOST_INPUT1, INPUT);
    pinMode(HOST_INPUT2, INPUT);
    pinMode(HOST_INPUT3, INPUT);
    pinMode(HOST_INPUT4, INPUT);
    pinMode(HOST_INPUT5, INPUT);
    
    pinMode(HOST_OUTPUT1, OUTPUT);
    pinMode(HOST_OUTPUT2, OUTPUT);
    
    //Enable ESP Now
    initESPNow();

    esp_now_set_self_role(ESP_NOW_ROLE_COMBO);

    int receiveUnsuccessfullyRegistered = esp_now_register_recv_cb(receiveCallBackFunction);
    int sendUnsuccessfullyRegistered = esp_now_register_send_cb(sendCallBackFunction);

    if (!sendUnsuccessfullyRegistered) {
      Serial.println("Send callback function registered!");
    }
    else {
      Serial.println("Send callback function not registered.");
    }
    if (!receiveUnsuccessfullyRegistered) {
      Serial.println("Receive callback function registered!");
    }
    else {
      Serial.println("Receive callback function not registered.");
    }
    
    Serial.println("Waiting for messages...");
  }
}

void loop() {

  if (myNodeInfo.isHost) {
    //host
    
    processEvents();
    processPatterns();
    
    sendData(NULL); //send data to all nodes connected to
    delay(2000);
  }
  else {
    //node
    
    processEvents();
    processPatterns();
    
    //get host's local MAC address
    int hostIntMAC [6];
    uint8_t hostUintMAC [6];
    
    sscanf(hostMACAdd, "%x:%x:%x:%x:%x:%x%*c",
    &hostIntMAC[0], &hostIntMAC[1], &hostIntMAC[2],
    &hostIntMAC[3], &hostIntMAC[4], &hostIntMAC[5]); 

    for(int i=0; i<6; ++i){
      hostUintMAC[i] = (uint8_t) hostIntMAC[i];
    }
     
    sendData(hostUintMAC); 
    delay(1000);
  }
}





//-----Function Definitions----//


void sendCallBackFunction(uint8_t * mac, uint8_t sendStatus) {
  Serial.println(sendStatus == ESP_OK ? "Delivery Success!" : "Delivery Fail.");
}

void receiveCallBackFunction(uint8_t *senderMAC, uint8_t *incomingData, uint8_t len) {
  Serial.printf("Message from %02x:%02x:%02x:%02x:%02x:%02x: ", senderMAC[0], senderMAC[1], senderMAC[2], senderMAC[3], senderMAC[4], senderMAC[5]);
  if(myNodeInfo.isHost){
    memcpy(&sentInfo, incomingData, len);
    Serial.printf("A is %d, ", sentInfo.A);
    Serial.printf("B is %d, ", sentInfo.B);
    Serial.printf("Pattern set to %d.\n\r", sentInfo.pattern);

    //convert senderMAC into string
    char senderMACString [18];
    sprintf(senderMACString, "%d:%d:%d:%d:%d:%d", senderMAC[0], senderMAC[1], senderMAC[2], senderMAC[3], senderMAC[4], senderMAC[5]);    
    eventMap[senderMACString] = {sentInfo.A, sentInfo.B};

    processEvents();
    
    if(esp_now_add_peer(senderMAC, ESP_NOW_ROLE_COMBO, channel, NULL, 0) == ESP_OK){
      Serial.printf("New node connected: %02x:%02x:%02x:%02x:%02x:%02x\n\r", senderMAC[0], senderMAC[1], senderMAC[2], senderMAC[3], senderMAC[4], senderMAC[5]);
    }
  }
  else{
    memcpy(&sentInfo, incomingData, len);
    Serial.printf("Node: Pattern %d selected.\n\r", sentInfo.pattern);
    myNodeInfo.pattern = sentInfo.pattern;
  }
}

void initESPNow() {
  if(myNodeInfo.isHost){
    WiFi.mode(WIFI_STA); // Host mode for esp-now host
    WiFi.disconnect();
    Serial.println("This is an ESP-Now host.");
  }
  else{
     WiFi.mode(WIFI_STA); // Station mode for esp-now node
     WiFi.disconnect();
     Serial.println("This is an ESP-Now node.");
  }
  if (esp_now_init() != 0){
    Serial.println("ESP_Now init failed...");
    delay(RETRY_INTERVAL);
    ESP.restart();
  }
}

void sendData(uint8_t * destination) {
  if(myNodeInfo.isHost){
     uint8_t message[sizeof(myNodeInfo)];
     memcpy(message, &myNodeInfo, sizeof(myNodeInfo));
     esp_now_send(destination, message, sizeof(myNodeInfo));
  }
  else{
    uint8_t message[sizeof(myNodeInfo)];
    memcpy(message, &myNodeInfo, sizeof(myNodeInfo));
    esp_now_send(destination, message, sizeof(myNodeInfo));
  }
}


void processEvents() {
  //if host: iterate through all devices and check if events A and B are active for any devices
  //if node: check for events A and B on device
  if(myNodeInfo.isHost) {
    //host
    std::map<std::string, std::vector<bool>>::iterator it = eventMap.begin();
    bool eventA = false;
    bool eventB = false;
    
    while(it != eventMap.end()){
      
      eventA |= it->second[0];
      eventB |= it->second[1];
  
      ++it;
    }
  
    if(eventA){
      digitalWrite(HOST_OUTPUT1, HIGH);
    }
    else{
      digitalWrite(HOST_OUTPUT1, LOW);
    }
    if(eventB){
      digitalWrite(HOST_OUTPUT2, HIGH);
    }
    else{
      digitalWrite(HOST_OUTPUT2, LOW);
    }
  
    Serial.printf("Host: A is %d, B is %d\n\r",eventA, eventB);
  }
  else{
    //node
    if(digitalRead(NODE_INPUT1) == HIGH){
      myNodeInfo.A = true;   
    }
    else{
      myNodeInfo.A = false;  
    }
    if(digitalRead(NODE_INPUT2) == HIGH){
      myNodeInfo.B = true;   
    }
    else{
      myNodeInfo.B = false;  
    }
  }
}

void processPatterns() {
  //if host: check each host input and select corresponding pattern. if multiple patterns are selected, choose the highest number pattern
  //if node: call appropriate pattern function
  if(myNodeInfo.isHost) {
    if(digitalRead(HOST_INPUT1) == HIGH){
      myNodeInfo.pattern = PATTERN1;
    }
    if(digitalRead(HOST_INPUT2) == HIGH){
      myNodeInfo.pattern = PATTERN2;
    }
    if(digitalRead(HOST_INPUT3) == HIGH){
      myNodeInfo.pattern = PATTERN3; 
    }
    if(digitalRead(HOST_INPUT4) == HIGH){
      myNodeInfo.pattern = PATTERN4;
    }
    if(digitalRead(HOST_INPUT5) == HIGH){
      myNodeInfo.pattern = PATTERN5;
    }
    if(digitalRead(HOST_INPUT1) == LOW && digitalRead(HOST_INPUT2) == LOW && digitalRead(HOST_INPUT3) == LOW && digitalRead(HOST_INPUT4) == LOW && digitalRead(HOST_INPUT5) == LOW){
      myNodeInfo.pattern = NO_PATTERN;
    }
    Serial.printf("Host: Pattern is %d \n\r", myNodeInfo.pattern);
  }
  else {
    switch(myNodeInfo.pattern){
      case PATTERN1:
        pattern1();
        break;
      case PATTERN2:
        pattern2();
        break;
      case PATTERN3:
        pattern3();
        break;
      case PATTERN4:
        pattern4();
        break;
      case PATTERN5:
        pattern5();
      case NO_PATTERN:
        noPattern();
    }
  }
  
}

void noPattern() {
  //clear all LEDs
  for(int led = 0; led < NUM_LEDS; ++led) {
    leds[led] = CRGB::Black;
  }
  FastLED.show();
}


void pattern1() {
  //constant fast blinking pattern
  for(int led = 0; led < NUM_LEDS; ++led) {
    leds[led] = CRGB::Blue;
  }
  FastLED.show();
  delay(30);
  for(int led = 0; led < NUM_LEDS; ++led) {
    leds[led] = CRGB::Black;
  }
  delay(30);
}

void pattern2() {
  //light moving down line pattern
  for(int led = 0; led < NUM_LEDS; ++led) { 
    leds[led] = CRGB::Green;
    FastLED.show();
    leds[led] = CRGB::Black;
    delay(30);
  }
}

void pattern3() {
  //every other LED lights up and alternates patterns
  for(int led = 0; led < NUM_LEDS; ++led) { 
    if(led%2){ // if odd number
      leds[led] = CRGB::Black;
    }
    else{     //if even number
      leds[led] = CRGB::Red;
    }
  }
  FastLED.show();
  delay(200);
  for(int led = 0; led < NUM_LEDS; ++led) { 
    if(led%2){ // if odd number
      leds[led] = CRGB::Teal;
    }
    else{     //if even number
      leds[led] = CRGB::Black;
    }
  }
  FastLED.show();
  delay(200);
}

void pattern4() {
  //two lights moving down line pattern 
  for(int led = 0; led < NUM_LEDS; ++led) { 
    leds[led] = CRGB::Purple;
    leds[NUM_LEDS-led] = CRGB::Gold; 
    FastLED.show();
    leds[led] = CRGB::Black;
    leds[NUM_LEDS-led] = CRGB::Black;
    delay(30);
  }
}

void pattern5() {
  //lights growing down line pattern
  for(int led = 0; led < NUM_LEDS; ++led) { 
    leds[led] = CRGB::Orange;
    FastLED.show();
    delay(30);
  }
  for(int led = 0; led < NUM_LEDS; ++led) { 
    leds[led] = CRGB::Black;
  }
  FastLED.show();
}
