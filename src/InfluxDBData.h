#ifndef InfluxDBData_h
#define InfluxDBData_h
#include "Arduino.h"
class InfluxDBData
{
private:
    String _host;
    String _token;
    String _organization;
    String _bucket;
    String _hostName;
    String _deviceIndex;
    String _metric;
    String _value;

public:
    InfluxDBData(
        String Host,
        String Token,
        String Organization,
        String Bucket,
        String HostName,
        String DeviceIndex,
        String Metric,
        String Value);
    int PutData();
};
#endif
