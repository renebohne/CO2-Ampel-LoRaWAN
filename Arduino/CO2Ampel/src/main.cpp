#include <Arduino.h>
#include "config.h"
#include "SPI.h"
#include <SoftwareSerial.h>

#include "Free_Fonts.h" // Include the header file attached to this sketch
#include "TFT_eSPI.h"

#include "sensirion_common.h"
#include "sgp30.h"



TFT_eSPI tft = TFT_eSPI();

#ifdef USE_SIM7080
#define SIMPLE_NB_MODEM_SIM7080
#define SIMPLE_NB_DEBUG SerialUSB

#include <SimpleNBClient.h>

// rx,tx,invert
SoftwareSerial SerialAT(BCM27, BCM22, false);

SimpleNB modem(SerialAT);

#endif

#ifdef USE_LORAWAN
#include "disk91_LoRaE5.h"
Disk91_LoRaE5 lorae5(&Serial);

#define Frequency DSKLORAE5_ZONE_EU868

char deveui[] = "70B3D57ED0055289";
char appeui[] = "0000000000000000";
char appkey[] = "C46BADB8C725E806A9A2034969D6BA5B";

void data_decord(int val_1, int val_2, uint8_t data[4])
{
  data[0] = val_1 >> 8 & 0xFF;
  data[1] = val_1 & 0xFF;
  data[2] = val_2 >> 8 & 0xFF;
  data[3] = val_2 & 0xFF;
}
#endif

#define STATE_GREEN 0
#define STATE_YELLOW 1
#define STATE_RED 2
int lastState = STATE_RED;
int currentState = STATE_GREEN;

bool startModem()
{
  if (!modem.init())
  {
    DBG("Failed to initialize modem, delaying 10s and retrying");
    // restart autobaud in case GSM just rebooted
    // SimpleNBBegin(SerialAT, 115200);
    return false;
  }

  String name = modem.getModemName();
  DBG("Modem Name:", name);

  String modemInfo = modem.getModemInfo();
  DBG("Modem Info:", modemInfo);

  String ccid = modem.getSimCCID();
  DBG("CCID:", ccid);

  String imei = modem.getIMEI();
  DBG("IMEI:", imei);

  String imsi = modem.getIMSI();
  DBG("IMSI:", imsi);

  modem.setNetworkMode(38);
  modem.setPreferredNarrowBandMode(1);
  DBG("Waiting for network registration...");
  if (!modem.waitForRegistration(60000L, true))
  {
    delay(1000);
    return false;
  }

  if (modem.isNetworkRegistered())
  {
    DBG("Network registered");
  }
  return true;
}

bool sendDataSIM7080(u16 tvoc_ppb, u16 co2_eq_ppm, int currentState)
{
  DBG("sending data");
  if (!startModem())
  {
    SerialUSB.println("sendDataSIM7080 failed");
    return false;
  }

  DBG("Activate Data Network... ");
  if (!modem.activateDataNetwork())
  {
    delay(1000);
    DBG(" failed.");
    return false;
  }

  bool res = modem.isGprsConnected();
  DBG("GPRS status:", res ? "connected" : "not connected");

  String cop = modem.getOperator();
  DBG("Operator:", cop);

  int csq = modem.getSignalQuality();
  DBG("Signal quality:", csq);

  // Use the following to get IP as a String
  // String local = modem.getLocalIP();
  IPAddress local = modem.localIP();
  DBG("Local IP:", local);

#ifdef USE_SSL
  SimpleNBClientSecure client(modem, 0);
#else
  SimpleNBClient client(modem, 0);
#endif

  DBG("Connecting to server ", server, "port", port);
  if (!client.connect(server, port))
  {
    DBG("... failed");
  }
  else
  {
    // Make a HTTP POST request via TCP:
    client.println("POST " + String(resource) + " HTTP/1.1");
    client.println("Host: " + String(server));
    client.println("Content-Type: application/json");
    client.println("Connection: close");
    client.print("Content-Length: ");

    char payload[50];
    sprintf(payload, "{ \"co2_eq_ppm\":\"%u\" }", co2_eq_ppm);

    client.println(strlen(payload));
    client.println();
    client.println(payload);

    // Wait for response to arrive
    uint32_t start = millis();
    while (client.connected() && !client.available() && millis() - start < 30000L)
    {
      delay(100);
    };

    start = millis();
    char received[680] = {'\0'};
    int read_chars = 0;
    while (client.connected() && millis() - start < 10000L)
    {
      while (client.available())
      {
        received[read_chars] = client.read();
        received[read_chars + 1] = '\0';
        read_chars++;
        start = millis();
      }
    }
    DBG(received);
    DBG("Data received:", strlen(received), "characters");

    client.stop();
  }

  DBG("Deactivate Data Network... ");
  if (!modem.deactivateDataNetwork())
  {
    delay(1000);
  }

  return true;
}

