#include <espnow.h>
#include <ESP8266WiFi.h>
#include <string.h>
#include <map>
#define FASTLED_ESP8266_NODEMCU_PIN_ORDER
#include <FastLED.h>

//Macros
#define RETRY_INTERVAL 5000
#define ESP_OK 0
#define NUM_HOSTS 21

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
#define LED_TYPE WS2812
#define COLOR_ORDER GRB

#define NO_PATTERN 0x00
#define PATTERN1 0x01
#define PATTERN2 0x02
#define PATTERN3 0x03
#define PATTERN4 0x04
#define PATTERN5 0x05
#define UNKNOWN_PATTERN 0x06

//-----Multiple Beacon WiFi Project----//
//-----Author: Stuart D'Amico----------//
//
// Last Date Modified: 7/27/20
//
// Description: This project is designed to have several node arduinos connected to a host arduino. Whenever an
// event is triggered on a node arduino, the host reacts to this and activates the corresponding LEDs.
// The host arduino acts as a server while the node arduinos act as clients. Both the hosts and clients send data
// back and forth to each other to update on events that may have occured at nodes or whether the host wants to change LED patterns.
// All the software is the same on host and client arduinos, so whether an arduino is a host
// or client is determined by its MAC Address. ESP_NOW limits devices to connect to a max of 20 peers, so in order to connect to 400 nodes,
// the following steps are taken:
//
// 1. Host (hostList[0]) is powered on.
// 2. The first 20 nodes that connect to the host have their MAC addresses changed to entries in hostList.
// 3. Every node after the first 20 connects to one of the 20 nodes in hostList.
//
// Progress: At the moment, this project successfully establishes two-way connections between the host and the nodes, lights up
// the corresponding LEDs on the host side depending on when events A and B occur, and selects patterns from the host side. 
// Now I'm working on being able to connect 400 nodes to a host. I have written code for displaying patterns, but 
// I cannot test it since I don't have access to WS2812 LED strips.


//-----Global Variables----//
char hostMACAdd [18] = "8C:AA:B5:0D:FB:A4"; //change this MAC address to change the host node
const uint8_t channel = 14;
int peerNum = 0; //global variable for host to use to track number of peers
volatile int secondCount = 0;
volatile int hostNum = 0;
CRGB leds[NUM_LEDS];

//-----Data Structures----//
struct __attribute__((packed)) NodeInfo {
  bool isHost; //flag that controls whether host or node code executes
  bool isSubHost = false; //flag that controls if subhost code executes or not
  uint8_t pattern;
  bool A; //event A has occured or is occuring
  bool B; //event B has occured or is occuring
  
  std::map<std::string, std::vector<bool>> eventMap; //list of local MAC addresses corresponding to events A and B
  uint8_t newMACAdd [6]; //new MAC address sent from host
};

char * hostList [NUM_HOSTS] = { //list of subhosts to be connected to
  hostMACAdd,
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
  "14:11:11:11:11:11",
  "24:22:22:22:22:22",
  "34:33:33:33:33:33",
  "44:44:44:44:44:44",
  "54:55:55:55:55:55"
};

NodeInfo myNodeInfo; //info for each node
NodeInfo sentInfo; //info received from another node

//-----Function Prototypes----//
void initESPNow();
void initTimer();
void sendData();
void ICACHE_RAM_ATTR onTime();
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
    myNodeInfo.pattern = UNKNOWN_PATTERN;
    
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

    for(int i=0; i<6; ++i) {
      hostUintMAC[i] = (uint8_t) hostIntMAC[i];
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
    delay(2000);
  }
}





//-----Function Definitions----//


void sendCallBackFunction(uint8_t * mac, uint8_t sendStatus) {
  Serial.println(sendStatus == ESP_OK ? "Delivery Success!" : "Delivery Fail.");
}

