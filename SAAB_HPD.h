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
const uint8_t syncPattern[] = {0x02, 0x81, 0x00, 0x83};
const uint8_t syncPatternLength = sizeof(syncPattern);

class SAAB_HPD {
public:
    // frame struct
    struct SerialFrame {
        uint8_t dlc; // Data Length Code
        uint8_t command; // Command byte
                        // padding is always 0x00
        uint8_t data[BUFFER_SIZE - 3]; // Data bytes
        uint8_t checksum; // Checksum byte
    };

    SAAB_HPD(HardwareSerial &serial = Serial2);

    void begin(uint32_t baudRate = 115200, uint8_t rxPin = RXD2, uint8_t txPin = TXD2);
    void setDebug(bool enable = false);
    void toggleDebug();
    
    void pollSerialData(); // Polls the SID serial data and processes it

    // sid communication functions
    void sendSidData(byte lenA, byte* data);
    void sendSidRawData(size_t len, byte* data);
    void sendTestModeMessage();

    // Functions to create and modify regions on the display
    // Should return the enum error/ack code from the SID
    void makeRegion(uint8_t regionID, uint8_t subRegionID0, uint8_t subRegionID1, uint8_t xPos, uint8_t yPos, uint8_t width, uint8_t fontStyle, char* text = nullptr);
    void changeRegion(uint8_t regionID, uint8_t subRegionID0, uint8_t subRegionID1, uint8_t visible, uint8_t style, char* text = nullptr);
    void replaceAuxPlayText(char* text);
    void drawRegion(uint8_t regionID, uint8_t drawFlag = 0x01);
    void clearRegion(uint8_t regionID, uint8_t clearFlag = 0x01);

    // Enum for display modes
    enum MODE {
        MODE_UNKNOWN,
        MODE_AUX,
        MODE_FM1,
        MODE_FM2,
        MODE_AM,
        MODE_CD,
        MODE_CDC,
        MODE_CDX
    };

    MODE currentMode(); // Returns the current display mode as an enum

    void poll(); // Polls and processes incoming SID serial data

    // Callback for handling processed frames
    typedef void (*FrameCallback)(const SerialFrame &frame);
    void setFrameCallback(FrameCallback callback);

private:
    HardwareSerial &SIDSerial;
    bool printDebug;
    uint8_t buffer[BUFFER_SIZE];
    uint8_t bufferIndex;
    uint8_t expectedLength;
    bool syncFound;

    FrameCallback frameCallback; // Callback function for processed frames

    // Internal methods
    bool readSIDserialData(SerialFrame &frame); // Now private
    uint8_t calculateChecksum(const SerialFrame &frame);
    bool verifyChecksum(const SerialFrame &frame);
    bool isValidDLC(uint8_t dlc);
};

#endif // SAAB_HPD_H