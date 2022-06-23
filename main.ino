// rotary encode
#include <AiEsp32RotaryEncoder.h>
#include <AiEsp32RotaryEncoderNumberSelector.h>
// ble
#include <BLEDevice.h>
#include <BLEUtils.h>
#include <BLEServer.h>
#include <BLE2902.h>

#include <math.h>


// ble definitions

// ble vars
#define SERVICE_UUID              "4fafc201-1fb5-459e-8fcc-c5c9c331914b"
#define ROT1_CHARACTERISTIC_UUID  "667f1c78-be2e-11ec-9d64-0242ac120002"
#define ROT2_CHARACTERISTIC_UUID  "667f1c78-be2e-11ec-9d64-0242ac120003"
#define ANCHOR_POS_CHARACTERISTIC_UUID  "667f1c78-be2e-11ec-9d64-0242ac120004"
BLECharacteristic *rot1Char;
BLECharacteristic *rot2Char;
BLECharacteristic *anchorPosChar;

// pins for nfc server
#define NFC_RST 23;
#define NFC_SDA 13;
#define NFC_SCK 12;
#define NFC_MOSI 14;
#define NFC_MISO 27;



// pins for rotary encoder
// rot 1
#define ROT1_A_PIN 34
#define ROT1_B_PIN 35
#define ROT1_BUTTON_PIN 32
// rot 2
#define ROT2_A_PIN 33
#define ROT2_B_PIN 25
#define ROT2_BUTTON_PIN 26

// vars vor rotary encoder
#define ROTARY_ENCODER_STEPS 4
AiEsp32RotaryEncoder *rot1 = new AiEsp32RotaryEncoder(ROT1_A_PIN, ROT1_B_PIN, ROT1_BUTTON_PIN, -1, ROTARY_ENCODER_STEPS);
AiEsp32RotaryEncoder *rot2 = new AiEsp32RotaryEncoder(ROT2_A_PIN, ROT2_B_PIN, ROT2_BUTTON_PIN, -1, ROTARY_ENCODER_STEPS);
AiEsp32RotaryEncoderNumberSelector rotNum1 = AiEsp32RotaryEncoderNumberSelector();
AiEsp32RotaryEncoderNumberSelector rotNum2 = AiEsp32RotaryEncoderNumberSelector();


// anchor positions
int anchor1x;
int anchor1y;

int anchor2x;
int anchor2y;

int posX;
int posY;

// interrupts for rotary encoder
// rot 1
void IRAM_ATTR readEncoder1ISR()
{
  rot1->readEncoder_ISR();


}
// rot 2
void IRAM_ATTR readEncoder2ISR()
{
  rot2->readEncoder_ISR();

}
// ble callbacks
// connection callbacks
bool _BLEClientConnected = false;
class MyServerCallbacks : public BLEServerCallbacks {
    void onConnect(BLEServer* pServer) {
      Serial.println("\n*******************\n A device has connected\n*******************\n");
      _BLEClientConnected = true;
    };

    void onDisconnect(BLEServer* pServer) {
      Serial.println("\n*******************\n A device has disconnected\n*******************\n");
      _BLEClientConnected = false;
      BLEDevice::startAdvertising();
    }
};

// characteristics callbacks
class RotCharCallback: public BLECharacteristicCallbacks {
    void onRead(BLECharacteristic* lpCharacteristic) {
      String value = "";
      String uuid = lpCharacteristic->getUUID().toString().c_str();

      if (uuid == ROT1_CHARACTERISTIC_UUID) {
        value = String(abs(rotNum1.getValue()));
      } else if (uuid == ROT2_CHARACTERISTIC_UUID) {
        value = String(abs(rotNum2.getValue()));
      } else {
        return;
      }


      lpCharacteristic->setValue(value.c_str());
    }
    void onWrite(BLECharacteristic* lpchar) {
      Serial.println(lpchar->getUUID().toString().c_str());
      Serial.println(lpchar->getValue().c_str());

      if (lpchar->getUUID().toString() == "667f1c78-be2e-11ec-9d64-0242ac120004") {
        Serial.println("anchor update");

        parseAnchorPositionString(lpchar->getValue().c_str());
      }
    }
};


class Anchor {

  public:

    Anchor() {
      this->x =0;
      this->y=0;
    }

    void update(String updatePos, char splitChar = '|') {
      int sepPos = updatePos.indexOf(splitChar);
      this->x = updatePos.substring(0, sepPos).toInt();
      this->y = updatePos.substring(sepPos + 1).toInt();
    }