void receiveCallBackFunction(uint8_t *senderMAC, uint8_t *incomingData, uint8_t len) {
  Serial.printf("Message from %02X:%02X:%02X:%02X:%02X:%02X: ", senderMAC[0], senderMAC[1], senderMAC[2], senderMAC[3], senderMAC[4], senderMAC[5]);
  if(myNodeInfo.isHost) {
    //host
    char senderMACString [18];
    sprintf(senderMACString, "%02X:%02X:%02X:%02X:%02X:%02X", senderMAC[0], senderMAC[1], senderMAC[2], senderMAC[3], senderMAC[4], senderMAC[5]);
    bool isInHostList = false;
    
    for(int i=1; i<20; ++i){
      if(!strcmp(senderMACString, hostList[i])) { //if senderMAC is in hostList, add as a peer
        isInHostList = true;
        break;
      }
    }
    if(isInHostList) {
      //if in hostList, attempt to add as peer
      if(esp_now_add_peer(senderMAC, ESP_NOW_ROLE_COMBO, channel, NULL, 0) == ESP_OK) {
        Serial.printf("New node connected: %02X:%02X:%02X:%02X:%02X:%02X\n\r", senderMAC[0], senderMAC[1], senderMAC[2], senderMAC[3], senderMAC[4], senderMAC[5]);
      }
      memcpy(&sentInfo, incomingData, len);
      Serial.printf("A is %d, ", sentInfo.A);
      Serial.printf("B is %d, ", sentInfo.B);
      Serial.printf("Pattern set to %d.\n\r", sentInfo.pattern);
  
      //convert senderMAC into string
      char senderMACString [18];
      sprintf(senderMACString, "%02X:%02X:%02X:%02X:%02X:%02X", senderMAC[0], senderMAC[1], senderMAC[2], senderMAC[3], senderMAC[4], senderMAC[5]);    
      myNodeInfo.eventMap[senderMACString] = {sentInfo.A, sentInfo.B};

      //if node is a subHost
      if(sentInfo.isSubHost) {
        std::map<std::string, std::vector<bool>>::iterator it = sentInfo.eventMap.begin();
        
        while(it != sentInfo.eventMap.end()) {
          myNodeInfo.eventMap[it->first] = {it->second[0], it->second[1]};
          ++it;
        }
      }
    }
    else {
      //if not in hostList, tell node to change MAC address unless peer limit is reached
      Serial.printf("Number of peers: %d \n\r", peerNum);
      if(peerNum < 20) {
        if(esp_now_add_peer(senderMAC, ESP_NOW_ROLE_COMBO, channel, NULL, 0) == ESP_OK){
          ++peerNum;

          //get new host MAC and convert it to a uint8_t array
          int intNewMAC [6];
          uint8_t uintNewMAC [6];
          
          sscanf(hostList[peerNum], "%02X:%02X:%02X:%02X:%02X:%02X%*c",
            &intNewMAC[0], &intNewMAC[1], &intNewMAC[2],
            &intNewMAC[3], &intNewMAC[4], &intNewMAC[5]); 

          for(int i=0; i<6; ++i){
            uintNewMAC[i] = (uint8_t) intNewMAC[i];
          }

          //send request to device to change its MAC address
          Serial.printf("Sending request to %02X:%02X:%02X:%02X:%02X:%02X to change MAC Address to %02X:%02X:%02X:%02X:%02X:%02X.\n\r",
          senderMAC[0], senderMAC[1], senderMAC[2], senderMAC[3], senderMAC[4], senderMAC[5],
          uintNewMAC[0], uintNewMAC[1], uintNewMAC[2], uintNewMAC[3], uintNewMAC[4], uintNewMAC[5]);

          for(int i = 0; i<6; ++i) {
             myNodeInfo.newMACAdd [i] = uintNewMAC[i];
          }
          sendData(senderMAC);
          for(int i = 0; i<6; ++i) {
             myNodeInfo.newMACAdd [i] = 0x00;
          }
          esp_now_del_peer(senderMAC);
        }
      }
    }
  }
  else{
    //node
    memcpy(&sentInfo, incomingData, len);

    char senderMACString [18];
    sprintf(senderMACString, "%02X:%02X:%02X:%02X:%02X:%02X", senderMAC[0], senderMAC[1], senderMAC[2], senderMAC[3], senderMAC[4], senderMAC[5]);
    
    if(strcmp(senderMACString, hostMACAdd)){
      //if sent from node
      myNodeInfo.eventMap[senderMACString] = {sentInfo.A, sentInfo.B};
    }
    else{
      //if sent from host
      Serial.printf("Set pattern to %d.", sentInfo.pattern);
    
      //check for MAC address change request
      bool isNullMAC = true;
      for(int i=0; i<6; ++i) {
        if(sentInfo.newMACAdd[i]!=0x00) {
          isNullMAC = false;
        }  
      }
  
      if(!isNullMAC) { //if MAC address change request was sent
        if(wifi_set_macaddr(STATION_IF, &sentInfo.newMACAdd[0]) == ESP_OK) {
          Serial.println("MAC Address Change Request.");
          Serial.println("MAC Address Changed to: " + WiFi.softAPmacAddress());
          myNodeInfo.isSubHost = true;
        }
        else{
          Serial.println("MAC Address unsuccessfully changed."); 
        }
        initTimer();
      }
      Serial.println();
      Serial.printf("Node: Pattern %d selected.\n\r", sentInfo.pattern);
      myNodeInfo.pattern = sentInfo.pattern;
    }

  }
}

