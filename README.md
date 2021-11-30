# AQI
This project consisted of monitoring the CO2(ppm) and VOC(ppb) in the school live via BLE. This worked by using the Nordic Thingy:52 as a BLE sensor and the ESP32 as a Client/Server. This repo will be mainily focusing on the ESP code, it will have the steps taken to achieve success in this project and multiple example codes according to the documentation.
# ESP32
ESP32 is supposed to connect to multiple Thingys and advertise the data they are advertising. Each new BLE sensor will be advertized in a new service / characteristic.
# Raspberry Pi
The raspberry pis job is to act as a central gateway to collect the data and log it using grafana.
