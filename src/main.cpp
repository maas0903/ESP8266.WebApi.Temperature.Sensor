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
#define MAX_SENSORS 5
#define POLLING_INTERVAL_MS 15000
#define MAX_DEVICE_RETRY 5
#define DEVICE_INIT_DELAY_MS 1000
#define NO_DEVICE_RETRY_DELAY_MS 5000
#define MAX_INFLUX_RETRY 3
#define INFLUX_RETRY_BASE_DELAY_MS 1000

int deviceCount;
bool forceMultipleDevices = true;

unsigned long previousMillisWiFi = 0;

// Status tracking for health check
struct SystemStatus {
    bool wifiConnected = false;
    bool sensorsOk = false;
    bool influxDbOk = false;
    String lastError = "";
    int lastInfluxReturnCode = 0;
    unsigned long lastSuccessfulRead = 0;
    unsigned long lastSuccessfulWrite = 0;
} systemStatus;

IPAddress localIP(staticIP);
IPAddress gateway(staticGateway);
IPAddress subnet(staticSubnet);
IPAddress primaryDNS(dns);
IPAddress secondaryDNS(dnsGoogle);

OneWire oneWire(ONE_WIRE_BUS);
DallasTemperature sensors(&oneWire);
float tempSensor[MAX_SENSORS];
DeviceAddress Thermometer[MAX_SENSORS];
uint8_t sensor[MAX_SENSORS][8];

String deviceAddress[MAX_SENSORS] = {"", "", "", "", ""};
byte gpio = 2;
String strTemperature[MAX_SENSORS] = {"-127", "-127", "-127", "-127", "-127"};

WiFiServer server(80);

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

    if (!WiFi.config(localIP, gateway, subnet, primaryDNS, secondaryDNS))
    {
        Serial.println("Failed to configure Static IP");
    }

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
        systemStatus.wifiConnected = true;
    }
    else
    {
        Serial.print("Error connecting to: ");
        Serial.println(ssid);
        systemStatus.wifiConnected = false;
        systemStatus.lastError = "WiFi connection failed";
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

int sendToInfluxDB(InfluxDBData &influxDBData)
{
    int returnCode = -1;
    int retryCount = 0;

    while (retryCount < MAX_INFLUX_RETRY)
    {
        returnCode = influxDBData.PutData();

        if (returnCode >= 200 && returnCode < 300)
        {
            // Success
            systemStatus.influxDbOk = true;
            systemStatus.lastInfluxReturnCode = returnCode;
            systemStatus.lastSuccessfulWrite = millis();
            Serial.println("ReturnCode=" + String(returnCode) + " (Success)");
            return returnCode;
        }

        retryCount++;
        if (retryCount < MAX_INFLUX_RETRY)
        {
            // Exponential backoff: 1s, 2s, 4s, etc.
            unsigned long delayTime = INFLUX_RETRY_BASE_DELAY_MS * (1 << (retryCount - 1));
            Serial.println("ReturnCode=" + String(returnCode) + " - Retry " + String(retryCount) + "/" + String(MAX_INFLUX_RETRY) + " in " + String(delayTime) + "ms");
            delay(delayTime);
        }
    }

    // All retries failed
    systemStatus.influxDbOk = false;
    systemStatus.lastInfluxReturnCode = returnCode;
    systemStatus.lastError = "InfluxDB write failed after " + String(MAX_INFLUX_RETRY) + " retries";

    if (returnCode > 0)
    {
        Serial.println("ReturnCode=" + String(returnCode) + " (HTTP Error - All retries exhausted)");
    }
    else
    {
        Serial.println("ReturnCode=" + String(returnCode) + " (Connection Failed - All retries exhausted)");
    }

    return returnCode;
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
            systemStatus.sensorsOk = false;
            systemStatus.lastError = "No sensors detected";
        }
        else
        {
            sensors.requestTemperatures();
            systemStatus.sensorsOk = true;
            systemStatus.lastSuccessfulRead = millis();

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

                InfluxDBData influxDBData(INFLUXDB_HOST,
                                          INFLUXDB_TOKEN,
                                          INFLUXDB_ORG,
                                          INFLUXDB_BUCKET,
                                          WiFi.hostname(),
                                          (String)i,
                                          "temperature",
                                          (String)tempSensor[i]);

                sendToInfluxDB(influxDBData);
                Serial.println();
            }
        }
    }
    catch (const std::exception &e)
    {
        Serial.print("Exception in get_temps: ");
        Serial.println(e.what());
        systemStatus.sensorsOk = false;
        systemStatus.lastError = "Exception: " + String(e.what());
        BlinkNTimes(LED_0, 5, 200);
    }
}

void getDevices()
{
    sensors.begin();
    delay(DEVICE_INIT_DELAY_MS);
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
        Serial.print("Exception in getDevices: ");
        Serial.println(e.what());
        systemStatus.sensorsOk = false;
        systemStatus.lastError = "getDevices exception: " + String(e.what());
        BlinkNTimes(LED_0, 10, 200);
    }
}

