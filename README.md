# Landsort Deep
Installation "Landsort Deep" in Sopot, Poland by Andrius Labašauskas.

This is the firmware for the DMX lightning control unit. 

Implemented functionality:
 * Provides means to configure WiFi netword credentials.
 * Connect to NTP server to fetch todays date.
 * Connect to http://satbaltyk.iopan.gda.pl/files/exports/sopot_sst_mean.txt to fetch todays mean temperature.
 * Use data in http://satbaltyk.iopan.gda.pl/files/exports/sopot_sst_mean_daily_average.txt to display color depending if todays mean temperature is higher or lower than historic mean.
 * Provide means to configure colors for high/low temperature display.

## Bill of materials

* OLIMEX ESP32-EVB-EA-IND
* OLIMEX BOX-ESP32-EVB-EA
* OLIMEX MOD-RS485
* MEAN WELL  HDR-15-5
* LINX - TE CONNECTIVITY  ANT-2.4-WRT-UFL
* PRO SIGNAL PS11474
* LAPP KABEL 53112010
* EUROPA COMPONENTS  EUC2P2C

## Specifications

[Landsort Deep specifications](https://github.com/Miceuz/sopoto-akmuo/blob/main/Landsort%20Deep%2C%20Sopot%20Control%20Unit%20Specifications.pdf)
