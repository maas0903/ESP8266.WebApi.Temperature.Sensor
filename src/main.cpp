#include <stdio.h>
#include <ArduinoJson.h>
#include <Arduino.h>

#include <credentials.h>

#include <WiFiUdp.h>

#include <ESP8266WiFi.h>
#include <ESP8266WebServer.h>

int period = 60000;

//#define DEBUG
IPAddress staticIP(192, 168, 63, 59);
#define URI "/status"
IPAddress gateway(192, 168, 63, 1);
IPAddress subnet(255, 255, 255, 0);
IPAddress dnsGoogle(8, 8, 8, 8);
String hostName = "brander";
const char *propertyHost = "pastei05.local";
const char *propertyHost2 = "maiden.pagekite.me";

int hourOfDay;
int dayOfMonth;
int hourOn = 8;
int durationOn = 1;
int override = 0;
int overrideHour = 0;
int summer = 0;
int hourOff = 23;
int degreesOff = 22;
int degreesOn;
int histeresis = 2; //maybe set as a property
int degrees = -177;
int absoluteMin = 10;

unsigned long time_now = 0;

String statusStr = "";

#define HTTP_REST_PORT 80
#define WIFI_RETRY_DELAY 500
#define MAX_WIFI_INIT_RETRY 50
#define RELAY_BUS 0
//#define LED_1 1
#define ONE_WIRE_BUS 2

bool relayOn = false;
bool goingUp = true;
bool switching = false;
bool defaults;

ESP8266WebServer http_rest_server(HTTP_REST_PORT);

void SPrintLn(String message)
{
#ifdef DEBUG
    Serial.println(message);
//#else
#endif
}

void SPrintLn(int message)
{
#ifdef DEBUG
    Serial.println(message);
//#else
#endif
}

void SPrint(String message)
{
#ifdef DEBUG
    Serial.print(message);
//#else
#endif
}

void charToStringL(const char S[], String &D)
{
    byte at = 0;
    const char *p = S;
    D = "";

    while (*p++)
    {
        D.concat(S[at++]);
    }
}

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

    SPrintLn("Connecting to WiFi");

    WiFi.config(staticIP, gateway, subnet, dnsGoogle);
    WiFi.mode(WIFI_STA);
    WiFi.hostname(hostName);
    WiFi.begin(ssid, password);

    while ((WiFi.status() != WL_CONNECTED) && (retries < MAX_WIFI_INIT_RETRY))
    {
        retries++;
        delay(WIFI_RETRY_DELAY);
        Serial.print("#");
    }
    Serial.println("connected");
    BlinkNTimes(LED_BUILTIN, 3, 500);
    return WiFi.status();
}

void set_defaults()
{
    SPrintLn("Setting defaults");
    defaults = true;
    durationOn = 1;
    hourOn = 21;
    override = 0;

    summer = 1;
    hourOff = 23;
    degreesOff = 22;
    absoluteMin = 10;
    degreesOn = degreesOff - histeresis;
    //to be sure it's off
    degrees = 1000;
}

String GetResultFromPropertHost(const char *host, uint16_t port)
{
    String url = "/MelektroApi/getbrandersettings2";
    WiFiClient client;
    String response = "";
    if (client.connect(host, 80))
    {
        //Serial.println("connected to property server");
        delay(200);
        client.print(String("GET " + url) + " HTTP/1.1\r\n" +
                     "Host: " + host + "\r\n" +
                     "Connection: close\r\n" +
                     "\r\n");
        //Serial.println("printed to property server");

        while (client.connected() || client.available())
        {
            delay(200);
            if (client.available())
            {
                //Serial.println("property server available - reading line");
                String line = client.readStringUntil('\n');
                delay(200);
                //Serial.println("property server available - line read");
                if (line.indexOf("hourOn") > 0)
                {
                    Serial.print("Connected to host ");
                    Serial.print(host);
                    Serial.println(" and getting line");
                    response = line;
                    Serial.println("line=" + line);
                }
            }
        }
        client.stop();
    }
    else
    {
        SPrint("not connected to ");
        SPrintLn(host);
    }

    return response;
}

