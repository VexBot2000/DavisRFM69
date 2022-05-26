// Sample usage of the DavisRFM69 library to sniff the packets from a Davis Instruments
        // wireless Integrated Sensor Suite (ISS), demostrating compatibility between the RFM69
        // and the TI CC1020 transmitter used in that hardware.  Packets received from the ISS are
// translated into a format compatible with the Davis Vantage Pro2 (VP2) and Vantage Vue
// consoles.  This example also reads BMP085 and DHT22 sensors connected directly to the
// Moteino (see below)
// See http://madscientistlabs.blogspot.com/2014/02/build-your-own-davis-weather-station_17.html

// This is part of the DavisRFM69 library from https://github.com/dekay/DavisRFM69
// (C) DeKay 2014 dekaymail@gmail.com
// Example released under the MIT License (http://opensource.org/licenses/mit-license.php)
// Get the RFM69 and SPIFlash library at: https://github.com/LowPowerLab/

// This program has been developed on a Moteino R3 Arduino clone with integrated RFM69W
// transceiver module.  Note that RFM12B-based modules will not work.  See the README
// for more details.

#include <DavisRFM69.h>       // From https://github.com/dekay/DavisRFM69 
#include <EEPROM.h>           // From standard Arduino library
#include <SPI.h>              // From standard Arduino library
#include <SerialCommand.h>    // From https://github.com/dekay/Arduino-SerialCommand
#include <arduino-timer.h>
#include <SPIFlash.h> 

// Reduce number of bogus compiler warnings
// http://forum.arduino.cc/index.php?PHPSESSID=uakeh64e6f5lb3s35aunrgfjq1&topic=102182.msg766625#msg766625
#undef PROGMEM
#define PROGMEM __attribute__(( section(".progmem.data") ))
#define wersja 0.95


// NOTE: *** One of DAVIS_FREQS_US, DAVIS_FREQS_EU, DAVIS_FREQS_AU, or
// DAVIS_FREQS_NZ MUST be defined at the top of DavisRFM69.h ***

#define IS_RFM69HW    //uncomment only for RFM69HW! Leave out if you have RFM69W!
#define SERIAL_BAUD     19200 // Davis console is 19200 by default
#define POLL_INTERVAL   5000  // Read indoor sensors every minute. Console uses 60 seconds

#define LATITUDE       -1071  // Station latitude in tenths of degrees East
#define LONGITUDE       541   // Station longitude in tenths of degrees North
#define ELEVATION       800   // Station height above sea level in feet

#define PACKET_INTERVAL 2555
#define LOOP_INTERVAL   2500

boolean strmon = true;       // Print the packet when received?
DavisRFM69 radio;

// flash(FLASH_SS, MANUFACTURER_ID)
//   FLASH_SS - SS pin attached to SPI flash chip (defined above)
//   MANUFACTURER_ID - OPTIONAL
//     0x1F44 for adesto(ex atmel) 4mbit flash
//     0xEF30 for windbond 4mbit flash
#define LED           9     // Moteinos have LEDs on D9
 #define FLASH_SS      8 
 SPIFlash flash(FLASH_SS, 0xEF30);

Timer<1, micros> timer;

SerialCommand sCmd;

LoopPacket loopData;
volatile bool oneMinutePassed = false;

void rtcInterrupt(void) {
  oneMinutePassed = true;
}

void(* resetFunc) (void) = 0;

