from esphome import automation
import esphome.codegen as cg
from esphome.config_helpers import filter_source_files_from_platform
import esphome.config_validation as cv
from esphome.const import (
    CONF_ID,
    CONF_NAME,
    CONF_PASSWORD,
    CONF_PATH,
    CONF_PORT,
    CONF_USERNAME,
    CONF_VALUE,
    PlatformFramework,
)
from esphome.core import CORE

CODEOWNERS = ["@batuakan"]
# DEPENDENCIES = ["wifi"]

CONF_HOST = "host"
CONF_UNIT = "unit"
CONF_META = 'meta'
CONF_SIGNALK_PATH = "signalk_path"


signalk_ns = cg.esphome_ns.namespace("signalk")
if CORE.is_esp32 and CORE.using_esp_idf:
    signalk = signalk_ns.class_("SignalKEspIdf", cg.PollingComponent)
    DEPENDENCIES = ["wifi"]
elif CORE.is_esp32 and CORE.using_arduino:
    signalk = signalk_ns.class_("SignalKArduino", cg.PollingComponent)
    DEPENDENCIES = ["wifi"]
elif CORE.is_host:
    signalk = signalk_ns.class_("SignalKx86", cg.PollingComponent)
else:
    raise Exception("Unsupported platform for SignalK component")
# PublishDeltaAction = signalk_ns.class_(
#     "PublishDeltaAction", automation.Action, cg.Parented.template(signalk)
# )

Unit = signalk_ns.enum("Unit")

