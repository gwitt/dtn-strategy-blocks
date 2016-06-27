/**************************************************************************/
/*! 
Much of this taken from Adafruit PN532 library example
  ----> https://www.adafruit.com/products/364
 
*/
/**************************************************************************/
#include <Wire.h>
#include <SPI.h>
#include <Adafruit_PN532.h>
#include <Adafruit_VS1053.h>
#include <SD.h>

// PN532 pins (RFID reader)
// define the pins for SPI communication
// via general purpose IO pins, not hardware SPI
#define PN532_SCK     2     // Blue
#define PN532_MOSI    8     // Purple
#define PN532_SS      9     // Grey
#define PN532_MISO    5     // Orange

// VS1053 pins (song player)
#define SHIELD_RESET  -1     // VS1053 reset pin (unused!)
#define SHIELD_CS     7      // VS1053 chip select pin (output)
#define SHIELD_DCS    6      // VS1053 Data/command select pin (output)
#define CARDCS        4      // Card chip select pin
#define DREQ          3      // VS1053 Data request, ideally an Interrupt pin
// DREQ should be an Int pin, see http://arduino.cc/en/Reference/attachInterrupt

// song player also uses pins 11, 12, 13

// LED Drivers
#define LED_BANK     A4      // L  -  H
#define LED_A        A0      // J1 - J5
#define LED_B        A1      // J2 - J6
#define LED_C        A2      // J3 - J7
#define LED_D        A3      // J4 - J8


Adafruit_PN532 nfc(PN532_SCK, PN532_MISO, PN532_MOSI, PN532_SS);


Adafruit_VS1053_FilePlayer musicPlayer = 
  Adafruit_VS1053_FilePlayer(SHIELD_RESET, SHIELD_CS, SHIELD_DCS, DREQ, CARDCS);

byte currentTrack= 0; // 1 to 8, 0 for none
char * filenames[]= {"1.MP3", "2.MP3", "3.MP3", "4.MP3", "5.MP3", "6.MP3", "7.MP3", "8.MP3"}; 
char * logFilename= "log.txt";

void setup(void) {
  Serial.begin(9600);

  if (!SD.begin(CARDCS)) {
    Serial.println(F("SD failed, or not present"));
    while (1);  // don't do anything more
  }
  Serial.println("SD OK!");
  
  logPrint("");
  logPrint("hello.");

  // LED drivers
  pinMode(LED_A, OUTPUT);
  pinMode(LED_B, OUTPUT);
  pinMode(LED_C, OUTPUT);
  pinMode(LED_D, OUTPUT);
  pinMode(LED_BANK, OUTPUT);

  digitalWrite(LED_A, LOW);
  digitalWrite(LED_B, LOW);
  digitalWrite(LED_C, LOW);
  digitalWrite(LED_D, LOW);
  digitalWrite(LED_BANK, LOW);

  nfc.begin();

  uint32_t versiondata = nfc.getFirmwareVersion();
  if (! versiondata) {
    logPrint("Didn't find PN53x board");
    while (1); // halt
  }
  // Got ok data, print it out!
  logPrint("RFID reader found.");
  Serial.print("Found chip PN5"); Serial.println((versiondata>>24) & 0xFF, HEX); 
  Serial.print("Firmware ver. "); Serial.print((versiondata>>16) & 0xFF, DEC); 
  Serial.print('.'); Serial.println((versiondata>>8) & 0xFF, DEC);
  // configure board to read RFID tags
  nfc.SAMConfig();

  // initialise the music player
  if (! musicPlayer.begin()) { // initialise the music player
     logPrint("Couldn't find VS1053");
     while (1);
  }
  logPrint("VS1053 found");

  //musicPlayer.sineTest(0x44, 500);    // Make a tone to indicate VS1053 is working
  
  // Set volume for left, right channels. lower numbers == louder volume!
  musicPlayer.setVolume(0, 0);

  if (! musicPlayer.useInterrupt(VS1053_FILEPLAYER_PIN_INT))
    logPrint("DREQ pin is not an interrupt pin");

}

void loop(void) {
  byte block= 0;
  logPrint("stopping music...");
  playTrack(0);
  logPrint("lights out...");
  fadeTo(0);
  while(block == 0){
    block= getBlock();
    delay(250);
  }
  logPrint("light on...");
  fadeTo(block);
  logPrint("playing music...");
  playTrack(block);

  boolean done= false;
  while(!done){
    boolean blockGone= true;
    for (int i=0; i< 3; i++){
      if ( getBlock() == block ) blockGone= false;
      delay(30);
    }
    if (blockGone) done= true;
    if (!musicPlayer.playingMusic) fadeTo(0);
  }
}