void setup() {
  Serial.begin(SERIAL_BAUD);
  Serial.println("Serial connection ON");
  delay(10);
  radio.initialize();
  radio.setChannel(0);  // Frequency / Channel *not* set in initialization. Do it right after.
#ifdef IS_RFM69HW
  radio.setHighPower(); // Uncomment only for RFM69HW!
#endif
  // Initialize the loop data array
  memcpy(&loopData, &loopInit, sizeof(loopInit));


  // Set up DS3231 real time clock on Moteino INT1
  timer.every(10000000, rtcInterrupt); //That makes it run better... right? Davis console supposedly has it set to "60000000"
  // Initialize the flash chip on the Moteino
  if (!flash.initialize()) {
    Serial.println(F("SPI Flash Init Failed. Is chip present and correct manufacturer specified in constructor?"));
    while (1) { }
  }
  
 
  // Setup callbacks for SerialCommand commands
  sCmd.addCommand("LOOP", cmdLoop);         // Send the loop data
  sCmd.addCommand("RXCHECK", cmdRxcheck);   // Receiver Stats check
  sCmd.addCommand("SETPER", cmdSetper);     // Set archive interval period
  sCmd.addCommand("STRMOFF", cmdStrmoff);   // Disable printing of received packet data
  sCmd.addCommand("STRMON", cmdStrmon);     // Enable printing of received packet data
  sCmd.addCommand("TEST", cmdTest);         // Echo's "TEST"
  sCmd.setDefaultHandler(cmdUnrecognized);  // Handler for command that isn't matched
  sCmd.setNullHandler(cmdWake);             // Handler for an empty line to wake the simulated console
  sCmd.addCommand("EEBRD", cmdEebrd);       // EEPROM Read
  sCmd.addCommand("DMPAFT", cmdDmpaft); // Download archive records after date:time specified
  sCmd.addCommand("INFO", processPacket);
  sCmd.addCommand("ALLREG", cmdReadRegs);// Opcja debugowania - wyświetla wszytskie stany rejestrów
  sCmd.addCommand("HELP", help);
  sCmd.addCommand("RESET", resetFunc);
  
#if 0
  printFreeRam();
#endif
}

// See https://github.com/dekay/im-me/blob/master/pocketwx/src/protocol.txt
// Console update rates: * indicates what is done, X indicates ignored, ? indicates in progress
// - Clock display updates once per minute
// * Barometric pressure once per minute
// - Dewpoint every ten to twelve seconds (calculated)
// - Forecast, evapotranspiration, and heat index every hour
// * Inside humidity every minute
// * Outside humidity every 50 to 60 seconds
// - Leaf wetness every 46 to 64 seconds
// - Rainfall and rain rate every 20 to 24 seconds
// - Soil moisture every 77 to 90 seconds
// - UV, UV Index and solar radiation every 50 to 60 secondes (5 minutes when dark)
// * Inside temperature every minute
// * Outside temperature every 10 to 12 seconds
// X Extra temperature sensors or probes every 10 to 12 seconds
// - Temperature humidity sun wind index every 10 to 12 seconds
// - Wind chill every 10 to 12 seconds (calculated)
// ? Wind direction every 2.5 - 3 seconds
// * Wind speed every 2.5 - 3 seconds
// - Wind speed 10 minute average every minute

/*
 * // Davis packet types, also defined by kobuki
#define VP2P_UV             0x4 // UV index
#define VP2P_RAINSECS       0x5 // seconds between rain bucket tips
#define VP2P_SOLAR          0x6 // solar irradiation
#define VP2P_TEMP           0x8 // outside temperature
#define VP2P_WINDGUST       0x9 // 10-minute wind gust
#define VP2P_HUMIDITY       0xA // outside humidity
#define VP2P_RAIN           0xE // rain bucket tips counter
 */
long czas = 0;
uint32_t czas2 = 0;
long lastPeriod = -1;
unsigned long czas_od = millis();
unsigned long stary_czas = millis();
unsigned long czas_pakietu = 0;
uint32_t lastRxTime = 0;
uint32_t lastLoopTime = 0;
uint8_t hopCount = 0;
uint16_t loopCount = 0;
unsigned int cnt = 0;
bool znaleziono = false;

