
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

float convert_to_base(float value, Unit sourceUnit) {
  switch (sourceUnit) {
    case UNIT_NONE:
      return value;

    // Angle
    case UNIT_DEGREES:
      return degrees_to_radians(value);
    case UNIT_GRADIANS:
      return gradians_to_radians(value);

    // Angular Velocity
    case UNIT_DEGREES_PER_SECOND:
      return degrees_per_second_to_radians_per_second(value);
    case UNIT_DEGREES_PER_MINUTE:
      return degrees_per_minute_to_radians_per_second(value);

    // Charge
    case UNIT_AMPERE_HOURS:
      return ampere_hours_to_coulomb(value);

    // Current
    case UNIT_MILLIAMPERES:
      return milliamperes_to_amperes(value);

    // Energy
    case UNIT_KILOWATT_HOURS:
      return kilowatt_hours_to_joules(value);

    // Flow
    case UNIT_LITERS_PER_MINUTE:
      return liters_per_minute_to_cubic_meters_per_second(value);
    case UNIT_LITERS_PER_HOUR:
      return liters_per_hour_to_cubic_meters_per_second(value);
    case UNIT_GALLONS_PER_MINUTE:
      return gallons_per_minute_to_cubic_meters_per_second(value);
    case UNIT_GALLONS_PER_HOUR:
      return gallons_per_hour_to_cubic_meters_per_second(value);

    // Frequency
    case UNIT_ROTATIONS_PER_MINUTE:
      return rotations_per_minute_to_hertz(value);
    case UNIT_KILOHERTZ:
      return kilohertz_to_hertz(value);
    case UNIT_MEGAHERTZ:
      return megahertz_to_hertz(value);
    case UNIT_GIGAHERTZ:
      return gigahertz_to_hertz(value);

    // Potential
    case UNIT_MILLIVOLTS:
      return millivolts_to_volts(value);

    // Power
    case UNIT_MILLIWATTS:
      return milliwatts_to_watts(value);

    // Ratio
    case UNIT_PERCENTAGE:
      return percentage_to_ratio(value);

    // Resistance
    case UNIT_KILO_OHMS:
      return kilo_ohms_to_ohms(value);

    // Speed
    case UNIT_KNOTS:
      return knots_to_meters_per_second(value);
    case UNIT_KILOMETERS_PER_HOUR:
      return kilometers_per_hour_to_meters_per_second(value);
    case UNIT_MILES_PER_HOUR:
      return miles_per_hour_to_meters_per_second(value);

    // Temperature
    case UNIT_CELCIUS:
      return celcius_to_kelvin(value);
    case UNIT_FAHRENHEIT:
      return fahrenheit_to_kelvin(value);

    // Time
    case UNIT_MINUTES:
      return minutes_to_seconds(value);
    case UNIT_HOURS:
      return hours_to_seconds(value);
    case UNIT_DAYS:
      return days_to_seconds(value);

    // Volume
    case UNIT_CUBIC_METERS:
      return cubic_meters_to_liters(value);
    case UNIT_GALLONS:
      return gallons_to_liters(value);

    default:
      return value;
  }
}

///
/// Convert from target to base
///

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

///
/// Convert to base
///

// Angle
float degrees_to_radians(float degrees) { return degrees * (3.141592653589793 / 180.0); }
float gradians_to_radians(float gradians) { return gradians * (3.141592653589793 / 200.0); }

// Angular Velocity
float degrees_per_second_to_radians_per_second(float deg_per_sec) { return deg_per_sec * (3.141592653589793 / 180.0); }
float degrees_per_minute_to_radians_per_second(float deg_per_min) {
  return deg_per_min * (3.141592653589793 / 180.0) / 60.0;
}

// Charge
float ampere_hours_to_coulomb(float amp_hours) { return amp_hours * 3600.0; }

// Current
float milliamperes_to_amperes(float milliamps) { return milliamps / 1000.0; }

// Energy
float kilowatt_hours_to_joules(float kwh) { return kwh * 3600000.0; }

// Flow
float liters_per_minute_to_cubic_meters_per_second(float lpm) { return lpm / 60000.0; }
float liters_per_hour_to_cubic_meters_per_second(float lph) { return lph / 3600000.0; }
float gallons_per_minute_to_cubic_meters_per_second(float gpm) { return gpm / 15850.3; }
float gallons_per_hour_to_cubic_meters_per_second(float gph) { return gph / 951018.0; }

// Frequency
float rotations_per_minute_to_hertz(float rpm) { return rpm / 60.0; }
float kilohertz_to_hertz(float khz) { return khz * 1000.0; }
float megahertz_to_hertz(float mhz) { return mhz * 1000000.0; }
float gigahertz_to_hertz(float ghz) { return ghz * 1000000000.0; }

// Length
float fathoms_to_meters(float fathoms) { return fathoms * 1.8288; }
float feet_to_meters(float feet) { return feet * 0.3048; }
float kilometers_to_meters(float km) { return km * 1000.0; }
float nautical_miles_to_meters(float nm) { return nm * 1852.0; }
float miles_to_meters(float miles) { return miles * 1609.344; }

// Potential
float millivolts_to_volts(float millivolts) { return millivolts / 1000.0; }

// Power
float milliwatts_to_watts(float milliwatts) { return milliwatts / 1000.0; }

// Ratio
float percentage_to_ratio(float percentage) { return percentage / 100.0; }

// Resistance
float kilo_ohms_to_ohms(float kohms) { return kohms * 1000.0; }

// Speed
float knots_to_meters_per_second(float knots) { return knots / 1.94384; }
float kilometers_per_hour_to_meters_per_second(float kph) { return kph / 3.6; }
float miles_per_hour_to_meters_per_second(float mph) { return mph / 2.23694; }

// Temperature
float celcius_to_kelvin(float celcius) { return celcius + 273.15; }
float fahrenheit_to_kelvin(float fahrenheit) { return (fahrenheit - 32.0) * 5.0 / 9.0 + 273.15; }

// Time
float minutes_to_seconds(float minutes) { return minutes * 60.0; }
float hours_to_seconds(float hours) { return hours * 3600.0; }
float days_to_seconds(float days) { return days * 86400.0; }

// Volume
float cubic_meters_to_liters(float cubic_meters) { return cubic_meters * 1000.0; }
float gallons_to_liters(float gallons) { return gallons / 0.264172; }

}  // namespace signalk
}  // namespace esphome
