#include <SAAB_HPD.h>

// SAAB_HPD class implementation

SAAB_HPD::SAAB_HPD(HardwareSerial &serial) 
    : SIDSerial(serial), printDebug(false), bufferIndex(0), expectedLength(0), syncFound(false), frameCallback(nullptr), currentMode(MODE_UNKNOWN) {}

void SAAB_HPD::begin(uint32_t baudRate, uint8_t rxPin, uint8_t txPin) {
    SIDSerial.begin(baudRate, SERIAL_8N1, rxPin, txPin);
}

void SAAB_HPD::setDebug(bool enable) {
    printDebug = enable;
}

void SAAB_HPD::toggleDebug() {
    printDebug = !printDebug;
    if (printDebug) {
        Serial.println("Debug printing enabled (SAAB_HPD.cpp)");
    } else {
        Serial.println("Debug printing disabled (SAAB_HPD.cpp)");
    }
}

/*!
  * @brief Send SID data with checksum calculation and wait for acknowledgment or error code.
  * @param lenA 
      Length of the data to be sent.
  * @param data 
      Pointer to the data to be sent. (without checksum or DLC)
  * @return ERROR
      ERROR_OK for success, ERROR_TIMEOUT for timeout, or error code for failure.
  
  * @note The function calculates the checksum based on the data and sends it along with the data.
  * @note The checksum is calculated by summing all bytes in the data and taking the LSB of the sum.
  * @note The first byte sent is the length of the data (DLC), followed by the data bytes and finally the checksum byte.
  
!*/
SAAB_HPD::ERROR SAAB_HPD::sendSidData(byte dlc, byte* data) {
    uint16_t sum = 0;

    SIDSerial.write(dlc);
    if (printDebug) Serial.printf("TX: 0x%02X,", dlc);
    sum += dlc;

    for (byte i = 0; i < dlc; i++) {
        SIDSerial.write(data[i]);
        if (printDebug) Serial.printf("0x%02X,", data[i]);
        sum += data[i];
    }

    byte checksum = sum & 0xFF;
    SIDSerial.write(checksum);
    if (printDebug) Serial.printf("0x%02X\n", checksum);

    // Wait for acknowledgment or error response
    SerialFrame responseFrame;
    unsigned long startTime = millis();
    while (millis() - startTime < 100) { // Timeout after 100ms
        if (readSIDserialData(responseFrame)) {
            if (responseFrame.command == 0xFF && responseFrame.data[0] == 0x00) {
                return ERROR_OK; // Success
            } else if (responseFrame.command == 0xFE) {
                return static_cast<ERROR>(responseFrame.data[2]); // Return error code
            }
        }
    }

    return ERROR_TIMEOUT; // Timeout or no valid response
}

/*!
  * @brief Send raw SID data without checksum calculation.
  * @param data 
    Pointer to the data to be sent.
  * @note The data is structured as a byte array.
  * @return void
  * 
  * @note The function sends the bytes without any processing or checksum calculation.
!*/
void SAAB_HPD::sendSidRawData(size_t len, byte* data) {
    if (printDebug) {
        Serial.print("TX: ");
        for (size_t i = 0; i < len; i++) {
            Serial.printf(",0x%02X", data[i]);
        }
        Serial.println();
    }
    SIDSerial.write(data, len);
}

/*!
  * @brief Send a test mode message to SID. it will enter selftest mode.
  * @return void
  *   
  * @note The function sends a predefined test mode message to the SID.
  * @note Use this function with caution, as SID will not respond to any other commands until it exits test mode.
!*/
void SAAB_HPD::sendTestModeMessage() {
    byte data[1] = {0x9F};
    sendSidData(1, data); // Send the test mode message to SID
}

/*!
  * @brief Verify the checksum of the received frame.
  * @param struct SerialFrame &frame 
      The frame to be verified.
      
  * @return true if the checksum is valid, false otherwise.
  
  * @note The function calculates the checksum based on the received frame and compares it with the expected checksum.
  * @note The expected checksum is located at the position DLC + 1 in the frame.
!*/
bool SAAB_HPD::verifyChecksum(const SerialFrame &frame) {
    // Check if the frame is valid
    uint16_t calculatedSum = 0;
    uint8_t calculatedChecksum = calculateChecksum(frame); // Calculate checksum
    uint8_t expectedChecksum = frame.checksum; // Extract expected checksum from the frame

    if (printDebug) {
        Serial.printf("Calculated sum: 0x%04X\n", calculatedSum);
        Serial.printf("Calculated checksum: 0x%02X\n", calculatedChecksum);
        Serial.printf("Expected checksum: 0x%02X\n", expectedChecksum);
        Serial.println(calculatedChecksum == expectedChecksum ? "Checksum match!" : "Checksum mismatch!");
    }

    return calculatedChecksum == expectedChecksum; // Return if calulated checksum matches expected checksum 
}

