#include <stdio.h>
#include <ArduinoJson.h>
#include <Arduino.h>

#include <credentials.h>

#include <OneWire.h>
#include <DallasTemperature.h>

#include <WiFiUdp.h>
#include <NTPClient.h>
#include <TimeLib.h>

#include <ESP8266WiFi.h>
#include <ESP8266WebServer.h>

#include "LittleFS.h"

//#define DEBUG
IPAddress staticIP(192, 168, 63, 56);
#define URI "/temps"
IPAddress gateway(192, 168, 63, 1);
IPAddress subnet(255, 255, 255, 0);
IPAddress dns(192, 168, 63, 21);
IPAddress dnsGoogle(8, 8, 8, 8);
String hostName = "esp02";

#define HTTP_REST_PORT 80
#define WIFI_RETRY_DELAY 500
#define MAX_WIFI_INIT_RETRY 50
#define ONE_WIRE_BUS 2
#define LED_0 0

// Define NTP Client to get time
WiFiUDP ntpUDP;
const long utcOffsetInSeconds = 0;
NTPClient timeClient(ntpUDP, "pool.ntp.org", utcOffsetInSeconds);

OneWire oneWire(ONE_WIRE_BUS);
DallasTemperature sensors(&oneWire);
float tempSensor[5];
DeviceAddress Thermometer[5];
uint8_t sensor[5][8];

String deviceAddress[5] = {"", "", "", "", ""};
byte gpio = 2;
String strTemperature[5] = {"-127", "-127", "-127", "-127", "-127"};

int deviceCount;

ESP8266WebServer http_rest_server(HTTP_REST_PORT);

void BlinkNTimes(int pin, int blinks, unsigned long millies)
{
    digitalWrite(pin, LOW);
    for (int i = 0; i < blinks; i++)
    {
        digitalWrite(pin, HIGH);
        delay(millies);
        digitalWrite(pin, LOW);
        delay(millies);
    }
}

int init_wifi()
{
    int retries = 0;

    Serial.println("Connecting to WiFi");

    WiFi.config(staticIP, gateway, subnet, dns, dnsGoogle);
    WiFi.mode(WIFI_STA);
    WiFi.hostname(hostName);
    WiFi.begin(ssid, password);

    while ((WiFi.status() != WL_CONNECTED) && (retries < MAX_WIFI_INIT_RETRY))
    {
        retries++;
        delay(WIFI_RETRY_DELAY);
        Serial.print("#");
    }
    Serial.println();
    BlinkNTimes(LED_0, 3, 500);
    return WiFi.status();
}

String GetCurrentTime()
{
    timeClient.update();
    unsigned long epochTime = timeClient.getEpochTime();
    char buff[32];

    sprintf(buff, "%02d-%02d-%02d %02d:%02d:%02d",
            year(epochTime),
            month(epochTime),
            day(epochTime),
            hour(epochTime),
            minute(epochTime),
            second(epochTime));
    String currentTime = buff;
    return currentTime;
}

String GetAddressToString(DeviceAddress deviceAddress)
{
    String str = "";
    for (uint8_t i = 0; i < 8; i++)
    {
        if (deviceAddress[i] < 16)
            str += String(0, HEX);
        str += String(deviceAddress[i], HEX);
    }
    return str;
}

void get_temps()
{
    BlinkNTimes(LED_0, 2, 500);
    StaticJsonBuffer<800> jsonBuffer;
    JsonObject &jsonObj = jsonBuffer.createObject();
    char JSONmessageBuffer[800];

    try
    {
#ifdef DEBUG
        jsonObj["DEBUG"] = "******* true *******";
#else
        jsonObj["DEBUG"] = "false";
#endif
        jsonObj["UtcTime"] = GetCurrentTime();
        jsonObj["Hostname"] = hostName;
        jsonObj["IpAddress"] = WiFi.localIP().toString();
        jsonObj["MacAddress"] = WiFi.macAddress();
        jsonObj["Gpio"] = gpio;
        jsonObj["DeviceType"] = "OneWire_Temp";
        jsonObj["DeviceCount"] = deviceCount;

        if (deviceCount == 0)
        {
            Serial.print("No Content");
            //http_rest_server.send(204);
            //CORS
            http_rest_server.sendHeader("Access-Control-Allow-Origin", "*");
            String sHostName(WiFi.hostname());

            http_rest_server.send(200, "text/html", "No devices found on " + sHostName + " (" + WiFi.macAddress() + ")");
        }
        else
        {
            sensors.requestTemperatures();
            for (int i = 0; i < deviceCount; i++)
            {
#ifdef DEBUG
                tempSensor[i] = 27 + i;
                deviceAddress[i] = (String)(100 + i);
#else
                tempSensor[i] = sensors.getTempC(sensor[i]);
                deviceAddress[i] = GetAddressToString(Thermometer[i]);
#endif
                strTemperature[i] = tempSensor[i];
                Serial.print(strTemperature[i] + " ");
            }
            Serial.println();

            JsonArray &sensors = jsonObj.createNestedArray("Sensors");
            for (int i = 0; i < deviceCount; i++)
            {
                JsonObject &sensor = sensors.createNestedObject();
                sensor["Id"] = deviceAddress[i];
                sensor["ValueType"] = "Temperature";
                sensor["Value"] = strTemperature[i];

                Serial.print("DeviceId=");
                Serial.println(deviceAddress[i]);
                Serial.print("Temp=");
                Serial.println(strTemperature[i]);
            }
        }
    }
    catch (const std::exception &e)
    {
        // String exception = e.what();
        // jsonObj["Exception"] = exception.substring(0, 99);
        jsonObj["Exception"] = " ";
        //std::cerr << e.what() << '\n';
    }

    jsonObj.prettyPrintTo(JSONmessageBuffer, sizeof(JSONmessageBuffer));

    http_rest_server.sendHeader("Access-Control-Allow-Origin", "*");
    http_rest_server.sendHeader("Access-Control-Allow-Headers", "Origin, X-Requested-With, Content-Type, Accept");

    http_rest_server.send(200, "application/json", JSONmessageBuffer);
}