boolean GetProperties()
{
    SPrintLn("Getting properties");
    boolean result = false;
    defaults = false;
    String response = GetResultFromPropertHost(propertyHost, 80);
    if (response == "")
    {
        SPrintLn("response is empty - attempting second propertyHost");
        response = GetResultFromPropertHost(propertyHost2, 80);
        if (response == "")
        {
            SPrintLn("response is empty");
            set_defaults();
        }
    }

    if (response != "")
    {
        SPrintLn("attempting deserialisation of response");
        StaticJsonBuffer<800> doc;
        if (response.length() > 0 && response.indexOf("hourOn") > 0)
        {
            JsonObject &root = doc.parseObject(response);
            SPrintLn("response parsed");
            if (!root.success())
            {
                SPrintLn(" but root not successfull");
                set_defaults();
                SPrintLn("deserializeJson failed, result from property server not valid - set defaults");
            }
            else
            {
                SPrintLn("getting properties from result object");
                const char *tempPtr;
                tempPtr = root["hourOn"];
                String tempStr;
                charToStringL(tempPtr, tempStr);
                hourOn = tempStr.toInt();
                SPrint("hourOn=");
                SPrintLn(hourOn);

                tempPtr = root["durationOn"];
                charToStringL(tempPtr, tempStr);
                durationOn = tempStr.toInt();
                SPrint("durationOn=");
                SPrintLn(durationOn);

                tempPtr = root["override"];
                charToStringL(tempPtr, tempStr);
                override = tempStr.toInt();
                SPrint("override=");
                SPrintLn(override);

                tempPtr = root["summer"];
                charToStringL(tempPtr, tempStr);
                summer = tempStr.toInt();
                SPrint("summer=");
                SPrintLn(summer);

                tempPtr = root["hourOff"];
                charToStringL(tempPtr, tempStr);
                hourOff = tempStr.toInt();
                SPrint("hourOff=");
                SPrintLn(hourOff);

                tempPtr = root["degreesOff"];
                charToStringL(tempPtr, tempStr);
                degreesOff = tempStr.toInt();
                SPrint("degreesOff=");
                SPrintLn(degreesOff);

                tempPtr = root["absoluteMin"];
                charToStringL(tempPtr, tempStr);
                absoluteMin = tempStr.toInt();
                SPrint("absoluteMin=");
                SPrintLn(absoluteMin);

                degreesOn = degreesOff - histeresis;
                SPrint("degreesOn=");
                SPrintLn(degreesOn);

                tempPtr = root["degrees"];
                charToStringL(tempPtr, tempStr);
                degrees = tempStr.toInt();
                SPrint("degrees=");
                SPrintLn(degrees);
                result = true;

                tempPtr = root["hourOfDay"];
                charToStringL(tempPtr, tempStr);
                hourOfDay = tempStr.toInt();
                SPrint("hourOfDay=");
                SPrintLn(hourOfDay);
                result = true;

                tempPtr = root["dayOfMonth"];
                charToStringL(tempPtr, tempStr);
                dayOfMonth = tempStr.toInt();
                // SPrint("dayOfMonth=");
                // SPrintLn(dayOfMonth);
                Serial.print("dayOfMonth=");
                Serial.print(dayOfMonth);
                result = true;
            }
        }
        else
        {
            SPrintLn("result from property server not valid - set defaults");
            set_defaults();
        }
    }

    return result;
}

void doSwitch(bool switcher)
{
    if (switcher)
    {
        relayOn = true;
        digitalWrite(RELAY_BUS, LOW);
    }
    else
    {
        relayOn = false;
        digitalWrite(RELAY_BUS, HIGH);
    }
    goingUp = !relayOn;
}