uint8_t SAAB_HPD::calculateChecksum(const SerialFrame &frame) {
    uint16_t sum = frame.dlc; // Start with the DLC byte
    sum += frame.command; // Add the command byte
    // Add all data bytes
    for (int i = 0; i < frame.dlc - 2; i++) {
        sum += frame.data[i];
    }

    return sum & 0xFF; // Return the LSB of the sum
}

/*!
  * @brief Validate the DLC value.
  * @param dlc 
      The DLC value to be validated.
  * @return true if the DLC is valid, false otherwise.
  
  * @note The function checks if the DLC is between (0x01 to 0xFF).
  * @note any other value is considered invalid.
!*/
bool SAAB_HPD::isValidDLC(uint8_t dlc) {
    return (dlc > 0x00 && dlc < 0xFF);
}

/*!
  * @brief Read SID serial data and process it.
  * @param frame 
      Pointer to the buffer where the received frame will be stored.
  * @return true if a valid frame is received, false otherwise.
  
  * @note The function looks for the OK message pattern to sync with the incoming data.
  * @note It reads the incoming frame and returns true if a valid frame is received.
  * @note The frame is copied to the provided buffer and the buffer index is reset for the next frame.
!*/
bool SAAB_HPD::readSIDserialData(SerialFrame &frame) {
    static uint8_t syncIndex = 0;

    while (SIDSerial.available()) {
        uint8_t byteReceived = SIDSerial.read();

        if (!syncFound) {
            if (byteReceived == syncPattern[syncIndex]) {
                syncIndex++;
                if (syncIndex == syncPatternLength) {
                    if (printDebug) Serial.println("Sync pattern detected! (0x81)");
                    syncFound = true;
                    bufferIndex = 0; // Reset buffer for new frame
                    syncIndex = 0;
                }
            } else {
                syncIndex = 0; // Reset sync search if mismatch
            }
            continue;
        }

        // If sync is found, process the incoming frame
        if (bufferIndex == 0) {
            // First byte is DLC (excluding itself and checksum)
            if (isValidDLC(byteReceived)) {
                frame.dlc = byteReceived; // Store DLC in the frame struct
        
                buffer[bufferIndex++] = byteReceived;
                expectedLength = frame.dlc + 2; // DLC + 2 (itself + checksum)
                if (printDebug) {
                    Serial.printf("DLC: 0x%02X\n", frame.dlc);
                    Serial.printf("Expected total length: %02X\n", expectedLength);
                }
            } else {
                Serial.println("Invalid DLC, resetting sync");
                syncFound = false; // Reset sync if DLC is invalid
                continue;
            }
        } else {
            buffer[bufferIndex++] = byteReceived;

            // If the full frame is received
            if (bufferIndex == expectedLength) {
                // populate the frame struct
                frame.command = buffer[1]; // Command byte

                memcpy(frame.data, &buffer[3], frame.dlc - 2); // Copy data bytes
                frame.checksum = buffer[frame.dlc + 1]; // Checksum byte
        
                if (!verifyChecksum(frame)) {
                    Serial.println("Checksum mismatch, resetting sync");
                    bufferIndex = 0; // Reset buffer for next frame
                    syncFound = false; // Reset sync if checksum is invalid
                
                    continue;
                }         

                bufferIndex = 0; // Reset buffer for next frame
                return true; // Valid frame received
            }
        }
    }

    return false; // No valid frame received
}

SAAB_HPD::ERROR SAAB_HPD::makeRegion(uint8_t regionID, uint8_t subRegionID0, uint8_t subRegionID1, uint8_t xPos, uint8_t yPos, uint8_t width, uint8_t fontStyle, char* text) {
    uint8_t data[100];

    // command statics
    data[0] = 0x10; // COMMAND
    data[1] = 0x00; // spacing
    data[3] = 0x00; // spacing
    data[9] = 0x00; // spacing
    data[11] = 0x00; // spacing

    // command data
    data[2] = regionID;
    data[4] = subRegionID0;
    data[5] = subRegionID1;
    data[6] = 0x00; // idk, 0x01 sometimes
    data[7] = fontStyle;
    data[8] = width;
    data[10] = xPos;
    data[12] = yPos;

    // Copy text to data. data is max 0xFE long
    int i = 0;
    if (text != nullptr) {
        while (text[i] != '\0' && i < 0xFE) {
            data[13 + i] = text[i];
            i++;
        }
    }

    // Send the data to SID
    return sendSidData(13 + i, data);
}

SAAB_HPD::ERROR SAAB_HPD::changeRegion(uint8_t regionID, uint8_t subRegionID0, uint8_t subRegionID1, uint8_t visible, uint8_t style, char* text) {
    uint8_t data[100];

    // command statics
    data[0] = 0x11; // COMMAND
    data[1] = 0x00; // spacing
    data[3] = 0x00; // spacing
    data[9] = 0x00; // spacing
    data[11] = 0x00; // spacing

    // command data
    data[2] = regionID;
    data[4] = subRegionID0;
    data[5] = subRegionID1;
    data[6] = visible; // 0x02, 0x08 show, 0x03 (0x01?) hide
    data[7] = style;

    // Copy text to data if provided
    int i = 0;
    if (text != nullptr) {
        while (text[i] != '\0' && i < 0xFE) {
            data[8 + i] = text[i];
            i++;
        }
    }

    // Send the data to SID
    return sendSidData(8 + i, data);
}

