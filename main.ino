#include <BLEDevice.h>
#include <BLEUtils.h>
#include <BLEServer.h>
#include "BLE2902.h"
#include "BLEHIDDevice.h"
#include "HIDTypes.h"
#include "HIDKeyboardTypes.h"
#include <driver/adc.h>

BLEHIDDevice* hid;
BLECharacteristic* input;
BLECharacteristic* output;


bool connected = false;
int report_key_index = 0;
uint8_t pinRow[] = {36, 39, 34, 35, 32};
uint8_t pinCol[] = {17, 5, 18, 19, 21, 3, 1, 22, 23, 14, 27, 26, 25, 33};
boolean currentState[sizeof(pinCol)][sizeof(pinRow)];
boolean beforeState[sizeof(pinCol)][sizeof(pinRow)];
uint8_t report_key[6] = {0x00, 0x00, 0x00, 0x00, 0x00, 0x00};
uint8_t   myKeymap[2][sizeof(pinRow)][sizeof(pinCol)] = 
          {
						{//Layer0
              {KC_JSW , KC_1   , KC_2   , KC_3   , KC_4   , KC_5   , KC_6   , KC_7   , KC_8   , KC_9   , KC_0   , KC_HYPN, KC_HAT , KC_VBAR},
              {KC_TAB , KC_Q   , KC_W   , KC_E   , KC_R   , KC_T   , KC_Y   , KC_U   , KC_I   , KC_O   , KC_P   , KC_ATMK, KC_LSQB, KC_BSPC},
              {KC_CLCK, KC_A   , KC_S   , KC_D   , KC_F   , KC_G   , KC_H   , KC_J   , KC_K   , KC_L   , KC_SCLN , KC_CLN, KC_RSQB, KC_ENTR},
              {KC_LSFT, KC_Z   , KC_X   , KC_C   , KC_V   , KC_B   , KC_N   , KC_M   , KC_COMM, KC_PROD, KC_SLH , KC_bSLH, KC_UP  , KC_RSFT},
              {KC_LCRL, KC_    , KC_LGUI, KC_LALT, KC_SPC , KC_    , KC_    , KC_    , KC_    , KC_RALT, KC_RCRL, KC_LEFT, KC_DOWN, KC_RGHT}
           	},
            {//Layer1
              {},
              {},
              {},
              {},
              {},
            },
					};


class MyCallbacks : public BLEServerCallbacks {
  void onConnect(BLEServer* pServer){
    connected = true;
    BLE2902* desc = (BLE2902*)input->getDescriptorByUUID(BLEUUID((uint16_t)0x2902));
    desc->setNotifications(true);
  }

  void onDisconnect(BLEServer* pServer){
    connected = false;
    BLE2902* desc = (BLE2902*)input->getDescriptorByUUID(BLEUUID((uint16_t)0x2902));
    desc->setNotifications(false);
  }
};

 class MyOutputCallbacks : public BLECharacteristicCallbacks {
  void onWrite(BLECharacteristic* me){
    uint8_t* value = (uint8_t*)(me->getValue().c_str());
    ESP_LOGI(LOG_TAG, "special keys: %d", *value);
  }
};

