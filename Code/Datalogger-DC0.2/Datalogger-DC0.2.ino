#include <SPI.h>
#include <SerialCommand.h>
/*  This code by default reads the 64 byte unique Device Identifier on
    the attached AT45DB081E, outputs it to the console, and then calculates
    the 64 bytes to be programmed in the Security Register to match a
    genuine Davis Weatherlink interface, and outputs those 64 bytes
    to the console.  By uncommenting the last section in the setup() loop
    you can choose to program the Security Register. But first make sure that
    the read was successful and the calculated bytes are correct before enabling
    writing.
    *******
    NOTE:  YOU ONLY GET ONE CHANCE TO WRITE THE SECURITY REGISTER!
    So make sure communication with the AT45 is OK or you'll have a dead
    dataflash on your hands!
    *******


    komenda "PROG" - po sprawdzeniu co najmniej 3 razy programuje chip. Można tylko raz zaprogramować tylko raz!!!!!
  komenda "CHECK" - pokazuje jakie wartości są wpisane w rejestrze programowalnym , domyslnie jest OxFF pusty niezaprogramowany
  komenda "REFRESH" - sprawdza czy chip nadaje się do zaprogramowania
*/
   
//DataFlash Commands
#define DATAFLASH_READ_MANUFACTURER_AND_DEVICE_ID 0x9F
#define DATAFLASH_READ_SECURITY_REGISTER 0x77
#define DATAFLASH_STATUS_REGISTER_READ 0xD7
#define DATAFLASH_CHIP_ERASE_0 0xC7
#define DATAFLASH_CHIP_ERASE_1 0x94
#define DATAFLASH_CHIP_ERASE_2 0x80
#define DATAFLASH_CHIP_ERASE_3 0x9A
#define DATAFLASH_READ_SECURITY_REGISTER 0x77
#define DATAFLASH_PROGRAM_SECURITY_REGISTER_0 0x9B
#define DATAFLASH_PROGRAM_SECURITY_REGISTER_1 0x00
#define DATAFLASH_PROGRAM_SECURITY_REGISTER_2 0x00
#define DATAFLASH_PROGRAM_SECURITY_REGISTER_3 0x00
#define wersja 0.2


//As per Watson's work described at http://www.wxforum.net/index.php?topic=18110.msg200376
int const GreenDot_Table[256] =
{
0x00, 0x04, 0x08, 0x0C, 0x10, 0x14, 0x18, 0x1C, 0x21, 0x25, 0x29, 0x2D, 0x31, 0x35, 0x39, 0x3D,
0x46, 0x42, 0x4E, 0x4A, 0x56, 0x52, 0x5E, 0x5A, 0x67, 0x63, 0x6F, 0x6B, 0x77, 0x73, 0x7F, 0x7B,
0x8C, 0x88, 0x84, 0x80, 0x9C, 0x98, 0x94, 0x90, 0xAD, 0xA9, 0xA5, 0xA1, 0xBD, 0xB9, 0xB5, 0xB1,
0xCA, 0xCE, 0xC2, 0xC6, 0xDA, 0xDE, 0xD2, 0xD6, 0xEB, 0xEF, 0xE3, 0xE7, 0xFB, 0xFF, 0xF3, 0xF7,
0x18, 0x1C, 0x10, 0x14, 0x08, 0x0C, 0x00, 0x04, 0x39, 0x3D, 0x31, 0x35, 0x29, 0x2D, 0x21, 0x25,
0x5E, 0x5A, 0x56, 0x52, 0x4E, 0x4A, 0x46, 0x42, 0x7F, 0x7B, 0x77, 0x73, 0x6F, 0x6B, 0x67, 0x63,
0x94, 0x90, 0x9C, 0x98, 0x84, 0x80, 0x8C, 0x88, 0xB5, 0xB1, 0xBD, 0xB9, 0xA5, 0xA1, 0xAD, 0xA9,
0xD2, 0xD6, 0xDA, 0xDE, 0xC2, 0xC6, 0xCA, 0xCE, 0xF3, 0xF7, 0xFB, 0xFF, 0xE3, 0xE7, 0xEB, 0xEF,
0x31, 0x35, 0x39, 0x3D, 0x21, 0x25, 0x29, 0x2D, 0x10, 0x14, 0x18, 0x1C, 0x00, 0x04, 0x08, 0x0C,
0x77, 0x73, 0x7F, 0x7B, 0x67, 0x63, 0x6F, 0x6B, 0x56, 0x52, 0x5E, 0x5A, 0x46, 0x42, 0x4E, 0x4A,
0xBD, 0xB9, 0xB5, 0xB1, 0xAD, 0xA9, 0xA5, 0xA1, 0x9C, 0x98, 0x94, 0x90, 0x8C, 0x88, 0x84, 0x80,
0xFB, 0xFF, 0xF3, 0xF7, 0xEB, 0xEF, 0xE3, 0xE7, 0xDA, 0xDE, 0xD2, 0xD6, 0xCA, 0xCE, 0xC2, 0xC6,
0x29, 0x2D, 0x21, 0x25, 0x39, 0x3D, 0x31, 0x35, 0x08, 0x0C, 0x00, 0x04, 0x18, 0x1C, 0x10, 0x14,
0x6F, 0x6B, 0x67, 0x63, 0x7F, 0x7B, 0x77, 0x73, 0x4E, 0x4A, 0x46, 0x42, 0x5E, 0x5A, 0x56, 0x52,
0xA5, 0xA1, 0xAD, 0xA9, 0xB5, 0xB1, 0xBD, 0xB9, 0x84, 0x80, 0x8C, 0x88, 0x94, 0x90, 0x9C, 0x98,
0xE3, 0xE7, 0xEB, 0xEF, 0xF3, 0xF7, 0xFB, 0xFF, 0xC2, 0xC6, 0xCA, 0xCE, 0xD2, 0xD6, 0xDA, 0xDE
};