void loop() {
  timer.tick();
  if (Serial.available() > 0) loopCount = 0; // if we receive anything while sending LOOP packets, stop the stream
  sendLoopPacket(); 

  if (oneMinutePassed) clearAlarmInterrupt();

  sCmd.readSerial(); // Process serial commands

  int16_t currPeriod = millis()/POLL_INTERVAL;

  if (currPeriod != lastPeriod) {
    lastPeriod=currPeriod;
  }
 czas2=millis();
 if (radio.receiveDone()) {
    packetStats.packetsReceived++;
    uint16_t crc = radio.crc16_ccitt(radio.DATA, 6);
    if ((crc == (word(radio.DATA[6], radio.DATA[7]))) && (crc != 0)) {
      if (strmon) printStrm();
      czas_pakietu = millis();
      czas_od = (czas_pakietu-stary_czas);
    Serial.println(" czas pakietu ");
    Serial.print(czas_od);
    stary_czas = millis();

    znaleziono = true;
    
      radio.hop();
     Serial.print(" skok na kanal ");
    Serial.print(radio.CHANNEL);
      packetStats.receivedStreak++;
      hopCount = 1;
      lastRxTime = millis();
    } 
    else if (czas2%800 == 0) {
      packetStats.crcErrors++;
      packetStats.receivedStreak = 0;    
       
    }

    // Whether CRC is right or not, we count that as reception and hop.
    
    
  
    //hopCount++; 
    
  }
  
  czas2=millis();

  // If a packet was not received at the expected time, hop the radio anyway
  // in an attempt to keep up.  Give up after 25 failed attempts.  Keep track
  // of packet stats as we go.  I consider a consecutive string of missed
  // packets to be a single resync.  Thx to Kobuki for this algorithm.
  if (znaleziono == true) {
    uint32_t czas_pakietu1 = czas2 - lastRxTime;
    uint32_t czas_bledu= (hopCount * PACKET_INTERVAL)+30;
   // Serial.println(" czas ");
    //Serial.println(czas_pakietu1);
   // Serial.println(czas_bledu);
    
    if(czas_pakietu1 >= czas_bledu)
    {
    packetStats.packetsMissed++;
    hopCount++;
    if (hopCount == 1) packetStats.numResyncs++;
    if (hopCount > 5){
      hopCount = 0;
      znaleziono = false;
        radio.CHANNEL = 0;
    packetStats.crcErrors = 0;
    packetStats.packetsMissed = 0;
     radio.setChannel(0);
      Serial.print(" Reset kanalu ");
    }
    radio.hop();
     Serial.print(" skok na kanal po braku respondu ");
    Serial.print(radio.CHANNEL);
    }
  }
  else if(hopCount == 0 && znaleziono == false){
    radio.CHANNEL = 0;
    packetStats.crcErrors = 0;
    packetStats.packetsMissed = 0;
     radio.setChannel(0);
    //Serial.print(" Reset kanalu ");
  }
}
//koniec void loop

