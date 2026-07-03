/* TAG - SIMPLE BROADCASTER
   - Logic: Just broadcasts "TAG_KEYS" robustly.
   - No Buzzer/Button logic (keeps it simple).
*/
#include <BLEDevice.h>
#include <BLEUtils.h>
#include <BLEServer.h>

#define DEVICE_NAME "TAG_KEYS" 

void setup() {
  Serial.begin(115200);
  
  BLEDevice::init(DEVICE_NAME);
  BLEServer *pServer = BLEDevice::createServer();
  
  // Dummy Service to be visible
  BLEService *pService = pServer->createService("12345678-1234-1234-1234-123456789012");
  pService->start();

  // Advertising
  BLEAdvertising *pAdvertising = BLEDevice::getAdvertising();
  pAdvertising->addServiceUUID("12345678-1234-1234-1234-123456789012");
  pAdvertising->setScanResponse(true); 
  pAdvertising->setMinPreferred(0x06);  
  pAdvertising->start();
  
  Serial.println("TAG BROADCASTING: " DEVICE_NAME);
}

void loop() {
  delay(1000); 
}