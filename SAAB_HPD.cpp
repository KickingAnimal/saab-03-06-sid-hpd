#include <SAAB_HPD.h>

// SAAB_HPD class implementation

SAAB_HPD::SAAB_HPD(HardwareSerial &serial) 
    : SIDSerial(serial), printDebug(false), bufferIndex(0), expectedLength(0), syncFound(false) {}

void SAAB_HPD::begin(uint32_t baudRate, uint8_t rxPin, uint8_t txPin) {
    SIDSerial.begin(baudRate, SERIAL_8N1, rxPin, txPin);
}

void SAAB_HPD::setDebug(bool enable) {
    printDebug = enable;
}

void SAAB_HPD::toggleDebug() {
    printDebug = !printDebug;
    if (printDebug) {
        Serial.println("Debug printing enabled");
    } else {
        Serial.println("Debug printing disabled");
    }
}

/*!
  * @brief Send SID data with checksum calculation.
  * @param lenA 
      Length of the data to be sent.
  * @param data 
      Pointer to the data to be sent. (without checksum or DLC)
  * @return void
  
  * @note The function calculates the checksum based on the data and sends it along with the data.
  * @note The checksum is calculated by summing all bytes in the data and taking the LSB of the sum.
  * @note The first byte sent is the length of the data (DLC), followed by the data bytes and finally the checksum byte.
  
!*/
void SAAB_HPD::sendSidData(byte lenA, byte* data) {
    uint16_t sum = 0;
    byte dlc = lenA;

    SIDSerial.write(dlc);
    if (printDebug) Serial.printf("TX: 0x%02X,", dlc);
    sum += dlc;

    for (byte i = 0; i < lenA; i++) {
        SIDSerial.write(data[i]);
        if (printDebug) Serial.printf("0x%02X,", data[i]);
        sum += data[i];
    }

    byte checksum = sum & 0xFF;
    SIDSerial.write(checksum);
    if (printDebug) Serial.printf("0x%02X\n", checksum);
}

/*!
  * @brief Send raw SID data without checksum calculation.
  * @param data 
      Pointer to the data to be sent.
  * @note The data is structured as a byte array.
  * @return void
  
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
  
  * @note The function sends a predefined test mode message to the SID.
  * @note Use this function with caution, as SID will not respond to any other commands until it exits test mode.
!*/
void SAAB_HPD::sendTestModeMessage() {
    byte data[1] = {0x9F};
    sendSidData(1,data); // Send the test mode message to SID
}

/*!
  * @brief Verify the checksum of the received frame.
  * @param frame 
      Pointer to the received frame data.
  * @return true if the checksum is valid, false otherwise.
  
  * @note The function calculates the checksum based on the received frame and compares it with the expected checksum.
  * @note The expected checksum is located at the position DLC + 1 in the frame.
!*/
bool SAAB_HPD::verifyChecksum(uint8_t *frame) {
  uint8_t dlc = frame[0]; // DLC (excluding itself and checksum)
  uint16_t calculatedSum = 0;

  // Sum all bytes except the checksum
  for (uint8_t i = 0; i < dlc + 1; i++) {
    calculatedSum += frame[i];
  }

  uint8_t expectedChecksum = frame[dlc + 1]; // Checksum is at DLC + 1 position
  uint8_t calculatedChecksum = calculatedSum & 0xFF; // Keep only LSB

  if (printDebug) {
    Serial.printf("DLC: %02X\n", dlc);
    Serial.printf("Calculated sum: 0x%04X\n", calculatedSum);
    Serial.printf("Calculated checksum: 0x%02X\n", calculatedChecksum);
    Serial.printf("Expected checksum: 0x%02X\n", expectedChecksum);
    Serial.println(calculatedChecksum == expectedChecksum ? "Checksum match!" : "Checksum mismatch!");
  }
  return calculatedChecksum == expectedChecksum;
}