/*  Pin connection definitions.  Pin connections between Arduino and Dataflash
    should be as follows:
    
    AT45DB011D  ------------  ARDUINO
    1 (SI/MOSI) ------------  Pin 11
    2 (SCK)     ------------  Pin 13
    3 (RESET)   ------------  Pin 8
    4 (CS)      ------------  Pin 10
    5 (WP)      ------------  Pin 7
    6 (VCC)     ------------  3.3V
    7 (GND)     ------------  Ground
    8 (SO)      ------------  Pin 12
   
    Note that pin #s on AT45DB are as follows:
    1   8
    2   7
    3   6
    4   5
   
    A breakout board for the SOIC package makes connections easier but isn't
    strictly necessary
*/
int csPin = 10;
// int resetPin = 8;
// int writeProtectPin = 7;

int delayTime = 500; //half-second typical delay
uint8_t ERRORS;

SerialCommand sCmd;
byte USER_SECURITY_REGISTER[64];

void setup(){
  //Initial setup & SPI Initialization

 
  pinMode(csPin,OUTPUT);
  SPI.begin();
  SPI.setDataMode(SPI_MODE3);
  SPI.setBitOrder(MSBFIRST);
  SPI.setClockDivider(SPI_CLOCK_DIV2);
  Serial.begin(19200);
  delay(delayTime*4); //wait 2 seconds before continuing

  //Begin communication tests
  Serial.println("Attempting communication..");
 
  //First we get the current status register
  uint8_t STATUS;
  digitalWrite(csPin,LOW);
  SPI.transfer(DATAFLASH_STATUS_REGISTER_READ);
  STATUS=SPI.transfer(0);
  digitalWrite(csPin,HIGH);
  Serial.print("Status Register: 0b");
  Serial.println(STATUS,BIN);
  Serial.println("Format: RDY MEMCOMP 0 0 1 1 SECPROT PAGESZ");
  delay(delayTime);
 
  //Next Manufacturer ID and Device ID
  byte MANUFACTURER_DEVICE_ID[4];
  digitalWrite(csPin,LOW);
  SPI.transfer(DATAFLASH_READ_MANUFACTURER_AND_DEVICE_ID);
  for (int i=0;i<4;i++){
    MANUFACTURER_DEVICE_ID[i]=SPI.transfer(0);
  }
  digitalWrite(csPin,HIGH);
 
  //Output result
  Serial.print("Manufacturer ID (Should be 0x1F): ");
  Serial.println(MANUFACTURER_DEVICE_ID[0],HEX);
  Serial.print("Device ID byte1 (should be 0x25): ");
  Serial.println(MANUFACTURER_DEVICE_ID[1],HEX);
  Serial.print("Device ID byte2 (should be 0x00): ");
  Serial.println(MANUFACTURER_DEVICE_ID[2],HEX);
  Serial.print("Length of Extended Device Info: ");
  Serial.println(MANUFACTURER_DEVICE_ID[3],HEX);
  delay(delayTime); 
 if(MANUFACTURER_DEVICE_ID[1]!=0x25)
 {
   Serial.println("Brak polaczenia z Flash");
   exit(0);
 }
 
  /*Security Register
  The Security Register consists of two 64-byte parts, composing a total
  of 128 bytes.  Bytes 0-63 are one-time user programmable.  Bytes 64-127
  are programmed at factory and can't be changed.  As outlined at
  http://www.wxforum.net/index.php?topic=18110.0 Davis calculates the first
  64 bytes based on an algorithm that uses the second 64 bytes and the values
  in the GreenDot_Table above.  Here we will read the 64 factory-programmed bytes
  and calculate what should be programmed for the other 64 bytes. 
 
  */
  byte FACTORY_SECURITY_REGISTER[64]; //Factory-programmed
   //Calculated by us
 
  //We need to read the entire 128 bytes in one shot. 
  //Start with verifying the first 64 bytes are unwritten
  Serial.println("Reading ...");
  digitalWrite(csPin,LOW);
  SPI.transfer(DATAFLASH_READ_SECURITY_REGISTER);
  SPI.transfer(0x00);//dummy 1
  SPI.transfer(0x00);//dummy 2
  SPI.transfer(0x00);//dummy 3

 

  Serial.println("Checking if is empty (0xFF)...");
  for (int i=0;i<64;i++){
    uint8_t CURRENT_BYTE;
    CURRENT_BYTE=SPI.transfer(0x00);
    Serial.print("Bajt programowalny #");
    Serial.print(i);
    Serial.print(" ");
    Serial.print(CURRENT_BYTE,HEX);
   if (CURRENT_BYTE != 0xFF){
    Serial.print(" Error!  Byte #");
    Serial.print(i);
    Serial.println(" is not 0xFF!!");
    ERRORS++;
   }
  }
   
  // Read Factory programmed 64 Byte security Register
  Serial.println("Reading Factory-programmed Register..,.");
  for(int i=0;i<64;i++){
    int byteNumber = i+64;
    Serial.print(byteNumber);
    Serial.print(" - 0x");
    FACTORY_SECURITY_REGISTER[i]=SPI.transfer(0x00);
    Serial.println(FACTORY_SECURITY_REGISTER[i],HEX);
  }
  digitalWrite(csPin,HIGH);
  delay(delayTime);
 
  // Calculate User-programmable security bytes
  Serial.println("Calculating User-Programmable Register...");
  // The first 3 bytes can be anything.  Davis uses it as a serial number
  USER_SECURITY_REGISTER[0]=0x01; //I just used 1, 2, 3...  Change as you like
  USER_SECURITY_REGISTER[1]=0x02;
  USER_SECURITY_REGISTER[2]=0x03; 

  /* Calculate the remaining 61 bytes
  This is taken verbatim from Watson's post, above. location is the location
  in the green dot table that the value we need at position i is located. 
  Because we're using an 8-bit unsigned integer when the calculation below for
  location is greater than 255 it loops around and starts at 0 again.
  */
  uint8_t location,i;
  for ( i=3; i<64; i++){
    Serial.print("Byte #");
    Serial.print(i);
    location =  (uint8_t)FACTORY_SECURITY_REGISTER[i]+i;
    Serial.print("   Location in Table: ");
    Serial.print(location,DEC);
    USER_SECURITY_REGISTER[i] = GreenDot_Table[location];
    Serial.print("   Value: 0x");
    Serial.println(USER_SECURITY_REGISTER[i],HEX);
  }
  delay(delayTime);

  sCmd.addCommand("PROG", programChip);
  sCmd.addCommand("CHECK", checkChip);
  sCmd.addCommand("REFRESH", chipErrors);
  //sCmd.setDefaultHandler(unrecognized);
  //sCmd.addCommand("HELP", help);
 
  //Uncomment (remove the /* at the start and */ at the end) the lines
  //below to program the Security Register
  /*
  boolean wait = true; //Make sure chip isn't busy
  while (wait == true){
    uint8_t STATUSREG;
    uint8_t NOTBUSY;
    digitalWrite(csPin,LOW);
    SPI.transfer(DATAFLASH_STATUS_REGISTER_READ);
    STATUSREG=SPI.transfer(0);
    digitalWrite(csPin,HIGH);
    NOTBUSY= getBit(STATUSREG, 7);
    if (NOTBUSY == 1){
     wait=false;
    }
  }

  digitalWrite(csPin,LOW);
  Serial.println("Programming security register...");
  SPI.transfer(DATAFLASH_PROGRAM_SECURITY_REGISTER_0);
  SPI.transfer(DATAFLASH_PROGRAM_SECURITY_REGISTER_1);
  SPI.transfer(DATAFLASH_PROGRAM_SECURITY_REGISTER_2);
  SPI.transfer(DATAFLASH_PROGRAM_SECURITY_REGISTER_3);
  for(int i=0;i<64;i++){
   Serial.print("Programming Byte ");
   Serial.println(i);
   SPI.transfer(USER_SECURITY_REGISTER[i]);
  }
  digitalWrite(csPin,HIGH); 
 
  //Verify everything wrote out OK
  wait=true; //Make sure chip isn't busy
  while (wait == true){
    uint8_t STATUSREG;
    uint8_t NOTBUSY;
    digitalWrite(csPin,LOW);
    SPI.transfer(DATAFLASH_STATUS_REGISTER_READ);
    STATUSREG=SPI.transfer(0);
    digitalWrite(csPin,HIGH);
    NOTBUSY= getBit(STATUSREG, 7);
    if (NOTBUSY == 1){
     wait=false;
    }
  }
  delay(delayTime);
 
  Serial.println("Verifying everything went OK...");
  byte VERIFY_SECURITY_REGISTER[128];
  digitalWrite(csPin,LOW);
  SPI.transfer(DATAFLASH_READ_SECURITY_REGISTER);
  SPI.transfer(0x00);//dummy 1
  SPI.transfer(0x00);//dummy 2
  SPI.transfer(0x00);//dummy 3
  for(int i=0;i<128;i++){
    VERIFY_SECURITY_REGISTER[i]=SPI.transfer(0x00);
  }
  digitalWrite(csPin,HIGH);
 
  boolean writeOk = true;
  for (int i=0;i<64;i++){
    if (USER_SECURITY_REGISTER[i]!=VERIFY_SECURITY_REGISTER[i]){
        Serial.print("Error at security register ");
        Serial.print(i);
        Serial.print(": Should be 0x");
        Serial.print(USER_SECURITY_REGISTER[i],HEX);
        Serial.print(".  Is 0x");
        Serial.print(VERIFY_SECURITY_REGISTER[i],HEX);
        Serial.println(".");       
        writeOk = false;
    }
  }
  if (writeOk == true){
      Serial.println("Looks like everything went great!");
   }
   else{
      Serial.println("Oh no!  There was a problem programming the security register!!");
   }



*/
}
void loop(){

sCmd.readSerial(); 
delay(50);

}

