/*
    Simple hello world Json REST response
     by Mischianti Renzo <https://www.mischianti.org>

    https://www.mischianti.org/

*/

#include "Arduino.h"
#include <ESP8266WiFi.h>
#include <WiFiClient.h>
#include <WiFiManager.h>
#include <ESP8266WebServer.h>
#include <ESP8266mDNS.h>
#include <uri/UriBraces.h>
#include "DHT.h"
#include <OneWire.h>
#include <DallasTemperature.h>
#include <Wire.h>

// -------- Менеджер WiFi ---------
#define AC_SSID "AutoConnectAP"
#define AC_PASS "12345678"

// -------- DHT22 ---------
#define DHTPIN D3        // Номер пина, который подключен к DHT22
#define DHTTYPE DHT22   // Указываем, какой тип датчика мы используем
#define ONE_WIRE_BUS D4  // контакт для передачи данных подключен к D1 на ESP8266 12-E (GPIO5):

//const char* ssid = "ASUS";
//const char* password = "03061986";

ESP8266WebServer server(80);
DHT dht(DHTPIN, DHTTYPE);
// создаем экземпляр класса oneWire; с его помощью
// можно коммуницировать с любыми девайсами, работающими
// через интерфейс 1-Wire, а не только с температурными датчиками
// от компании Maxim/Dallas:
OneWire oneWire(ONE_WIRE_BUS);

// передаем объект oneWire объекту DS18B20:
DallasTemperature DS18B20(&oneWire);

// ----------------- ПЕРЕМЕННЫЕ ------------------
const char* autoConnectSSID = AC_SSID;
const char* autoConnectPass = AC_PASS;
float h; // Влажность
float t; // Температура
float temperatures[10];
char temperatureOutside[2];
int address = 0x50;
// создаём указатель массив для хранения адресов датчиков
DeviceAddress *sensorsUnique;
// количество датчиков на шине
int countSensors;

// функция вывода адреса датчика
void printAddress(DeviceAddress deviceAddress) {
  for (uint8_t i = 0; i < 8; i++) {
    if (deviceAddress[i] < 16) Serial.print("0");
    Serial.print(deviceAddress[i], HEX);
  }
}

void getTemperature() {
  DS18B20.requestTemperatures();
  // считываем данные из регистра каждого датчика по очереди
  for (int i = 0; i < countSensors; i++) {
    temperatures[i] = DS18B20.getTempCByIndex(i);
    Serial.println(temperatures[i]);
  }
}


void readFromI2C() {
  Wire.requestFrom(address, 2);
  Wire.readBytes(temperatureOutside, 2);

  for (int i = 0; i < 2; i++) {
    Serial.print(temperatureOutside[i], HEX);
    Serial.print(" ");
  }
  Serial.println();
}

void sendJSON(String value, String type) {
  server.send(200, "text/json", "{\"value\": " + value + ", \"type\": \"" + type + "\"}");
}


void getHumidity() {
  String value = String(h);
  sendJSON(value, "HUM");
}

void getTemp() {
  String value = String(t);
  sendJSON(value, "TEMP");
}

void getTemp2() {
  char temperature[6];
  dtostrf(temperatures[1], 2, 2, temperature);
  sendJSON(temperature, "TEMP");
}

void getTemp3() {
  bool negative = false;
  char buffer[50];
  char tDec = temperatureOutside[0];
  char tFloat = temperatureOutside[1];

  if (tDec >= 127) {
    tDec = 256 - tDec;
    tFloat = 256 - tFloat;
    negative = true;
  }

  sprintf(buffer, "%s%d\.%d", (negative ? "-" : ""), tDec, tFloat);
  String value = String(buffer);

  sendJSON(value, "TEMP");
}

void getTemp4() {
  char temperature[6];
  dtostrf(temperatures[0], 2, 2, temperature);
  sendJSON(temperature, "TEMP");
}

// Define routing
void restServerRouting() {
  server.on("/", HTTP_GET, []() {
    server.send(200, F("text/html"),
                F("Welcome to the REST Web Server"));
  });
  server.on(F("/hum"), HTTP_GET, getHumidity);
  //  server.on(F("/temp"), HTTP_GET, getTemp);
  //  server.on(F("/temp2"), HTTP_GET, getTemp2 );
  //  server.on(F("/temp3"), HTTP_GET, getTemp3 );
  //  server.on(F("/temp4"), HTTP_GET, getTemp4 );
  server.on(UriBraces("/temp/{}"), []() {
    int temp = server.pathArg(0).toInt();
    switch (temp) {
      case 1:
        getTemp();
        break;
      case 2:
        getTemp2();
        break;
      case 3:
        getTemp3();
        break;
      case 4:
        getTemp4();
        break;
      default:
        server.send(200, "text/plain", "Sensor: '" + server.pathArg(0) + "'");
    }
  });
}

