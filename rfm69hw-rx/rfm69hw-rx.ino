// rf69 demo tx rx.pde
// -*- mode: C++ -*-
// Example sketch showing how to create a simple messaging client
// with the RH_RF69 class. RH_RF69 class does not provide for addressing
// or reliability, so you should only use RH_RF69 if you do not need the
// higher level messaging abilities.
// It is designed to work with the other example RadioHead69_RawDemo_TX.
// Demonstrates the use of AES encryption, setting the frequency and
// modem configuration.

#include <SPI.h>
#include <RH_RF69.h>

#include <LiquidCrystal.h>
/************ Radio Setup ***************/

// Change to 434.0 or other frequency, must match RX's freq!
#define RF69_FREQ 433.0

#define RFM69_CS    4  //
#define RFM69_INT   3  //
#define RFM69_RST   2  // "A"
#define LED        13

//#elif defined(ESP8266)  // ESP8266 feather w/wing
//  #define RFM69_CS    2  // "E"
//  #define RFM69_INT  15  // "B"
//  #define RFM69_RST  16  // "D"
//  #define LED         0



/* Teensy 3.x w/wing
#define RFM69_CS     10  // "B"
#define RFM69_INT     4  // "C"
#define RFM69_RST     9  // "A"
#define RFM69_IRQN   digitalPinToInterrupt(RFM69_INT)
*/

/* WICED Feather w/wing
#define RFM69_CS     PB4  // "B"
#define RFM69_INT    PA15 // "C"
#define RFM69_RST    PA4  // "A"
#define RFM69_IRQN   RFM69_INT
*/

// Singleton instance of the radio driver
RH_RF69 rf69(RFM69_CS, RFM69_INT);

// Creates an LCD object. Parameters: (rs, enable, d4, d5, d6, d7)
LiquidCrystal lcd(5, 6, 7, 8, 9, 10);

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

  // The encryption key has to be the same as the one in the server
  uint8_t key[] = { 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08,
                    0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08};
  rf69.setEncryptionKey(key);

  Serial.print("RFM69 radio @");  Serial.print((int)RF69_FREQ);  Serial.println(" MHz");

  // set up the LCD's number of columns and rows:
	lcd.begin(16, 2);

	// Clears the LCD screen
	lcd.clear();
}

void loop() {
 if (rf69.available()) {
    // Should be a message for us now
    uint8_t buf[RH_RF69_MAX_MESSAGE_LEN];
    uint8_t len = sizeof(buf);
    if (rf69.recv(buf, &len)) {
      if (!len) return;
      buf[len] = 0;
      Serial.print("Received [");
      Serial.print(len);
      Serial.print("]: ");
      Serial.println((char*)buf);
      Serial.print("RSSI: ");
      Serial.println(rf69.lastRssi(), DEC);
      lcd.clear();
      lcd.println(rf69.lastRssi(), DEC);

    } else {
      Serial.println("Receive failed");
    }
  }
}

void Blink(byte pin, byte delay_ms, byte loops) {
  while (loops--) {
    digitalWrite(pin, HIGH);
    delay(delay_ms);
    digitalWrite(pin, LOW);
    delay(delay_ms);
  }
}