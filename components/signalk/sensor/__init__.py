import esphome.codegen as cg
from esphome.components import sensor
import esphome.config_validation as cv
from esphome.const import CONF_PATH

from .. import signalk, signalk_ns

CONF_PERIOD = "period"
CONF_FORMAT = "format"
CONF_POLICY = "policy"
CONF_MIN_PERIOD = "minperiod"
CONF_SIGNALK_PARENT_ID = "SignalK"
CONF_UNIT = "unit"

SignalkSensor = signalk_ns.class_("SignalkSensor", sensor.Sensor, cg.Component)


Unit = signalk_ns.enum("Unit")

UNIT = {
    "none": Unit.UNIT_NONE,
    # Angle
    "radians": Unit.UNIT_RADIANS,
    "degrees": Unit.UNIT_DEGREES,
    "gradians": Unit.UNIT_GRADIANS,
    # Angular velocity
    "radians per second": Unit.UNIT_RADIANS_PER_SECOND,
    "degrees per second": Unit.UNIT_DEGREES_PER_SECOND,
    "degrees per minute": Unit.UNIT_DEGREES_PER_MINUTE,
    # Charge
    "coulumb": Unit.UNIT_COULUMB,
    "ampere_hours": Unit.UNIT_AMPERE_HOURS,
    # Current
    "amperes": Unit.UNIT_AMPERES,
    "milliamperes": Unit.UNIT_MILLIAMPERES,
    # Energy
    "joules": Unit.UNIT_JOULES,
    "kilowatt_hours": Unit.UNIT_KILOWATT_HOURS,
    # Flow
    "cubic_meters_per_second": Unit.UNIT_CUBIC_METERS_PER_SECOND,
    "liters_per_minute": Unit.UNIT_LITERS_PER_MINUTE,
    "liters_per_hour": Unit.UNIT_LITERS_PER_HOUR,
    "gallons_per_minute": Unit.UNIT_GALLONS_PER_MINUTE,
    "gallons_per_hour": Unit.UNIT_GALLONS_PER_HOUR,
    # Frequency
    "hertz": Unit.UNIT_HERTZ,
    "rotations_per_minute": Unit.UNIT_ROTATIONS_PER_MINUTE,
    "kilohertz": Unit.UNIT_KILOHERTZ,
    "megahertz": Unit.UNIT_MEGAHERTZ,
    "gigahertz": Unit.UNIT_GIGAHERTZ,
    # Length
    "meters": Unit.UNIT_METERS,
    "fathoms": Unit.UNIT_FATHOMS,
    "feet": Unit.UNIT_FEET,
    "kilometers": Unit.UNIT_KILOMETERS,
    "nautical_miles": Unit.UNIT_NAUTICAL_MILES,
    "miles": Unit.UNIT_MILES,
    # Potential
    "volts": Unit.UNIT_VOLTS,
    "millivolts": Unit.UNIT_MILLIVOLTS,
    # Power
    "watts": Unit.UNIT_WATTS,
    "milliwatts": Unit.UNIT_MILLIWATTS,
    # Ratio
    "ratio": Unit.UNIT_RATIO,
    "percentage": Unit.UNIT_PERCENTAGE,
    # Resistance
    "ohms": Unit.UNIT_OHMS,
    "kilo_ohms": Unit.UNIT_KILO_OHMS,
    # Speed
    "meters_per_second": Unit.UNIT_METERS_PER_SECOND,
    "knots": Unit.UNIT_KNOTS,
    "kilometers_per_hour": Unit.UNIT_KILOMETERS_PER_HOUR,
    "miles_per_hour": Unit.UNIT_MILES_PER_HOUR,
    # Temperature
    "kelvin": Unit.UNIT_KELVIN,
    "celcius": Unit.UNIT_CELCIUS,
    "fahrenheit": Unit.UNIT_FAHRENHEIT,
    # Time
    "seconds": Unit.UNIT_SECONDS,
    "minutes": Unit.UNIT_MINUTES,
    "hours": Unit.UNIT_HOURS,
    "days": Unit.UNIT_DAYS,
    # Volume
    "liters": Unit.UNIT_LITERS,
    "cubic_meters": Unit.UNIT_CUBIC_METERS,
    "gallons": Unit.UNIT_GALLONS,
}

CONFIG_SCHEMA = (
    sensor.sensor_schema(
        SignalkSensor,
    )
    .extend(
        {
            cv.GenerateID(CONF_SIGNALK_PARENT_ID): cv.use_id(signalk),
            cv.Required(CONF_PATH): cv.string,
            cv.Optional(CONF_PERIOD, default=1000): cv.int_,
            cv.Optional(CONF_FORMAT, default="delta"): cv.string,
            cv.Optional(CONF_POLICY, default="instant"): cv.string,
            cv.Optional(CONF_MIN_PERIOD, default=200): cv.int_,
            cv.Optional(CONF_UNIT, default="none"): cv.enum(UNIT),
        }
    )
    .extend(cv.COMPONENT_SCHEMA)
)


async def to_code(config):
    var = await sensor.new_sensor(config)
    await cg.register_component(var, config)
    cg.add(var.set_path(config[CONF_PATH]))
    cg.add(var.set_period(config[CONF_PERIOD]))
    cg.add(var.set_format(config[CONF_FORMAT]))
    cg.add(var.set_policy(config[CONF_POLICY]))
    cg.add(var.set_min_period(config[CONF_MIN_PERIOD]))
    cg.add(var.set_unit(config[CONF_UNIT]))

    parent = await cg.get_variable(config[CONF_SIGNALK_PARENT_ID])
    cg.add(parent.subscribe(var))
