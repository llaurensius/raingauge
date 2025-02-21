#include <EEPROM.h>
#include <RHRouter.h>
#include <RHMesh.h>
#include <RH_RF95.h>
#include <SoftwareSerial.h>
#include <Arduino.h>
#include "FS.h"
#include <SD.h>
#include <Wire.h>
#include <DS3231-RTC.h>
#include <FastLED.h>

#define LED 13
#define N_NODES 5 // Total number of nodes: N1, N2, N3, N4, N5
#define EEPROM_ADDRESS 0 // EEPROM address to store node ID
#define GOSSIP_PROBABILITY 0.7 // 70% chance to forward the message

/// Pin definitions for ESP32-MisRed
#define RFM95_CS 15    // Chip Select
#define RFM95_RST 26   // Reset
#define RFM95_INT 27   // DIO0
//*/
/*// Pin definitions for ESP32-WROOM32
#define RFM95_CS 15    // Chip Select
#define RFM95_RST 26   // Reset
#define RFM95_INT 27   // DIO0
//*/
/*// Pin definitions for ESP32-TTGO
#define RFM95_CS 15    // Chip Select
#define RFM95_RST 26   // Reset
#define RFM95_INT 27   // DIO0
//*/

#define enTxPin 4   // HIGH: TX and LOW: RX
#define LED_PIN     2
#define NUM_LEDS    2

const int RX_PIN = 14;  // Pin RX ESP32
const int TX_PIN = 0;  // Pin TX ESP32

RTClib myRTC; // RTC library instance
const int CS = 5; // Chip select pin for SD card

SoftwareSerial mySerial(16, 17); // RX, TX
SoftwareSerial mySerial1(35, 34); // RX, TX
SoftwareSerial mySerial2(RX_PIN, TX_PIN); // RX, TX
String dataMeasurement = "";

CRGB leds[NUM_LEDS];
int hue = 0; // Variabel untuk menyimpan nilai Hue

// Set Parameter LoRa
uint8_t setTxPower = 13;
uint8_t setSpreadingFactor = 6;

// Prototipe deklarasi fungsi
uint16_t calculateCRC(uint8_t *data, uint8_t len);

uint8_t nodeId; 
char buf[RH_MESH_MAX_MESSAGE_LEN]; // Buffer for messages
RH_RF95 rf95(RFM95_CS, RFM95_INT); // RF95 driver with specified pins
RHMesh *manager; // Mesh manager

void blinkLED0(CRGB color, int times, int delayTime) {
  for (int i = 0; i < times; i++) {
    leds[0] = color;
    FastLED.show();
    delay(delayTime);
    leds[0] = CRGB::Black;  // Matikan LED
    FastLED.show();
    delay(delayTime);
  }
}

unsigned char data[4] = {};
String outgoing;              // Outgoing message
byte msgCount = 0;            // Count of outgoing messages
byte MasterNode = 0xFF;
byte Node1 = 0xBB;

int Rain_val;
String Mymessage = "";
float distance;

// Timing
unsigned long previousMillis = 0; // Menyimpan waktu terakhir  
const long interval = 5000; // Interval waktu dalam milidetik  

void initSDCard() {
  while (true) { // Loop tak terbatas untuk mencoba inisialisasi
    Serial.println("t0.txt=\"Initializing SD card...\"");

    if (SD.begin(CS)) {
      Serial.println("t0.txt=\"SD card initialization successful!\""); // Jika berhasil
      break; // Keluar dari loop jika inisialisasi berhasil
    } else {
      Serial.print("t0.txt=\"Initialization failed! Retrying in 5 seconds...\"");

      // Jika SD card gagal, LED0 berkedip merah 1 kali
      blinkLED0(CRGB::Red, 1, 500);

      // Tunggu 5 detik sebelum mencoba lagi
      delay(5000);
    }
  }
}

void bacaRain() {
  Serial.println("Baca Rain jalan");
  uint8_t id = 1;

  uint8_t buffer[] = { id, 0x03, 0x01, 0x05, 0x00, 0x01, 0x00, 0x00 };
  uint16_t crc = calculateCRC(buffer, sizeof(buffer) - 2);
  buffer[sizeof(buffer) - 2] = crc & 0xFF;  // LSB
  buffer[sizeof(buffer) - 1] = crc >> 8;    // MSB

  mySerial.write(buffer, sizeof(buffer));

  digitalWrite(enTxPin, LOW);  // Set ke RX mode untuk menerima balasan

  unsigned long timeLimit = millis();
  while (millis() - timeLimit < 500) {
    if (mySerial.available()) {
      byte responseHeader[8];
      mySerial.readBytes(responseHeader, 8);

      Serial.println("Balasan dari RS485:");
      for (int i = 0; i < 8; i++) {
        Serial.print("Byte ");
        Serial.print(i);
        Serial.print(": 0x");
        Serial.println(responseHeader[i], HEX);
      }

      if (responseHeader[0] == id && responseHeader[1] == 0x03 && responseHeader[2] == 0x02) {
        uint16_t rawValue = (responseHeader[3] << 8) | responseHeader[4];
        Rain_val = rawValue;
        Serial.print("Nilai Rain (int): ");
        Serial.println(Rain_val);
      }
    }
  }

  digitalWrite(enTxPin, HIGH);  // Set ke TX mode

  Serial.println("Baca Rain Selesai");
}

