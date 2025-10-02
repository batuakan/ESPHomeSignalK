# ESPHome SignalK Component

ESPHome external component to fetch data from signalk server using websockets on arduino frameword and esp-idf framework. ESPHome is an open-source project that allows you to easily create custom firmware for ESP32 microcontrollers to integrate with home automation systems like Home Assistant. It uses a simple YAML-based configuration file to define how your device should behave.

This component allows you to connect your esp device to your signalk server. It has support to subscribe signalk paths as well as to push data to the server. The fetched data can be displayed on a custom navigational instrument made by the LVGL library, as well to activate actuators, relays etc. Also values that you get from your sensors on the esp device can easily be pushed back to the server using the signalk.publish_delta action.

The component supports esp-idf framework as well as esp arduino framework. It is also possible to use this component locally using the host: component, ESPHome will built a local binary, which you can run on your computer. No more lengthy compile process and uploading to an ESP32 device. Makes LVGL development much smoother. This has been tested with Ubuntu 24.04 and the LVGL library. See the example SDL.yaml for more details. 

Sample wind instrument view showing apparent wind angle, true wind angle, speed over ground (SOG), true wind speed (TWS), apparent wind speed (AWS) and magnetic heading using LVGL graphics library. For more information see my other project [SignalK_ESPHome_LVGL](https://github.com/batuakan/SignalK_ESPHome_LVGL)

![Wind rose](./images/Wind.png)

## Installing

If using homeassistant esphome addon, just link to this repo as an external component, this will allow use of esp-idf and arduino frameworks.

Otherwise install esphome into a virtual python environment and build your project using esphome run nameofyaml.yaml

To run on local machine, ix::websockets library is required, whoch should be installed system wide, or in a path where platformio can locate it.

## Known issues and limitations

1. This component will not work with SSL, if you signalk server is sitting behind an SSL connection esp32 wont be able to connect to it yet.

2. If you use the signalk component and dont define any signalk sensors, you will get a compile error saying sensor.h was not found. Just add a sensor even if you dont use it.

```ỳaml
sensor:
  - platform: signalk
    id: environment_wind_angleApparent
    path: environment.wind.angleApparent
```

## Usage

Signalk component provides two types sensors to fetch data from the signalk server: 
1. **text_sensor** to fetch text/json based information 
2. **sensor** to access numerical data. 

Each sensor must declare an id and signalk path, other optional values are:
- id: id of the sensor
- name: unique name to identify the sensor
- path: signalk path to subscribe to
- period: Default 1000
- format: Defaults to delta
- policy: Defaults to instant
- minperiod: Defaults to 200
- unit: Defines which unot the data should be converted to, default to none (No conversion). 
  Note that this compoennt doesn't know the units of the incoming value so technically it ispoosible to convert between unmatching values, therefore it is the developers responsiblity to check the unit of the incoming value and apply the correct unit conversion.
  Possible values are:

  `No conversion`
  none,

  `Angle`
  radians,
  degrees,
  gradians

  `Angular velocity`
  radians_per_second,
  degrees_per_second,
  degrees_per_minute,
  radians_per_second

  `Charge`
  coulumb,
  ampere_hours

  `Current`
  amperes,
  milliamperes

  `Energy`
  joules,
  kilowatt_hours

  `Flow`
  cubic_meters_per_second,
  liters_per_minute,
  liters_per_hour,
  gallons_per_minute,
  gallons_per_hour

  `Frequency`
  hertz,
  rotations_per_minute,
  kilohertz,
  megahertz,
  gigahertz

  `Length`
  meters,
  fathoms,
  feet,
  kilometers,
  nautical_miles,
  miles

  `Potential`
  volts,
  millivolts

  `Power`
  watts,
  milliwatts

  `Ratio`
  ratio,
  percentage

  `Resistance`
  ohms,
  kilo_ohms

  `Speed`
  meters_per_second,
  knots,
  kilometers_per_hour,
  miles_per_hour,

  `Temperature`
  kelvin,
  celcius,
  fahrenheit

  `Time`
  seconds,
  minutes,
  hours,
  days

  `Volume`
  liters,
  cubic_meters,
  gallons

```yaml
external_components:
  - source: github://batuakan/ESPHomeSignalK@main
    components: [ signalk ]

signalk:
  host: 10.10.10.1 # Ip address for your 
  port: 3000 # Optional Port for signalk, defaults 3000

# text sensor for text based data
text_sensor:
  - platform: signalk
    id: name
    path: name

# sensor for numerical data
sensor:
  - platform: signalk
    id: environment_wind_angleApparent
    path: environment.wind.angleApparent
    unit: knots

# use text_sensor to fetch json data and parse with the json component to extract information from it
text_sensor:
  - platform: signalk
    id: navigation_position
    path: navigation.position
    on_value:
      - lambda: |-
            json::parse_json(x, [](JsonObject root) -> bool {
                if (root["longitude"]) {
                    ESP_LOGI("json_parser", "Longitude: %f", root["longitude"].as<float>());
                    ESP_LOGI("json_parser", "Latitude: %f", root["latitude"].as<float>());
                }
                return true;
            });
```

In order to send data back to server use the **signalk.publish_delta** action. publish_delta action supports string and floating values. The unit field is optional, however if applied converts from the sensors unit format to the base SI format that signalk uses, e.g. if the sensor provides temperature values in celcius they will be converted to fahrenheit while being sent to the server.


```yaml
sensor:
- platform: template
  name: "Test Fixed Sensor"
  unit_of_measurement: "°C"
  accuracy_decimals: 1
  update_interval: 5s
  lambda: |-
    return random_float() * 100.0;
  on_value:
    - signalk.publish_delta:
        path: "environment.temperature"
        unit: "celcius"
```

## Contributing

Contributions of all kinds are welcome! Whether it's feedback, bug reports, feature suggestions, testing, or code improvements, your input helps make the project better for everyone. Feel free to open an issue or submit a pull request—every bit of help is appreciated!

If you have created something cool using this component, please let me know, we can can share a link to your project here, or add it to the examples.

### Possible new features

1. Adding support for other esphome supported hardware types, e.g. rpi pico
1. Better schema validation
