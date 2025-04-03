#ifndef SAAB_HPD_H
#define SAAB_HPD_H

#include <Arduino.h>
#include <HardwareSerial.h>

// Namespace for constants
namespace SAAB_HPD_Constants {
    // Style constants
    constexpr uint8_t HPD_STYLE_NORMAL = 0x00;
    constexpr uint8_t HPD_STYLE_RIGHT_ALIGN = 0x10;
    constexpr uint8_t HPD_STYLE_BLINKING = 0x20;
    constexpr uint8_t HPD_STYLE_INVERTED = 0x40;
    constexpr uint8_t HPD_STYLE_UNDERLINE = 0x80;
    
    // Visibility constants
    constexpr uint8_t HPD_VISIBLE = 0x02;
    constexpr uint8_t HPD_HIDDEN = 0x01;
    constexpr uint8_t HPD_VISIBLE_2 = 0x08;
    constexpr uint8_t HPD_HIDDEN_2 = 0x03;
    
    // Font/size constants
    constexpr uint8_t HPD_FONT_SMALL = 0x00;
    constexpr uint8_t HPD_FONT_LARGE = 0x01;
    constexpr uint8_t HPD_FONT_MEDIUM = 0x02;
    constexpr uint8_t HPD_FONT_TIME = 0x04;
    constexpr uint8_t HPD_FONT_TIME_2 = 0x14;
}
using namespace SAAB_HPD_Constants; // Use the constants namespace

// Buffer size for incoming data
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

    void begin(uint8_t rxPin, uint8_t txPin);
    void setDebug(bool enable = false);
    void toggleDebug();
    
    void pollSerialData(); // Polls the SID serial data and processes it

    // Enum for error codes
    enum ERROR {
        ERROR_OK = 0,          // Success
        ERROR_TIMEOUT = -1,    // Timeout waiting for response
        ERROR_INVALID_COMMAND = 0x31, // Invalid command
        ERROR_REGION_EXISTS = 0x33,   // Region already exists
        ERROR_INVALID_ARGS = 0x34,    // Invalid arguments/length
        ERROR_UNKNOWN_35 = 0x35,      // Unknown error 0x35
        ERROR_UNKNOWN_37 = 0x37       // Unknown error 0x37
    };

    // sid communication functions
    ERROR sendSidData(SerialFrame &frame); // Returns an ERROR enum
    void sendSidRawData(size_t len, byte* data);
    void sendTestModeMessage();

    // Functions to create and modify regions on the display
    // Should return the enum error/ack code from the SID
    ERROR makeRegion(uint8_t regionID, uint8_t subRegionID0, uint8_t subRegionID1, uint8_t xPos, uint8_t yPos, uint8_t width, uint8_t fontStyle, char* text = nullptr);
    ERROR changeRegion(uint8_t regionID, uint8_t subRegionID0, uint8_t subRegionID1, uint8_t visible, uint8_t style, char* text = nullptr);
    void replaceAuxPlayText(char* text);
    ERROR drawRegion(uint8_t regionID, uint8_t drawFlag = 0x01);
    ERROR clearRegion(uint8_t regionID, uint8_t clearFlag = 0x01);

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

    MODE getMode(); // Returns the current mode based on the last processed frame

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

    void processMode(const SerialFrame &frame); // Updates the current mode based on the frame

    MODE currentMode; // Stores the current mode based on the last processed frame
};

#endif // SAAB_HPD_H