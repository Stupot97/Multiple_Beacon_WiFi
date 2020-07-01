#include <espnow.h>
#include <ESP8266WiFi.h>
#include <string.h>

//Macros
#define RETRY_INTERVAL 5000
#define ESP_OK 0

//-----Multiple Beacon WiFi Project----//
//-----Author: Stuart D'Amico----------//
//
// Last Date Modified: 7/1/20
//
// Description: This project is designed to have several node arduinos connected to a host arduino. Whenever an
// event is triggered on a node arduino, the host reacts to this and activates the corresponding LEDs.
// The host arduino acts as a server while the node arduinos act as clients. Both the hosts and clients send data
// back and forth to each other to ensure arduinos are still active and are responding properly.
// All the software is the same on host and client arduinos, so whether an arduino is a host
// or client is determined by its MAC Address.
//
// Progress: At the moment, this project successfully establishes connections between the host and the nodes.
// I just figured out how to successfully send data between node and host, and now I want to figure out how to send data between host and node.


//Data Structures
struct __attribute__((packed)) NodeInfo {
  bool A; //event A has occured or is occuring
  bool B; //event B has occured or is occuring
  //maybe add pattern to this as well
};


//Global Variables
char hostMACAdd [18] = "8C:AA:B5:0D:FB:A4"; //change this MAC address to change the host node
char localMACAdd [18] = "82:88:88:88:88:88"; //locally administered MAC address
bool isHost; //flag that controls whether host or node code executes
const uint8_t channel = 14;

//Server-Specific Globals

//Client-Specific Globals
NodeInfo myNodeInfo;



//Function Prototypes
uint8_t * MACAddToInt(String MAC);
void initESPNow();
void sendData();
void sendCallBackFunction(uint8_t* mac, uint8_t sendStatus);
void receiveCallBackFunction(uint8_t *senderMac, uint8_t *incomingData, uint8_t len);



void setup() {
  Serial.begin(74880);
  Serial.println();

  Serial.println("MAC Address: " + WiFi.macAddress());

  //uint8_t* localIntMAC = MACAddToInt(localMACAdd);
  uint8_t localIntMAC [] = {0x82,0x88,0x88,0x88,0x88,0x88};
  
  //is it a host or node?
  if (strcmp(WiFi.macAddress().c_str(), hostMACAdd)) {
    //node
    isHost = false;


    //Enable ESP Now
    initESPNow();

    esp_now_set_self_role(ESP_NOW_ROLE_CONTROLLER); //change to combo
    if (esp_now_add_peer(localIntMAC, ESP_NOW_ROLE_SLAVE, channel, NULL, 0) != ESP_OK) {
      Serial.println("Error connecting to host.");
      //TODO: make this station a host and a node if node limit reached
    }

    int unsuccessfullyRegistered = esp_now_register_send_cb(sendCallBackFunction);
    
    if (!unsuccessfullyRegistered) {
      Serial.println("Callback function registered!");
    }
    else {
      Serial.println("Callback function not registered.");
    }
  }
  else {
    //host
    isHost = true;
    
    //Enable ESP Now
    initESPNow();

    esp_now_set_self_role(ESP_NOW_ROLE_SLAVE); //change to combo

    int unsuccessfullyRegistered = esp_now_register_recv_cb(receiveCallBackFunction);
    if (!unsuccessfullyRegistered) {
      Serial.println("Callback function registered!");
    }
    else {
      Serial.println("Callback function not registered.");
    }
    
    Serial.println("Waiting for messages...");
  }
}

void loop() {

  if (isHost) {

  }
  else {

    //FOR TESTING
    myNodeInfo.A = true;
    myNodeInfo.B = true;


    //uint8_t * localIntMAC = MACAddToInt(localMACAdd);
    uint8_t localIntMAC [] = {0x82,0x88,0x88,0x88,0x88,0x88};
    sendData(localIntMAC); 
    delay(1000);
  }

}







void sendCallBackFunction(uint8_t* mac, uint8_t sendStatus) {
  Serial.println(sendStatus == ESP_OK ? "Delivery Success!" : "Delivery Fail.");
}

void receiveCallBackFunction(uint8_t *senderMac, uint8_t *incomingData, uint8_t len) {
  memcpy(&myNodeInfo, incomingData, len);
  Serial.printf("Message from %02x:%02x:%02x:%02x:%02x:%02x: ", senderMac[0], senderMac[1], senderMac[2], senderMac[3], senderMac[4], senderMac[5]);
  Serial.printf("A is %d", myNodeInfo.A);
  Serial.printf("B is %d", myNodeInfo.B);
}

//convert MAC Address string to int
uint8_t * MACAddToInt(String MAC) {
  uint8_t hostMACAddArray [6];
  int values[6];

  sscanf(hostMACAdd, "%x:%x:%x:%x:%x:%x", &values[0], &values[1], &values[2], &values[3], &values[4], &values[5]);

  for (int i = 0; i < 6; ++i) {
    hostMACAddArray[i] = (uint8_t) values[i];
  }
  return hostMACAddArray;
}


void initESPNow() {
  if(isHost){
    WiFi.mode(WIFI_AP); // Host mode for esp-now host
    //uint8_t * localIntMAC = MACAddToInt(localMACAdd);
    uint8_t localIntMAC [] = {0x82,0x88,0x88,0x88,0x88,0x88};  //0x8C,0xAA,0xB5,0x0D,0xFB,0xA4
    if(!wifi_set_macaddr(SOFTAP_IF, localIntMAC)){
      Serial.println("MAC Address not properly set. Try a different MAC address.");
    }
    Serial.println("MAC Address: " + WiFi.macAddress());
    WiFi.disconnect();
    
    Serial.println("This is an ESP-Now host.");
  }
  else{
     WiFi.mode(WIFI_STA); // Station mode for esp-now node
     WiFi.disconnect();
     Serial.println("This is an ESP-Now node.");
  }
  if (esp_now_init() != 0) {
    Serial.println("ESP_Now init failed...");
    delay(RETRY_INTERVAL);
    ESP.restart();
  }
}

void sendData(uint8_t * destination) {
  uint8_t message[sizeof(myNodeInfo)];
  memcpy(message, &myNodeInfo, sizeof(myNodeInfo));

  esp_now_send(destination, message, sizeof(myNodeInfo));
}
