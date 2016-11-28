/*
  This uses the red Sparkfun RFID Eval Board for Writing tags
  This is a different board than is used in the acutal radio for reading tags
  
  RFID Eval 13.56MHz Shield example sketch v10

  Aaron Weiss, aaron at sparkfun dot com
  OSHW license: http://freedomdefined.org/OSHW

  works with 13.56MHz MiFare 1k tags

  Based on hardware v13:
  D7 -> RFID RX
  D8 -> RFID TX

  Note: RFID Reset attached to D13 (aka status LED)

  Note: be sure include the SoftwareSerial lib, 
    http://arduiniana.org/libraries/newsoftserial/
  
  06/04/2013 - Modified for compatibility with Arudino 1.0. Seb Madgwick.

  we're using mifare ultralight tags, which are different
  See datasheet!
  Do not authenticate.
  Memory is organized into 4 byte pages
  User memory starts on page 4
  Write data is 16 bytes, but only the first 4 are used (one page)
  We're using page 4 to store block/song number. all 4 bytes are set the same.
  after writing, page 4 could be permanently locked by setting its lock bit:
  write 0bd00010000 to byte 0x2 of page 2

  Block / Song numbers:
  1 - Saying I'm sorry...
  2 - When something seems bad...
  3 - Keep trying...
  4 - You can be a big helper...
  5 - Friends help each other...
  6 - Grownups come back
  7 - It's OK to feel sad...
  8 - When you feel so mad...
  
*/

#include <SoftwareSerial.h>

SoftwareSerial rfid(7, 8);

//Global var
byte response[30];
int currentBlock= 1;

//INIT
void setup()
{
  Serial.begin(9600);
  Serial.println();
  Serial.println("DTN Strategy Block Writing Utility");
  
  // set the data rate for the SoftwareSerial ports
  rfid.begin(19200);
  delay(10);
  halt();

  //while(1) dump_response();
}

void loop(){ while(1) {
  Serial.println();
  Serial.print("Current Block: ");
  Serial.println(currentBlock);
  Serial.println("<enter> to write or (1 to 8) to change number.");
  int newNum= getInput();
  if (newNum != -1){ currentBlock= newNum; break; }

  Serial.print("Searching for tag... ");
  while(!searchForTag());
  int tagNum= readTag();
  if (tagNum == -1) break;
  if(tagNum != 0) Serial.print(tagNum);
  if(tagNum == currentBlock){ Serial.println("\nNo write needed."); delay(500); break; }
  Serial.println();
  
  writeTag(currentBlock);
  Serial.print("Verifying... ");
  while(!searchForTag());
  tagNum= readTag();
  if (tagNum == currentBlock) Serial.println("OK");
  else Serial.println("ERROR");
  delay(500);
  if (tagNum == -1) break;
}}

void writeTag(byte n){
  Serial.print("Writing tag number: ");
  Serial.println(n);
  byte data[]= {n,n,n,n, 0,0,0,0, 0,0,0,0, 0,0,0,0};
  writeBlock(0x04, data);
  
}

// -1 for error
int getInput(){
  while(!Serial.available());
  delay(100); 
  int num= Serial.read();
  while(Serial.available()) Serial.read();
  Serial.flush();
  num-= 48; // '0' == 48
  if (num < 0) return -1;
  if (num > 9) return -1;
  return num;
}

int readTag(){
  byte addr= 0x04;
  byte cmd= readBlock(addr);
  int len= getResponse(cmd);
  if (len == -1) Serial.println("Read aborted.");
  else if (len == 0) Serial.println("Read failed.");
  else if (response[0] != addr) Serial.println("Read failed: unexpected data");
  else if (response[0] == addr){
    //Serial.print("Tag Read: ");
    //Serial.println(response[1], HEX);
  }

  if (len < 1) return -1;
  int tagNum= response[1];
  for(int i=1; i<5; i++) if (response[i] != tagNum){
    Serial.print("found new tag.");
    return 0;
  }
  return tagNum;
}

boolean searchForTag(){
  delay(20);
  byte cmd= seek();
    
  int len;
  len= getResponse(cmd);
  if (len == -1) Serial.println("aborted.");
  else if (len == 0) Serial.println("failed.");
  else if (response[0] == 0x01); // Serial.print("found: ");
  else return false;
  
  return true;
}

// returns length of response, -1 for abort, 0 for fail
int getResponse(byte cmd){
  // minimum response length is 5 bytes: header, reserved, length, command, csum
  while(rfid.available() < 5) if (checkAbort()) return -1;
  delay(50); // wait for UART to fill

  // check message validity
  byte csum= 0;
  if (rfid.read() != 0xFF){ rfid.flush(); return 0; } // header
  if (rfid.read() != 0x00){ rfid.flush(); return 0; } // reserved
  byte len= rfid.read();
  csum+= len;
  byte c= rfid.read(); // command
  csum+= c;
  len--;
  if (rfid.available() < len+1){ rfid.flush(); return 0; }
  for (int i= 0; i<len; i++){
    byte in= rfid.read();
    response[i]= in;
    csum+= in;
  }
  if (csum != rfid.read()){ rfid.flush(); Serial.println("bad csum"); return 0; }
  if (c != cmd){
    if (rfid.available()) len= getResponse(cmd);
    else{
      Serial.println("wrong command");
      return 0; 
    }
  }
  return len;
}

boolean checkAbort(){
  boolean a= false;
  while (Serial.available()) if (Serial.read() == 'q') a= true;
  return a;
}

void writeBlock(byte addr, byte * data){
  byte msg[18];
  msg[0]= 0x89; // write command
  msg[1]= addr; // block number
  for (int i=0; i<16; i++) msg[i+2]= data[i];
  sendCommand(18, msg); 
}

byte readBlock(byte addr){
  byte cmd= 0x86;
  byte msg[]= {cmd, addr};
  sendCommand(2, msg);
  return cmd;
}

void sendCommand(byte len, byte * msg){
  byte csum= len;
  rfid.write((uint8_t)0xFF); // Header
  rfid.write((uint8_t)0x00); // Reserved
  rfid.write((uint8_t)len); // Length (command+data)
  for (byte i=0; i<len; i++){
    rfid.write((uint8_t)msg[i]);
    csum+= msg[i];
  }
  rfid.write((uint8_t)csum);
  delay(10);
  
  //Serial.println();
  //Serial.print("Command: ");
  //for (int i=0; i<len; i++){
  //  Serial.print(msg[i], HEX);
  //  Serial.print(" ");
  //}
}

void dumpResponse(){
  while(rfid.available()){
    int nextByte= rfid.read();
    if (nextByte == 0xFF) Serial.println();
    Serial.print(nextByte, HEX);
    Serial.print(' ');
  }
  //Serial.println();
}

void halt()
{
 //Halt tag
  rfid.write((uint8_t)255);
  rfid.write((uint8_t)0);
  rfid.write((uint8_t)1);
  rfid.write((uint8_t)147); // 0x93 Halt
  rfid.write((uint8_t)148); 
}

// returns command byte
byte seek()
{
  byte msg[1];
  msg[0]= 0x82;
  sendCommand(1, msg);
  return 0x82;
}


