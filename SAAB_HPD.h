#ifndef SAAB_HPD_H
#define SAAB_HPD_H

#include <Arduino.h>
#include <HardwareSerial.h>

// Style constants
// Can be combined
#define HPD_STYLE_NORMAL 0x00
#define HPD_STYLE_RIGHT_ALIGN 0x10
#define HPD_STYLE_BLINKING 0x20
#define HPD_STYLE_INVERTED 0x40
#define HPD_STYLE_UNDERLINE 0x80

// Visibility constants
#define HPD_VISIBLE 0x02
#define HPD_HIDDEN 0x01
#define HPD_VISIBLE_2 0x08
#define HPD_HIDDEN_2 0x03 

// Font/size constants
#define HPD_FONT_SMALL 0x00
#define HPD_FONT_LARGE 0x01
#define HPD_FONT_MEDIUM 0x02
#define HPD_FONT_TIME 0x04
#define HPD_FONT_TIME_2 0x14

// UART Configuration
#define RXD2 33
#define TXD2 32
#define BUFFER_SIZE 0xFF

// Sync pattern for SID communication
const uint8_t okPattern[] = {0x02, 0xFF, 0x00, 0x01};
const uint8_t okPatternLength = sizeof(okPattern);

class SAAB_HPD {
public:
    SAAB_HPD(HardwareSerial &serial = Serial2);

    void begin(uint32_t baudRate = 115200, uint8_t rxPin = RXD2, uint8_t txPin = TXD2);
    void setDebug(bool enable = false);
    void toggleDebug();

    void sendSidData(byte lenA, byte* data);
    void sendSidRawData(size_t len, byte* data);
    void sendTestModeMessage();


    bool verifyChecksum(uint8_t *frame);
    bool isValidDLC(uint8_t dlc);
    bool readSIDserialData(uint8_t* frame);

    void makeRegion(uint8_t regionID, uint8_t subRegionID0, uint8_t subRegionID1, uint8_t xPos, uint8_t yPos, uint8_t width, uint8_t fontStyle, char* text = nullptr);
    void changeRegion(uint8_t regionID, uint8_t subRegionID0, uint8_t subRegionID1, uint8_t visible, uint8_t style, char* text = nullptr);
    void replaceAuxPlayText(char* text);
    void drawRegion(uint8_t regionID, uint8_t drawFlag = 0x01);
    void clearRegion(uint8_t regionID, uint8_t clearFlag = 0x01);

private:
    HardwareSerial &SIDSerial;
    bool printDebug;
    uint8_t buffer[BUFFER_SIZE];
    uint8_t bufferIndex;
    uint8_t expectedLength;
    bool syncFound;
};

#endif // SAAB_HPD_H