void setup()
{
  s16 err = -1;
  u16 scaled_ethanol_signal, scaled_h2_signal;
  SerialUSB.begin(BAUD_RATE);
  delay(2000);

  SerialUSB.println("serial start!!");
  SerialUSB.println("demo will send co2 level when traffic light state changed.");

  tft.begin();
  tft.setRotation(3);
  tft.fillScreen(TFT_BLACK); // Clear screen
  // Set text datum to top left
  tft.setTextDatum(TL_DATUM);
  // Set text colour to orange with black background
  tft.setTextColor(TFT_WHITE, TFT_BLACK);
  tft.setFreeFont(FF19); // Select the font

  /*  Init module,Reset all baseline,The initialization takes up to around 15 seconds, during which
      all APIs measuring IAQ(Indoor air quality ) output will not change.Default value is 400(ppm) for co2,0(ppb) for tvoc*/
  if (sgp_probe() != STATUS_OK)
  {
    SerialUSB.println("SGP failed");
    tft.fillScreen(TFT_BLACK); // Clear screen
    tft.drawString("Sensor not connected?", 20, 80, GFXFF);
    delay(1000);
  }

  while (err != STATUS_OK)
  {
    /*Read H2 and Ethanol signal in the way of blocking*/
    err = sgp_measure_signals_blocking_read(&scaled_ethanol_signal,
                                            &scaled_h2_signal);
    if (err == STATUS_OK)
    {
      SerialUSB.println("get ram signal!");
    }
    else
    {
      SerialUSB.println("error reading SGP30");
      tft.fillScreen(TFT_RED); // Clear screen
      tft.drawString("SGP30 ERROR", 20, 80, GFXFF);
    }
    delay(1000);
  }
  
  sgp_set_absolute_humidity(ABSOLUTE_HUMIDITY);
  err = sgp_iaq_init();

#ifdef USE_SIM7080
  if (!SimpleNBBegin(SerialAT, BAUD_RATE))
  {
    tft.fillScreen(TFT_RED); // Clear screen
    tft.drawString("SIM7080 HW ERROR", 20, 80, GFXFF);
    Serial.println("SIM7080 Init Failed");
    delay(5000);
  }
  if (!startModem())
  {
    tft.fillScreen(TFT_RED); // Clear screen
    tft.drawString("SIM7080 HW ERROR", 20, 80, GFXFF);
    Serial.println("SIM7080 Init Failed");
    delay(5000);
  }

#endif

#ifdef USE_LORAWAN
  // init the library, search the LORAE5 over the different WIO port available
  if (!lorae5.begin(DSKLORAE5_SWSERIAL_WIO_P2))
  {
    tft.fillScreen(TFT_RED); // Clear screen
    tft.drawString("LORA HW ERROR", 20, 80, GFXFF);
    Serial.println("LoRa E5 Init Failed");
    delay(5000);
  }

  // Setup the LoRaWan Credentials
  if (!lorae5.setup(
          Frequency,
          deveui,
          appeui,
          appkey))
  {
    Serial.println("LoRa E5 Setup Failed");
    tft.fillScreen(TFT_RED); // Clear screen
    tft.drawString("LORA SETUP ERROR", 20, 80, GFXFF);
    delay(5000);
  }
#endif
}

