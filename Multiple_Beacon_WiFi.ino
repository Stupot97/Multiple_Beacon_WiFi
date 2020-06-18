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
// Last Date Modified: 6/17/20
// 
// Description: This project is designed to have several node arduinos connected to a host arduino. Whenever an 
// event is triggered on a node arduino, the host reacts to this and activates the corresponding LEDs.
// The host arduino acts as a server while the node arduinos act as clients. Both the hosts and clients send data
// back and forth to each other to ensure arduinos are still active and are responding properly.
// All the software is the same on host and client arduinos, so whether an arduino is a host
// or client is determined by its MAC Address.


//Global Variables
char hostMACAdd [18] = "8C:AA:B5:0D:F8:AC"; //change this MAC address to change the host node 
String SSID = "Host Arduino"; //change this SSID to change the network name
String Password = "7x?j9h*3d("; //change this to change the password required to connect. If node, connect to SSID with password given

bool isHost; //flag that controls whether host or node code executes


//Server-Specific Globals
WiFiServer server(PORT); // change PORT macro to change access port

//Client-Specific Globals


//Function Prototypes
uint8_t * MACAddrToInt(String MAC);
void configDeviceAP(void);
void initESPNow();


void setup() {
    Serial.begin(115200);
    Serial.println();
    Serial.println();
    delay(500);
    
    //is it a host or node?
    if(strcmp(WiFi.macAddress().c_str(),hostMACAdd)){
    //node
      isHost = false;

      int dotCount;                                       //variable used to count the amount of dots printed to serial while waiting
      //attempt to connect to host
      Serial.printf("Connecting to %s ", SSID.c_str());
      WiFi.begin(SSID, Password);
      while (WiFi.status() != WL_CONNECTED)
        {
          delay(500);
          if(dotCount<3){                                 //dot count limited to 3 at a time 
            Serial.print(".");                            
            ++dotCount;
          }
          else{
            dotCount = 0;
            Serial.println();  
          }
        }
        dotCount = 0;                                    //reset dotCount
      Serial.println(" connected");
    }
    else{
      //host
      isHost = true;
      Serial.println("Host powered!");
      Serial.println();

      
      //set device in AP mode if it's the host
      WiFi.mode(WIFI_AP);
      configDeviceAP();
  
      //Enable ESP Now in case WiFi isn't available
      initESPNow();
  
      Serial.println("Waiting for clients...");
    }
}

void loop() {
  
  if(isHost){
    WiFiClient client = server.available(); //listen for incoming clients

    //if client connects
    if(client){
      Serial.println("New Client.");
      String currentLine = "";              //String to hold incoming data
      while(client.connected()){
        if(client.available()){             //if there's bytes to read from client
          char c = client.read();           //read byte
          Serial.print(c);
          /*
          if(c != '\r'){
            if(c == '\n'){
              currentLine = "";
              Serial.println();
            }
            else{
              currentLine+=c;
            } 
          }
*/
           
        }  
      }
    }
   delay(1000);  
  }
  else{
    while (WiFi.status() == WL_CONNECTED)
        {
          String IP = WiFi.localIP().toString();
          uint8_t flagMsg;                  //message for events A and B.


          //             Inputs | Output
          //                    |
          //             A   B  |  flagMsg
          //             0   0  |  0
          //             0   1  |  1
          //             1   0  |  2
          //             1   1  |  3

          //event A flag
          //remove
          bool A = true;
          bool B = false;

          
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

          /*
          while(esp_now_send(intMAC, &flagMsg, sizeof(flagMsg))!=ESP_OK){
            Serial.println("Failed to send.");
          }
          */
          Serial.println("Message Sent!");
          delay(5000);
        }
    
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
    Serial.println("AP Config failed.");
  }
  else
  {
    Serial.println("AP Config Success. Broadcasting with AP: " + String(SSID));
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
