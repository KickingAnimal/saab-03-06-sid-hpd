#include <SAAB_HPD.h>

// SAAB_HPD class implementation

SAAB_HPD::SAAB_HPD(HardwareSerial &serial) 
    : SIDSerial(serial), printDebug(false), bufferIndex(0), expectedLength(0), syncFound(false), frameCallback(nullptr), currentMode(MODE_UNKNOWN) {}

void SAAB_HPD::begin(uint8_t rxPin, uint8_t txPin) {
    SIDSerial.begin(115200, SERIAL_8N1, rxPin, txPin);
}

void SAAB_HPD::setDebug(bool enable) {
    printDebug = enable;
    if (printDebug) {
        Serial.println("Debug printing enabled (SAAB_HPD.cpp)");
    } else {
        Serial.println("Debug printing disabled (SAAB_HPD.cpp)");
    }
}

void SAAB_HPD::toggleDebug() {
    printDebug = !printDebug;
    if (printDebug) {
        Serial.println("Debug printing enabled: toggled (SAAB_HPD.cpp)");
    } else {
        Serial.println("Debug printing disabled: toggled (SAAB_HPD.cpp)");
    }
}

/*!
  * @brief Send SID data with checksum calculation and wait for acknowledgment or error code.
  * @param frame 
      The SerialFrame structure containing the data to be sent.
  * @return ERROR
      ERROR_OK for success, ERROR_TIMEOUT for timeout, or error code for failure.
  
  * @note The function sends the frame data and waits for acknowledgment or error response.
!*/
SAAB_HPD::ERROR SAAB_HPD::sendSidData(SerialFrame &frame) {
    // Calculate DLC if not already set
    if (frame.dlc == 0) {
        frame.dlc = 2 + strlen(reinterpret_cast<const char*>(frame.data)); // Command + padding + data length
    }

    // Calculate checksum
    frame.checksum = calculateChecksum(frame);

    // Send the frame
    SIDSerial.write(frame.dlc);
    SIDSerial.write(frame.command);
    if (frame.dlc >= 2) {
        SIDSerial.write(0x00); // Send padding byte
    }
    for (byte i = 0; i < frame.dlc - 2; i++) {
        SIDSerial.write(frame.data[i]);
    }
    SIDSerial.write(frame.checksum);

    if (printDebug) {
        Serial.println("\n--- Frame Sent ---");
        Serial.printf("TX: DLC: 0x%02X, COMMAND: 0x%02X, ", frame.dlc, frame.command);
        if (frame.dlc >= 2) Serial.print("0x00, "); // Print padding byte
        for (byte i = 0; i < frame.dlc - 2; i++) {
            Serial.printf("0x%02X, ", frame.data[i]);
        }
        Serial.printf("CHECKSUM: 0x%02X\n", frame.checksum);
        Serial.println("------------------");
    }

    // Wait for acknowledgment or error response
    SerialFrame responseFrame;
    unsigned long startTime = millis();
    while (millis() - startTime < 100) { // Timeout after 100ms
        if (readSIDserialData(responseFrame)) {
            if (responseFrame.command == 0xFF && responseFrame.dlc == 0x02) {
                return ERROR_OK; // Success
            } else if (responseFrame.command == 0xFE) {
                return static_cast<ERROR>(responseFrame.data[0]); // Return error code
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
        Serial.println("\n--- Raw Data Sent ---");
        Serial.print("TX: ");
        for (size_t i = 0; i < len; i++) {
            Serial.printf(",0x%02X", data[i]);
        }
        Serial.println("\n");
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
    SerialFrame data = {
        .dlc = 0x01, // DLC of 2 does also work with the padding byte
        .command = 0x9F, // COMMAND
    };

    sendSidData(data); // Send the test mode message to SID
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
    uint16_t calculatedChecksum = calculateChecksum(frame); // Calculate checksum
    uint8_t expectedChecksum = frame.checksum; // Extract expected checksum from the frame

    if (printDebug) {
        Serial.printf("Calculated checksum: 0x%02X\n", calculatedChecksum);
        Serial.printf("Expected checksum: 0x%02X\n", expectedChecksum);
        Serial.println(calculatedChecksum == expectedChecksum ? "Checksum match!\n" : "\n!!!!!\nChecksum mismatch!\n");
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
                    if (printDebug) Serial.println("\nSync pattern detected! (0x81)");
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
                memset(frame.data, 0, sizeof(frame.data)); // Clear residual data
                if (printDebug) {
                    Serial.printf("Expected total frame length: %02X\n", expectedLength);
                }
            } else {
                if (printDebug) Serial.println("\nInvalid DLC, resetting sync");
                syncFound = false; // Reset sync if DLC is invalid
                continue;
            }
        } else {
            buffer[bufferIndex++] = byteReceived;

            // If the full frame is received
            if (bufferIndex == expectedLength) {
                // Populate the frame struct
                frame.command = buffer[1]; // Command byte
                if (frame.dlc > 2) { // Only copy data if DLC > 2 (command + checksum)
                    memcpy(frame.data, &buffer[3], frame.dlc - 2); // Copy data bytes
                }
                frame.checksum = buffer[frame.dlc + 1]; // Checksum byte

                // Verify checksum
                if (!verifyChecksum(frame)) {
                    if (printDebug) Serial.println("\nChecksum mismatch, resetting sync");
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

SAAB_HPD::ERROR SAAB_HPD::makeRegion(uint8_t regionID, uint8_t subRegionID0, uint8_t subRegionID1, uint16_t xPos, uint8_t yPos, uint8_t width, uint8_t fontStyle, char* text) {
    SerialFrame frame;
    frame.command = 0x10; // COMMAND
    frame.dlc = 13; // Base DLC

    frame.data[0] = regionID;
    frame.data[1] = 0x00; // Unknown, always 0x00
    frame.data[2] = subRegionID0;
    frame.data[3] = subRegionID1;
    frame.data[4] = 0x01; // Unknown, sometimes 0x00
    frame.data[5] = fontStyle;
    frame.data[6] = width;
    frame.data[7] = 0x00;

    // Split xPos into LSB and MSB
    frame.data[8] = xPos & 0xFF;       // LSB of xPos
    frame.data[9] = (xPos >> 8) & 0xFF; // MSB of xPos

    frame.data[10] = yPos;

    // Copy text to frame data if provided
    int i = 0;
    if (text != nullptr) {
        while (text[i] != '\0' && i < BUFFER_SIZE - 11) {
            frame.data[11 + i] = text[i];
            i++;
        }
        frame.dlc += i; // Adjust DLC for text length
    }

    frame.checksum = calculateChecksum(frame);
    return sendSidData(frame);
}

SAAB_HPD::ERROR SAAB_HPD::changeRegion(uint8_t regionID, uint8_t subRegionID0, uint8_t subRegionID1, uint8_t visible, uint8_t style, char* text) {
    SerialFrame frame;
    frame.command = 0x11; // COMMAND
    frame.dlc = 8; // Base DLC

    frame.data[0] = regionID;
    frame.data[1] = 0x00; // Unknown, always 0x00
    frame.data[2] = subRegionID0;
    frame.data[3] = subRegionID1;
    frame.data[4] = visible; // 0x02, 0x08 show, 0x03 (0x01?) hide
    frame.data[5] = style;
    

    // Copy text to frame data if provided
    int i = 0;
    if (text != nullptr) {
        while (text[i] != '\0' && i < BUFFER_SIZE - 6) {
            frame.data[6 + i] = text[i];
            i++;
        }
        frame.dlc += i; // Adjust DLC for text length
    }

    frame.checksum = calculateChecksum(frame);
    return sendSidData(frame);
}

SAAB_HPD::ERROR SAAB_HPD::drawRegion(uint8_t regionID, uint8_t drawFlag) {
    SerialFrame frame;
    frame.command = 0x70; // COMMAND
    frame.dlc = 5; // DLC for drawRegion

    frame.data[0] = regionID;
    frame.data[1] = 0x00;
    frame.data[2] = drawFlag; // 0x01 to draw, 0x00 to hide

    frame.checksum = calculateChecksum(frame);
    return sendSidData(frame);
}

SAAB_HPD::ERROR SAAB_HPD::clearRegion(uint8_t regionID, uint8_t clearFlag) {
    SerialFrame frame;
    frame.command = 0x60; // COMMAND
    frame.dlc = 5; // DLC for clearRegion

    frame.data[0] = regionID;
    frame.data[1] = 0x00; // Unknown, always 0x00
    frame.data[2] = clearFlag; // 0x01?

    frame.checksum = calculateChecksum(frame);
    return sendSidData(frame);
}

void SAAB_HPD::recreateAuxRegion() {
    // Clear the region
    if (clearRegion(0x01, 0x00) != ERROR_OK) {
        Serial.println("Error: Failed to clear region 0x01.");
        return;
    }

    // Helper lambda to handle errors
    auto handleError = [](ERROR result, const char* description) {
        if (result != ERROR_OK) {
            Serial.printf("Error: %s failed with code 0x%02X.\n", description, result);
            return false;
        }
        return true;
    };

    // Recreate regions
    if (!handleError(makeRegion(0x01, 0x00, 0x3C, 187, 34, 8, HPD_FONT_LARGE), "makeRegion 0x3C") ||
        !handleError(makeRegion(0x01, 0x00, 0x3D, 252, 31, 20, HPD_FONT_LARGE), "makeRegion 0x3D") ||
        !handleError(makeRegion(0x01, 0x00, 0x3E, 207, 31, 44, HPD_FONT_LARGE), "makeRegion 0x3E") ||
        !handleError(makeRegion(0x01, 0x02, 0xBF, 230, 54, 8, HPD_FONT_SMALL, (char*)"1"), "makeRegion 0xBF") ||
        !handleError(makeRegion(0x01, 0x02, 0xC0, 238, 54, 8, HPD_FONT_SMALL, (char*)"2"), "makeRegion 0xC0") ||
        !handleError(makeRegion(0x01, 0x02, 0xC1, 246, 54, 8, HPD_FONT_SMALL, (char*)"3"), "makeRegion 0xC1") ||
        !handleError(makeRegion(0x01, 0x02, 0xC2, 254, 54, 8, HPD_FONT_SMALL, (char*)"4"), "makeRegion 0xC2") ||
        !handleError(makeRegion(0x01, 0x02, 0xC3, 262, 54, 8, HPD_FONT_SMALL, (char*)"5"), "makeRegion 0xC3") ||
        !handleError(makeRegion(0x01, 0x02, 0xC4, 270, 54, 8, HPD_FONT_SMALL, (char*)"6"), "makeRegion 0xC4") ||
        !handleError(makeRegion(0x01, 0x02, 0xCD, 142, 34, 30, HPD_FONT_LARGE, (char*)"BT"), "makeRegion 0xCD") ||
        !handleError(makeRegion(0x01, 0x02, 0xCF, 142, 34, 30, HPD_FONT_LARGE, (char*)"CD"), "makeRegion 0xCF") ||
        !handleError(makeRegion(0x01, 0x02, 0xD0, 142, 34, 30, HPD_FONT_LARGE, (char*)"CDC"), "makeRegion 0xD0") ||
        !handleError(makeRegion(0x01, 0x02, 0xD2, 142, 34, 30, HPD_FONT_LARGE, (char*)"CDX"), "makeRegion 0xD2") ||
        !handleError(makeRegion(0x01, 0x02, 0xD5, 142, 34, 40, HPD_FONT_LARGE, (char*)"SCAN"), "makeRegion 0xD5") ||
        !handleError(makeRegion(0x01, 0x02, 0xD7, 187, 34, 230, HPD_FONT_LARGE, (char*)"Checking magazine"), "makeRegion 0xD7") ||
        !handleError(makeRegion(0x01, 0x02, 0xD9, 187, 34, 230, HPD_FONT_LARGE, (char*)"No magazine"), "makeRegion 0xD9") ||
        !handleError(makeRegion(0x01, 0x02, 0xDB, 187, 34, 230, HPD_FONT_LARGE, (char*)"Press 1-6 to select CD"), "makeRegion 0xDB") ||
        !handleError(makeRegion(0x01, 0x02, 0xDD, 207, 31, 61, HPD_FONT_MEDIUM, (char*)"No CD"), "makeRegion 0xDD") ||
        !handleError(makeRegion(0x01, 0x02, 0xDF, 187, 31, 230, HPD_FONT_MEDIUM, (char*)"Play"), "makeRegion 0xDF") ||
        !handleError(makeRegion(0x01, 0x02, 0xE6, 142, 54, 13, HPD_FONT_SMALL, (char*)"NO"), "makeRegion 0xE6") ||
        !handleError(makeRegion(0x01, 0x02, 0xEA, 170, 54, 19, HPD_FONT_SMALL, (char*)"PTY"), "makeRegion 0xEA") ||
        !handleError(makeRegion(0x01, 0x02, 0xEB, 192, 54, 19, HPD_FONT_SMALL, (char*)"RDM"), "makeRegion 0xEB") ||
        !handleError(makeRegion(0x01, 0x02, 0xED, 154, 54, 13, HPD_FONT_SMALL, (char*)"TP"), "makeRegion 0xED")) {
        Serial.println("Error: Failed to recreate AUX region.");
        return;
    }

    // Draw the region
    if (drawRegion(0x01, 0x01) != ERROR_OK) {
        Serial.println("Error: Failed to draw region 0x01.");
    }
}

void SAAB_HPD::replaceAuxPlayText(char* text) {
    // Change the "Play" region to the specified text
    changeRegion(0x01, 0x02, 0xDF, HPD_VISIBLE, HPD_STYLE_NORMAL, text);

    // Change BT region to visible
    changeRegion(0x01,0x02,0xCD, HPD_VISIBLE, HPD_STYLE_NORMAL);
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
        if (frame.data[0] == 0x01 && frame.data[2] == 0x02 && frame.data[3] == 0xCF) {
            currentMode = MODE_CD;
        } else if (frame.data[0] == 0x01 && frame.data[2] == 0x02 && frame.data[3] == 0xCD) {
            currentMode = MODE_AUX;
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
        }
    }
}

SAAB_HPD::MODE SAAB_HPD::getMode() {
    return currentMode; // Return the last known mode
}

void SAAB_HPD::setFrameCallback(FrameCallback callback) {
    frameCallback = callback;
}