//taken from https://github.com/limengdu/Seeed-Studio-LoRaWAN-Dev-Kit/blob/main/sensor/Get-SGP30-value/Get-SGP30-value.ino
#include <Arduino.h>
#include "sensirion_common.h"
#include "sgp30.h"

void setup() {
    s16 err;
    u32 ah = 0;
    u16 scaled_ethanol_signal, scaled_h2_signal;
    SerialUSB.begin(115200);
    delay(3000);
    SerialUSB.println("serial start!!");

    SerialUSB.println("Initializing SGP30...");
    /*  Init module,Reset all baseline,The initialization takes up to around 15 seconds, during which
        all APIs measuring IAQ(Indoor air quality ) output will not change.Default value is 400(ppm) for co2,0(ppb) for tvoc*/
    while (sgp_probe() != STATUS_OK) {
        SerialUSB.print(".");
        delay(100);
    }
    Serial.println();
    /*Read H2 and Ethanol signal in the way of blocking*/
    err = sgp_measure_signals_blocking_read(&scaled_ethanol_signal,
                                            &scaled_h2_signal);
    if (err == STATUS_OK) {
        SerialUSB.println("get ram signal!");
    } else {
        SerialUSB.println("error reading signals");
    }

    // Set absolute humidity to 13.000 g/m^3
    //It's just a test value
    sgp_set_absolute_humidity(13000);
    err = sgp_iaq_init();
}

void loop() {
    s16 err = 0;
    u16 tvoc_ppb, co2_eq_ppm;
    err = sgp_measure_iaq_blocking_read(&tvoc_ppb, &co2_eq_ppm);
    if (err == STATUS_OK) {
        SerialUSB.print("tVOC  Concentration:");
        SerialUSB.print(tvoc_ppb);
        SerialUSB.println("ppb");

        SerialUSB.print("CO2eq Concentration:");
        SerialUSB.print(co2_eq_ppm);
        SerialUSB.println("ppm");
    } else {
        SerialUSB.println("error reading IAQ values\n");
    }
    delay(1000);
}