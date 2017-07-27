

/*
 * The circuit:
 * LCD RS pin to digital pin 6
 * LCD Enable pin to digital pin 10
 * LCD D4 pin to digital pin 5
 * LCD D5 pin to digital pin 4
 * LCD D6 pin to digital pin 3
 * LCD D7 pin to digital pin 2
 * LCD R/W pin to ground
 * 10K resistor:
 * ends to +5V and ground
 * wiper to LCD VO pin (pin 3)
 */
#include <Adafruit_Sensor.h>
#include "DHT.h"
#include "RF24.h"
#include "RF24Network.h"
#include "RF24Mesh.h"
#include <SPI.h>


/**** Configure the nrf24l01 CE and CS pins ****/
RF24 radio(7,8);
RF24Network network(radio);
RF24Mesh mesh(radio, network);

// Initialize DHT sensor
#define DHTPIN 9
// specifiy DHT sensor
#define DHTTYPE DHT11
// initialize
DHT dht(DHTPIN, DHTTYPE);

/**
   User Configuration: nodeID - A unique identifier for each radio. Allows addressing
   to change dynamically with physical changes to the mesh.

   Configuration takes place below, prior to uploading the sketch to the device
   A unique value from 1-255 must be configured for each node.
   This will be stored in EEPROM on AVR devices, so remains persistent between further uploads, loss of power, etc.

 **/
#define nodeID 10
uint32_t displayTimer = 0;

void setup() {  
  dht.begin();
  Serial.begin(115200);

  // Set the nodeID manually
  mesh.setNodeID(nodeID);
  //connect to the mesh
  Serial.println(F("Connecting to the mesh..."));
  // configure the mesh and request an address
  mesh.begin();
}

void loop() {

  // similiar to network.update(), called regularly to keep network & mesh going
  mesh.update();

  nodeload_t nodeload;
  
  nodeload.temp = dht.readTemperature(true);
  nodeload.id = nodeID;
  // payload.humid = dht.readHumidity();

// Increase 1000 to slow sending of payload (nodeload)
if (millis() - displayTimer > 1000) {
  
  displayTimer = millis();
  if (! (isnan(nodeload.temp) )) {
    
    if (! ( mesh.write(&nodeload,'M', sizeof(nodeload)) )) {
      
      if (! mesh.checkConnection() ) {
        Serial.println("Renewing Address");
        mesh.renewAddress();
        
      } else {
        Serial.println("Send fail, Test OK");
      }
      
    } else {
      Serial.print("Send Ok:"); 
      Serial.print("Temp:"); Serial.print(nodeload.temp); 
     // Serial.print("Time:"); Serial.print( nodeload.time); 
      Serial.print(" nodeID:"); Serial.println(nodeload.id);
    }
  }
}
  while (network.available()) {
    RF24NetworkHeader header;
    network.read(header, &nodeload, sizeof(nodeload));
    Serial.print("Received temp: ");
    Serial.print(nodeload.temp);
  }

  
}

