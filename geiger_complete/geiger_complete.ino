#include <SPI.h>
#include <MFRC522.h>
#include <RH_RF69.h>

//****** RF Setup ************************************

// Change to 434.0 or other frequency, must match RX's freq!
#define RF69_FREQ 433.0
#define PIEZO_PIN 2
#define RFM69_CS    4  //
#define RFM69_INT   3  //
#define RFM69_RST   A0  // "A"
#define LED        13

RH_RF69 rf69(RFM69_CS, RFM69_INT);

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

unsigned long previousMillisRF = 0;
unsigned long intervalRF = 0;

double decaySlope[3]; //health decay parameters
int decayTicker;
unsigned long previousMillisDecay = 0;
unsigned long intervalDecay = 200;

//****** RFID Setup ************************************

/*Using Hardware SPI of Arduino */
/*MOSI (11), MISO (12) and SCK (13) are fixed */
/*You can configure SS and RST Pins*/
#define SS_PIN 53 /* Slave Select Pin */
#define RST_PIN 5 /* Reset Pin */
/* Create an instance of MFRC522 */
MFRC522 mfrc522(SS_PIN, RST_PIN);
/* Create an instance of MIFARE_Key */
MFRC522::MIFARE_Key key;
// {LARP, 0x01 = healing item, 0x14 = 20 hp}
byte header[4] = { 0x4C, 0x41, 0x52, 0x50 };  //checks against this header to identify LARP-related RFIDs
int healAmount = 0;
int HP = 100;
byte bufferLen = 18;
/* Length of buffer should be 2 Bytes more than the size of Block (16 Bytes) */
MFRC522::StatusCode status;
unsigned long previousMillisRFID = 0;

void setup() {
  /* Initialize serial communications with the PC */
  Serial.begin(9600);
  /* Initialize SPI bus */
  SPI.begin();

  /*************** Initialize RFM69HW Module */

  pinMode(LED, OUTPUT);
  pinMode(RFM69_RST, OUTPUT);
  digitalWrite(RFM69_RST, LOW);
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
  uint8_t keyRF[] = { 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08,
                    0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08};
  rf69.setEncryptionKey(keyRF);

  Serial.print("RFM69 radio @");  Serial.print((int)RF69_FREQ);  Serial.println(" MHz");

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
  


  /*************** Initialize MFRC522 Module */
  mfrc522.PCD_Init();
  Serial.println("Scan a MIFARE 1K Tag to write data...");
  /* Prepare the ksy for authentication */
  /* All keys are set to FFFFFFFFFFFFh at chip delivery from the factory */
  for (byte i = 0; i < 6; i++) {
    key.keyByte[i] = 0xFF;
  }
}

void loop() {

  //check signal strength
  geigerRF();
  //HP loss first three values define sig strength breakpoints, last four values how many seconds to go form 100 to 0 HP at that signal value
  if (millis() - previousMillisDecay >= 1000) {
    healthDecay(-45,-60,-80,60,120,1200,7200);
  previousMillisDecay = millis();
  }
  //imitate geiger sounds
  buzzer();

  //check for RFID tags and resolve them
  if (millis() - previousMillisRFID >= 1000) {
    geigerRFID();
    previousMillisRFID = millis();
  }
}

void healthDecay(int critical, int unsafe, int safe, int minimum, int critVal, int unsVal, int safeVal) {

  //limit the signal values if they go out of defined bounds
  if (geiger[4] < geigerSet[1][4]) {
    geiger[4] = geigerSet[1][4];
  } else if (geiger[4] > geigerSet[0][4]) {
    geiger[4] = geigerSet[0][4];
  }

  decaySlope[0] = ( (safeVal / minimum) - (safeVal / critVal) ) / (geigerSet[0][4] - critical);
  decaySlope[1] = ( (safeVal / critVal) - (safeVal / unsVal) ) / (critical - unsVal);
  decaySlope[2] = ( (safeVal / unsVal) - 1 ) / (unsVal - safeVal);

  if(geiger[4] > critical) {
    decayTicker = decayTicker + decaySlope[0]*(geiger[4] - geigerSet[0][4]) + (safeVal / minimum) ;
  } else if(geiger[4] > unsafe) {
    decayTicker = decayTicker + decaySlope[1]*(geiger[4] - critical) + (safeVal / critVal) ;
  } else if(geiger[4] > safe) {
    decayTicker = decayTicker + decaySlope[2]*(geiger[4] - unsVal) + (safeVal / unsVal) ;
  }

  if(decayTicker > safeVal*(1000 / intervalDecay) ){
    HP--;
    Serial.print("Lost 1 HP, currently at: ");  Serial.print((int)HP);  Serial.println(" HP total");
  }
}


/*************** RFM69HW-related functions */

void geigerRF() {
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
    } else {
      Serial.println("Receive failed");
      geiger[4] = geigerSet[1][4]; //set to lowest value
    }
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

void buzzer() { //
  if (millis() - previousMillisRF >= intervalRF) {

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
    randNumber = random(1, geiger[3]); // Generate a random number in the interval

    // Ensure the random number is within the desired range
    if (randNumber > geiger[0] && randNumber < geiger[1]) {
      randNumber = geiger[1];
    } else if (randNumber > geiger[2]) {
      randNumber = geiger[2];
    }

    if (randNumber <= geiger[0]) {
    // Play a click sound
      tone(PIEZO_PIN, 510); // Adjust the frequency as needed
      delay(10);
      noTone(PIEZO_PIN);
    } else {
      tone(PIEZO_PIN, 500); // Adjust the frequency as needed
      delay(10);
      noTone(PIEZO_PIN);
    }
  
    previousMillisRF = millis();
    intervalRF = randNumber;
  }
   
}


