#include <SPI.h>
#include <MFRC522.h>






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
unsigned long intervalRFID = 0;

void setup() {
  /* Initialize serial communications with the PC */
  Serial.begin(9600);
  /* Initialize SPI bus */
  SPI.begin();


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

  //check for RFID tags and resolve them
  if (millis() - previousMillisRFID >= 1000) {
    geigerRFID();
    previousMillisRFID = millis();
  }
}

void geigerRFID {
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