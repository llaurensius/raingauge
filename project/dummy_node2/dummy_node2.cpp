/*
  Lora Node2
  The IoT Projects
*/

#include <SPI.h>              // Include libraries
#include <LoRa.h>
#include <SoftwareSerial.h>
#include <Arduino.h>
#include "FS.h"
#include <SD.h>
#include <Wire.h>
#include <DS3231-RTC.h>
#include <FastLED.h>

#define ss 15
#define rst 26
#define dio0 27
#define enTxPin 4   // HIGH: TX and LOW: RX
#define LED_PIN     2
#define NUM_LEDS    2

RTClib myRTC; // RTC library instance
const int CS = 5; // Chip select pin for SD card

SoftwareSerial mySerial(16, 17); // RX, TX
SoftwareSerial mySerial1(35, 34); // RX, TX (ceck kembali apakah pin benar)

CRGB leds[NUM_LEDS];
int hue = 0; // Variabel untuk menyimpan nilai Hue

// Prototipe deklarasi fungsi
uint16_t calculateCRC(uint8_t *data, uint8_t len);

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
byte Node2 = 0xCC;

int Rain_val;
String Mymessage = "";
float distance;

// Timing
unsigned long previousMillis = 0;  // Timer for sending data
const int sendInterval = 5000;     // Send data every 5 seconds