// Manage not found URL
void handleNotFound() {
  String message = "File Not Found\n\n";
  message += "URI: ";
  message += server.uri();
  message += "\nMethod: ";
  message += (server.method() == HTTP_GET) ? "GET" : "POST";
  message += "\nArguments: ";
  message += server.args();
  message += "\n";
  for (uint8_t i = 0; i < server.args(); i++) {
    message += " " + server.argName(i) + ": " + server.arg(i) + "\n";
  }
  server.send(404, "text/plain", message);
}


void setup(void) {
  Serial.begin(115200);

  WiFiManager wifiManager;
  wifiManager.setDebugOutput(false);
  wifiManager.autoConnect(autoConnectSSID, autoConnectPass);
  Serial.print("Connected! IP address: ");
  Serial.println(WiFi.localIP());

  DS18B20.begin();
  // выполняем поиск устройств на шине
  countSensors = DS18B20.getDeviceCount();
  Serial.print("Found sensors: ");
  Serial.println(countSensors);
  // выделяем память в динамическом массиве под количество обнаруженных сенсоров
  sensorsUnique = new DeviceAddress[countSensors];

  // делаем запрос на получение адресов датчиков
  for (int i = 0; i < countSensors; i++) {
    DS18B20.getAddress(sensorsUnique[i], i);
  }
  // выводим полученные адреса
  for (int i = 0; i < countSensors; i++) {
    Serial.print("Device ");
    Serial.print(i);
    Serial.print(" Address: ");
    printAddress(sensorsUnique[i]);
    Serial.println();
  }
  Serial.println();
  // устанавливаем разрешение всех датчиков в 12 бит
  for (int i = 0; i < countSensors; i++) {
    DS18B20.setResolution(sensorsUnique[i], 12);
  }

  // Activate mDNS this is used to be able to connect to the server
  // with local DNS hostmane esp8266.local
  if (MDNS.begin("esp8266")) {
    Serial.println("MDNS responder started");
  }

  dht.begin();

  // Set server routing
  restServerRouting();
  // Set not found response
  server.onNotFound(handleNotFound);
  // Start server
  server.begin();
  Serial.println("HTTP server started");
  Wire.begin(); // join i2c bus with SDA=D3 and SCL=D4 of NodeMCU
  Wire.setClock(10000);

  Serial.println();
  Serial.println("Start I2C scanner ...");
  Serial.print("\r\n");
  byte count = 0;
  for (byte i = 8; i < 120; i++)
  {
    Wire.beginTransmission(i);
    if (Wire.endTransmission() == 0)
    {
      Serial.print("Found I2C Device: ");
      Serial.print(" (0x");
      Serial.print(i, HEX);
      Serial.println(")");
      count++;
      delay(1);
    }
  }

  Serial.print("\r\n");
  Serial.println("Finish I2C scanner");
  Serial.print("Found ");
  Serial.print(count, HEX);
  Serial.println(" Device(s).");

  Wire.beginTransmission(address);
  Wire.write(0x1e);
  Wire.endTransmission(false);
}

int timeSinceLastRead = 0;
void loop(void) {
  server.handleClient();

  // Каждые 5 секунд.
  if (timeSinceLastRead > 10000) {
    readFromI2C();

    getTemperature(); // DS18B20
    // Считывание температуры и влажности около 250 милисекунд!
    // Показания датчика также могут составлять до 2 секунд
    h = dht.readHumidity(); // Влажность
    t = dht.readTemperature(); // Температура


    // Проверяем, получили ли данные с датчика.
    if (isnan(h) || isnan(t)) {
      Serial.println("Данных нет! Останавливаем цикл и запускаем по новой");
      timeSinceLastRead = 0;
      return;
    }
    // Вычисляем индекс тепла в градусах Цельсия
    float hic = dht.computeHeatIndex(t, h, false);

    Serial.print("Влажность: ");
    Serial.print(h);
    Serial.print(" %\t");
    Serial.print("Температура: ");
    Serial.print(t);
    Serial.print(" *C ");
    Serial.print("Индекс тепла: ");
    Serial.print(hic);
    Serial.println(" *C ");

    timeSinceLastRead = 0;
  }
  delay(1);
  timeSinceLastRead++;
}
