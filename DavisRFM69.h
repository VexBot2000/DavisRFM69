// **********************************************************************************
// Driver definition for HopeRF RFM69W/RFM69HW, Semtech SX1231/1231H
// **********************************************************************************
// Creative Commons Attrib Share-Alike License
// You are free to use/extend this library but please abide with the CC-BY-SA license:
// http://creativecommons.org/licenses/by-sa/3.0/
// 2014-01-03 (C) DeKay (dekaymail@gmail.com)
// **********************************************************************************
#ifndef DAVISRFM69_h
#define DAVISRFM69_h
#include <Arduino.h>            //assumes Arduino IDE v1.0 or greater
#include <../RFM69/RFM69.h>

class DavisRFM69: public RFM69 {
  public:
    void send(byte toAddress, const void* buffer, byte bufferSize, bool requestACK=false);
    static volatile byte hopIndex;
    bool initialize(byte freqBand, byte ID, byte networkID=1);
    void setChannel(byte channel);
    void hop();
    unsigned int crc16_ccitt(volatile byte *buf, byte len);

  protected:
    void virtual interruptHandler();
    void sendFrame(byte toAddress, const void* buffer, byte size, bool requestACK=false, bool sendACK=false);
    byte reverseBits(byte b);
};

// FRF_MSB, FRF_MID, and FRF_LSB for the 51 North American channels
// used by Davis in frequency hopping
static const uint8_t __attribute__ ((progmem)) FRF[51][3] =
{
  {0xE3, 0xDA, 0x7C},
  {0xE1, 0x98, 0x71},
	{0xE3, 0xFA, 0x92},
	{0xE6, 0xBD, 0x01},
	{0xE4, 0xBB, 0x4D},
	{0xE2, 0x99, 0x56},
	{0xE7, 0x7D, 0xBC},
	{0xE5, 0x9C, 0x0E},
	{0xE3, 0x39, 0xE6},
	{0xE6, 0x1C, 0x81},
	{0xE4, 0x5A, 0xE8},
	{0xE1, 0xF8, 0xD6},
	{0xE5, 0x3B, 0xBF},
	{0xE7, 0x1D, 0x5F},
	{0xE3, 0x9A, 0x3C},
	{0xE2, 0x39, 0x00},
	{0xE4, 0xFB, 0x77},
	{0xE6, 0x5C, 0xB2},
	{0xE2, 0xD9, 0x90},
	{0xE7, 0xBD, 0xEE},
	{0xE4, 0x3A, 0xD2},
	{0xE1, 0xD8, 0xAA},
	{0xE5, 0x5B, 0xCD},
	{0xE6, 0xDD, 0x34},
	{0xE3, 0x5A, 0x0A},
	{0xE7, 0x9D, 0xD9},
	{0xE2, 0x79, 0x41},
	{0xE4, 0x9B, 0x28},
	{0xE5, 0xDC, 0x40},
	{0xE7, 0x3D, 0x74},
	{0xE1, 0xB8, 0x9C},
	{0xE3, 0xBA, 0x60},
	{0xE6, 0x7C, 0xC8},
	{0xE4, 0xDB, 0x62},
	{0xE2, 0xB9, 0x7A},
	{0xE5, 0x7B, 0xE2},
	{0xE7, 0xDE, 0x12},
	{0xE6, 0x3C, 0x9D},
	{0xE3, 0x19, 0xC9},
	{0xE4, 0x1A, 0xB6},
	{0xE5, 0xBC, 0x2B},
	{0xE2, 0x18, 0xEB},
	{0xE6, 0xFD, 0x42},
	{0xE5, 0x1B, 0xA3},
	{0xE3, 0x7A, 0x2E},
	{0xE5, 0xFC, 0x64},
	{0xE2, 0x59, 0x16},
	{0xE6, 0x9C, 0xEC},
	{0xE2, 0xF9, 0xAC},
	{0xE4, 0x7B, 0x0C},
	{0xE7, 0x5D, 0x98}
}; 

#endif  // DAVISRFM_h
