#include <espnow.h>
#include <ESP8266WiFi.h>
#include <string.h>
#include <map>

//Macros
#define RETRY_INTERVAL 5000
#define ESP_OK 0

#define HOST_INPUT1 D0
#define HOST_INPUT2 D1
#define HOST_INPUT3 D2
#define HOST_INPUT4 D3
#define HOST_INPUT5 D4
#define HOST_OUTPUT1 D7
#define HOST_OUTPUT2 D8

#define NODE_INPUT1 D2
#define NODE_INPUT2 D3


//-----Multiple Beacon WiFi Project----//
//-----Author: Stuart D'Amico----------//
//
// Last Date Modified: 7/6/20
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
// Now I'm working on being able to toggle host LEDs from multiple node inputs.


//Data Structures
struct __attribute__((packed)) NodeInfo {
  bool isHost; //flag that controls whether host or node code executes
  uint8_t pattern;
  bool A; //event A has occured or is occuring
  bool B; //event B has occured or is occuring
};


//Global Variables
char hostMACAdd [18] = "8C:AA:B5:0D:FB:A4"; //change this MAC address to change the host node
const uint8_t channel = 14;


//To create a local MAC address, use the following pattern
//   x2-xx-xx-xx-xx-xx
//   x6-xx-xx-xx-xx-xx
//   xA-xx-xx-xx-xx-xx
//   xE-xx-xx-xx-xx-xx


//list of MAC addresses corresponding to local MAC addresses to be used
std::map<std::string, std::vector<uint8_t>> localMACAddMap = {
  {"8C:AA:B5:0D:FB:A4", {0x82,0x88,0x88,0x88,0x88,0x88}}, //host
  {"F4:CF:A2:D4:40:F8", {0x84,0x88,0x88,0x88,0x88,0x88}}
};

NodeInfo myNodeInfo;
NodeInfo sentInfo;


//Function Prototypes
void initESPNow();
void sendData();
void sendCallBackFunction(uint8_t* mac, uint8_t sendStatus);
void receiveCallBackFunction(uint8_t *senderMAC, uint8_t *incomingData, uint8_t len);



void setup() {
  Serial.begin(74880);
  Serial.println();

  Serial.println("MAC Address: " + WiFi.macAddress());
  
  //is it a host or node?
  if (strcmp(WiFi.macAddress().c_str(), hostMACAdd)){
    //node
    myNodeInfo.isHost = false;
    
    pinMode(NODE_INPUT1, INPUT);
    pinMode(NODE_INPUT2, INPUT);
    
    //Enable ESP Now
    initESPNow();

    esp_now_set_self_role(ESP_NOW_ROLE_COMBO);
    //get corresponding local MAC address from map
    
    uint8_t localHostMACAdd[localMACAddMap[hostMACAdd].size()];
    std::copy(localMACAddMap[hostMACAdd].begin(), localMACAddMap[hostMACAdd].end(), localHostMACAdd);
    
    if (esp_now_add_peer(localHostMACAdd, ESP_NOW_ROLE_COMBO, channel, NULL, 0) != ESP_OK) {
      Serial.println("Error connecting to host.");
      //TODO: make this station a host and a node if node limit reached
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

    myNodeInfo.pattern = 0x00;
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
    
    if(digitalRead(HOST_INPUT1) == HIGH){
      myNodeInfo.pattern = 0x01;
    }
    if(digitalRead(HOST_INPUT2) == HIGH){
      myNodeInfo.pattern = 0x02;
    }
    if(digitalRead(HOST_INPUT3) == HIGH){
      myNodeInfo.pattern = 0x03; 
    }
    if(digitalRead(HOST_INPUT4) == HIGH){
      myNodeInfo.pattern = 0x04;
    }
    if(digitalRead(HOST_INPUT5) == HIGH){
      myNodeInfo.pattern = 0x05;
    }
    
    sendData(NULL);
    delay(2000);
  }
  else {
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

    //get host's local MAC address
    uint8_t hostLocalMACAdd[localMACAddMap[hostMACAdd].size()];
    std::copy(localMACAddMap[hostMACAdd].begin(), localMACAddMap[hostMACAdd].end(), hostLocalMACAdd);
     
    sendData(hostLocalMACAdd); 
    delay(1000);
  }

}







void sendCallBackFunction(uint8_t* mac, uint8_t sendStatus) {
  Serial.println(sendStatus == ESP_OK ? "Delivery Success!" : "Delivery Fail.");
}

void receiveCallBackFunction(uint8_t *senderMAC, uint8_t *incomingData, uint8_t len) {
  Serial.printf("Message from %02x:%02x:%02x:%02x:%02x:%02x: ", senderMAC[0], senderMAC[1], senderMAC[2], senderMAC[3], senderMAC[4], senderMAC[5]);
  if(myNodeInfo.isHost){
    memcpy(&sentInfo, incomingData, len);
    Serial.printf("A is %d, ", sentInfo.A);
    Serial.printf("B is %d\n\r", sentInfo.B);
    if(sentInfo.A){
      digitalWrite(HOST_OUTPUT1, HIGH);
    }
    else{
      digitalWrite(HOST_OUTPUT1, LOW);
    }

    if(sentInfo.B){
      digitalWrite(HOST_OUTPUT2, HIGH);
    }
    else{
      digitalWrite(HOST_OUTPUT2, LOW);
    }
    if(esp_now_add_peer(senderMAC, ESP_NOW_ROLE_COMBO, channel, NULL, 0) == ESP_OK){
      Serial.printf("New node connected: %02x:%02x:%02x:%02x:%02x:%02x\n\r", senderMAC[0], senderMAC[1], senderMAC[2], senderMAC[3], senderMAC[4], senderMAC[5]);
    }
  }
  else{
    memcpy(&sentInfo, incomingData, len);
    Serial.printf("Pattern %d selected.\n\r", sentInfo.pattern);
  }
}

void initESPNow() {
  if(myNodeInfo.isHost){
    WiFi.mode(WIFI_AP); // Host mode for esp-now host
    uint8_t localMACAdd[localMACAddMap[WiFi.macAddress().c_str()].size()];
    std::copy(localMACAddMap[WiFi.macAddress().c_str()].begin(), localMACAddMap[WiFi.macAddress().c_str()].end(), localMACAdd); 
    if(!wifi_set_macaddr(SOFTAP_IF, localMACAdd)){
      Serial.println("MAC Address not properly set. Try a different MAC address.");
    }
    else{
      Serial.printf("Local MAC Address Set To %02x:%02x:%02x:%02x:%02x:%02x\n\r", localMACAdd[0], localMACAdd[1], localMACAdd[2], localMACAdd[3], localMACAdd[4], localMACAdd[5]);
    }
    WiFi.disconnect();
    
    Serial.println("This is an ESP-Now host.");
  }
  else{
     WiFi.mode(WIFI_STA); // Station mode for esp-now node
     //get corresponding local MAC address from map
     uint8_t localMACAdd[localMACAddMap[WiFi.macAddress().c_str()].size()];
     std::copy(localMACAddMap[WiFi.macAddress().c_str()].begin(), localMACAddMap[WiFi.macAddress().c_str()].end(), localMACAdd);
     
     if(!wifi_set_macaddr(STATION_IF, localMACAdd)){
      Serial.println("MAC Address not properly set. Try a different MAC address.");
     }
     else{
      Serial.printf("Local MAC Address Set To %02x:%02x:%02x:%02x:%02x:%02x\n\r", localMACAdd[0], localMACAdd[1], localMACAdd[2], localMACAdd[3], localMACAdd[4], localMACAdd[5]); 
     }
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
