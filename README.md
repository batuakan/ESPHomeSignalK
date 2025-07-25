# ESPHome SignalK Component

ESPHome external component to fetch data from signalk server using websockets on arduino frameword and esp-idf framework. ESPHome is an open-source project that allows you to easily create custom firmware for ESP32 microcontrollers to integrate with home automation systems like Home Assistant. It uses a simple YAML-based configuration file to define how your device should behave.

This component allows you to connect to your signalk server and fetch data so that it can be displayed on a custom navigational instrument.

Sample instrument view showing apparent wind angle, true wind angle, speed over ground (SOG), true wind speed (TWS), apparent wind speed (AWS) and magnetic heading using LVGL graphics library

![Wind rose](./images/Wind.png)

## Usage

Signalk component provides to types sensors to fetch data from the signalk server: 
1. **text_sensor** to fetch text/json based information 
2. **sensor** to access numerical data. 

Each sensor must declare an id and signalk path, other optional values are:
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

## Contributing

Contributions of all kinds are welcome! Whether it's feedback, bug reports, feature suggestions, testing, or code improvements, your input helps make the project better for everyone. Feel free to open an issue or submit a pull request—every bit of help is appreciated!

### Possible new features
1. Schema validation
1. Ability push data back into signalk server