void config_rest_server_routing()
{
    http_rest_server.on("/", HTTP_GET, []() {
        http_rest_server.send(200, "text/html",
                              "Welcome to the ESP8266 REST Web Server: " + GetCurrentTime());
    });
    http_rest_server.on(URI, HTTP_GET, get_temps);
}

void PrintDeviceInfo()
{
    LittleFS.begin();
    FSInfo fs_info;
    LittleFS.info(fs_info);

    float fileTotalKB = (float)fs_info.totalBytes / 1024.0;
    float fileUsedKB = (float)fs_info.usedBytes / 1024.0;

    float flashChipSize = (float)ESP.getFlashChipSize() / 1024.0 / 1024.0;
    float realFlashChipSize = (float)ESP.getFlashChipRealSize() / 1024.0 / 1024.0;
    float flashFreq = (float)ESP.getFlashChipSpeed() / 1000.0 / 1000.0;
    FlashMode_t ideMode = ESP.getFlashChipMode();

    Serial.printf("\n#####################\n");

    Serial.printf("__________________________\n\n");
    Serial.println("Firmware: ");
    Serial.printf("    Chip Id: %08X\n", ESP.getChipId());
    Serial.print("    Core version: ");
    Serial.println(ESP.getCoreVersion());
    Serial.print("    SDK version: ");
    Serial.println(ESP.getSdkVersion());
    Serial.print("    Boot version: ");
    Serial.println(ESP.getBootVersion());
    Serial.print("    Boot mode: ");
    Serial.println(ESP.getBootMode());

    Serial.printf("__________________________\n\n");

    Serial.println("Flash chip information: ");
    Serial.printf("    Flash chip Id: %08X (for example: Id=001640E0  Manuf=E0, Device=4016 (swap bytes))\n", ESP.getFlashChipId());
    Serial.printf("    Sketch thinks Flash RAM is size: ");
    Serial.print(flashChipSize);
    Serial.println(" MB");
    Serial.print("    Actual size based on chip Id: ");
    Serial.print(realFlashChipSize);
    Serial.println(" MB ... given by (2^( \"Device\" - 1) / 8 / 1024");
    Serial.print("    Flash frequency: ");
    Serial.print(flashFreq);
    Serial.println(" MHz");
    Serial.printf("    Flash write mode: %s\n", (ideMode == FM_QIO ? "QIO" : ideMode == FM_QOUT ? "QOUT" : ideMode == FM_DIO ? "DIO" : ideMode == FM_DOUT ? "DOUT" : "UNKNOWN"));

    Serial.printf("__________________________\n\n");

    Serial.println("File system (SPIFFS): ");
    Serial.print("    Total KB: ");
    Serial.print(fileTotalKB);
    Serial.println(" KB");
    Serial.print("    Used KB: ");
    Serial.print(fileUsedKB);
    Serial.println(" KB");
    Serial.printf("    Block size: %lu\n", fs_info.blockSize);
    Serial.printf("    Page size: %lu\n", fs_info.pageSize);
    Serial.printf("    Maximum open files: %lu\n", fs_info.maxOpenFiles);
    Serial.printf("    Maximum path length: %lu\n\n", fs_info.maxPathLength);

    Dir dir = SPIFFS.openDir("/");
    Serial.println("SPIFFS directory {/} :");
    while (dir.next())
    {
        Serial.print("  ");
        Serial.println(dir.fileName());
    }

    Serial.printf("__________________________\n\n");

    Serial.printf("CPU frequency: %u MHz\n\n", ESP.getCpuFreqMHz());
    Serial.print("#####################");
}

void setup(void)
{
    Serial.begin(115200);
    //pinMode(LED_BUILTIN, OUTPUT);
    pinMode(LED_0, OUTPUT);

    sensors.begin();

#ifdef DEBUG
    deviceCount = 5;
#else
    deviceCount = sensors.getDeviceCount();
    Serial.print("DeviceCount=");
    Serial.println(deviceCount);
    try
    {
        for (int j = 0; j < deviceCount; j++)
        {
            if (sensors.getAddress(Thermometer[j], j))
            {
                for (uint8_t i = 0; i < 8; i++)
                {
                    sensor[j][i] = Thermometer[j][i];
                }
            }
        }
    }
    catch (const std::exception &e)
    {
        BlinkNTimes(LED_0, 10, 200);
    }
#endif

    if (init_wifi() == WL_CONNECTED)
    {
        Serial.print("Connected to ");
        Serial.print(ssid);
        Serial.print("--- IP: ");
        Serial.println(WiFi.localIP());
    }
    else
    {
        Serial.print("Error connecting to: ");
        Serial.println(ssid);
    }

    timeClient.begin();

    config_rest_server_routing();

    http_rest_server.begin();
    Serial.println("HTTP REST Server Started");

    PrintDeviceInfo();
}

void loop(void)
{
    http_rest_server.handleClient();
}