void taskServer(void*){
    BLEDevice::init("ESP32-keyboard");
    BLEServer *pServer = BLEDevice::createServer();
    pServer->setCallbacks(new MyCallbacks());

    hid = new BLEHIDDevice(pServer);
    input = hid->inputReport(1); // <-- input REPORTID from report map
    output = hid->outputReport(1); // <-- output REPORTID from report map

    output->setCallbacks(new MyOutputCallbacks());

    std::string name = "chegewara";
    hid->manufacturer()->setValue(name);

    hid->pnp(0x02, 0xe502, 0xa111, 0x0210);
    hid->hidInfo(0x00,0x02);

    BLESecurity *pSecurity = new BLESecurity();
    //  pSecurity->setKeySize();
    pSecurity->setAuthenticationMode(ESP_LE_AUTH_BOND);

    const uint8_t report[] = {
      USAGE_PAGE(1),      0x01,       // Generic Desktop Ctrls
      USAGE(1),           0x06,       // Keyboard
      COLLECTION(1),      0x01,       // Application
      REPORT_ID(1),       0x01,       //   Report ID (1)
      USAGE_PAGE(1),      0x07,       //   Kbrd/Keypad
      USAGE_MINIMUM(1),   0xE0,
      USAGE_MAXIMUM(1),   0xE7,
      LOGICAL_MINIMUM(1), 0x00,
      LOGICAL_MAXIMUM(1), 0x01,
      REPORT_SIZE(1),     0x01,       //   1 byte (Modifier)
      REPORT_COUNT(1),    0x08,
      HIDINPUT(1),        0x02,       //   Data,Var,Abs,No Wrap,Linear,Preferred State,No Null Position
      REPORT_COUNT(1),    0x01,       //   1 byte (Reserved)
      REPORT_SIZE(1),     0x08,
      HIDINPUT(1),        0x01,       //   Const,Array,Abs,No Wrap,Linear,Preferred State,No Null Position
      REPORT_COUNT(1),    0x06,       //   6 bytes (Keys)
      REPORT_SIZE(1),     0x08,
      LOGICAL_MINIMUM(1), 0x00,
      LOGICAL_MAXIMUM(1), 0x65,       //   101 keys
      USAGE_MINIMUM(1),   0x00,
      USAGE_MAXIMUM(1),   0x65,
      HIDINPUT(1),        0x00,       //   Data,Array,Abs,No Wrap,Linear,Preferred State,No Null Position
      REPORT_COUNT(1),    0x05,       //   5 bits (Num lock, Caps lock, Scroll lock, Compose, Kana)
      REPORT_SIZE(1),     0x01,
      USAGE_PAGE(1),      0x08,       //   LEDs
      USAGE_MINIMUM(1),   0x01,       //   Num Lock
      USAGE_MAXIMUM(1),   0x05,       //   Kana
      HIDOUTPUT(1),       0x02,       //   Data,Var,Abs,No Wrap,Linear,Preferred State,No Null Position,Non-volatile
      REPORT_COUNT(1),    0x01,       //   3 bits (Padding)
      REPORT_SIZE(1),     0x03,
      HIDOUTPUT(1),       0x01,       //   Const,Array,Abs,No Wrap,Linear,Preferred State,No Null Position,Non-volatile
      END_COLLECTION(0)
    };

    hid->reportMap((uint8_t*)report, sizeof(report));
    hid->startServices();

    BLEAdvertising *pAdvertising = pServer->getAdvertising();
    pAdvertising->setAppearance(HID_KEYBOARD);
    pAdvertising->addServiceUUID(hid->hidService()->getUUID());
    pAdvertising->start();
    hid->setBatteryLevel(7);

    ESP_LOGD(LOG_TAG, "Advertising started!");
    delay(portMAX_DELAY);
};

void setup() {
  Serial.begin(115200);
  Serial.println("Starting BLE work!");

  for(int row=0;row<sizeof(pinRow);row++){
  	pinMode(pinRow[row], INPUT);
  }
  for(int col=0;col<sizeof(pinCol);col++){
    pinMode(pinCol[col], OUTPUT);
  }

  xTaskCreate(taskServer, "server", 20000, NULL, 5, NULL);
}

void loop() {
  if(connected){
		for(int col=0;col<sizeof(pinCol);col++){
		digitalWrite(pinCol[col], LOW);
			for(int row=0;row<sizeof(pinRow);row++){
				beforeState[col][row] = currentState[col][row];
				currentState[col][row] = 0x00;
			}
		}
		report_key_index = 0;
		for(int i=0;i<6;i++){
			report_key[i]=0x00;
		}

		for(int col=0;col<sizeof(pinCol);col++){
			for(int row=0;row<sizeof(pinRow);row++){
				digitalWrite(pinCol[col], HIGH);
				currentState[col][row] = digitalRead(pinRow[row]);
				if(currentState[col][row] == true){
					report_key[report_key_index] = myKeymap[0][row][col];
					Serial.println(report_key_index);
					Serial.println(row);
					Serial.println(col);
					Serial.println(myKeymap[0][row][col]);
					report_key_index++;
				}
			}
			digitalWrite(pinCol[col], LOW);
		}
		uint8_t report[] = {0x00, 0x00, report_key[0], report_key[1], report_key[2], report_key[3], report_key[4], report_key[5]};
		input -> setValue(report, sizeof(report));
		input -> notify();
		
    Serial.print("{0x0, 0x0, ");
		for(int i=0;i<6;i++){
			Serial.print("0x");
			Serial.print(report_key[i], HEX);
			Serial.print(", ");
		}
		Serial.println("}");
	}
  delay(30);
}