void doSwitch()
{
    statusStr = "";

    if (summer == 1)
    {
        statusStr = "summer - ";
        if (override == 1 && !relayOn)
        {
            doSwitch(true);
            overrideHour = hourOfDay + 1;
            statusStr += "override is on, relay is off -> switching on ";
        }

        if ((override == 0 && digitalRead(RELAY_BUS) == LOW) ||
            (override == 1 && relayOn && hourOfDay > overrideHour))
        {
            doSwitch(false);
            override = 0;
            overrideHour = 0;
            statusStr += "passed on-time -> switching off ";
        }

        if ((dayOfMonth % 2 == 0 || dayOfMonth == 31) && hourOfDay >= hourOn && hourOfDay < hourOn + durationOn && !relayOn)
        {
            doSwitch(true);
            statusStr += "normal switching on ";
        }

        if (hourOfDay > hourOn + durationOn && digitalRead(RELAY_BUS) == LOW)
        {
            doSwitch(false);
            statusStr += "normal switching off ";
        }
    }
    else //winter
    {
        statusStr = "winter - ";
        if (degrees <= absoluteMin)
        {
            statusStr += String(degrees) + " is equal or less than " + String(absoluteMin) + " - set override on ";
            override = 1;
        }

        //Hysteresis
        if ((hourOfDay > hourOn && hourOfDay < hourOff) || (override == 1))
        {
            //handle temperature histeresis
            if (goingUp)
            {
                if (degrees >= degreesOff)
                {
                    goingUp = false;
                    statusStr += "Phase change from going up to going down - " + String(degrees) + " >= " + String(degreesOff);
                }
                else
                {
                    if (digitalRead(RELAY_BUS) == HIGH)
                    {
                        //swith warming on
                        statusStr += "* switching on * - ";
                        doSwitch(true);
                    }
                    statusStr += "going up - " + String(degrees) + " < (Upper) " + String(degreesOff) + " " + hostName + " is ON";
                }
            }
            else
            {
                if (degrees <= degreesOn)
                {
                    statusStr += "Phase change from going down to going up - " + String(degrees) + " <= " + String(degreesOn);
                    goingUp = true;
                }
                else
                {
                    if (digitalRead(RELAY_BUS) == LOW)
                    {
                        //swith warming off
                        statusStr += "* switching off * - ";
                        doSwitch(false);
                    }

                    statusStr += "going down - " + String(degrees) + " > (Lower)" + String(degreesOn) + " " + hostName + " is OFF ";
                }
            }
        }
        else
        {
            statusStr += "sleeping - switching off - hourOfDay (" + (String)hourOfDay + "), hourOff(" + (String)hourOff + "), hourOn(" + (String)hourOn + ")";
            if (digitalRead(RELAY_BUS) == LOW)
            {
                doSwitch(false);
            }
        }
    }

    Serial.println(statusStr);
    Serial.println();

    switching = false;
}

void get_status()
{
    GetProperties();
    if (!switching)
    {
        SPrintLn("calling doSwitch");
        doSwitch();
    }
    else
    {
        SPrintLn("switching is true! ?????????????????????????");
    }

    BlinkNTimes(LED_BUILTIN, 2, 500);
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
        jsonObj["Hostname"] = hostName;
        jsonObj["IpAddress"] = WiFi.localIP().toString();
        jsonObj["MacAddress"] = WiFi.macAddress();
        jsonObj["Gpio_Relay"] = RELAY_BUS;
        jsonObj["DeviceType"] = "Relay";
        jsonObj["Status"] = relayOn;
        jsonObj["GPIOPin Status"] = digitalRead(RELAY_BUS);
        jsonObj["summer"] = summer;
        jsonObj["hourOn"] = hourOn;
        if (summer == 1)
        {
            jsonObj["durationOn"] = durationOn;
        }
        else
        {
            jsonObj["degreesOff"] = degreesOff;
            jsonObj["degreesOn"] = degreesOn;
            jsonObj["histeresis"] = histeresis;
            jsonObj["degrees"] = degrees;
            jsonObj["hourOff"] = hourOff;
            jsonObj["absoluteMin"] = absoluteMin;
        }
        jsonObj["override"] = override;
        jsonObj["dayOfMonth"] = dayOfMonth;
        jsonObj["hourOfDay"] = hourOfDay;
        if (defaults)
        {
            statusStr += "******** DEFAULT SETTINGS ********";
        }

        jsonObj["status"] = statusStr;
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
                              "Welcome to the ESP8266 REST Web Server: ");
    });
    http_rest_server.on(URI, HTTP_GET, get_status);
}

void setup(void)
{
    Serial.begin(115200);
    pinMode(RELAY_BUS, OUTPUT);
    digitalWrite(RELAY_BUS, HIGH);

    set_defaults();
    relayOn = false;
    if (init_wifi() == WL_CONNECTED)
    {
        Serial.print("Connected to ");
        Serial.print(ssid);
        Serial.print("--- IP: ");
        Serial.println(WiFi.localIP());

        config_rest_server_routing();

        http_rest_server.begin();
        Serial.println("HTTP REST Server Started");

        GetProperties();
    }
    else
    {
        Serial.print("Error connecting to: ");
        Serial.println(ssid);
    }
}

void loop(void)
{
    if (millis() > time_now + period)
    {
        if (!switching)
        {
            get_status();
        }
        time_now = millis();
    }

    http_rest_server.handleClient();
}