/*!
  * @brief Validate the DLC value.
  * @param dlc 
      The DLC value to be validated.
  * @return true if the DLC is valid, false otherwise.
  
  * @note The function checks if the DLC is between (0x01 to 0xFE).
  * @note any other value is considered invalid.
!*/
bool SAAB_HPD::isValidDLC(uint8_t dlc) {
    return (dlc > 0x00 && dlc < 0xFE);
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
bool SAAB_HPD::readSIDserialData(uint8_t* frame) {
  static uint8_t syncIndex = 0;

  while (SIDSerial.available()) {
    uint8_t byteReceived = SIDSerial.read();

    if (!syncFound) {
      if (byteReceived == okPattern[syncIndex]) {
        syncIndex++;
        if (syncIndex == okPatternLength) {
          if (printDebug) Serial.println("OK msg pattern detected!");
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
        buffer[bufferIndex++] = byteReceived;
        expectedLength = byteReceived + 2; // DLC + 2 (itself + checksum)
        if (printDebug) {
          Serial.printf("DLC: 0x%02X\n", byteReceived);
          Serial.printf("Expected length: %02X\n", expectedLength);
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
        memcpy(frame, buffer, expectedLength);
        bufferIndex = 0; // Reset buffer for next frame
        return true; // Valid frame received
      }
    }
  }
  return false; // No valid frame received
}

void SAAB_HPD::makeRegion(uint8_t regionID, uint8_t subRegionID0, uint8_t subRegionID1, uint8_t xPos, uint8_t yPos, uint8_t width, uint8_t fontStyle, char* text) {
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
  sendSidData(13 + i, data);
}

void SAAB_HPD::changeRegion(uint8_t regionID, uint8_t subRegionID0, uint8_t subRegionID1, uint8_t visible, uint8_t style, char* text) {
  // Change the visibility and/or text of the specified region
  // Format: changeRegion <regionID> <subRegionID0> <subRegionID1> <visible> <style> [<text>]
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
  // 0x00 normal, 0x10 right align, 0x20 blinking, 
  // 0x40 inverted, 0x80 underline (and more -> 0xff everything)

  // Copy text to data if provided
  int i = 0;
  if (text != nullptr) {
    while (text[i] != '\0' && i < 0xFE) {
      data[8 + i] = text[i];
      i++;
    }
  }

  // Send the data to SID
  sendSidData(8 + i, data);
}

void SAAB_HPD::drawRegion(uint8_t regionID, uint8_t drawFlag) {
  // Draw the specified region -- when tested it deleted the region.?
  // Format: drawRegion <regionID> <drawFlag>
  uint8_t data[4];

  // command statics
  data[0] = 0x70; // COMMAND
  data[1] = 0x00; // spacing
  data[3] = 0x00; // spacing

  // command data
  data[2] = regionID;
  data[4] = drawFlag; //0x01?

  // Send the data to SID
  sendSidData(4, data);
}
// these draw and clear functions are not well understood.
void SAAB_HPD::clearRegion(uint8_t regionID, uint8_t clearFlag){
  // Clear the specified region -- did about the same as 0x70?
  // Format: clearRegion <regionID> <clearFlag>
  uint8_t data[4];

  // command statics
  data[0] = 0x60; // COMMAND
  data[1] = 0x00; // spacing
  data[3] = 0x00; // spacing

  // command data
  data[2] = regionID;
  data[4] = clearFlag; // 0x01?

  // Send the data to SID
  sendSidData(4, data);
}

void SAAB_HPD::replaceAuxPlayText(char* text) {
  // Replace the text in the aux play region (hide the Play and show own region)
  // Format: replaceAuxPlayText <text>
  // Example: replaceAuxPlayText "Song name - Artist" -> BT Song name - Artist (instead of AUX Play)
  // This function needs to be reworked, to change the Play region to have greater width. and detect region states.
  uint8_t regionID = 0x01;
  uint8_t subRegionID0 = 0x02;
  uint8_t subRegionID0_text = 0x08;
  uint8_t subRegionID1 = 0xDF;

  //Hide the 'Play' text
  changeRegion(regionID, subRegionID0, subRegionID1, HPD_HIDDEN, HPD_STYLE_NORMAL);
  delay(10);

  // Change AUX to BT

  const char* aux = "BT ";
  changeRegion(regionID, subRegionID0, 0xCD, HPD_VISIBLE, HPD_STYLE_NORMAL, (char*)aux);
  delay(10);

  // Setup the new region

  makeRegion(regionID, subRegionID0_text, subRegionID1, 207, 31, 0xE6, HPD_FONT_MEDIUM);
  delay(10);

  // Change the new region to visible and set the text
  changeRegion(regionID, subRegionID0_text, subRegionID1, HPD_VISIBLE, HPD_STYLE_NORMAL, text);
}