void stopPlaying(){
  if (musicPlayer.playingMusic){
    // fade out..
    // Set volume for left, right channels. lower numbers == louder volume!
    for (int i=0; i < 100; i++){
      musicPlayer.setVolume(i,i);
      delay(7);
    }
  }
  musicPlayer.stopPlaying();
  delay(100);
  musicPlayer.setVolume(0,0);
}

void playTrack(byte track){
  if (track==0){
    stopPlaying();
    return;
  }
  
  // Start playing a file, then we can do stuff while waiting for it to finish
  if (! musicPlayer.startPlayingFile(filenames[track-1])) {
    logPrint("Could not open file");
    while (1);
  }
  Serial.print("playing ");
  Serial.println(filenames[track-1]);
}

byte getBlock(){
  uint8_t success;
  uint8_t uid[] = { 0, 0, 0, 0, 0, 0, 0 };  // Buffer to store the returned UID
  uint8_t uidLength;                        // Length of the UID (4 or 7 bytes depending on ISO14443A card type)
  uint8_t data[32];
    
  // Wait for an ISO14443A type cards (Mifare, etc.).  When one is found
  // 'uid' will be populated with the UID, and uidLength will indicate
  // if the uid is 4 bytes (Mifare Classic) or 7 bytes (Mifare Ultralight)
  logPrint("Checking for block...");
  //Serial.println("Waiting for an ISO14443A Card ...");
  boolean blockFound = nfc.readPassiveTargetID(PN532_MIFARE_ISO14443A, uid, &uidLength, 250); // timeout at 250ms
  if (!blockFound) return 0;
  logPrint("found. ");  
  if (blockFound) {
    // Display some basic information about the card
    //Serial.println("Found an ISO14443A card");
    //Serial.print("  UID Length: ");Serial.print(uidLength, DEC);Serial.println(" bytes");
    //Serial.print("  UID Value: ");
    //nfc.PrintHex(uid, uidLength);
    //Serial.println("");
    
    if (uidLength == 4)
    {
      // We probably have a Mifare Classic card ... 
      //Serial.println("Seems to be a Mifare Classic card (4 byte UID)");
      //Serial.println("Expected Mifare Ultralight (7 byte UID)");
    }
    
    if (uidLength == 7)
    {
      // We probably have a Mifare Ultralight card ...
      //Serial.println("Seems to be a Mifare Ultralight tag (7 byte UID)");
    
      // Try to read the first general-purpose user page (#4)
      //Serial.println("Reading page 4");
      boolean success = nfc.mifareultralight_ReadPage (4, data);
      if (success)
      {
        // Data seems to have been read ... spit it out
        //nfc.PrintHexChar(data, 4);
        //Serial.println("");
        logPrint("Block Valid.");
        Serial.print("valid: #");
        Serial.println(data[0]);
    
        // Wait a bit before reading the card again
        //delay(1000);
      }
      else
      {
        logPrint("Unable to read the requested page");
      }
    }
  }
  return data[0];
}

void fadeTo(byte target){
  boolean currentBank= HIGH;
  if (currentTrack < 5) currentBank= LOW;

  boolean targetBank= HIGH;
  if (target < 5) targetBank= LOW;

  // get pin number A0 is 14...
  byte currentPin= currentTrack;
  if (currentPin > 4) currentPin-= 4;
  currentPin+= A0;
  currentPin--;
  if (currentTrack == 0) currentPin= 0;

  byte targetPin= target;
  if (targetPin > 4) targetPin -= 4;
  targetPin+= A0;
  targetPin--; // track numbers start at 1
  if (target == 0) targetPin= 0;
  
  // leds at J1 to J8, 0 for none
  // fade out current
  digitalWrite(LED_BANK, currentBank);
  if (currentTrack != 0){
    for (int i= 0; i < 500; i++){
      digitalWrite(currentPin, HIGH);
      delayMicroseconds(500-i);
      digitalWrite(currentPin, LOW);
      delayMicroseconds(i);
    }
  }
  
  // fade in target
  digitalWrite(LED_BANK, targetBank);
  if (target != 0){
    for (int i= 0; i < 250; i++){
      digitalWrite(targetPin, LOW);
      delayMicroseconds(250-i);
      digitalWrite(targetPin, HIGH);
      delayMicroseconds(i);
    }
  }
  
  currentTrack= target;
}

void logPrint(char * str){
  Serial.println(str);
  File logFile= SD.open(logFilename, O_READ | O_WRITE | O_CREAT | O_APPEND);
  logFile.println(str);
  logFile.close();
}