// Read the data from the ISS and figure out what to do with it
void processPacket() {
  // Every packet has wind speed, direction, and battery status in it

  //DANE - PRĘDKOŚĆ WIATRU
  Serial.println(radio.DATA[0]);
  loopData.windSpeed = radio.DATA[1];
  Serial.print(" ws ");
  Serial.print(loopData.windSpeed);

  //DANE - KIERUNEK WIATRU
  // There is a dead zone on the wind vane. No values are reported between 8
  // and 352 degrees inclusive. These values correspond to received byte
  // values of 1 and 255 respectively
  // See http://www.wxforum.net/index.php?topic=21967.50
  loopData.windDirection = 9 + radio.DATA[2] * 342.0f / 255.0f;
  Serial.print(F(" wd "));
  Serial.print(loopData.windDirection);

  //DANE - STAN NAŁADOWANIA BATERII
  loopData.transmitterBatteryStatus = (radio.DATA[0] & 0x8) >> 3;
  Serial.print(" bat ");
  Serial.print(loopData.transmitterBatteryStatus);
  //Debug
  
 // Serial.print(" test ");
  uint8_t test = radio.DATA[0]>>4;
 // Serial.print(test,DEC);
 
  // Now look at each individual packet. The high order nibble is the packet type.
  // The highest order bit of the low nibble is set high when the ISS battery is low.
  // The low order three bits of the low nibble are the station ID.

  //DANE - U

     if (test == VP2P_HUMIDITY) //WILGOTNOSC
  {
        loopData.outsideHumidity = (float)(word(((radio.DATA[4] >> 4)<< 8) + radio.DATA[3])) / 10.0;
      Serial.print(" rh ");
      Serial.print(loopData.outsideHumidity);
  }
     if (test == VP2P_RAIN) //INTESYWNOSC DESZCZU
  {
    loopData.rainRate = (float)(word(radio.DATA[3]));
    uint8_t z = (uint8_t)(word(radio.DATA[5]));
    Serial.print(" re ");  
    if (z == 41)
    {
    Serial.print(" true ");
    Serial.print(" ra ");
    Serial.print(loopData.rainRate);
    }
    else
      {
      Serial.print(" false ");
      }
 
  }
  switch (test) {
    // DANE - INDEX UV
    case VP2P_UV:
      loopData.uV = (float)(word(radio.DATA[3] << 8 + radio.DATA[4]) >> 6) / 50.0;
      if(radio.DATA[3]==0xFF)
        Serial.print(" uv-none ");
        else{
      Serial.print(F(" uv "));
      Serial.print(loopData.uV);
        }
      break;

    //DANE - PROMIENIOWANIE SŁONECZNE(?)
    case VP2P_SOLAR:
      loopData.solarRadiation = (float)(word(radio.DATA[3] << 8 + radio.DATA[4]) >> 6) * 1.757936;
      if(radio.DATA[3]==0xFF)
        Serial.print(" sr-none ");
        else{
      Serial.print(F(" sr "));
      Serial.print(loopData.solarRadiation);
        }
      break;

    //DANE - TEMPERATURA
    case VP2P_TEMP:
      loopData.outsideTemperature = (float)(word((radio.DATA[3]<<8) | (radio.DATA[4]))) / 160.0;
      float x = loopData.outsideTemperature-32;
      x =x *5;
      x = x/9;
      Serial.print(F(" ta "));
      Serial.print(x);
     break;
  /*
    //DANE - WILGOTNOŚĆ //nie łapie
    case 0xA:
      loopData.outsideHumidity = (float)(word(((radio.DATA[4] >> 4)<< 8) + radio.DATA[3])) / 10.0;
      Serial.print(" rh ");
      Serial.print(loopData.outsideHumidity);
      break;
    // DANE - DESZCZ 
    case 0xE:
    loopData.rainRate = (float)(word(radio.DATA[3]));
    float z = (float)(word(radio.DATA[5]));
    Serial.print(" re ");  
    if (z == 41)
    {
     Serial.print(" true ");
    }
    else
      {
      Serial.print(" false ");
      }
    Serial.print(" ra ");
    Serial.print(loopData.rainRate);
    break;
*/
     //default:
  
  }
 
#if 0
  printFreeRam();
#endif
}


//--- Console command emulation ---//

