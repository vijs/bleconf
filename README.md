# blemanager
 

### About


`blemanager` is a configuration management system that uses BLE as a transport. The intent is to use this application to do out-of-box system setup and post setup diagnostics. 

### Architecture

The native transport is GATT over BLE. The server supports the Device Information Service and a non-standard Diagnostics service.

The Device Infomormation Service can be found in the GATT profile specs here:
https://www.bluetooth.com/specifications/gatt

The Diagnostics service defines the following GATT characteristics (service uuid - 0xFDB9)
1. Device Status             - `1f113f2c-cc01-4f03-9c5c-4b273ed631bb`
1. Firmmware Download Status - `915f96a6-3788-4271-a7ea-6820e98896b8`
1. WebPA Status              - `9d5d3aae-51e3-4767-a055-59febd71de9d`
1. WiFi Radio1 Status        - `59a99d5a-3d2f-4265-af13-316c7c76b1f0`
1. WiFi Radio2 Status        - `9d6cf473-4fa6-4868-bf2b-c310f38df0c8`
1. RF Status                 - `91b9497e-634c-408a-9f77-8375b1461b8b`

### Implementation Details

This code was originally developed on Raspberry Pi running Raspian using BlueZ with HCI and c++ 11. The code is strucuted in such a way that it should be easy to provide additional transports like TCP, other BLE APIs, etc.

### DEPENDENCIES
* Hostapd 2.8
* BlueZ 5.47
* cJSON 1.7.12

### BUILD

1. Set these three
```
export HOSTAPD_HOME=/home/pi/work/hostapd-2.8/
export BLUEZ_HOME=/home/pi/work/bluez-5.47
export CJSON_HOME=/home/pi/work/cJSON-1.7.12/
```

2. `make`
