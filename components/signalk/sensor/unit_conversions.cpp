
#include "unit_conversions.h"

namespace esphome {
namespace signalk {

float convert_from_base(float value, Unit targetUnit) {
  switch (targetUnit) {
    case UNIT_NONE:
      return value;

    // Angle
    case UNIT_DEGREES:
      return radians_to_degrees(value);
    case UNIT_GRADIANS:
      return radians_to_gradians(value);

    // Angular Velocity
    case UNIT_DEGREES_PER_SECOND:
      return radians_per_second_to_degrees_per_second(value);
    case UNIT_DEGREES_PER_MINUTE:
      return radians_per_second_to_degrees_per_minute(value);

    // Charge
    case UNIT_AMPERE_HOURS:
      return coulumb_to_ampere_hours(value);

    // Current
    case UNIT_MILLIAMPERES:
      return amperes_to_milliamperes(value);

    // Energy
    case UNIT_KILOWATT_HOURS:
      return joules_to_kilowatt_hours(value);

    // Flow
    case UNIT_LITERS_PER_MINUTE:
      return cubic_meters_per_second_to_liters_per_minute(value);
    case UNIT_LITERS_PER_HOUR:
      return cubic_meters_per_second_to_liters_per_hour(value);
    case UNIT_GALLONS_PER_MINUTE:
      return cubic_meters_per_second_to_gallons_per_minute(value);
    case UNIT_GALLONS_PER_HOUR:
      return cubic_meters_per_second_to_gallons_per_hour(value);

    // Frequency
    case UNIT_ROTATIONS_PER_MINUTE:
      return hertz_to_rotations_per_minute(value);
    case UNIT_KILOHERTZ:
      return hertz_to_kilohertz(value);
    case UNIT_MEGAHERTZ:
      return hertz_to_megahertz(value);
    case UNIT_GIGAHERTZ:
      return hertz_to_gigahertz(value);

    // Potential
    case UNIT_MILLIVOLTS:
      return volts_to_millivolts(value);

    // Power
    case UNIT_MILLIWATTS:
      return watts_to_milliwatts(value);

    // Ratio
    case UNIT_PERCENTAGE:
      return ratio_to_percentage(value);

    // Resistance
    case UNIT_KILO_OHMS:
      return ohms_to_kilo_ohms(value);

    // Speed
    case UNIT_KNOTS:
      return meters_per_second_to_knots(value);
    case UNIT_KILOMETERS_PER_HOUR:
      return meters_per_second_to_kilometers_per_hour(value);
    case UNIT_MILES_PER_HOUR:
      return meters_per_second_to_miles_per_hour(value);

    // Temperature
    case UNIT_CELCIUS:
      return kelvin_to_celcius(value);
    case UNIT_FAHRENHEIT:
      return kelvin_to_fahrenheit(value);

    // Time
    case UNIT_MINUTES:
      return seconds_to_minutes(value);
    case UNIT_HOURS:
      return seconds_to_hours(value);
    case UNIT_DAYS:
      return seconds_to_days(value);

    // Volume
    case UNIT_CUBIC_METERS:
      return liters_to_cubic_meters(value);
    case UNIT_GALLONS:
      return liters_to_gallons(value);

    default:
      return value;
  }
}

// Angle
float radians_to_degrees(float radians) { return radians * (180.0 / 3.141592653589793); }

float radians_to_gradians(float radians) { return radians * (200.0 / 3.141592653589793); }

// Angular Velocity
float radians_per_second_to_degrees_per_second(float rad_per_sec) { return rad_per_sec * (180.0 / 3.141592653589793); }

float radians_per_second_to_degrees_per_minute(float rad_per_sec) {
  return rad_per_sec * (180.0 / 3.141592653589793) * 60.0;
}

// Charge
float coulumb_to_ampere_hours(float coulombs) { return coulombs / 3600.0; }

// Current
float amperes_to_milliamperes(float amperes) { return amperes * 1000.0; }

// Energy
float joules_to_kilowatt_hours(float joules) { return joules / 3600000.0; }

// Flow
float cubic_meters_per_second_to_liters_per_minute(float m3_per_sec) { return m3_per_sec * 60000.0; }

float cubic_meters_per_second_to_liters_per_hour(float m3_per_sec) { return m3_per_sec * 3600000.0; }

float cubic_meters_per_second_to_gallons_per_minute(float m3_per_sec) { return m3_per_sec * 15850.3; }

float cubic_meters_per_second_to_gallons_per_hour(float m3_per_sec) { return m3_per_sec * 951018.0; }

// Frequency
float hertz_to_rotations_per_minute(float hz) { return hz * 60.0; }

float hertz_to_kilohertz(float hz) { return hz / 1000.0; }

float hertz_to_megahertz(float hz) { return hz / 1000000.0; }

float hertz_to_gigahertz(float hz) { return hz / 1000000000.0; }

// Potential
float volts_to_millivolts(float volts) { return volts * 1000.0; }

// Power
float watts_to_milliwatts(float watts) { return watts * 1000.0; }

// Ratio
float ratio_to_percentage(float ratio) { return ratio * 100.0; }

// Resistance
float ohms_to_kilo_ohms(float ohms) { return ohms / 1000.0; }

// Speed
float meters_per_second_to_knots(float mps) { return mps * 1.94384; }

float meters_per_second_to_kilometers_per_hour(float mps) { return mps * 3.6; }

float meters_per_second_to_miles_per_hour(float mps) { return mps * 2.23694; }

// Temperature
float kelvin_to_celcius(float kelvin) { return kelvin - 273.15; }

float kelvin_to_fahrenheit(float kelvin) { return (kelvin - 273.15) * 9.0 / 5.0 + 32.0; }

// Time
float seconds_to_minutes(float seconds) { return seconds / 60.0; }

float seconds_to_hours(float seconds) { return seconds / 3600.0; }

float seconds_to_days(float seconds) { return seconds / 86400.0; }

// Volume
float liters_to_cubic_meters(float liters) { return liters / 1000.0; }

float liters_to_gallons(float liters) { return liters * 0.264172; }

}  // namespace signalk
}  // namespace esphome