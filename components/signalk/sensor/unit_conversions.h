#pragma once

namespace esphome {
namespace signalk {

typedef enum {
  UNIT_NONE,

  // Angle
  UNIT_RADIANS,  // BASE
  UNIT_DEGREES,
  UNIT_GRADIANS,

  // Angular velocity
  UNIT_RADIANS_PER_SECOND,  // BASE
  UNIT_DEGREES_PER_SECOND,
  UNIT_DEGREES_PER_MINUTE,

  // Charge
  UNIT_COULUMB,  // BASE
  UNIT_AMPERE_HOURS,

  // Current
  UNIT_AMPERES,  // BASE
  UNIT_MILLIAMPERES,

  // Energy
  UNIT_JOULES,  // BASE
  UNIT_KILOWATT_HOURS,

  // Flow
  UNIT_CUBIC_METERS_PER_SECOND,  // BASE
  UNIT_LITERS_PER_MINUTE,
  UNIT_LITERS_PER_HOUR,
  UNIT_GALLONS_PER_MINUTE,
  UNIT_GALLONS_PER_HOUR,

  // Frequency
  UNIT_HERTZ,  // BASE
  UNIT_ROTATIONS_PER_MINUTE,
  UNIT_KILOHERTZ,
  UNIT_MEGAHERTZ,
  UNIT_GIGAHERTZ,

  // Length
  UNIT_METERS,  // BASE,
  UNIT_FATHOMS,
  UNIT_FEET,
  UNIT_KILOMETERS,
  UNIT_NAUTICAL_MILES,
  UNIT_MILES,

  // Potential
  UNIT_VOLTS,  // BASE
  UNIT_MILLIVOLTS,

  // Power
  UNIT_WATTS,  // BASE
  UNIT_MILLIWATTS,

  // Ratio
  UNIT_RATIO,  // BASE
  UNIT_PERCENTAGE,

  // Resistance
  UNIT_OHMS,  // BASE
  UNIT_KILO_OHMS,

  // Speed
  UNIT_METERS_PER_SECOND,  // BASE
  UNIT_KNOTS,
  UNIT_KILOMETERS_PER_HOUR,
  UNIT_MILES_PER_HOUR,

  // Temperature
  UNIT_KELVIN,  // BASE
  UNIT_CELCIUS,
  UNIT_FAHRENHEIT,

  // Time
  UNIT_SECONDS,  // BASE
  UNIT_MINUTES,
  UNIT_HOURS,
  UNIT_DAYS,

  // Volume
  UNIT_LITERS,  // BASE
  UNIT_CUBIC_METERS,
  UNIT_GALLONS

} Unit;

float convert_from_base(float value, Unit targetUnit);

// Angle
float radians_to_degrees(float radians);
float radians_to_gradians(float radians);

// Angular Velocity
float radians_per_second_to_degrees_per_second(float rad_per_sec);
float radians_per_second_to_degrees_per_minute(float rad_per_sec);

// Charge
float coulumb_to_ampere_hours(float coulombs);

// Current
float amperes_to_milliamperes(float amperes);

// Energy
float joules_to_kilowatt_hours(float joules);

// Flow
float cubic_meters_per_second_to_liters_per_minute(float m3_per_sec);
float cubic_meters_per_second_to_liters_per_hour(float m3_per_sec);
float cubic_meters_per_second_to_gallons_per_minute(float m3_per_sec);
float cubic_meters_per_second_to_gallons_per_hour(float m3_per_sec);

// Frequency
float hertz_to_rotations_per_minute(float hz);
float hertz_to_kilohertz(float hz);
float hertz_to_megahertz(float hz);
float hertz_to_gigahertz(float hz);

// Potential
float volts_to_millivolts(float volts);

// Power
float watts_to_milliwatts(float watts);

// Ratio
float ratio_to_percentage(float ratio);

// Resistance
float ohms_to_kilo_ohms(float ohms);

// Speed
float meters_per_second_to_knots(float mps);
float meters_per_second_to_kilometers_per_hour(float mps);
float meters_per_second_to_miles_per_hour(float mps);

// Temperature
float kelvin_to_celcius(float kelvin);
float kelvin_to_fahrenheit(float kelvin);

// Time
float seconds_to_minutes(float seconds);
float seconds_to_hours(float seconds);
float seconds_to_days(float seconds);

// Volume
float liters_to_cubic_meters(float liters);
float liters_to_gallons(float liters);

}  // namespace signalk
}  // namespace esphome