void cmdEebrd() {
  // TODO This is in the middle of a transition and will need to be reworked at
  // some point.  The idea is to use the real Moteino EEPROM to store this
  // stuff.  See the EEPROM_ARCHIVE_PERIOD for a start.
  char *cmdMemLocation, *cmdNumBytes;
  uint8_t response[2] = {0, 0};
  uint8_t responseLength = 0;
  bool validCommand = true;

  cmdMemLocation = sCmd.next();
  cmdNumBytes = sCmd.next();
  if ((cmdMemLocation != NULL) && (cmdNumBytes != NULL)) {
    switch (strtol(cmdMemLocation, NULL, 16)) {
    case EEPROM_LATITUDE_LSB:
      response[0] = lowByte(LATITUDE);
      response[1] = highByte(LATITUDE);
      responseLength = 2;
      break;
    case EEPROM_LONGITUDE_LSB:
      response[0] = lowByte(LONGITUDE);
      response[1] = highByte(LONGITUDE);
      responseLength = 2;
      break;
    case EEPROM_ELEVATION_LSB:
      response[0] = lowByte(ELEVATION);
      response[1] = highByte(ELEVATION);
      responseLength = 2;
      break;
    case EEPROM_TIME_ZONE_INDEX:
      response[0] = GMT_ZONE_MINUS600;
      responseLength = 1;
      break;
    case EEPROM_DST_MANAUTO:
      response[0] = DST_USE_MODE_MANUAL;
      responseLength = 1;
      break;
    case EEPROM_DST_OFFON:
      response[0] = DST_SET_MODE_STANDARD;
      responseLength = 1;
      break;
    case EEPROM_GMT_OFFSET_LSB:
      // TODO GMT_OFFSETS haven't been calculated yet. Zero for now.
      response[0] = lowByte(GMT_OFFSET_MINUS600);
      response[1] = highByte(GMT_OFFSET_MINUS600);
      responseLength = 2;
      break;
    case EEPROM_GMT_OR_ZONE:
      response[0] = GMT_OR_ZONE_USE_INDEX;
      responseLength = 1;
      break;
    case EEPROM_UNIT_BITS:  // Memory location for setup bits. Assume one byte is asked for.
      response[0] = BAROMETER_UNITS_IN | TEMP_UNITS_TENTHS_F | ELEVATION_UNITS_FEET | RAIN_UNITS_IN | WIND_UNITS_MPH;
      responseLength = 1;
      break;
    case EEPROM_SETUP_BITS:  // Memory location for setup bits. Assume one byte is asked for.
      // TODO The AM / PM indication isn't set yet.  Need my clock chip first.
      response[0] = LONGITUDE_WEST | LATITUDE_NORTH | RAIN_COLLECTOR_01IN | WIND_CUP_LARGE | MONTH_DAY_MONTHDAY | AMPM_TIME_MODE_24H;
      responseLength = 1;
      break;
    case EEPROM_RAIN_YEAR_START:
      response[0] = RAIN_SEASON_START_JAN;
      responseLength = 1;
      break;
    case EEPROM_ARCHIVE_PERIOD:
      // TODO We don't actually implement archiving yet
      // Once everything is in EEPROM, we just need the number of bytes
      // for each special memory location rather than this stupid case statement.
      response[0] = EEPROM.read(EEPROM_ARCHIVE_PERIOD);
      responseLength = 1;
      break;
    default:
      validCommand = false;
      printNack();
    }

    if (validCommand) {
      uint16_t crc = radio.crc16_ccitt(response, responseLength);
      printAck();
      for (uint8_t i = 0; i < responseLength; i++) Serial.write(response[i]);
      Serial.write(highByte(crc));
      Serial.write(lowByte(crc));
    }
  } else {
    printNack();
  }
}

void cmdLoop() {
  char *loops;
  lastLoopTime = 0;
  printAck();
  loops = sCmd.next();
  if (loops != NULL) {
    loopCount = strtol(loops, NULL, 10);
  } else {
    loopCount = 1;
  }
}

void cmdReadRegs(){ // Opcja debugowania wyświetla wszystkie rejestry moteino
  Serial.print(" DEBUG - Read all registers ");
radio.readAllRegs(); 
 Serial.print(" DONE ");
}

void sendLoopPacket() {
  if (loopCount <= 0 || millis() - lastLoopTime < LOOP_INTERVAL) return;
  lastLoopTime = millis();
  loopCount--;
  // Calculate the CRC over the entire length of the loop packet.
  uint8_t *loopPtr = (uint8_t *)&loopData;
  uint16_t crc = radio.crc16_ccitt(loopPtr, sizeof(loopData));

  for (uint8_t i = 0; i < sizeof(loopData); i++) Serial.write(loopPtr[i]);
  Serial.write(highByte(crc));
  Serial.write(lowByte(crc));
}