void ICACHE_RAM_ATTR onTime() {
  ++secondCount;
  if(secondCount == 5 && myNodeInfo.pattern == UNKNOWN_PATTERN) { //if 4 seconds have passed
    int intMAC [6];
    uint8_t uintMAC [6];
    
    sscanf(hostList[hostNum], "%02X:%02X:%02X:%02X:%02X:%02X%*c",
      &intMAC[0], &intMAC[1], &intMAC[2],
      &intMAC[3], &intMAC[4], &intMAC[5]); 

    for(int i=0; i<6; ++i){
      uintMAC[i] = (uint8_t) intMAC[i];
    }
    esp_now_del_peer(uintMAC);
    hostNum = (hostNum + 1) % NUM_HOSTS;
    
    sscanf(hostList[hostNum], "%02X:%02X:%02X:%02X:%02X:%02X%*c",
      &intMAC[0], &intMAC[1], &intMAC[2],
      &intMAC[3], &intMAC[4], &intMAC[5]); 
      
    for(int i=0; i<6; ++i){
      uintMAC[i] = (uint8_t) intMAC[i];
    }
    esp_now_add_peer(uintMAC, ESP_NOW_ROLE_COMBO, channel, NULL, 0);
  }
    
  timer1_write(5000000);
}

void initESPNow() {
  if(myNodeInfo.isHost) {
    Serial.println("This is an ESP-Now host.");
  }
  else{
     Serial.println("This is an ESP-Now node.");
  }
  WiFi.mode(WIFI_STA);
  WiFi.disconnect();
  if (esp_now_init() != ESP_OK) {
    Serial.println("ESP_Now init failed...");
    delay(RETRY_INTERVAL);
    ESP.restart();
  }
}

void initTimer() {
  //Initialize Ticker every 1s
  timer1_attachInterrupt(onTime); // Add ISR Function
  timer1_enable(TIM_DIV16, TIM_EDGE, TIM_SINGLE);
  /* Dividers:
    TIM_DIV1 = 0,   //80MHz (80 ticks/us - 104857.588 us max)
    TIM_DIV16 = 1,  //5MHz (5 ticks/us - 1677721.4 us max)
    TIM_DIV256 = 3  //312.5Khz (1 tick = 3.2us - 26843542.4 us max)
  Reloads:
    TIM_SINGLE  0 //on interrupt routine you need to write a new value to start the timer again
    TIM_LOOP  1 //on interrupt the counter will start with the same value again
  */
  
  // Arm the Timer for our 1s Interval
  secondCount = 0;
  timer1_write(5000000); // 5000000 / 5 ticks per us from TIM_DIV16 == 1,000,000 us interval  
}

void sendData(uint8_t * destination) {
  uint8_t message[sizeof(myNodeInfo)];
  memcpy(message, &myNodeInfo, sizeof(myNodeInfo));
  esp_now_send(destination, message, sizeof(myNodeInfo));
}


void processEvents() {
  //if host: iterate through all devices and check if events A and B are active for any devices
  //if node: check for events A and B on device
  if(myNodeInfo.isHost) {
    //host
    std::map<std::string, std::vector<bool>>::iterator it = myNodeInfo.eventMap.begin();
    bool eventA = false;
    bool eventB = false;
    
    while(it != myNodeInfo.eventMap.end()){
      
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
  
    Serial.printf("Host: A is %d, B is %d\n\r", eventA, eventB);
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
        break;
      case NO_PATTERN:
      case UNKNOWN_PATTERN:
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