SAAB_HPD::ERROR SAAB_HPD::drawRegion(uint8_t regionID, uint8_t drawFlag) {
    uint8_t data[4];

    // command statics
    data[0] = 0x70; // COMMAND
    data[1] = 0x00; // spacing
    data[3] = 0x00; // spacing

    // command data
    data[2] = regionID;
    data[4] = drawFlag; // 0x01?

    // Send the data to SID
    return sendSidData(4, data);
}

SAAB_HPD::ERROR SAAB_HPD::clearRegion(uint8_t regionID, uint8_t clearFlag) {
    uint8_t data[4];

    // command statics
    data[0] = 0x60; // COMMAND
    data[1] = 0x00; // spacing
    data[3] = 0x00; // spacing

    // command data
    data[2] = regionID;
    data[4] = clearFlag; // 0x01?

    // Send the data to SID
    return sendSidData(4, data);
}

void SAAB_HPD::replaceAuxPlayText(char* text) {
    uint8_t regionID = 0x01;
    uint8_t subRegionID0 = 0x02;
    uint8_t subRegionID0_text = 0x08;
    uint8_t subRegionID1 = 0xDF;
    const int maxRetries = 5; // Maximum number of retries for each operation

    // Retry until the 'Play' text is hidden or retries are exhausted
    int retries = 0;
    while (changeRegion(regionID, subRegionID0, subRegionID1, HPD_HIDDEN, HPD_STYLE_NORMAL) != ERROR_OK) {
        if (++retries >= maxRetries) return;
    }

    // Retry until 'AUX' is changed to 'BT' or retries are exhausted
    retries = 0;
    const char* aux = "BT ";
    while (changeRegion(regionID, subRegionID0, 0xCD, HPD_VISIBLE, HPD_STYLE_NORMAL, (char*)aux) != ERROR_OK) {
        if (++retries >= maxRetries) return;
    }

    // Retry until the new region is created (makeRegion can fail if the region already exists) or retries are exhausted
    retries = 0;
    while (makeRegion(regionID, subRegionID0_text, subRegionID1, 207, 31, 0xE6, HPD_FONT_MEDIUM) != ERROR_OK) {
        if (++retries >= maxRetries) break; // Proceed even if the region already exists
        if(makeRegion(regionID, subRegionID0_text, subRegionID1, 207, 31, 0xE6, HPD_FONT_MEDIUM) == ERROR_REGION_EXISTS) {
            break; // Region already exists, exit the loop
        }
    }

    // Retry until the new region is visible and the text is set or retries are exhausted
    retries = 0;
    while (changeRegion(regionID, subRegionID0_text, subRegionID1, HPD_VISIBLE, HPD_STYLE_NORMAL, text) != ERROR_OK) {
        if (++retries >= maxRetries) return;
    }
}

void SAAB_HPD::poll() {
    SerialFrame frame;
    if (readSIDserialData(frame)) {
        // Process the frame to update the current mode
        processMode(frame);

        // Invoke the callback with the processed frame, if set
        if (frameCallback) {
            frameCallback(frame);
        }
    }
}

void SAAB_HPD::processMode(const SerialFrame &frame) {
    if (frame.command == 0x11) { // Check if the frame is a display update
        if (frame.data[0] == 0x01 && frame.data[2] == 0x02 && frame.data[3] == 0xCD) {
            currentMode = MODE_AUX;
        } else if (frame.data[0] == 0x01 && frame.data[2] == 0x02 && frame.data[3] == 0xCF) {
            currentMode = MODE_CD;
        } else if (frame.data[0] == 0x01 && frame.data[2] == 0x02 && frame.data[3] == 0xD2) {
            currentMode = MODE_CDX;
        } else if (frame.data[0] == 0x01 && frame.data[2] == 0x02 && frame.data[3] == 0xD0) {
            currentMode = MODE_CDC;
        } else if (frame.data[0] == 0x00 && frame.data[2] == 0x00 && frame.data[3] == 0x13) {
            // Check for FM1, FM2, or AM
            if (frame.data[6] == 0x46 && frame.data[7] == 0x4D) { // FM
                if (frame.data[8] == 0x31) {
                    currentMode = MODE_FM1;
                } else if (frame.data[8] == 0x32) {
                    currentMode = MODE_FM2;
                }
            } else if (frame.data[6] == 0x41 && frame.data[7] == 0x4D) { // AM
                currentMode = MODE_AM;
            }
        } else {
            currentMode = MODE_UNKNOWN;
        }
    }
}

SAAB_HPD::MODE SAAB_HPD::getMode() {
    return currentMode; // Return the last known mode
}