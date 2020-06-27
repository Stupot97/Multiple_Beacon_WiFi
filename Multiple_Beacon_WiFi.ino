#include <espnow.h>
#include <ESP8266WiFi.h>
#include <string.h>


//Macros

#define PORT 80
#define CHANNEL 1
#define ESP_OK 0

//-----Multiple Beacon WiFi Project----//
//-----Author: Stuart D'Amico----------//
//
// Last Date Modified: 6/27/20
// 
// Description: This project is designed to have several node arduinos connected to a host arduino. Whenever an 
// event is triggered on a node arduino, the host reacts to this and activates the corresponding LEDs.
// The host arduino acts as a server while the node arduinos act as clients. Both the hosts and clients send data
// back and forth to each other to ensure arduinos are still active and are responding properly.
// All the software is the same on host and client arduinos, so whether an arduino is a host
// or client is determined by its MAC Address.
//
// Progress: At the moment, this project successfully establishes connections between the host and the nodes.
// I am currently working on figuring out why the nodes do not successfully send to the host and why two of my ESP8266 boards
// do not produce any serial output. 


//Global Variables
char hostMACAdd [18] = "F4:CF:A2:D4:40:F8"; //change this MAC address to change the host node 
String SSID = "Host Arduino"; //change this SSID to change the network name
String Password = "7x?j9h*3d("; //change this to change the password required to connect. If node, connect to SSID with password given

bool isHost; //flag that controls whether host or node code executes
bool A = true; //flag for event A
bool B = false; //flag for event B

//Server-Specific Globals
bool haveMessage;        //global for detecting readings

//Client-Specific Globals


//Function Prototypes
uint8_t * MACAddrToInt(String MAC);
void configDeviceAP(void);
void initESPNow();
void wifiConnect();

void setup() {
    Serial.begin(115200);
    Serial.println();
    delay(500);
    
    //is it a host or node?
    if(strcmp(WiFi.macAddress().c_str(),hostMACAdd)){
      //node
      isHost = false;
      uint8_t* intMAC = MACAddrToInt(hostMACAdd);

      Serial.println("Client powered!");
      Serial.println();

      //set device in station mode if it's a node
      WiFi.mode(WIFI_STA); // Station mode for esp-now sensor node

      //Enable ESP Now
      initESPNow();
      
      Serial.println("Initialization successful!");
      
      //attempt to connect to host
      Serial.println("Connecting to " + SSID);

      esp_now_set_self_role(ESP_NOW_ROLE_COMBO); 
      if(esp_now_add_peer(intMAC, ESP_NOW_ROLE_SLAVE, CHANNEL, NULL, 0)!=ESP_OK){
        Serial.println("Error connecting to host.");
        //TODO: make this station a host and a node if node limit reached
      }

      Serial.println("Connection to host successful!");
    }
    else{
      //host
      isHost = true;
      Serial.println("Host powered!");
      Serial.println();

      //set device in AP mode if it's the host
      WiFi.mode(WIFI_AP);
      wifi_set_macaddr(SOFTAP_IF, &intMAC[0]);
      
      //Enable ESP Now
      initESPNow();
      
      Serial.println("Initialization successful!");
      esp_now_set_self_role(ESP_NOW_ROLE_COMBO); 
      
      int unsuccessfullyRegistered = esp_now_register_recv_cb([](uint8_t *mac, uint8_t *data, uint8_t len) {
      
        String deviceMac = "";
        deviceMac += String(mac[0], HEX);
        deviceMac += String(mac[1], HEX);
        deviceMac += String(mac[2], HEX);
        deviceMac += String(mac[3], HEX);
        deviceMac += String(mac[4], HEX);
        deviceMac += String(mac[5], HEX);
        
    
        Serial.println("Message received from device: "); 
        Serial.println(deviceMac);
        Serial.println(*data, DEC);
        //haveMessage = true;
      });

      if(!unsuccessfullyRegistered){
        Serial.println("Callback function registered!");
      }
      else{
        Serial.println("Callback function not registered.");
      }

      Serial.println("Waiting for clients...");
    }
}

void loop() {
  
  if(isHost){
    
  }
  else{
    
    String MAC = WiFi.macAddress();
    uint8_t flagMsg;                  //message for events A and B.
    
    
    //             Inputs | Output
    //                    |
    //             A   B  |  flagMsg
    //             0   0  |  0
    //             0   1  |  1
    //             1   0  |  2
    //             1   1  |  3
    
    
    //FOR TESTING
    A = true;
    B = true;
    
    if(A){
      if(B){
        flagMsg = 3;
      }
      else{
        flagMsg = 2;  
      }
    }
    else{
      if(B){
        flagMsg = 1;  
      }
      else{
        flagMsg = 0;
      }
    }
    
    //String message = IP + " " + String(flagMsg); //maybe add pattern to this as well
    
    uint8_t * intMAC = MACAddrToInt(hostMACAdd);
    
    
    while(esp_now_send(intMAC, &flagMsg, sizeof(flagMsg))!=ESP_OK){
      Serial.println("Failed to send.");
      delay(2000);
    }
    
    Serial.println("Message Sent!");
    delay(5000);
  }
   
}









//convert MAC Address string to int
uint8_t * MACAddrToInt(String MAC){
  uint8_t hostMACAddArray [6];
  int values[6];
  
  sscanf(hostMACAdd, "%x:%x:%x:%x:%x:%x", &values[0], &values[1], &values[2], &values[3], &values[4], &values[5]);
  
  for(int i = 0; i<6; ++i){
    hostMACAddArray[i] = (uint8_t) values[i];
  }
  return hostMACAddArray;
}




//set as Access Point
void configDeviceAP(){
  bool result = WiFi.softAP(SSID.c_str(), Password.c_str(), CHANNEL, 0);
  if (!result)
  {
    Serial.printf("AP Config failed.");
  }
  else
  {
    Serial.printf("AP Config Success. Broadcasting with AP: ");
    Serial.printf("%s", SSID.c_str());
  }
}

void initESPNow(){
  WiFi.disconnect();
  if (esp_now_init() == ESP_OK) {
    Serial.println("ESPNow Init Success");
  }
  else
  {
    Serial.println("ESPNow Init Failed");
    ESP.restart();
  }
}


void wifiConnect() {
  WiFi.mode(WIFI_STA);
  Serial.printf("");
  Serial.printf("Connecting... "); //Serial.printf("%s",SSID.c_str());
  WiFi.begin(SSID, Password);
  while (WiFi.status() != WL_CONNECTED) {
     delay(250);
     Serial.print(".");
  }  
  Serial.print("\nWiFi connected."); //Serial.printf("%s",WiFi.localIP().c_str());
}