void loop()
{
  s16 err = 0;
  u16 tvoc_ppb, co2_eq_ppm;
  err = sgp_measure_iaq_blocking_read(&tvoc_ppb, &co2_eq_ppm);
  if (err == STATUS_OK)
  {
    SerialUSB.print("tVOC  Concentration:");
    SerialUSB.print(tvoc_ppb);
    SerialUSB.println("ppb");

    SerialUSB.print("CO2eq Concentration:");
    SerialUSB.print(co2_eq_ppm);
    SerialUSB.println("ppm");

    if (co2_eq_ppm < TH_LOW)
    {
      currentState = STATE_GREEN;
    }
    else if (co2_eq_ppm < TH_HIGH)
    {
      currentState = STATE_YELLOW;
    }
    else
    {
      currentState = STATE_RED;
    }

    tft.fillRect(100, 120, 100, 30, TFT_BLACK);
    String co2_str = String(co2_eq_ppm);
    tft.drawString(co2_str.c_str(), 100, 120, GFXFF);

    if (lastState != currentState)
    {
      tft.fillScreen(TFT_BLACK); // Clear screen

      tft.drawString("CO2 ppm:", 100, 80, GFXFF);
      tft.setFreeFont(FF19);
      tft.drawString(co2_str.c_str(), 100, 120, GFXFF);

      tft.fillEllipse(45, 40 + 10, 30, 30, TFT_LIGHTGREY);
      tft.fillEllipse(45, 40 + 10 + 70, 30, 30, TFT_LIGHTGREY);
      tft.fillEllipse(45, 40 + 10 + 70 + 70, 30, 30, TFT_LIGHTGREY);

      if (currentState == STATE_GREEN)
      {
        tft.fillEllipse(45, 40 + 10 + 70 + 70, 30, 30, TFT_GREEN);
      }
      else if (currentState == STATE_YELLOW)
      {
        tft.fillEllipse(45, 40 + 10 + 70, 30, 30, TFT_YELLOW);
      }
      else
      {
        tft.fillEllipse(45, 40 + 10, 30, 30, TFT_RED);
      }
      tft.drawRect(5, 5, 80, 230, TFT_WHITE);
      lastState = currentState;

#ifdef USE_SIM7080
      sendDataSIM7080(tvoc_ppb, co2_eq_ppm, currentState);

#endif

#ifdef USE_LORAWAN
      static uint8_t data[4] = {0x00}; // Use the data[] to store the values of the sensors

      data_decord(tvoc_ppb, co2_eq_ppm, data);

      if (lorae5.send_sync( // Sending the sensor values out
              8,            // LoRaWan Port
              data,         // data array
              sizeof(data), // size of the data
              false,        // we are not expecting a ack
              7,            // Spread Factor
              14            // Tx Power in dBm
              ))
      {
        Serial.println("Uplink done");
        if (lorae5.isDownlinkReceived())
        {
          Serial.println("A downlink has been received");
          if (lorae5.isDownlinkPending())
          {
            Serial.println("More downlink are pending");
          }
        }
      }

      if (lorae5.isJoined())
      {
        tft.fillEllipse(300, 20, 10, 10, TFT_BLUE);
      }
      else
      {
        tft.fillEllipse(300, 20, 10, 10, TFT_LIGHTGREY);
      }
#endif
    }
  }
  else
  {
    tft.fillScreen(TFT_RED); // Clear screen
    tft.drawString("SENSOR ERROR", 20, 80, GFXFF);
    SerialUSB.println("error reading IAQ values\n");
  }

  delay(REFRESH_RATE);
}