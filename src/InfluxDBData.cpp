#include "Arduino.h"
#include <ESP8266HTTPClient.h>
#include <ESP8266WiFi.h>

#include "InfluxDBData.h"

InfluxDBData::InfluxDBData(
    String Host,
    String Token,
    String Organization,
    String Bucket,
    String HostName,
    String DeviceIndex,
    String Metric,
    String Value)
{
    _host = Host;
    _token = Token;
    _organization = Organization;
    _bucket = Bucket;
    _hostName = HostName;
    _deviceIndex = DeviceIndex;
    _metric = Metric;
    _value = Value;
}

int InfluxDBData::PutData()
{
    String Url = "http://" + _host + "/write?db=" + _organization + "&precision=ms";

    Serial.println("URL=" + Url);

    String Body = _bucket +
                  ",host-name=" + _hostName +
                  ",device-index=" + _deviceIndex +
                  " " + _metric + "=" + _value;

    Serial.println("Body=" + Body);

    WiFiClient client;
    HTTPClient http;

    http.begin(client, Url);
    http.addHeader("Content-Type", "text/plain");
    http.addHeader("Authorization", "Token " + _token);

    int httpCode = http.POST(Body);
    http.end();

    return httpCode;
}