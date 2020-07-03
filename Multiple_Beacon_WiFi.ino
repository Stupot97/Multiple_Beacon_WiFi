#include <espnow.h>
#include <ESP8266WiFi.h>
#include <string.h>
#include <map>

//Macros
#define RETRY_INTERVAL 5000
#define ESP_OK 0

//-----Multiple Beacon WiFi Project----//
//-----Author: Stuart D'Amico----------//
//
// Last Date Modified: 7/3/20
//
// Description: This project is designed to have several node arduinos connected to a host arduino. Whenever an
// event is triggered on a node arduino, the host reacts to this and activates the corresponding LEDs.
// The host arduino acts as a server while the node arduinos act as clients. Both the hosts and clients send data
// back and forth to each other to ensure arduinos are still active and are responding properly.
// All the software is the same on host and client arduinos, so whether an arduino is a host
// or client is determined by its MAC Address.
//
// Progress: At the moment, this project successfully establishes two-way connections between the host and the nodes.
// Now I want to refine the communication process and involve switch interrupts for events A and B.


//Data Structures
struct __attribute__((packed)) NodeInfo {
  bool isHost; //flag that controls whether host or node code executes
  uint8_t pattern = 0x00;
  bool A; //event A has occured or is occuring
  bool B; //event B has occured or is occuring
};


//Global Variables
char hostMACAdd [18] = "8C:AA:B5:0D:FB:A4"; //change this MAC address to change the host node
uint8_t localMACAdd [] = {0x82,0x88,0x88,0x88,0x88,0x88}; //locally administered MAC address
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

    //Enable ESP Now
    initESPNow();

    esp_now_set_self_role(ESP_NOW_ROLE_COMBO); //controller
    //get corresponding local MAC address from map
    localMACAddMap[WiFi.macAddress().c_str()];
    uint8_t localMACAdd[localMACAddMap[WiFi.macAddress().c_str()].size()];
    std::copy(localMACAddMap[WiFi.macAddress().c_str()].begin(), localMACAddMap[WiFi.macAddress().c_str()].end(), localMACAdd);
    
    if (esp_now_add_peer(localMACAdd, ESP_NOW_ROLE_COMBO, channel, NULL, 0) != ESP_OK) {
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
    
    //Enable ESP Now
    initESPNow();

    esp_now_set_self_role(ESP_NOW_ROLE_COMBO); //slave

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
    
    sendData(NULL);
    delay(2000);
  }
  else {
    //node

    //FOR TESTING
    myNodeInfo.A = true;
    myNodeInfo.B = true;
  
    sendData(localMACAdd); 
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
    Serial.printf("B is %d\n", sentInfo.B);
    if(esp_now_add_peer(senderMAC, ESP_NOW_ROLE_COMBO, channel, NULL, 0) == ESP_OK){
      Serial.printf("New node connected: %02x:%02x:%02x:%02x:%02x:%02x\n", senderMAC[0], senderMAC[1], senderMAC[2], senderMAC[3], senderMAC[4], senderMAC[5]);
    }
  }
  else{
    memcpy(&sentInfo, incomingData, len);
    Serial.printf("Pattern %d selected.\n", sentInfo.pattern);
  }
}

void initESPNow() {
  if(myNodeInfo.isHost){
    WiFi.mode(WIFI_AP); // Host mode for esp-now host
    if(!wifi_set_macaddr(SOFTAP_IF, localMACAdd)){
      Serial.println("MAC Address not properly set. Try a different MAC address.");
    }
    WiFi.disconnect();
    
    Serial.println("This is an ESP-Now host.");
  }
  else{
     WiFi.mode(WIFI_STA); // Station mode for esp-now node
     //get corresponding local MAC address from map
     localMACAddMap[WiFi.macAddress().c_str()];
     uint8_t localMACAdd[localMACAddMap[WiFi.macAddress().c_str()].size()];
     std::copy(localMACAddMap[WiFi.macAddress().c_str()].begin(), localMACAddMap[WiFi.macAddress().c_str()].end(), localMACAdd);
     
     if(!wifi_set_macaddr(STATION_IF, localMACAdd)){
      Serial.println("MAC Address not properly set. Try a different MAC address.");
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
