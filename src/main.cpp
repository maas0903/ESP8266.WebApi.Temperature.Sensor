#include <ESP8266WiFi.h>
//#include "AnotherIFTTTWebhook.h"
#include <stdio.h>
#include <ArduinoJson.h>
#include <Arduino.h>
#include <credentials.h>
#include <OneWire.h>
#include <DallasTemperature.h>
#include <ESP8266WebServer.h>
#include <InfluxDbClient.h>

//#define IFTTT_Event "brander_toggle"

//#define DEBUG

#define URI "/temps"

#define HTTP_REST_PORT 80
#define WIFI_RETRY_DELAY 500
#define MAX_WIFI_INIT_RETRY 50
#define ONE_WIRE_BUS 2
#define LED_0 0

#define INFLUXDB_URL "http://192.168.63.28:8086"
#define INFLUXDB_ORG "huis"
#define INFLUXDB_BUCKET "huis"

InfluxDBClient influxDbClient(INFLUXDB_URL, INFLUXDB_ORG, INFLUXDB_BUCKET, INFLUXDB_TOKEN);

unsigned long previousMillisWiFi = 0;

OneWire oneWire(ONE_WIRE_BUS);
DallasTemperature sensors(&oneWire);
float tempSensor[5];
DeviceAddress Thermometer[5];
uint8_t jsonSensor[5][8];

String deviceAddress[5] = {"", "", "", "", ""};
byte gpio = 2;
String strTemperature[5] = {"-127", "-127", "-127", "-127", "-127"};

int deviceCount;

ESP8266WebServer http_rest_server(HTTP_REST_PORT);

void BlinkNTimes(int pin, int blinks, unsigned long millies, String fromStr)
{
    digitalWrite(pin, LOW);
    for (int i = 0; i < blinks; i++)
    {
        digitalWrite(pin, HIGH);
        delay(millies);
        digitalWrite(pin, LOW);
        delay(millies);
    }
    Serial.println("BlinkNTimes from " + fromStr);
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
    BlinkNTimes(LED_0, 3, 500, "init_wifi");
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
    // BlinkNTimes(LED_0, 2, 500);
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
            Serial.print("No Content");
            // http_rest_server.send(204);
            // CORS
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
                tempSensor[i] = sensors.getTempC(jsonSensor[i]);
                deviceAddress[i] = GetAddressToString(Thermometer[i]);
#endif
                strTemperature[i] = (String)tempSensor[i];
                Serial.print(strTemperature[i] + " ");
            }
            Serial.println();

            JsonArray sensors = jsonObj.createNestedArray("Sensors");
            for (int i = 0; i < deviceCount; i++)
            {
                JsonObject jsonSensor = sensors.createNestedObject();
                jsonSensor["Id"] = deviceAddress[i];
                jsonSensor["ValueType"] = "Temperature";
                jsonSensor["Value"] = strTemperature[i];

                Serial.print("DeviceId=");
                Serial.println(deviceAddress[i]);
                Serial.print("Temp=");
                Serial.println(strTemperature[i]);

                Point influxDbSensor(hostName + String(i));
                influxDbSensor.clearFields();
                influxDbSensor.addField("ipaddress", WiFi.localIP().toString());
                influxDbSensor.addField("mac-address", WiFi.macAddress());
                influxDbSensor.addField("temperature", tempSensor[i]);
                Serial.println(influxDbClient.pointToLineProtocol(influxDbSensor));

                if (!influxDbClient.writePoint(influxDbSensor))
                {
                    Serial.print("InfluxDB write failed: ");
                    Serial.println(influxDbClient.getLastErrorMessage());
                }
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
                    jsonSensor[j][i] = Thermometer[j][i];
                }
            }
        }
    }
    catch (const std::exception &e)
    {
        BlinkNTimes(LED_0, 10, 200, "getDevices - exception");
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

    if (init_wifi() == WL_CONNECTED)
    {
        Serial.print("Connected to ");
        Serial.print(ssid);
        Serial.print("--- IP: ");
        Serial.println(WiFi.localIP());
        String str = "ESP8266 Webserver started on " + hostName;
        // char *cstr = &str[0];
        // send_webhook(IFTTT_Event, IFTTT_Key, cstr, "", "");
        // Serial.println("Webhook sent");
    }
    else
    {
        Serial.print("Error connecting to: ");
        Serial.println(ssid);
    }

    digitalWrite(LED_0, HIGH);

    config_rest_server_routing();

    http_rest_server.begin();
    Serial.println("HTTP REST Server Started");

    // PrintDeviceInfo();
}

void loop(void)
{
    if (deviceCount == 0)
    {
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
    http_rest_server.handleClient();
}