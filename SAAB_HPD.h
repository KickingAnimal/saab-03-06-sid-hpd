#ifndef SAAB_HPD_H
#define SAAB_HPD_H

#include <Arduino.h>
#include <HardwareSerial.h>

// style constants
// effects can be combined/stacked
#define HPD_STYLE_NORMAL 0x00
#define HPD_STYLE_RIGHT_ALIGN 0x10
#define HPD_STYLE_BLINKING 0x20
#define HPD_STYLE_INVERTED 0x40
#define HPD_STYLE_UNDERLINE 0x80

// visibility
#define HPD_VISIBLE 0x02 // show
#define HPD_HIDDEN 0x01 // hide
#define HPD_VISIBLE_2 0x08 // show (alternative?)
#define HPD_HIDDEN_2 0x03 // hide (alternative?)

// font/size // T.B.D where to find/use these (0x10?) and if correct.
// 0x00 small (bottom row),
// 0x01 medium (radio mode),
// 0x02 standard (RDS/CD text),
//  0x04 & 0x14 (time portion)
#define HPD_FONT_SMALL = 0x00
#define HPD_FONT_LARGE = 0x01
#define HPD_FONT_STANDARD = 0x02
#define HPD_FONT_TIME = 0x04
#define HPD_FONT_TIME_2 = 0x14

//uart configuration
#define RXD2 33
#define TXD2 32
#define BUFFER_SIZE 0xFF
extern HardwareSerial &SIDSerial;

extern bool printDebug; // Toggle with 'DEBUG' command

// sid protocol constants
#define REGION_ID 3
#define SUB_REGION_ID0 5
#define SUB_REGION_ID1 6
#define OK_MSG_FRAME {0x02, 0xFF, 0x00, 0x01} // OK message frame

//
const uint8_t okPattern[] = OK_MSG_FRAME;
const uint8_t okPatternLength = sizeof(okPattern);


//function prototypes

void sendSidData(byte lenA, byte* data);
void sendSidRawData(size_t len, byte* data);

bool verifyChecksum(uint8_t *frame);
bool isValidDLC(uint8_t dlc);
bool readSIDserialData(uint8_t* frame);
void processFrame(uint8_t* frame);

void makeRegion(uint8_t regionID, uint8_t subRegionID0, uint8_t subRegionID1, uint8_t xPos, uint8_t yPos, uint8_t width, uint8_t fontStyle, char* text = nullptr);
void changeRegion(uint8_t regionID, uint8_t subRegionID0, uint8_t subRegionID1, uint8_t visible, uint8_t style, char* text = nullptr);
void replaceAuxPlayText(char* text);
void drawRegion(uint8_t regionID, uint8_t drawFlag = 0x01); // command 0x70
void clearRegion(uint8_t regionID, uint8_t clearFlag = 0x01); // command 0x60

#endif // SAAB_HPD_H