uint8_t getBit(uint8_t bits, uint8_t pos){
   return (bits >> pos) & 0x01;

}

void programChip()
{
if(ERRORS == 0){
 boolean wait = true; //Make sure chip isn't busy
  while (wait == true){
    uint8_t STATUSREG;
    uint8_t NOTBUSY;
    digitalWrite(csPin,LOW);
    SPI.transfer(DATAFLASH_STATUS_REGISTER_READ);
    STATUSREG=SPI.transfer(0);
    digitalWrite(csPin,HIGH);
    NOTBUSY= getBit(STATUSREG, 7);
    if (NOTBUSY == 1){
     wait=false;
    }
  }

  digitalWrite(csPin,LOW);
  Serial.println("Programming register...");
  SPI.transfer(DATAFLASH_PROGRAM_SECURITY_REGISTER_0);
  SPI.transfer(DATAFLASH_PROGRAM_SECURITY_REGISTER_1);
  SPI.transfer(DATAFLASH_PROGRAM_SECURITY_REGISTER_2);
  SPI.transfer(DATAFLASH_PROGRAM_SECURITY_REGISTER_3);
  for(int i=0;i<64;i++){
   Serial.print("Programming Byte ");
   Serial.println(i);
   SPI.transfer(USER_SECURITY_REGISTER[i]);
  }
  digitalWrite(csPin,HIGH); 
 
  //Verify everything wrote out OK
  wait=true; //Make sure chip isn't busy
  while (wait == true){
    uint8_t STATUSREG;
    uint8_t NOTBUSY;
    digitalWrite(csPin,LOW);
    SPI.transfer(DATAFLASH_STATUS_REGISTER_READ);
    STATUSREG=SPI.transfer(0);
    digitalWrite(csPin,HIGH);
    NOTBUSY= getBit(STATUSREG, 7);
    if (NOTBUSY == 1){
     wait=false;
    }
  }
  delay(delayTime);
 
  Serial.println("Verifying everything went OK...");
  byte VERIFY_SECURITY_REGISTER[128];
  digitalWrite(csPin,LOW);
  SPI.transfer(DATAFLASH_READ_SECURITY_REGISTER);
  SPI.transfer(0x00);//dummy 1
  SPI.transfer(0x00);//dummy 2
  SPI.transfer(0x00);//dummy 3
  for(int i=0;i<128;i++){
    VERIFY_SECURITY_REGISTER[i]=SPI.transfer(0x00);
  }
  digitalWrite(csPin,HIGH);
 
  boolean writeOk = true;
  for (int i=0;i<64;i++){
    if (USER_SECURITY_REGISTER[i]!=VERIFY_SECURITY_REGISTER[i]){
        Serial.print("Error at register ");
        Serial.print(i);
        Serial.print(": Should be 0x");
        Serial.print(USER_SECURITY_REGISTER[i],HEX);
        Serial.print(".  Is 0x");
        Serial.print(VERIFY_SECURITY_REGISTER[i],HEX);
        Serial.println(".");       
        writeOk = false;
    }
  }
  if (writeOk == true){
      Serial.println("verything went great!");
   }
   else{
      Serial.println("problem programming register!!");
   }

}
else
{
  Serial.print("Chip zaprogramowany!!!");
}
}

