#include <SPI.h>
#include <LoRa.h>
#include <WiFi.h>
#include <HTTPClient.h>
#include <DS3231-RTC.h>
#include <SPI.h>
#include <SD.h>
#include <Wire.h>
#include <Arduino.h>

// LoRa Pin Setup
#define ss 15  // GPIO 15
#define rst 26  // GPIO 16
#define dio0 27  // GPIO 4

// LED Built-in Pin
#define LED_BUILTIN 2  // GPIO2 adalah pin untuk LED built-in ESP32

byte MasterNode = 0xFF;
byte Node1 = 0xBB;
byte Node2 = 0xCC;
byte Node3 = 0xDD;
byte Node4 = 0xEE;

bool inisiasi = false;
bool koneksi = false;
bool sd_isi = false;

String outgoing;
byte msgCount = 0;
String incoming = "";

// Timing
unsigned long previousMillisWeb = 0;   // Timer for sending data to web server
const int webInterval = 25000;         // 20 seconds for sending data to the server

// Data from each node
float node1_value1, node1_value2;
float node2_value1, node2_value2;
float node3_value1, node3_value2;
float node4_value1, node4_value2;

// WiFi credentials
//const char* ssid = "TP-Link_BA98";
//const char* password = "99008864";
char ssid[32];
char password[64];

// URL API for POST
const char* serverName = "https://misred-iot.com/api/projects/6/values";

// Auth token
const char* authToken = "01J8D7YTYJ3FQ8RG7MVBSFMHW5";

//RTC
RTClib myRTC;

//SD Card
const int CS = 5;

void setup() {
  Serial.begin(115200);
  Wire.begin();
  delay(500);

  // Inisialisasi LED Built-in sebagai output
  pinMode(LED_BUILTIN, OUTPUT);
  digitalWrite(LED_BUILTIN, LOW);  // Matikan LED saat awal
  
  while (!Serial) {
    ;  // wait for serial port to connect. Needed for native USB port only
  }

  Serial.print("Initializing SD card...");
  if (!SD.begin(CS)) {
    Serial.print("initialization failed!");
    return;
  }
  Serial.print("initialization done.");

  // Initialize LoRa
  Serial.println("LoRa Master Node");
  LoRa.setPins(ss, rst, dio0);
  if (!LoRa.begin(922E6)) {
    Serial.println("Starting LoRa failed!");
    while (1);
  }

  // Initialize WiFi
  //  WiFi.begin(ssid, password);
  //  Serial.print("Connecting to WiFi");
  //  while (WiFi.status() != WL_CONNECTED) {
  //    delay(1000);
  //    Serial.print(".");
  //  }
  //  Serial.println();
  //  Serial.println("Connected to WiFi");

  //cek file WIFI apakah ada isinya
  File file10 = SD.open("/WIFI.txt");
  if (file10) {
    //jika ada maka akan masuk ke koneksi wifi
    if (file10.size() > 0) {
      String line = file10.readStringUntil('\n');
      sd_isi = true;
      parseCredentialsAndConnect(line);
    } else {
      sd_isi = false;
      inisiasi = true;
    }
    file10.close();
  } else {
  }
}

void loop() {
  unsigned long currentMillis = millis();

  // Parse incoming LoRa packets
  onReceive(LoRa.parsePacket());

  // Send data to web server every 20 seconds
  if (currentMillis - previousMillisWeb >= webInterval) {

        // Kedip 3x cepat sebelum mengirim data ke server
    blinkLED(3, 50);  // Kedip 3 kali dengan delay 100ms
    
    sendDataToWeb();
    Datalog();
    previousMillisWeb = currentMillis;  // Reset timer for next web upload
  }
}

void onReceive(int packetSize) {
  if (packetSize == 0) return;

    // Kedip LED sekali saat menerima data LoRa
  blinkLED(1, 100);  // Kedip 1 kali dengan delay 200ms

  // Read packet header
  int recipient = LoRa.read();
  byte sender = LoRa.read();
  byte incomingMsgId = LoRa.read();
  byte incomingLength = LoRa.read();

  while (LoRa.available()) {
    incoming += (char)LoRa.read();
  }

  if (incomingLength != incoming.length()) {
    return;
  }

  if (recipient != MasterNode) {
    return;
  }

  // Process incoming data from Node1, Node2, Node3, or Node4
  String v1 = getValue(incoming, ',', 0);
  String v2 = getValue(incoming, ',', 1);

  if (sender == Node1) {
    node1_value1 = v1.toFloat();
    node1_value2 = v2.toFloat();
    Serial.println("Data from Node1:");
  } else if (sender == Node2) {
    node2_value1 = v1.toFloat();
    node2_value2 = v2.toFloat();
    Serial.println("Data from Node2:");
  } else if (sender == Node3) {
    node3_value1 = v1.toFloat();
    node3_value2 = v2.toFloat();
    Serial.println("Data from Node3:");
  } else if (sender == Node4) {
    node4_value1 = v1.toFloat();
    node4_value2 = v2.toFloat();
    Serial.println("Data from Node4:");
  }

  // Print values to Serial Monitor
  Serial.print("Value1: ");
  Serial.println(v1);
  Serial.print("Value2: ");
  Serial.println(v2);

  incoming = "";  // Clear incoming data
}

