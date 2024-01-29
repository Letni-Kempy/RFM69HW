#include <SPI.h>
#include <RH_RF69.h>

#include <LiquidCrystal.h>
/************ Radio Setup ***************/

// Change to 434.0 or other frequency, must match RX's freq!
#define RF69_FREQ 433.0
#define PIEZO_PIN 2
#define RFM69_CS    4  //
#define RFM69_INT   3  //
#define RFM69_RST   A0  // "A"
#define LED        13

//#elif defined(ESP8266)  // ESP8266 feather w/wing
//  #define RFM69_CS    2  // "E"
//  #define RFM69_INT  15  // "B"
//  #define RFM69_RST  16  // "D"
//  #define LED         0

RH_RF69 rf69(RFM69_CS, RFM69_INT);

// Creates an LCD object. Parameters: (rs, enable, d4, d5, d6, d7)
LiquidCrystal lcd(5, 6, 7, 8, 9, 10);

//movingAverage()
const int numReadings = 10;
int readings[numReadings];
int readIndex = 0;  
long total = -1000; //sum of readings array

double signalNorm;
int geiger[5] = {300, 500 , 2500, 3000, -40};
//geigerSet[2][4] = {{fastest irregular interval, fast beep limit, slow beep limit, end of rng band(affects probability distribution), max signal strength[dBm]} , {repeat for far signal}}
int geigerSet[2][5] = {{5, 10, 20, 25, -40}, {300, 500, 2500, 3000, -100}}; 
//define the linear functions between the two extreme beeping speeds
double geigerSlope[5];
int randNumber = random(11, geiger[3]); // Generate a random number in the interval [50, 500]

unsigned long previousMillis = 0;
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

  // The encryption key has to be the same as the one in the server
  uint8_t key[] = { 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08,
                    0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08};
  rf69.setEncryptionKey(key);

  Serial.print("RFM69 radio @");  Serial.print((int)RF69_FREQ);  Serial.println(" MHz");

  // set up the LCD's number of columns and rows:
	lcd.begin(16, 2);

	// Clears the LCD screen
	lcd.clear();

  //fill the array for moving average with -100 dBm
  for (int i = 0; i < numReadings; i++) {
    readings[i] = -100; 
  }

  //buzzer
  pinMode(PIEZO_PIN, OUTPUT);
  randomSeed(analogRead(0)); // Seed the random number generator

  // Calculate geigerSlope values
  geigerSlope[4] = geigerSet[0][4]  - geigerSet[1][4];
  geigerSlope[0] = (geigerSet[0][0] - geigerSet[1][0])/geigerSlope[4];
  geigerSlope[1] = (geigerSet[0][1] - geigerSet[1][1])/geigerSlope[4];
  geigerSlope[2] = (geigerSet[0][2] - geigerSet[1][2])/geigerSlope[4];
  geigerSlope[3] = (geigerSet[0][3] - geigerSet[1][3])/geigerSlope[4];
  

}

void loop() {
 if (rf69.available()) {
    // Should be a message for us now
    uint8_t buf[RH_RF69_MAX_MESSAGE_LEN];
    uint8_t len = sizeof(buf);
    if (rf69.recv(buf, &len)) {
      if (!len) return;
      buf[len] = 0;
      geiger[4] = movingAverage(rf69.lastRssi());
      Serial.print("Received [");
      Serial.print(len);
      Serial.print("]: ");
      Serial.println((char*)buf);
      Serial.print(geiger[4], DEC);
       //lcd.clear();
      lcd.setCursor(0,0);
      lcd.print((char*)buf);
      lcd.setCursor(0,1);
      lcd.print(geiger[4], DEC);
    } else {
      Serial.println("Receive failed");
      geiger[4] = geigerSet[1][4]; //set to lowest value
    }
  }
  buzzer();
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

void buzzer() { //
  if (millis() - previousMillis >= interval) {

    //limit the signal values if they go out of defined bounds
    if (geiger[4] < geigerSet[1][4]) {
      geiger[4] = geigerSet[1][4];
    } else if (geiger[4] > geigerSet[0][4]) {
      geiger[4] = geigerSet[0][4];
    }
    signalNorm = geiger[4] - geigerSet[1][4];
    geiger[3] = (geigerSlope[3])*signalNorm + geigerSet[1][3];
    geiger[2] = (geigerSlope[2])*signalNorm + geigerSet[1][2];
    geiger[1] = (geigerSlope[1])*signalNorm + geigerSet[1][1];
    geiger[0] = (geigerSlope[0])*signalNorm + geigerSet[1][0]; 
    randNumber = random(1, geiger[3]); // Generate a random number in the interval [50, 500]

    // Ensure the random number is within the desired range [100, 300]   TADY MODIFIKOVAT INTERVALY SNIŽOVAT ELSE IF Z 800 NA 300 TŘEBA
    if (randNumber > geiger[0] && randNumber < geiger[1]) {
      randNumber = geiger[1];
    } else if (randNumber > geiger[2]) {
      randNumber = geiger[2];
    }

    if (randNumber <= geiger[0]) {
    // Play a click sound
      tone(PIEZO_PIN, 510); // Adjust the frequency as needed
      delay(10);
      noTone(PIEZO_PIN);                 //TENHLE ZVUK MI PŘIJDE NEJMÍŇ OTRAVNEJ
    } else {
      tone(PIEZO_PIN, 500); // Adjust the frequency as needed
      delay(10);
      noTone(PIEZO_PIN);                  //TENHLE ZVUK MI PŘIJDE NEJMÍŇ OTRAVNEJ
    }
  
    previousMillis = millis();
    interval = randNumber;
  }
   
}