void initSDCard() {
  while (true) { // Loop tak terbatas untuk mencoba inisialisasi
    Serial.print("t0.txt=\"Initializing SD card...\"");

    if (SD.begin(CS)) {
      Serial.print("t0.txt=\"SD card initialization successful!\""); // Jika berhasil
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

void sendMessage(String outgoing, byte MasterNode, byte Node2) {
  LoRa.beginPacket();               // Start packet
  LoRa.write(MasterNode);           // Add destination address (Master Node)
  LoRa.write(Node2);                // Add sender address (this Node2)
  LoRa.write(msgCount);             // Add message ID
  LoRa.write(outgoing.length());    // Add payload length
  LoRa.print(outgoing);             // Add payload (the data)
  LoRa.endPacket();                 // Finish packet and send it
  msgCount++;                       // Increment message ID
}

void bacaRain() {
  Serial.println("Baca Rain jalan");
  uint8_t id = 1; // Ubah tipe data menjadi uint8_t

  // Kirim permintaan baca data ke RS485
  uint8_t buffer[] = { id, 0x03, 0x01, 0x05, 0x00, 0x01, 0x00, 0x00 };
  uint16_t crc = calculateCRC(buffer, sizeof(buffer) - 2);
  buffer[sizeof(buffer) - 2] = crc & 0xFF;  // LSB
  buffer[sizeof(buffer) - 1] = crc >> 8;    // MSB

  // Kirim permintaan ke RS485
  mySerial.write(buffer, sizeof(buffer));

  digitalWrite(enTxPin, LOW);  // Set ke RX mode untuk menerima balasan

  // Menunggu respons dari RS485
  unsigned long timeLimit = millis();
  while (millis() - timeLimit < 500) {  // Tunggu hingga 500ms
    if (mySerial.available()) {
      byte responseHeader[8];

      // Membaca 8 byte dari RS485
      mySerial.readBytes(responseHeader, 8);

      // Tampilkan balasan byte per byte di Serial Monitor
      Serial.println("Balasan dari RS485:");
      for (int i = 0; i < 8; i++) {
        Serial.print("Byte ");
        Serial.print(i);
        Serial.print(": 0x");
        Serial.println(responseHeader[i], HEX);  // Tampilkan dalam format heksadesimal
      }

      // Jika balasan sesuai (misal, id dan kode fungsi benar)
      if (responseHeader[0] == id && responseHeader[1] == 0x03 && responseHeader[2] == 0x02) {
        // Gabungkan byte 3 dan byte 4 menjadi satu nilai 16-bit
        uint16_t rawValue = (responseHeader[3] << 8) | responseHeader[4];

        // Simpan nilai rawValue sebagai int (tidak perlu pembagian karena ingin hasil integer)
        Rain_val = rawValue;

        // Tampilkan nilai Rain di Serial Monitor
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

  // Tunggu sampai data tersedia, dengan batas waktu 2 detik
  while (mySerial1.available() < 4) {
    if (millis() - startTime >= 2000) { // Jika waktu sudah lebih dari 2 detik
      Serial.println("ERROR: Timeout, no data received");
      return; // Keluar dari fungsi
    }
  }

  // Baca data sebanyak 4 byte
  for (int i = 0; i < 4; i++) {
    data[i] = mySerial1.read();
  }

  // Cek jika byte pertama adalah 0xff
  if (data[0] == 0xff) {
    int sum = (data[0] + data[1] + data[2]) & 0x00FF;  // Hitung checksum
    if (sum == data[3]) {  // Jika checksum sesuai
      distance = (data[1] << 8) + data[2];  // Hitung jarak
      if (distance > 280) {  // Jika jarak di atas ambang batas 28 cm
        Serial.print("distance = ");
        Serial.print(distance / 10);  // Konversi ke cm
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

  // Flush untuk memastikan buffer bersih
  while (mySerial1.available()) {
    mySerial1.read();
  }

  Serial.println("Baca Distance selesai");
}

void Datalog() {
  DateTime now = myRTC.now();

  // Mendapatkan nilai bulan dengan format dua digit
  String getMonthStr = now.getMonth() < 10 ? "0" + String(now.getMonth()) : String(now.getMonth());

  // Mendapatkan nilai hari dengan format dua digit
  String getDayStr = now.getDay() < 10 ? "0" + String(now.getDay()) : String(now.getDay());

  // Menggabungkan semua komponen menjadi string namaFile dengan format "DDMMYYYY"
  String namaFile = getDayStr + getMonthStr + String(now.getYear(), DEC);

  File myFile10 = SD.open("/datalog/" + namaFile + ".txt", FILE_APPEND);
  if (myFile10) {
    // Write timestamp
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
    myFile10.print("Rain: ");
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

void generateDummyData() {
  // Menghasilkan data dummy untuk Rain_val dan distance
  Rain_val = random(0, 100); // Nilai Rain_val antara 0-99
  distance = random(100, 3000); // Jarak antara 100 cm hingga 3000 cm
  Serial.println("Generated Dummy Data - Rain: " + String(Rain_val) + " mm, Distance: " + String(distance / 10) + " cm");
}

void setup() {
  Serial.begin(115200);        // Initialize serial
  mySerial.begin(9600);        // Initialize SoftwareSerial
  mySerial1.begin(9600);
  Wire.begin();

  FastLED.addLeds<WS2812, LED_PIN, GRB>(leds, NUM_LEDS);

  // Indikator LED0 kedip hijau saat inisialisasi
  leds[0] = CRGB(0, 255, 0);      // LED 0 warna hijau dengan kecerahan penuh
  leds[0].nscale8(255);           // Set kecerahan LED 0
  FastLED.show();

  Serial.println("LoRa Node2");

//   initSDCard();

  LoRa.setPins(ss, rst, dio0);

  if (!LoRa.begin(922E6)) {
    Serial.println("Starting LoRa failed!");

    // Jika LoRa gagal, LED0 berkedip merah 2 kali
    blinkLED0(CRGB::Red, 2, 500);
    while (1);
  }

  pinMode(enTxPin, OUTPUT);
  digitalWrite(enTxPin, HIGH);  // Default ke TX mode
}

void loop() {
  unsigned long currentMillis = millis();

  leds[0] = CRGB(0, 255, 0);      // LED 0 warna hijau
  leds[0].nscale8(255);           // Set kecerahan LED 0
  FastLED.show();

  leds[1] = CRGB(255, 255, 255);  // LED 1 warna putih
  leds[1].nscale8(100);           // Set kecerahan LED 1
  FastLED.show();

//   bacaRain();
//   bacaDistance();
  // Datalog();
  generateDummyData();
  delay(5000);

  // Send data every 5 seconds
  if (currentMillis - previousMillis >= sendInterval) {
    Mymessage = String(Rain_val) + "," + String(distance / 10);

    // Send the message to MasterNode
    sendMessage(Mymessage, MasterNode, Node2);
    Serial.println("Data sent: " + Mymessage);

    blinkLED0(CRGB::Blue, 2, 100);

    // Reset the timer for the next interval
    previousMillis = currentMillis;
  }

  delay(100); // Short delay to avoid flooding LoRa network
}
