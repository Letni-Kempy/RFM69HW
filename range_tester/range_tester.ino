#include <SPI.h>
#include <RH_RF69.h>

#include <LiquidCrystal.h>
/************ Radio Setup ***************/

// Change to 434.0 or other frequency, must match RX's freq!
#define RF69_FREQ 433.0
#define RFM69_SS PA4    //slave select - NSS - 9 or 53
#define RFM69_INT PA0   //interrupt pin - DI00
#define RFM69_RST PA1  // "A" reset, set to analog to preserve DIO pins
#define LED PC13

RH_RF69 rf69(RFM69_SS, RFM69_INT);

// Creates an LCD object. Parameters: (rs, enable, d4, d5, d6, d7)
LiquidCrystal lcd(PA9, PA8, PB15, PB14, PB13, PB12);

//movingAverage()
const int numReadings = 10;
int readings[numReadings];
int readIndex = 0;  
long total = -1000; //sum of readings array

double signalNorm;
int signalStr;
unsigned long interval = 0;

void setup() {
  Serial.begin(115200);
  //while (!Serial) delay(1); // Wait for Serial Console (comment out line if no computer)
  pinMode(LED, OUTPUT);
  pinMode(RFM69_RST, OUTPUT);
  digitalWrite(RFM69_RST, LOW);

  Serial.println("Feather RFM69 RX Test!");
  Serial.println();

  // manual reset
  digitalWrite(RFM69_RST, HIGH);
  delay(10);
  digitalWrite(RFM69_RST, LOW);
  delay(10);

  if (!rf69.init()) {
    Serial.println("RFM69 radio init failed");
    while (1);
  }
  Serial.println("RFM69 radio init OK!");

  // Defaults after init are 434.0MHz, modulation GFSK_Rb250Fd250, +13dbM (for low power module)
  // No encryption
  if (!rf69.setFrequency(RF69_FREQ)) {
    Serial.println("setFrequency failed");
  }

  // If you are using a high power RF69 eg RFM69HW, you *must* set a Tx power with the
  // ishighpowermodule flag set like this:
  rf69.setTxPower(20, true);  // range from 14-20 for power, 2nd arg must be true for 69HCW
  rf69.setModemConfig(RH_RF69::GFSK_Rb9_6Fd19_2);

  // The encryption key has to be the same as the one in the server
  uint8_t keyRF[] = { 0x4C, 0x41, 0x52, 0x50, 0x05, 0x06, 0x07, 0x08,
                      0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08 };
  rf69.setEncryptionKey(keyRF);

  Serial.print("RFM69 radio @");  Serial.print((int)RF69_FREQ);  Serial.println(" MHz");

  // set up the LCD's number of columns and rows:
	//lcd.begin(16, 2);

	// Clears the LCD screen
	//lcd.clear();

  //fill the array for moving average with -100 dBm
  for (int i = 0; i < numReadings; i++) {
    readings[i] = -100; 
  }
}

void loop() {
  geigerRF();
}

void geigerRF() {
 if (rf69.available()) {
    // Should be a message for us now
    uint8_t buf[RH_RF69_MAX_MESSAGE_LEN];
    uint8_t len = sizeof(buf);
    if (rf69.recv(buf, &len)) {
      if (!len) return;
      buf[len] = 0;
      signalStr = movingAverage(rf69.lastRssi());
      Serial.print("Received [");
      Serial.print(len);
      Serial.print("]: ");
      Serial.println((char*)buf);
      Serial.print(signalStr, DEC);
      lcd.clear();
      lcd.setCursor(0,0);
      lcd.print(signalStr, DEC);
    } else {
      Serial.println("Receive failed");
      signalStr = -999; //set to lowest value
    }
    interval = millis();  
  } else if (millis() - interval > 1000){
      Serial.println("No signal");
      lcd.clear();
      lcd.setCursor(0,0);
      lcd.print("No signal");
      interval = millis();
  }
}

int movingAverage(int input_val) {
  total = total - readings[readIndex];
  total = total + input_val;

  readings[readIndex] = input_val;

  readIndex++;
  if (readIndex >= numReadings) {
    readIndex = 0; 
  }

  int average = total / numReadings;
  return average; 
}