uint16_t calculateCRC(uint8_t *data, uint8_t len) {
  uint16_t crc = 0xFFFF;
  for (uint8_t i = 0; i < len; i++) {
    crc ^= data[i];
    for (uint8_t j = 0; j < 8; j++) {
      if (crc & 0x0001) {
        crc >>= 1;
        crc ^= 0xA001;
      } else {
        crc >>= 1;
      }
    }
  }
  return crc;
}

void bacaDistance() {
  Serial.println("Baca Distance jalan");

  unsigned long startTime = millis(); // Catat waktu saat mulai membaca data

  while (mySerial1.available() < 4) {
    if (millis() - startTime >= 2000) {
      Serial.println("ERROR: Timeout, no data received");
      return;
    }
  }

  for (int i = 0; i < 4; i++) {
    data[i] = mySerial1.read();
  }

  if (data[0] == 0xff) {
    int sum = (data[0] + data[1] + data[2]) & 0x00FF;  // Hitung checksum
    if (sum == data[3]) {
      distance = (data[1] << 8) + data[2];
      if (distance > 280) {
        Serial.print("distance = ");
        Serial.print(distance / 10);
        Serial.println(" cm");
      } else {
        Serial.println("Below the lower limit");
      }
    } else {
      Serial.println("ERROR: Checksum mismatch");
    }
  } else {
    Serial.println("ERROR: Invalid start byte");
  }

  while (mySerial1.available()) {
    mySerial1.read();
  }

  Serial.println("Baca Distance selesai");
}

void Datalog() {
  DateTime now = myRTC.now();

  String getMonthStr = now.getMonth() < 10 ? "0" + String(now.getMonth()) : String(now.getMonth());
  String getDayStr = now.getDay() < 10 ? "0" + String(now.getDay()) : String(now.getDay());
  String namaFile = getDayStr + getMonthStr + String(now.getYear(), DEC) + "_TxPower " + String(setTxPower) +"_SpreadingFactor " + String(setSpreadingFactor);

  File myFile10 = SD.open("/datalog/" + namaFile + ".txt", FILE_APPEND);
  if (myFile10) {
    myFile10.print(now.getYear(), DEC);
    myFile10.print("-");
    myFile10.print(now.getMonth(), DEC);
    myFile10.print("-");
    myFile10.print(now.getDay(), DEC);
    myFile10.print(" ");
    myFile10.print(now.getHour(), DEC);
    myFile10.print(":");
    myFile10.print(now.getMinute(), DEC);
    myFile10.print(":");
    myFile10.print(now.getSecond(), DEC);
    myFile10.print(", ");
    myFile10.print("Success send: Rain: ");
    myFile10.print(Rain_val);
    myFile10.print(" mm, ");
    myFile10.print("Distance: ");
    myFile10.print(distance / 10);
    myFile10.print(" cm");
    myFile10.println("");
    myFile10.close();

  } else {
    Serial.println("error");
  }
}

void DatalogFailed() {
  DateTime now = myRTC.now();

  String getMonthStr = now.getMonth() < 10 ? "0" + String(now.getMonth()) : String(now.getMonth());
  String getDayStr = now.getDay() < 10 ? "0" + String(now.getDay()) : String(now.getDay());
  String namaFile = getDayStr + getMonthStr + String(now.getYear(), DEC) + "_TxPower " + String(setTxPower) +"_SpreadingFactor " + String(setSpreadingFactor);

  File myFile10 = SD.open("/datalog/" + namaFile + ".txt", FILE_APPEND);
  if (myFile10) {
    myFile10.print(now.getYear(), DEC);
    myFile10.print("-");
    myFile10.print(now.getMonth(), DEC);
    myFile10.print("-");
    myFile10.print(now.getDay(), DEC);
    myFile10.print(" ");
    myFile10.print(now.getHour(), DEC);
    myFile10.print(":");
    myFile10.print(now.getMinute(), DEC);
    myFile10.print(":");
    myFile10.print(now.getSecond(), DEC);
    myFile10.print(", ");
    myFile10.print("Failed send: Rain: ");
    myFile10.print(Rain_val);
    myFile10.print(" mm, ");
    myFile10.print("Distance: ");
    myFile10.print(distance / 10);
    myFile10.print(" cm");
    myFile10.println("");
    myFile10.close();

  } else {
    Serial.println("error");
  }
}