// Print receiver stats
void cmdRxcheck() {
  printAck();
  printOk();
  uint16_t *statsPtr = (uint16_t *)&packetStats;
  for (byte i = 0; i < (sizeof(packetStats) >> 1); i++) {
    Serial.print(F(" ")); // Note. The real console has this leading space.
    Serial.print(statsPtr[i]);
  }
  Serial.print(F("\n\r"));
}

void cmdSetper() {
  char *period;
  period = sCmd.next();
  if (period != NULL) {
    uint8_t minutes = strtol(period, NULL, 10);
    if (minutes != EEPROM.read(EEPROM_ARCHIVE_PERIOD)) {
      // The console erases the flash chip if the archive period is changed
      // so that all records in the flash have the same interval.
      EEPROM.write(EEPROM_ARCHIVE_PERIOD, strtol(period, NULL, 10));
#if 0
      Serial.print(F("New archive interval = "));
      Serial.println(EEPROM.read(EEPROM_ARCHIVE_PERIOD));
#endif
    }
    printAck();
  } else {
    printNack();
  }
}

void cmdStrmoff() {
  printOk();
  strmon =  false;
}

void cmdStrmon() {
  printOk();
  strmon =  true;
}

void cmdTest() {
  Serial.print(F("TEST\n"));
}


void cmdWake() {
  Serial.print(F("\n\r"));

 }

void cmdWRD() {
  printAck();
  Serial.write(16);
}
// This gets set as the default handler, and gets called when no other command matches.
void cmdUnrecognized(const char *command) {
  Serial.println("Nieznana komenda - wpisz 'HELP' aby wyswietlic mozliwe komendy"); 
}

// Print related support functions
void printAck() {
  Serial.write(0x06);
}

void printNack() {
  Serial.write(0x21);
}

// From http://jeelabs.org/2011/05/22/atmega-memory-use/
void printFreeRam() {
  extern int __heap_start, *__brkval;
  int16_t v;
  Serial.print(F("Free mem: "));
  Serial.println((int16_t) &v - (__brkval == 0 ? (int16_t) &__heap_start : (int16_t) __brkval));
}

void printOk() {
  Serial.print(F("\n\rOK\n\r"));
}
//Funkcja odpowiadająca za wysyłanie pakietów
void printStrm() {
/* Struktura pakietu:
    cnt <NUMER PAKIERTU> hex <DANE JAK NIŻEJ, TYLKO ZEBRANE W JEDNYM CIĄGU> ws <PRĘDKOŚĆ WIATRU> wd <KIERUNEK WIATRU> bat <STATUS BATERII> chan <KANAŁ> RSSI <MOC ODEBRANEGO SYGNAŁU> errors <WYKRYTE BŁĘDY Z CRC>  missed <PAKIETY UTRACONE> numresyncs <>
    <DANE JAK W POLU 'hex', ALE W FORMACIE STRM DAVISA>
    ...
 */
  cnt++;
  Serial.print(F("cnt "));
  Serial.print(cnt);
  Serial.print(F(" hex "));
  char charBuffer[128];
  for (uint8_t i = 0; i < DAVIS_PACKET_LEN; i++) {
    sprintf(charBuffer,"%02x",radio.DATA[i]);
    Serial.print(charBuffer);
    }
  processPacket();
  Serial.print(F(" chan "));
  Serial.print(radio.CHANNEL);
  Serial.print(F(" RSSI "));
  Serial.print(radio.RSSI);
  Serial.print(F(" errors "));
  //Max amount of errors is 3. If there are 3 or more it resets back to 0
  Serial.print(packetStats.crcErrors);
  Serial.print(F(" missed "));
  Serial.print(packetStats.packetsMissed);
  Serial.print(F(" numresyncs "));
  Serial.println(packetStats.numResyncs);

  //Transmisja odebranych danych w formacie jak w STRM
  for (uint8_t i = 0; i < DAVIS_PACKET_LEN; i++) {
    Serial.print(i);
    Serial.print(" = ");
    String dane = String(radio.DATA[i], HEX);
    dane.toLowerCase();
    Serial.println(dane);
  }
  Serial.println();
} 

