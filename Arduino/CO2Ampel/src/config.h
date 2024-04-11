// #define USE_LORAWAN
#define USE_SIM7080

const char apn[] = "hub.telefonica.de";
const char gprsUser[] = "";
const char gprsPass[] = "";

//#define USE_SSL 1
const char server[]   = "10.3.0.20";
const int port = 8080;
//curl -v -X POST http://192.168.100.240:8080/api/v1/lNRAqgOcVSSI10M8LzQ1/telemetry --header Content-Type:application/json --data "{temperature:25}"
const char resource[] = "/api/v1/lNRAqgOcVSSI10M8LzQ1/telemetry";

#define BAUD_RATE 9600


//13.000 g/m^3
#define ABSOLUTE_HUMIDITY 13000


#define TH_LOW 700
#define TH_HIGH 2700

// 15000 ms between measurements
#define REFRESH_RATE 15000