void DatalogMeasurement() {
  if (mySerial2.available() > 0) {
    // Membaca data yang diterima
    dataMeasurement = mySerial2.readStringUntil('\n');
    // Serial.println("Data diterima: " + dataMeasurement); // Debugging
  }  else {
    // Serial.println("Tidak ada data yang tersedia di mySerial2."); // Debugging
  }

  DateTime now = myRTC.now();

  String getMonthStr = now.getMonth() < 10 ? "0" + String(now.getMonth()) : String(now.getMonth());
  String getDayStr = now.getDay() < 10 ? "0" + String(now.getDay()) : String(now.getDay());
  String namaFile = getDayStr + getMonthStr + String(now.getYear(), DEC) + "_measurementlog_TxPower " + String(setTxPower) + "_SpreadingFactor " + String(setSpreadingFactor);

  File myFile10 = SD.open("/datalog/" + namaFile + ".txt", FILE_APPEND);
  if (myFile10) {
    myFile10.print(now.getYear(), DEC);
    myFile10.print("-");
    myFile10.print(now.getMonth(), DEC);
    myFile10.print("-");
    myFile10.print(now.getDay(), DEC);
    myFile10.print(" ");
    myFile10.print(now.getHour(), DEC);
    myFile10.print(":");
    myFile10.print(now.getMinute(), DEC);
    myFile10.print(":");
    myFile10.print(now.getSecond(), DEC);
    myFile10.print(", ");
    myFile10.print(dataMeasurement);
    myFile10.println("");
    myFile10.close();

  } else {
    Serial.println("error");
  }
}

void generateDummyData() {
  Rain_val = random(0, 100); // Nilai Rain_val antara 0-99
  distance = random(100, 3000); // Jarak antara 100 cm hingga 3000 cm
  // Serial.println("Generated Dummy Data - Rain: " + String(Rain_val) + " mm, Distance: " + String(distance / 10) + " cm");
}

void setup() {
    randomSeed(analogRead(0));
    Serial.begin(115200);
    mySerial.begin(9600);        // Initialize SoftwareSerial
    mySerial1.begin(9600);
    mySerial2.begin(115200);
    Wire.begin();
    while (!Serial); // Wait for Serial Monitor

    FastLED.addLeds<WS2812, LED_PIN, GRB>(leds, NUM_LEDS);

    leds[0] = CRGB(0, 255, 0);      // LED 0 warna hijau dengan kecerahan penuh
    leds[0].nscale8(255);           // Set kecerahan LED 0
    FastLED.show();

    Serial.println("LoRa Node1");
    initSDCard();

    // Read node ID from EEPROM
    nodeId = EEPROM.read(EEPROM_ADDRESS);
    if (nodeId < 1 || nodeId > N_NODES) {
        nodeId = 3; // Change this value for each node before uploading
        EEPROM.write(EEPROM_ADDRESS, nodeId);
        EEPROM.commit();
    }

    Serial.print(F("Initializing node "));
    Serial.println(nodeId);
    manager = new RHMesh(rf95, nodeId);
    
    if (!manager->init()) {
        Serial.println(F("Initialization failed"));
        return;
    }
    
    rf95.setFrequency(922.0);
    rf95.setTxPower(setTxPower, false);
    rf95.setSpreadingFactor(setSpreadingFactor);
    Serial.println(F("RF95 ready"));
    pinMode(enTxPin, OUTPUT);
    digitalWrite(enTxPin, HIGH);  // Default ke TX mode
}

void loop() {  
    // Mendengarkan pesan yang masuk
    DatalogMeasurement();  
    uint8_t len = sizeof(buf);  
    uint8_t from;  
    if (manager->recvfromAckTimeout((uint8_t *)buf, &len, 1000, &from)) {  
        buf[len] = '\0'; // Null terminate string  
        Serial.print(F("Received from N"));  
        Serial.print(from);  
        Serial.print(F(": "));  
        Serial.println(buf);  
          
        // Meneruskan pesan berdasarkan probabilitas gossip  
        if (random(0, 100) < GOSSIP_PROBABILITY * 100) {  
            if (nodeId == 2) {
                generateDummyData();  
                sprintf(buf + len, " | Value N2: Rain: %d mm | Distance : %.2f cm", Rain_val, distance / 10);  
                Serial.println(buf);  
                uint8_t errorN3 = manager->sendtoWait((uint8_t *)buf, strlen(buf), 3);  
                if (errorN3 != RH_ROUTER_ERROR_NONE) {  
                    Serial.print(F("Error sending to N3: "));  
                    Serial.println(errorN3);
                    DatalogFailed();  
                } else {  
                    Serial.println(F("Message forwarded to N3 successfully"));
                    Datalog();  
                }  
            }  
        }  
    }  
  
    // Node 1 mengirim pesan ke Node 2  
    if (nodeId == 1) {  
        // Cek apakah cukup waktu telah berlalu  
        unsigned long currentMillis = millis();  
        if (currentMillis - previousMillis >= interval) {  
            previousMillis = currentMillis; // Simpan waktu terakhir
            generateDummyData();  
  
            sprintf(buf, "Value N1: Rain: %d mm | Distance: %.2f cm", Rain_val, distance / 10);  
            Serial.print(F("Sending to N2: "));  
            Serial.println(buf);  
            uint8_t errorN2 = manager->sendtoWait((uint8_t *)buf, strlen(buf), 2);  
            if (errorN2 != RH_ROUTER_ERROR_NONE) {  
                Serial.print(F("Error sending to N2: "));  
                Serial.println(errorN2);
                // DatalogFailed();  
            } else {  
                Serial.println(F("Message sent to N2 successfully"));
                // Datalog();  
            }  
        }  
    }  
}  
