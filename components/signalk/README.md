# SignalK component for ESPHome

Welcome to my feature branch for signalk component


# TODO

1. Fix hard coded library dependency for WiFiClientSecure, HTTPCLient
1. Proper yaml validation for esphome conf file, currently esphome home allows any string/number for the signalk sensor entries.
    1. numbers should adhere to min/max criteria
    2. strings should be validated where applicable
    3. signalk paths should be validated
1. DONE ESPHome Signalk component only supports paths that return numberic value or text value. Additional sensors should be added to support json based paths. 
1. DONE Automatically reconnect if connection to signalk server is lost
1. Create an event to notify user if the connection is lost