// Ancillary functions
void blink(uint8_t PIN, uint16_t DELAY_MS)
{
  pinMode(PIN, OUTPUT);
  digitalWrite(PIN,HIGH);
  delay(DELAY_MS);
  digitalWrite(PIN,LOW);
}

void cmdDmpaft() {
  printAck();
  // Read 2 byte vantageDateStamp, the 2 byte vantageTimeStamp, and a 2 byte CRC
  while (Serial.available() <= 0);
  for (uint8_t i = 0; i < 6; i++) Serial.read();
  printAck();

  // From Davis' docs:
  //   Each archive record is 52 bytes. Records sent to the PC in 264 byte pages. Each page
  //   contains 5 archive records and 4 unused bytes.

  // Send the number of "pages" that will be sent (2 bytes), the location within the first
  // page of the first record, and 2 Byte CRC
  uint8_t response[4] = { 1, 0, 0, 0 }; // L,H;L,H -- 1 page; first record is #0
  uint16_t crc = radio.crc16_ccitt(response, 4);
  for (uint8_t i = 0; i < 4; i++)
    Serial.write(response[i]);
  Serial.write(highByte(crc));
  Serial.write(lowByte(crc));
  while (Serial.available() <= 0);
  if (Serial.read() != 0x06); // should return if condition is true, but can't get a 0x06 here for the life of me, even if weewx does send it...

  // The format of each page is:
  // 1 Byte sequence number (starts at 0 and wraps from 255 back to 0)
  // 52 Byte Data record [5 times]
  // 4 Byte unused bytes
  // 2 Byte CRC
  response[0] = 0;
  crc = radio.crc16_ccitt(response, 1);
  Serial.write(0);
  uint8_t * farp = (uint8_t *)&fakeArchiveRec;
  for (uint8_t i = 0; i < 5; i++) {
    crc = radio.crc16_ccitt(farp, sizeof(fakeArchiveRec), crc);
    for (uint8_t j = 0; j < sizeof(fakeArchiveRec); j++) Serial.write(farp[j]);
  }
  for (uint8_t i = 0; i < 4; i++) Serial.write(0);
  crc = radio.crc16_ccitt(response, 4, crc);
  Serial.write(highByte(crc));
  Serial.write(lowByte(crc));
}


void clearAlarmInterrupt()
{
  oneMinutePassed = false;
}


  



void help()
{
Serial.println(""); 
Serial.print("DAVISVP2 v"); 
Serial.println(wersja); 
Serial.println("spis komend:"); 

  Serial.println("RESET"); 
  Serial.println("LOOP - Send the loop data"); 
 // Serial.println("RXCHECK - Receiver Stats check"); 
 // Serial.println("SETPER - Set archive interval period"); 
  Serial.println("STRMOFF - Disable printing of received packet data");
  Serial.println("STRMON - Enable printing of received packet data");
  Serial.println("TEST - Echo's TEST");
 //Serial.println("EEBRD - EEPROM Read");
 // Serial.println("DMPAFT - Download archive records after date:time specified");
  Serial.println("INFO - Analizuje ostatnio odebrany pakiet");
  Serial.println("ALLREG - wyswietla wszytskie stany rejestrow");
 // Serial.println("Oznaczenia danych: ws - predkosc wiatru [mile/h],wd - kierunek wiatru w stopniach,bat - stan baterii (1 brak lub rozładowana,a 0 naładowana)\nta - temperatura w stopniach celsjusza,sr - promieniowanie sloneczne w W/m2[none - oznacza brak czujnika],uV - index uV [none - oznacza brak czujnika]\nrh - wilgotnosc w %, re - poziom deszczu[false oznacza brak]");
  //Serial.println("ta - temperatura w stopniach celsjusza,sr - promieniowanie sloneczne w W/m2[none - oznacza brak czujnika],uV - index uV [none - oznacza brak czujnika]");
 // Serial.println("rh - wilgotnosc w %, re - poziom deszczu[false oznacza brak]");

}