void checkChip()
{

    Serial.println("Reading Register...");
  digitalWrite(csPin,LOW);
  SPI.transfer(DATAFLASH_READ_SECURITY_REGISTER);
  SPI.transfer(0x00);//dummy 1
  SPI.transfer(0x00);//dummy 2
  SPI.transfer(0x00);//dummy 3

  Serial.println("Checking that Programmable Register ");
  for (int i=0;i<64;i++){
    uint8_t CURRENT_BYTE;
    CURRENT_BYTE=SPI.transfer(0x00);
    Serial.print("Bajt programowalny #");
    Serial.print(i);
    Serial.print(" ");
    Serial.println(CURRENT_BYTE,HEX);
   
  }
   digitalWrite(csPin,HIGH);

}

void chipErrors(){
ERRORS = 0;
digitalWrite(csPin,LOW);
SPI.transfer(DATAFLASH_READ_SECURITY_REGISTER);
  SPI.transfer(0x00);//dummy 1
  SPI.transfer(0x00);//dummy 2
  SPI.transfer(0x00);//dummy 3


for (int i=0;i<64;i++){
    uint8_t CURRENT_BYTE;
    CURRENT_BYTE=SPI.transfer(0x00);
   if (CURRENT_BYTE != 0xFF){
    Serial.print(" Error!  Byte #");
    Serial.print(i);
    Serial.println(" is not 0xFF!!");
    ERRORS++;
   }
}
digitalWrite(csPin,HIGH);
}
/*
void unrecognized()
{
  //Serial.println("Nieznana komenda - wpisz 'HELP' aby wyswietlic mozliwe komendy"); 

}

void help()
{
  Serial.println(""); 
Serial.print("DLOGGER v"); 
Serial.println(wersja); 
Serial.println("spis komend:"); 

 //Serial.println("PROG - po sprawdzeniu co najmniej 3 razy programuje chip. Mozna tylko raz zaprogramowac tylko raz!!!!");
 //Serial.println("CHECK - pokazuje jakie wartości są wpisane w rejestrze programowalnym , domyslnie jest OxFF pusty niezaprogramowany");
 //Serial.println("REFRESH - sprawdza czy chip nadaje się do zaprogramowania");

}*/