UNIT = {
    "none": Unit.UNIT_NONE,
    # Angle
    "radians": Unit.UNIT_RADIANS,
    "degrees": Unit.UNIT_DEGREES,
    "gradians": Unit.UNIT_GRADIANS,
    # Angular velocity
    "radians_per_second": Unit.UNIT_RADIANS_PER_SECOND,
    "degrees_per_second": Unit.UNIT_DEGREES_PER_SECOND,
    "degrees_per_minute": Unit.UNIT_DEGREES_PER_MINUTE,
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
    "celsius": Unit.UNIT_CELSIUS,
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


PublishDeltaAction = signalk_ns.class_("PublishDeltaAction")
PublishDeltaActionFloat = PublishDeltaAction.template(cg.float_)
PublishDeltaActionBool = PublishDeltaAction.template(cg.bool_)

PublishDeltaActionString = PublishDeltaAction.template(cg.std_string)

PutRequestAction = signalk_ns.class_("PutRequestAction")
PutRequestFloat = PutRequestAction.template(cg.float_)
PutRequestBool = PutRequestAction.template(cg.bool_)

PutRequestString = PutRequestAction.template(cg.std_string)

# Schema for zones array
SIGNALK_ZONE_SCHEMA = cv.Schema(
    {
        cv.Optional("lower"): cv.float_,
        cv.Optional("upper"): cv.float_,
        cv.Required("state"): cv.one_of("normal", "warn", "alarm", "emergency", lower=True),
        cv.Optional("message"): cv.string,
    }
)

# Schema for displayScale
SIGNALK_DISPLAY_SCALE_SCHEMA = cv.Schema(
    {
        cv.Required("lower"): cv.float_,
        cv.Required("upper"): cv.float_,
        cv.Required("type"): cv.one_of("linear", "log", lower=True),
    }
)

# Schema for metadata block
SIGNALK_META_SCHEMA = cv.Schema(
    {
        cv.Optional("displayName"): cv.string,
        cv.Optional("longName"): cv.string,
        cv.Optional("shortName"): cv.string,
        cv.Optional("description"): cv.string,
        cv.Optional("units"): cv.string,  # could restrict to known units if desired
        cv.Optional("timeout"): cv.positive_float,
        cv.Optional("supportPut"): cv.boolean,
        cv.Optional("displayScale"): SIGNALK_DISPLAY_SCALE_SCHEMA,
        cv.Optional("alertMethod"): cv.ensure_list(cv.string),
        cv.Optional("warnMethod"): cv.ensure_list(cv.string),
        cv.Optional("alarmMethod"): cv.ensure_list(cv.string),
        cv.Optional("emergencyMethod"): cv.ensure_list(cv.string),
        cv.Optional("zones"): cv.ensure_list(SIGNALK_ZONE_SCHEMA),
    }
)

CONFIG_SCHEMA = cv.Schema(
    {
        cv.GenerateID(): cv.declare_id(signalk),
        cv.Optional(CONF_HOST, default=""): cv.string,
        cv.Optional(CONF_PORT, default=3000): cv.int_,
        cv.Optional(CONF_NAME, default="esp32"): cv.string,
        cv.Optional(CONF_USERNAME, default=""): cv.string,
        cv.Optional(CONF_PASSWORD, default=""): cv.string,
    }
)


PUBLISH_DELTA_SCHEMA = automation.maybe_simple_id(
    {
        cv.GenerateID(): cv.use_id(signalk),
        cv.Optional(CONF_VALUE): cv.templatable(cv.Any(
                cv.float_,
                cv.string,
                cv.boolean,
            )),
        cv.Required(CONF_PATH): cv.string,
        cv.Optional(CONF_UNIT, default="none"): cv.enum(UNIT),
    }
)

PUT_REQUEST_SCHEMA = automation.maybe_simple_id(
    {
        cv.GenerateID(): cv.use_id(signalk),
        cv.Optional(CONF_VALUE): cv.templatable(cv.Any(
                cv.float_,
                cv.string,
                cv.boolean,
            )),
        cv.Required(CONF_PATH): cv.string,
        cv.Optional(CONF_UNIT, default="none"): cv.enum(UNIT),
    }
)

@automation.register_action(
    "signalk.put_request", PutRequestAction, PUT_REQUEST_SCHEMA
)

async def put_request_to_code(config, action_id, template_arg, args):
    parent = await cg.get_variable(config[CONF_ID])
    var = cg.new_Pvariable(action_id, template_arg, parent)
    # cg.add(var.set_value(config[CONF_VALUE]))
    cg.add(var.set_path(config[CONF_PATH]))
    if CONF_UNIT in config:
        cg.add(var.set_unit(config[CONF_UNIT]))
    if CONF_VALUE in config:
        templ = await cg.templatable(config[CONF_VALUE], args, cg.float_)
        cg.add(var.set_value(templ))
    return var

@automation.register_action(
    "signalk.publish_delta", PublishDeltaAction, PUBLISH_DELTA_SCHEMA
)

async def publish_delta_to_code(config, action_id, template_arg, args):
    parent = await cg.get_variable(config[CONF_ID])
    var = cg.new_Pvariable(action_id, template_arg, parent)
    # cg.add(var.set_value(config[CONF_VALUE]))
    cg.add(var.set_path(config[CONF_PATH]))
    if CONF_UNIT in config:
        cg.add(var.set_unit(config[CONF_UNIT]))
    if CONF_VALUE in config:
        templ = await cg.templatable(config[CONF_VALUE], args, cg.float_)
        cg.add(var.set_value(templ))
    return var


async def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)
    cg.add(var.set_host(config[CONF_HOST]))
    cg.add(var.set_port(config[CONF_PORT]))
    cg.add(var.set_device_name(config[CONF_NAME]))
    cg.add(var.set_user_name(config[CONF_USERNAME]))
    cg.add(var.set_user_password(config[CONF_PASSWORD]))

    cg.add(var.set_setup_priority(100))
    cg.add_library("bblanchon/ArduinoJson", "7.4.2")
    if CORE.is_esp32 and CORE.using_esp_idf:
        cg.add_library(
            "esp_websocket_client",
            None,
            repository="https://components.espressif.com/api/downloads/?object_type=component&object_id=dbc87006-9a4b-45e6-a6ab-b286174cb413",
        )
    if CORE.is_esp32 and CORE.using_arduino:
        cg.add_library("NetworkClientSecure", None)
        cg.add_library("HTTPClient", None)
        # cg.add_library("links2004/WebSockets", "2.6.1")
        cg.add_library("gilmaimon/ArduinoWebsockets", "0.5.4")
    if CORE.is_host:
        cg.add_build_flag("-I/usr/local/include")
        cg.add_build_flag("-L/usr/local/lib")
        cg.add_build_flag("-lixwebsocket")
        cg.add_build_flag("-lz")
        cg.add_build_flag("-lssl")
        cg.add_build_flag("-lcrypto")
        
        
        
        cg.add_platformio_option("build_type", "debug")


FILTER_SOURCE_FILES = filter_source_files_from_platform(
    {
        "signalk_client_arduino_esp32.cpp": {PlatformFramework.ESP32_ARDUINO},
        "signalk_esp_idf.cpp": {PlatformFramework.ESP32_IDF},
        "signalk_x86.cpp": {PlatformFramework.HOST_NATIVE},
    }
)
