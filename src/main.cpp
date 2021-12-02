#include <WiFi.h>
#include <WebServer.h>

#include <stdio.h>
#include <ArduinoJson.h>
#include <Arduino.h>

#include <credentials.h>
#include <OTA.h>

#include <OneWire.h>
#include <DallasTemperature.h>

#include <driver/dac.h>

//#include "LittleFS.h"

//#define IFTTT_Event "brander_toggle"

//#define DEBUG

#define URI "/temps"

#define HTTP_REST_PORT 80
#define WIFI_RETRY_DELAY 500
#define MAX_WIFI_INIT_RETRY 50
#define ONE_WIRE_BUS 4
#define LED_0 2

OneWire oneWire(ONE_WIRE_BUS);
DallasTemperature sensors(&oneWire);
float tempSensor[5];
DeviceAddress Thermometer[5];
uint8_t sensor[5][8];

String deviceAddress[5] = {"", "", "", "", ""};
byte gpio = 2;
String strTemperature[5] = {"-127", "-127", "-127", "-127", "-127"};

int deviceCount;

WebServer http_rest_server(HTTP_REST_PORT);

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
    StaticJsonDocument<1024> jsonObj;

    try
    {
#ifdef DEBUG
        jsonObj["DEBUG"] = "******* true *******";
#else
        jsonObj["DEBUG"] = "false";
#endif
        jsonObj["Hostname"] = hostName;
        jsonObj["IpAddress"] = WiFi.localIP().toString();
        jsonObj["MacAddress"] = WiFi.macAddress();
        jsonObj["Gpio"] = gpio;
        jsonObj["DeviceType"] = "OneWire_Temp";
        jsonObj["DeviceCount"] = deviceCount;

        if (deviceCount == 0)
        {
            Serial.println("No Content");
            // http_rest_server.send(204);
            // CORS
            http_rest_server.sendHeader("Access-Control-Allow-Origin", "*");
            Serial.println("sendHeader(´Access-Control-Allow-Origin´, ´*´);");
            String sHostName = /*WiFi.hostname()*/ "wroom";

            http_rest_server.send(200, "text/html", "No devices found on " + sHostName + " (" + WiFi.macAddress() + ")");
            // http_rest_server.send(200, "text/html", "No devices found");
            Serial.println("send(200, ...");
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

            JsonArray sensors = jsonObj.createNestedArray("Sensors");
            for (int i = 0; i < deviceCount; i++)
            {
                JsonObject sensor = sensors.createNestedObject();
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
        // std::cerr << e.what() << '\n';
    }

    String jSONmessageBuffer;
    serializeJsonPretty(jsonObj, jSONmessageBuffer);

    // jsonObj.prettyPrintTo(JSONmessageBuffer, sizeof(JSONmessageBuffer));

    http_rest_server.sendHeader("Access-Control-Allow-Origin", "*");
    http_rest_server.sendHeader("Access-Control-Allow-Headers", "Origin, X-Requested-With, Content-Type, Accept");

    http_rest_server.send(200, "application/json", jSONmessageBuffer);
}

void config_rest_server_routing()
{
    http_rest_server.on("/", HTTP_GET, []()
                        { http_rest_server.send(200, "text/html", "Welcome to the ESP8266 REST Web Server: " + hostName); });
    http_rest_server.on(URI, HTTP_GET, get_temps);
}

/*
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
    Serial.printf("    Flash write mode: %s\n", (ideMode == FM_QIO ? "QIO" : ideMode == FM_QOUT ? "QOUT"
                                                                         : ideMode == FM_DIO    ? "DIO"
                                                                         : ideMode == FM_DOUT   ? "DOUT"
                                                                                                : "UNKNOWN"));

    Serial.printf("__________________________\n\n");

    Serial.println("File system (SPIFFS): ");
    Serial.print("    Total KB: ");
    Serial.print(fileTotalKB);
    Serial.println(" KB");
    Serial.print("    Used KB: ");
    Serial.print(fileUsedKB);
    Serial.println(" KB");
    Serial.print("    Block size: ");
    Serial.println(fs_info.blockSize);
    Serial.print("    Page size: ");
    Serial.println(fs_info.pageSize);
    Serial.print("    Maximum open files: ");
    Serial.println(fs_info.maxOpenFiles);
    Serial.print("    Maximum path length: ");
    Serial.println(fs_info.maxPathLength);
    Serial.println();

    //Dir dir = SPIFFS.openDir("/");
    //Serial.println("SPIFFS directory {/} :");
    //while (dir.next())
    //{
    //    Serial.print("  ");
    //    Serial.println(dir.fileName());
    //}

    Serial.printf("__________________________\n\n");

    Serial.printf("CPU frequency: %u MHz\n\n", ESP.getCpuFreqMHz());
    Serial.print("#####################");
}
*/
void getDevices()
{
    sensors.begin();
    delay(1000);
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
}

void setup(void)
{
    Serial.begin(115200);
    // pinMode(LED_BUILTIN, OUTPUT);
    pinMode(LED_0, OUTPUT);

#ifdef DEBUG
    deviceCount = 5;
#else
    //getDevices();
#endif

    /*
    if (init_wifi() == WL_CONNECTED)
    {
        Serial.print("Connected to ");
        Serial.print(ssid);
        Serial.print("--- IP: ");
        Serial.println(WiFi.localIP());
        String str = "ESP8266 Webserver started on "+hostName;
        //char *cstr = &str[0];
        //send_webhook(IFTTT_Event, IFTTT_Key, cstr, "", "");
        //Serial.println("Webhook sent");
    }
    else
    {
        Serial.print("Error connecting to: ");
        Serial.println(ssid);
    }
    */

    setupOTA("TemplateSketch", ssid, password);

    config_rest_server_routing();

    http_rest_server.begin();
    Serial.println("HTTP REST Server Started");

    dac_output_enable(DAC_CHANNEL_1);
    dac_output_voltage(DAC_CHANNEL_1, 200);

    touch_pad_init();

    // PrintDeviceInfo();
}

void loop(void)
{
    ArduinoOTA.handle();
    if (deviceCount == 0)
    {
        getDevices();
        delay(5000);
    }
    http_rest_server.handleClient();

    int touchValue = 0;
    
    for (int i = 0; i < 100; i++)
    {
        touchValue += touchRead(15);
    }
    
    touchValue = touchValue / 100;
    
    //Serial.println(touchValue);
    
    if (touchValue < 10)
    {
        Serial.print("touched - value = ");
        Serial.println(touchValue);
        BlinkNTimes(LED_0, 1, 1000);
    }
    else
    {
        delay(500);
    }
}