    double distanceToCoords(int otherX, int otherY) {
      return sqrt(pow(this->x - otherX, 2) + pow(this->y - otherY, 2));
    }

    int x;
    int y;

    String toString() {
      return String(this->x) + "|" + String(this->y);
    }

};


Anchor anchors[] = {Anchor(), Anchor(), Anchor()};
char anchorsSplitChar = ':';
void parseAnchorPositionString(String anchorsPos) {

  int sepPos;
  int counter = 0;
  while ((sepPos = anchorsPos.indexOf(anchorsSplitChar)) != -1 && counter < 2){

    String x = anchorsPos.substring(0, sepPos);
    anchorsPos = anchorsPos.substring(sepPos + 1);
    anchors[counter].update(x);

    Serial.println(anchors[counter].toString());

    counter++;
  }
  anchors[counter].update(anchorsPos);
  
  

  for (int i=0; i < 3; i++) {
    Serial.println(anchors[i].toString());
  }
}


void setup()
{
  Serial.begin(115200);

  // rotary encoder setup
  Serial.println("setting up rotary encoder");
  rot1->begin();
  rot1->setup(readEncoder1ISR);
  rotNum1.attachEncoder(rot1);


  rot2->begin();
  rot2->setup(readEncoder2ISR);
  rotNum2.attachEncoder(rot2);

  rotNum1.setRange(-21474836, 21474836, 1, false, 0);
  rotNum1.setValue(0);
  rotNum2.setRange(-2147483, 21474836, 1, false, 0);
  rotNum2.setValue(0);
  Serial.println("rotary encoder setup completed");

  // ble setup
  Serial.println("starting ble setup");
  BLEDevice::init("ESP32 TEST DEVICE");
  BLEServer *pServer = BLEDevice::createServer();

  pServer->setCallbacks(new MyServerCallbacks());
  BLEService *pService = pServer->createService(SERVICE_UUID);

  rot1Char = pService->createCharacteristic(
               ROT1_CHARACTERISTIC_UUID,
               BLECharacteristic::PROPERTY_READ   |
               BLECharacteristic::PROPERTY_NOTIFY
             );
  rot1Char->addDescriptor(new BLE2902());
  rot1Char->setCallbacks(new RotCharCallback());

  rot2Char = pService->createCharacteristic(
               ROT2_CHARACTERISTIC_UUID,
               BLECharacteristic::PROPERTY_READ   |
               BLECharacteristic::PROPERTY_NOTIFY
             );
  rot2Char->addDescriptor(new BLE2902());
  rot2Char->setCallbacks(new RotCharCallback());

  anchorPosChar = pService->createCharacteristic(
                    ANCHOR_POS_CHARACTERISTIC_UUID,
                    BLECharacteristic::PROPERTY_WRITE
                  );
  anchorPosChar->addDescriptor(new BLE2902());
  anchorPosChar->setCallbacks(new RotCharCallback());


  pService->start();
  // BLEAdvertising *pAdvertising = pServer->getAdvertising();  // this still is working for backward compatibility
  BLEAdvertising *pAdvertising = BLEDevice::getAdvertising();
  pAdvertising->addServiceUUID(SERVICE_UUID);
  pAdvertising->setScanResponse(true);
  pAdvertising->setMinPreferred(0x06);  // functions that help with iPhone connections issue
  pAdvertising->setMinPreferred(0x12);
  BLEDevice::startAdvertising();
  Serial.println("Characteristic defined! Now you can read it in your phone!");


  posX = rotNum1.getValue();
  posY = rotNum2.getValue();
}

void loop()
{


  bool rot1c = rot1->encoderChanged();
  bool rot2c = rot2->encoderChanged();
  if (rot1c)
  {
    Serial.print("rot1: ");
    Serial.println(abs(rotNum1.getValue()));
    posX = rotNum1.getValue();
  }

  if (rot2c)
  {
    Serial.print("rot2: ");
    Serial.println(abs(rotNum2.getValue()));
    posY = rotNum2.getValue();
  }

  if (rot1c || rot2c) {
    Serial.println(posX);
    Serial.println(posY);
    Serial.print("Anchor 1: \t"); Serial.println(anchors[0].distanceToCoords(posX, posY));
    Serial.print("Anchor 2: \t"); Serial.println(anchors[1].distanceToCoords(posX, posY));

    rot1Char->setValue(String(anchors[0].distanceToCoords(posX, posY)).c_str());
    rot2Char->setValue(String(anchors[1].distanceToCoords(posX, posY)).c_str());
    rot1Char->notify();
    rot2Char->notify();
  }


}

