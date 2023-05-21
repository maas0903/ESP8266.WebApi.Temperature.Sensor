#include <ESP8266WiFi.h>
#include <stdio.h>
#include <Arduino.h>
#include <credentials.h>
#include <OneWire.h>
#include <DallasTemperature.h>
#include <InfluxDBData.h>

// #define DEBUG

#define WIFI_RETRY_DELAY 500
#define MAX_WIFI_INIT_RETRY 50
#define ONE_WIRE_BUS 2
#define LED_0 0

unsigned long previousMillisWiFi = 0;

OneWire oneWire(ONE_WIRE_BUS);
DallasTemperature sensors(&oneWire);
float tempSensor[5];
DeviceAddress Thermometer[5];
uint8_t sensor[5][8];

String deviceAddress[5] = {"", "", "", "", ""};
byte gpio = 2;
String strTemperature[5] = {"-127", "-127", "-127", "-127", "-127"};

int deviceCount;

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

void init_wifi()
{
    int retries = 0;

    Serial.println("Connecting to WiFi");

    WiFi.begin(ssid, password);
    WiFi.setHostname(hostName.c_str());

    while ((WiFi.status() != WL_CONNECTED) && (retries < MAX_WIFI_INIT_RETRY))
    {
        retries++;
        delay(WIFI_RETRY_DELAY);
        Serial.print("#");
    }
    Serial.println();

    if (WiFi.status() == WL_CONNECTED)
    {
        Serial.print("Connected to ");
        Serial.print(ssid);
        Serial.print("--- IP: ");
        Serial.println(WiFi.localIP());
        // BlinkNTimes(LED_0, 3, 500);
    }
    else
    {
        Serial.print("Error connecting to: ");
        Serial.println(ssid);
    }
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

    try
    {
#ifdef DEBUG
#else
#endif
        if (deviceCount == 0)
        {
            Serial.print("No Content");
            String sHostName(WiFi.hostname());
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

            for (int i = 0; i < deviceCount; i++)
            {
                Serial.println(hostName + String(i) + " = " + String(tempSensor[i]));

                InfluxDBData influxDBData("192.168.63.28:8086",
                                          INFLUXDB_TOKEN,
                                          "huis",
                                          "huis",
                                          WiFi.hostname(),
                                          "0",
                                          "temperature",
                                          (String)tempSensor[i]);
                int returnCode = influxDBData.PutData();
                Serial.println("ReturnCode=" + String(returnCode));
            }
        }
    }
    catch (const std::exception &e)
    {
    }
}

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
    getDevices();
#endif

    init_wifi();
}

void loop(void)
{
    if (deviceCount == 0)
    {
        Serial.print("Devices(s) not found - getting devices");
        getDevices();
        delay(5000);
    }
    else
    {
        unsigned long currentMillis = millis();
        if (currentMillis - previousMillisWiFi >= 15 * 1000)
        {
            get_temps();
            previousMillisWiFi = currentMillis;
            Serial.print(F("Wifi is still connected with IP: "));
            Serial.println(WiFi.localIP()); // inform user about his IP address
        }
    }
}