/*************** MFRC522-related functions */
void geigerRFID() {
  /* Look for new cards */
  /* Reset the loop if no new card is present on RC522 Reader */
  if (mfrc522.PICC_IsNewCardPresent()) {

    /* Select one of the cards */
    if (mfrc522.PICC_ReadCardSerial()) {

      Serial.print("\n");
      Serial.println("**Card Detected**");
      /* Print UID of the Card */
      Serial.print(F("Card UID:"));
      for (byte i = 0; i < mfrc522.uid.size; i++) {
        Serial.print(mfrc522.uid.uidByte[i] < 0x10 ? " 0" : " ");
        Serial.print(mfrc522.uid.uidByte[i], HEX);
      }
      Serial.print("\n");
      /* Print type of card (for example, MIFARE 1K) */
      Serial.print(F("PICC type: "));
      MFRC522::PICC_Type piccType = mfrc522.PICC_GetType(mfrc522.uid.sak);
      Serial.println(mfrc522.PICC_GetTypeName(piccType));

      int blockNum = 1;
      byte readBlockData[18];
      bufferLen = 18;
      byte blockData[16];
      if (ReadDataFromBlock(blockNum, readBlockData)) {
        copy(readBlockData, blockData, 16);

        //check if RFID has correct header
        for (int i = 0; i < 4; i++) {
          if (blockData[i] != header[i]) {
            Serial.print("\n");
            Serial.println("Not a LARP card.");
            mfrc522.PICC_HaltA();
            mfrc522.PCD_StopCrypto1();
            return;
          }
        }
        //check if the item is a healing item, apply heal if it is
        if (blockData[4] == 0x01) {
          if (blockData[5] > 0x00) {
            healAmount = (int)blockData[5];
            //if healing item sucessfully emptied, heal for the correct amount
            if (wipeItem(blockData)) {
              heal(healAmount);
            }
            healAmount = 0;
          } else {
            //***indicate empty
            Serial.print("\n");
            Serial.println("Healing kit empty.");
          }
        }
      }
      Serial.print("\n");
      Serial.println("RFID tick");
      mfrc522.PICC_HaltA();
      mfrc522.PCD_StopCrypto1();
    }
  }
}

void heal(int healAmount) {
  if (HP + healAmount > 100) {
    HP = 100;
  } else {
    HP = HP + healAmount;
  }
  healAmount = 0;
  Serial.print("\n");
  Serial.println("**Healing applied**");
}

void WriteDataToBlock(int blockNum, byte blockData[]) {
  /* Authenticating the desired data block for write access using Key A */
  status = mfrc522.PCD_Authenticate(MFRC522::PICC_CMD_MF_AUTH_KEY_A, blockNum, &key, &(mfrc522.uid));
  if (status != MFRC522::STATUS_OK) {
    Serial.print("Authentication failed for Write: ");
    Serial.println(mfrc522.GetStatusCodeName(status));
    return;
  } else {
    Serial.println("Authentication success");
  }


  /* Write data to the block */
  status = mfrc522.MIFARE_Write(blockNum, blockData, 16);
  if (status != MFRC522::STATUS_OK) {
    Serial.print("Writing to Block failed: ");
    Serial.println(mfrc522.GetStatusCodeName(status));
    return;
  } else {
    Serial.println("Data was written into Block successfully");
  }
}

bool wipeItem(byte blockData[]) {
  /* Authenticating the desired data block for write access using Key A */
  status = mfrc522.PCD_Authenticate(MFRC522::PICC_CMD_MF_AUTH_KEY_A, 1, &key, &(mfrc522.uid));
  if (status != MFRC522::STATUS_OK) {
    Serial.print("Authentication failed for Write: ");
    Serial.println(mfrc522.GetStatusCodeName(status));
    return false;
  } else {
    Serial.println("Authentication success");
  }

  /* Write data to the block */
  blockData[5] = 0x00;
  status = mfrc522.MIFARE_Write(1, blockData, 16);
  if (status != MFRC522::STATUS_OK) {
    Serial.print("Writing to Block failed: ");
    Serial.println(mfrc522.GetStatusCodeName(status));
    return false;
  } else {
    Serial.println("Data was written into Block successfully");
  }
  return true;
}

bool ReadDataFromBlock(int blockNum, byte readBlockData[]) {
  /* Authenticating the desired data block for Read access using Key A */
  byte status = mfrc522.PCD_Authenticate(MFRC522::PICC_CMD_MF_AUTH_KEY_A, blockNum, &key, &(mfrc522.uid));

  if (status != MFRC522::STATUS_OK) {
    Serial.print("Authentication failed for Read: ");
    Serial.println(mfrc522.GetStatusCodeName(status));
    return false;
  } else {
    Serial.println("Authentication success");
  }

  /* Reading data from the Block */
  status = mfrc522.MIFARE_Read(blockNum, readBlockData, &bufferLen);
  if (status != MFRC522::STATUS_OK) {
    Serial.print("Reading failed: ");
    Serial.println(mfrc522.GetStatusCodeName(status));
    Serial.print(F("Data in block "));
    dump_byte_array(readBlockData, 16);
    Serial.println();
    Serial.println();

    return false;
  } else {
    Serial.println("Block was read successfully");
  }
  return true;
}

// Function to copy 'len' elements from 'src' to 'dst'
void copy(byte* src, byte* dst, int len) {
  memcpy(dst, src, sizeof(src[0]) * len);
}

void dump_byte_array(byte* buffer, byte bufferSize) {
  for (byte i = 0; i < bufferSize; i++) {
    Serial.print(buffer[i] < 0x10 ? " 0" : " ");
    Serial.print(buffer[i], HEX);
  }
}