// Fungsi untuk membuat LED berkedip
void blinkLED(int times, int delayMs) {
  for (int i = 0; i < times; i++) {
    digitalWrite(LED_BUILTIN, HIGH);  // Nyalakan LED
    delay(delayMs);                   // Tunggu
    digitalWrite(LED_BUILTIN, LOW);   // Matikan LED
    delay(delayMs);                   // Tunggu
  }
}

String getValue(String data, char separator, int index) {
  int found = 0;
  int strIndex[] = { 0, -1 };
  int maxIndex = data.length() - 1;

  for (int i = 0; i <= maxIndex && found <= index; i++) {
    if (data.charAt(i) == separator || i == maxIndex) {
      found++;
      strIndex[0] = strIndex[1] + 1;
      strIndex[1] = (i == maxIndex) ? i + 1 : i;
    }
  }
  return found > index ? data.substring(strIndex[0], strIndex[1]) : "";
}


void parseCredentialsAndConnect(const String &input) {
  int firstDelimiter = input.indexOf('n');
  int secondDelimiter = input.indexOf('xx');
  int thirdDelimiter = input.indexOf('z');

  if (firstDelimiter != -1 && secondDelimiter != -1 && thirdDelimiter != -1) {
    String ssidStr = input.substring(firstDelimiter + 1, secondDelimiter);
    String passStr = input.substring(secondDelimiter + 2, thirdDelimiter);

    ssidStr.toCharArray(ssid, sizeof(ssid));
    passStr.toCharArray(password, sizeof(password));
    sd_isi = true ;

    connectToWiFi();
  } else {
    Serial.println("Invalid format. Please enter in 'n(ssid)xx(password)z' format.");
  }
}

void connectToWiFi() {
  Serial.println("\nConnecting to WiFi...");
  WiFi.begin(ssid, password);
  delay(1000);
  Serial.println(ssid);
  Serial.println(password);

  int attemptCount = 0;
  while (WiFi.status() != WL_CONNECTED && attemptCount < 40) {
    delay(500);
    Serial.print(".");
    attemptCount++;
  }

  if (WiFi.status() == WL_CONNECTED) {
    Serial.println("\nKoneksi WiFi berhasil");
    Serial.print("Alamat IP: ");
    Serial.println(WiFi.localIP());
    koneksi = true;
    inisiasi = true;
    delay (2000);

  } else {
    Serial.println("\nGagal menghubungkan ke WiFi. Periksa SSID dan kata sandi.");
    koneksi = false;
    if (inisiasi == false) {
      inisiasi = true;
    }
  }
}


void sendDataToWeb() {
  if (WiFi.status() == WL_CONNECTED) {
    HTTPClient http;

    http.begin(serverName);
    http.addHeader("Content-Type", "application/json");
    http.addHeader("X-Auth-Token", authToken);

    // Create JSON payload for 8 data streams
    String jsonPayload = "{";
    jsonPayload += "\"datastreams\": [11, 12, 13, 14, 15, 16, 17, 18],";
    jsonPayload += "\"values\": [";
    jsonPayload += String(node1_value1) + ", " + String(node1_value2) + ", ";
    jsonPayload += String(node2_value1) + ", " + String(node2_value2) + ", ";
    jsonPayload += String(node3_value1) + ", " + String(node3_value2) + ", ";
    jsonPayload += String(node4_value1) + ", " + String(node4_value2);
    jsonPayload += "]}";

    Serial.println("Sending JSON data:");
    Serial.println(jsonPayload);

    int httpResponseCode = http.POST(jsonPayload);

    if (httpResponseCode > 0) {
      String response = http.getString();
      Serial.println("HTTP Response code: " + String(httpResponseCode));
      Serial.println("Response from server: " + response);
    } else {
      Serial.println("Error on sending POST: " + String(httpResponseCode));
    }

    http.end();
  } else {
    Serial.println("WiFi Disconnected");
  }
}

void Datalog () {

  //masukkan nama sensor ke vector kemudian lakukan if disini untuk dari countfount2 untuk mengambil nilai value dari nama sensor


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
    myFile10.print("Node1 Rain: ");
    myFile10.print(node1_value1);
    myFile10.print(", ");
    myFile10.print("Node1 Water Level: ");
    myFile10.print(node1_value1);
    myFile10.print(", ");
    myFile10.print("Node2 Rain: ");
    myFile10.print(node2_value1);
    myFile10.print(", ");
    myFile10.print("Node2 Water Level: ");
    myFile10.print(node2_value2);
    myFile10.print(", ");
    myFile10.print("Node3 Rain: ");
    myFile10.print(node3_value1);
    myFile10.print(", ");
    myFile10.print("Node3 Water Level: ");
    myFile10.print(node3_value2);
    myFile10.print(", ");
    myFile10.print("Node4 Rain: ");
    myFile10.print(node4_value1);
    myFile10.print(", ");
    myFile10.print("Node4 Water Level: ");
    myFile10.print(node4_value2);
    myFile10.print(", ");

    myFile10.println();
    myFile10.close();
  } else {

  }
}
