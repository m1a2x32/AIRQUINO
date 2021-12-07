# AIRQUINO

# Introduction
The following documentation will serve as the steps taken by the group members to complete the following project. The environment used to program the ESP32 is ESP-IDF on Visual Studio. The reason for us using ESP-IDF rather than Arduino IDE was because we ran into some problems when connecting to the Nordic Thingy:52. Not only did we consider the problems, but also found more documentation on the handling BLE GATT client / servers. It is recommended for the ESP to hold the BLE connections to 3, even though it can reach 9.
Steps

# GATT Client
As we are not so familiar with the ESP-IDF framework, this project had to be divided into certain steps to complete it. The steps go as follows:
1.	Connect to Nordic Thingy:52 with the ESP
  - Find Service of GATT Server
  - Find Characteristic of GATT Server
  - Subscribe to a notify characteristic (air quality)
This was a little challenging because it required a 128-bit char UUID, whilst the example codes provided only an 8-bit UUID. Note: Read through walkthrough on GitHub of espressif examples. Solution is there
2.	Connect to multiple Nordic Thingy:52’s
  -Get average of all air quality characteristics
  
Having read the walkthrough of a single connection, setting up multiple connections was easier. First the git hub example was used as a boiler plate, rather than creating a project from scratch. Like the first step, the second step consisted of creating a profile for each Server and changing their UUID for both service and characteristics. The walkthrough provides a flow chart of how the 2nd step works:

# GATT Server / Client
Share the value found with another device (Raspberry Pi) – Multiple ways to target this approach
  - ESP32 can coexist as Client / Server
  - Raspberry Pi can act as another Server with a Write type Characteristic
3.	Client / Server advertising data collected from one thingy
  - According to the thingy:52 data sheet the data is advertised as 4 bytes. First 2 bytes as CO2 and Last 2 bytes as VOC. 
  - ESP is little endian, research before trying to understand data being advertised!

# References
-	Single Server - https://github.com/espressif/esp-idf/blob/master/examples/bluetooth/bluedroid/ble/gatt_client/tutorial/Gatt_Client_Example_Walkthrough.md 
-	Multiple servers - https://github.com/espressif/esp-idf/tree/master/examples/bluetooth/bluedroid/ble/gattc_multi_connect 
-	Client / Server - https://github.com/espressif/esp-idf/tree/master/examples/bluetooth/bluedroid/coex/gattc_gatts_coex 
-	Section 34. ESP BLE connection limit - https://docs.espressif.com/projects/espressif-esp-faq/en/latest/software-framework/ble-bt.html 
