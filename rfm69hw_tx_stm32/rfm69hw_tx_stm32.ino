#include <SPI.h>
#include <RH_RF69.h>

/************ Radio Setup ***************/

// Change to 434.0 or other frequency, must match RX's freq!
#define RF69_FREQ 433.0
#define RFM69_SS PA4    //slave select - NSS - 9 or 53
#define RFM69_INT PA0   //interrupt pin - DI00
#define RFM69_RST PA1  // "A" reset, set to analog to preserve DIO pins
#define LED PC13
//SCK to pin D13 or 52
//MISO to pin D12 or 50
//MOSI to pin D11 or 51

// ESP8266 feather w/wing
//  #define RFM69_CS    2  // "E"
//  #define RFM69_INT  15  // "B"
//  #define RFM69_RST  16  // "D"
//  #define LED         0

RH_RF69 rf69(RFM69_SS, RFM69_INT);
int16_t packetnum = 0;  // packet counter, we increment per xmission

void setup() {
  Serial.begin(9600);
  //while (!Serial) delay(1); // Wait for Serial Console (comment out line if no computer)

  pinMode(LED, OUTPUT);
  pinMode(RFM69_RST, OUTPUT);
  digitalWrite(RFM69_RST, LOW);

  Serial.println("RFM69 TX Test!");
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
}

void loop() {
  delay(100);  // Wait 1 second between transmits, could also 'sleep' here!

  char radiopacket[20] = "Hello World #";
  itoa(packetnum++, radiopacket+13, 10);
  Serial.print("Sending "); Serial.println(radiopacket);

  // Send a message!
  rf69.send((uint8_t *)radiopacket, strlen(radiopacket));
  rf69.waitPacketSent();
}