void handleWebRequest()
{
    WiFiClient client = server.available();
    if (!client)
    {
        return;
    }

    Serial.println("New client request");

    // Wait for data from client
    unsigned long timeout = millis() + 1000;
    while (!client.available() && millis() < timeout)
    {
        delay(1);
    }

    if (!client.available())
    {
        client.stop();
        return;
    }

    // Read the first line of HTTP request
    String request = client.readStringUntil('\r');
    Serial.print("Request: ");
    Serial.println(request);
    client.flush();

    // Extract path from request (between "GET " and " HTTP")
    String path = "";
    int start = request.indexOf("GET ");
    if (start >= 0)
    {
        start += 4; // Length of "GET "
        int end = request.indexOf(" HTTP", start);
        if (end > start)
        {
            path = request.substring(start, end);
        }
    }
    Serial.print("Path: ");
    Serial.println(path);

    // Prepare the response
    String response;
    String contentType = "text/plain";

    // Parse request path - check most specific first
    if (path == "/health")
    {
        Serial.println("Handling /health");
        // Health check endpoint
        contentType = "application/json";
        response = "{";
        response += "\"status\":\"" + String((systemStatus.wifiConnected && systemStatus.sensorsOk) ? "healthy" : "unhealthy") + "\",";
        response += "\"wifi\":\"" + String(systemStatus.wifiConnected ? "connected" : "disconnected") + "\",";
        response += "\"sensors\":\"" + String(systemStatus.sensorsOk ? "ok" : "error") + "\",";
        response += "\"sensorCount\":" + String(deviceCount) + ",";
        response += "\"influxDb\":\"" + String(systemStatus.influxDbOk ? "ok" : "error") + "\",";
        response += "\"lastInfluxCode\":" + String(systemStatus.lastInfluxReturnCode) + ",";
        response += "\"lastError\":\"" + systemStatus.lastError + "\",";
        response += "\"uptime\":" + String(millis() / 1000) + ",";
        response += "\"lastRead\":" + String(systemStatus.lastSuccessfulRead / 1000) + ",";
        response += "\"lastWrite\":" + String(systemStatus.lastSuccessfulWrite / 1000);
        response += "}";
        Serial.print("Response length: ");
        Serial.println(response.length());
    }
    else if (path == "/temps")
    {
        Serial.println("Handling /temps");
        // Temperature endpoint
        contentType = "application/json";
        response = "{\"temperatures\":[";
        for (int i = 0; i < deviceCount; i++)
        {
            if (i > 0) response += ",";
            response += "{";
            response += "\"index\":" + String(i) + ",";
            response += "\"address\":\"" + deviceAddress[i] + "\",";
            response += "\"value\":" + String(tempSensor[i]);
            response += "}";
        }
        response += "],\"count\":" + String(deviceCount) + "}";
        Serial.print("Response length: ");
        Serial.println(response.length());
    }
    else if (path == "/" || path == "")
    {
        Serial.println("Handling /");
        // Root endpoint
        contentType = "text/html";
        response = "<html><body><h1>" + hostName + " Temperature Sensor</h1>";
        response += "<p>Status: " + String((systemStatus.wifiConnected && systemStatus.sensorsOk) ? "OK" : "ERROR") + "</p>";
        response += "<p>Sensors: " + String(deviceCount) + "</p>";
        response += "<h2>Endpoints:</h2>";
        response += "<ul>";
        response += "<li><a href='/health'>/health</a> - Health check (JSON)</li>";
        response += "<li><a href='/temps'>/temps</a> - Temperature readings (JSON)</li>";
        response += "</ul></body></html>";
    }
    else
    {
        Serial.print("404 - Unknown path: ");
        Serial.println(path);
        // 404 Not Found
        client.println("HTTP/1.1 404 Not Found");
        client.println("Connection: close");
        client.println();
        client.stop();
        return;
    }

    // Send HTTP response
    Serial.println("Sending response...");
    client.println("HTTP/1.1 200 OK");
    client.println("Content-Type: " + contentType);
    client.println("Content-Length: " + String(response.length()));
    client.println("Connection: close");
    client.println();
    client.print(response);
    client.flush();

    delay(10);
    client.stop();
    Serial.println("Client disconnected");
}

void setup(void)
{
    forceMultipleDevices = false;

    Serial.begin(115200);
    // pinMode(LED_BUILTIN, OUTPUT);
    pinMode(LED_0, OUTPUT);

#ifdef DEBUG
    deviceCount = 5;
#else
    getDevices();
    if (forceMultipleDevices)
    {
        int cnt = 0;
        while ((deviceCount == 1) && (cnt < MAX_DEVICE_RETRY))
        {
            getDevices();
            cnt++;
        }
    }

#endif

    init_wifi();

    server.begin();
}

void loop(void)
{
    // Update WiFi status
    systemStatus.wifiConnected = (WiFi.status() == WL_CONNECTED);

    // Handle incoming web requests
    handleWebRequest();

    if (deviceCount == 0)
    {
        Serial.print("Devices(s) not found - getting devices");
        getDevices();
        delay(NO_DEVICE_RETRY_DELAY_MS);
    }
    else
    {
        unsigned long currentMillis = millis();
        if (currentMillis - previousMillisWiFi >= POLLING_INTERVAL_MS)
        {
            get_temps();
            previousMillisWiFi = currentMillis;
            Serial.print(F("Wifi is still connected with IP: "));
            Serial.println(WiFi.localIP()); // inform user about his IP address
        }
    }
}