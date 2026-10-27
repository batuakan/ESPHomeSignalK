import json
import re

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

# from esphome.components.esp32 import idf_component_register
from esphome.core import CORE, ID
from esphome.components.esp32 import add_idf_component, include_builtin_idf_component

CODEOWNERS = ["@batuakan"]
# DEPENDENCIES = ["wifi"]

CONF_HOST = "host"
CONF_UNIT = "unit"
CONF_META = "meta"
CONF_SIGNALK_PATH = "signalk_path"

SIGNALK_PATH_REGEX = re.compile(r"^[A-Za-z0-9_]+(.[A-Za-z0-9_]+)(..*)?$")


def validate_signalk_path(value):
    value = cv.string(value)
    if not SIGNALK_PATH_REGEX.match(value):
        raise cv.Invalid("Invalid SignalK path. Use dot-separated segments with A-Za-z0-9_ only.")
    return value


SIGNALK_PERIOD = cv.int_range(min=0, max=86400000)
SIGNALK_FORMAT = cv.one_of("delta", "full", lower=True)
SIGNALK_POLICY = cv.one_of("instant", "ideal", "fixed", lower=True)


signalk_ns = cg.esphome_ns.namespace("signalk")
if CORE.is_esp32 and not CORE.using_arduino:
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


# SignalK always carries SI base units; this maps each unit to the one its values have on the server.
_SI_UNITS = {
    "rad": ("radians", "degrees", "gradians"),
    "rad/s": ("radians_per_second", "degrees_per_second", "degrees_per_minute"),
    "C": ("coulumb", "ampere_hours"),
    "A": ("amperes", "milliamperes"),
    "J": ("joules", "kilowatt_hours"),
    "m3/s": (
        "cubic_meters_per_second",
        "liters_per_minute",
        "liters_per_hour",
        "gallons_per_minute",
        "gallons_per_hour",
    ),
    "Hz": ("hertz", "rotations_per_minute", "kilohertz", "megahertz", "gigahertz"),
    "m": ("meters", "fathoms", "feet", "kilometers", "nautical_miles", "miles"),
    "V": ("volts", "millivolts"),
    "W": ("watts", "milliwatts"),
    "ratio": ("ratio", "percentage"),
    "ohm": ("ohms", "kilo_ohms"),
    "m/s": ("meters_per_second", "knots", "kilometers_per_hour", "miles_per_hour"),
    "K": ("kelvin", "celsius", "fahrenheit"),
    "s": ("seconds", "minutes", "hours", "days"),
    "m3": ("liters", "cubic_meters", "gallons"),
}
UNIT_SI = {unit: si for si, units in _SI_UNITS.items() for unit in units}


SignalkPublishedMeta = signalk_ns.class_("SignalkPublishedMeta")
PublishDeltaAction = signalk_ns.class_("PublishDeltaAction")
PublishDeltaActionFloat = PublishDeltaAction.template(cg.float_)
PublishDeltaActionBool = PublishDeltaAction.template(cg.bool_)

PublishDeltaActionString = PublishDeltaAction.template(cg.std_string)

PutRequestAction = signalk_ns.class_("PutRequestAction")
PutRequestFloat = PutRequestAction.template(cg.float_)
PutRequestBool = PutRequestAction.template(cg.bool_)

PutRequestString = PutRequestAction.template(cg.std_string)

# std::variant<float, std::string, bool>, see signalk.h
DeltaValue = signalk_ns.struct("DeltaValue")


def delta_value_constant(value):
    """Emit a constant that selects the intended DeltaValue alternative unambiguously."""
    if isinstance(value, bool):
        return cg.RawExpression("true" if value else "false")
    if isinstance(value, str):
        return cg.RawExpression(f"std::string({cg.safe_exp(value)})")
    return cg.RawExpression(f"{float(value)!r}f")


async def delta_value_templatable(value, args):
    if cg.is_template(value):
        return await cg.templatable(value, args, cg.float_)
    return await cg.templatable(value, args, DeltaValue, to_exp=delta_value_constant)


SIGNALK_ALERT_METHOD = cv.ensure_list(cv.one_of("visual", "sound", lower=True))


def validate_zone(zone):
    if "lower" in zone and "upper" in zone and zone["lower"] >= zone["upper"]:
        raise cv.Invalid("A zone's 'lower' must be smaller than its 'upper'")
    if "lower" not in zone and "upper" not in zone:
        raise cv.Invalid("A zone needs at least one of 'lower' and 'upper'")
    return zone


def validate_display_scale(scale):
    if scale["lower"] >= scale["upper"]:
        raise cv.Invalid("displayScale 'lower' must be smaller than 'upper'")
    return scale


# Schema for zones array
SIGNALK_ZONE_SCHEMA = cv.All(
    cv.Schema(
        {
            cv.Optional("lower"): cv.float_,
            cv.Optional("upper"): cv.float_,
            cv.Required("state"): cv.one_of("normal", "nominal", "alert", "warn", "alarm", "emergency", lower=True),
            cv.Optional("message"): cv.string,
        }
    ),
    validate_zone,
)

# Schema for displayScale
SIGNALK_DISPLAY_SCALE_SCHEMA = cv.All(
    cv.Schema(
        {
            cv.Required("lower"): cv.float_,
            cv.Required("upper"): cv.float_,
            cv.Required("type"): cv.one_of("linear", "log", "squareroot", "power", lower=True),
            cv.Optional("power"): cv.float_,
        }
    ),
    validate_display_scale,
)

# Schema for metadata block (SignalK data model, metadata)
SIGNALK_META_SCHEMA = cv.Schema(
    {
        cv.Optional("displayName"): cv.string,
        cv.Optional("longName"): cv.string,
        cv.Optional("shortName"): cv.string,
        cv.Optional("description"): cv.string,
        cv.Optional("units"): cv.string,
        cv.Optional("timeout"): cv.positive_float,
        cv.Optional("supportsPut"): cv.boolean,
        cv.Optional("displayScale"): SIGNALK_DISPLAY_SCALE_SCHEMA,
        cv.Optional("alertMethod"): SIGNALK_ALERT_METHOD,
        cv.Optional("warnMethod"): SIGNALK_ALERT_METHOD,
        cv.Optional("alarmMethod"): SIGNALK_ALERT_METHOD,
        cv.Optional("emergencyMethod"): SIGNALK_ALERT_METHOD,
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
    # The access/connection state machine runs in update(). Without an interval ESPHome
    # (2026.x) never calls update() on a PollingComponent, so default to 1s.
).extend(cv.polling_component_schema("1s"))


PUBLISH_DELTA_SCHEMA = automation.maybe_simple_id(
    {
        cv.GenerateID(): cv.use_id(signalk),
        cv.Optional(CONF_VALUE): cv.templatable(
            cv.Any(
                cv.float_,
                cv.string,
                cv.boolean,
            )
        ),
        cv.Required(CONF_PATH): validate_signalk_path,
        cv.Optional(CONF_UNIT, default="none"): cv.enum(UNIT),
        cv.Optional(CONF_META): SIGNALK_META_SCHEMA,
    }
)

PUT_REQUEST_SCHEMA = automation.maybe_simple_id(
    {
        cv.GenerateID(): cv.use_id(signalk),
        cv.Optional(CONF_VALUE): cv.templatable(
            cv.Any(
                cv.float_,
                cv.string,
                cv.boolean,
            )
        ),
        cv.Required(CONF_PATH): validate_signalk_path,
        cv.Optional(CONF_UNIT, default="none"): cv.enum(UNIT),
    }
)


@automation.register_action("signalk.put_request", PutRequestAction, PUT_REQUEST_SCHEMA, synchronous=False)
async def put_request_to_code(config, action_id, template_arg, args):
    parent = await cg.get_variable(config[CONF_ID])
    var = cg.new_Pvariable(action_id, template_arg, parent)
    # cg.add(var.set_value(config[CONF_VALUE]))
    cg.add(var.set_path(config[CONF_PATH]))
    if CONF_UNIT in config:
        cg.add(var.set_unit(config[CONF_UNIT]))
    if CONF_VALUE in config:
        templ = await delta_value_templatable(config[CONF_VALUE], args)
        cg.add(var.set_value(templ))
    return var


@automation.register_action("signalk.publish_delta", PublishDeltaAction, PUBLISH_DELTA_SCHEMA, synchronous=False)
async def publish_delta_to_code(config, action_id, template_arg, args):
    parent = await cg.get_variable(config[CONF_ID])
    var = cg.new_Pvariable(action_id, template_arg, parent)
    # cg.add(var.set_value(config[CONF_VALUE]))
    cg.add(var.set_path(config[CONF_PATH]))
    if CONF_UNIT in config:
        cg.add(var.set_unit(config[CONF_UNIT]))
    if CONF_VALUE in config:
        templ = await delta_value_templatable(config[CONF_VALUE], args)
        cg.add(var.set_value(templ))
    if CONF_META in config:
        # The path is only published, so the metadata lives in its own object that the parent syncs
        # with the server on every (re)connect, instead of being sent along with every delta.
        meta = cg.new_Pvariable(ID(f"{action_id.id}_meta", is_declaration=True, type=SignalkPublishedMeta))
        cg.add(meta.set_path(config[CONF_PATH]))
        await signalk_meta(config, meta)
        cg.add(parent.add_metadata_source(meta))
    return var


async def signalk_meta(config, var, defaults=None):
    """Add the entity's `meta:` block to its C++ object.

    `defaults` are used for keys the user did not set; `units` is derived from `unit:` when it is not
    given. At runtime the device only sends the keys the server does not have yet.
    """
    meta = dict(defaults or {})
    unit = config.get(CONF_UNIT)
    if unit is not None and "units" not in meta:
        si = UNIT_SI.get(unit)
        if si:
            meta["units"] = si
    meta.update(config.get("meta", {}))

    for key, value in meta.items():
        # bool must be checked before numbers: bool is an int
        if isinstance(value, bool):
            cg.add(var.add_metadata(key, value))
        elif isinstance(value, int | float):
            cg.add(var.add_metadata(key, float(value)))
        elif isinstance(value, str):
            cg.add(var.add_metadata(key, cg.std_string(value)))
        elif isinstance(value, list | dict):
            cg.add(var.add_metadata_from_json(key, cg.std_string(json.dumps(value))))
        else:
            raise cv.Invalid(f"Unsupported metadata type for '{key}': {value}")


async def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)
    cg.add(var.set_host(config[CONF_HOST]))
    cg.add(var.set_port(config[CONF_PORT]))
    cg.add(var.set_device_name(config[CONF_NAME]))
    cg.add(var.set_user_name(config[CONF_USERNAME]))
    cg.add(var.set_user_password(config[CONF_PASSWORD]))

    # cg.add_library("bblanchon/ArduinoJson", "7.4.3")
    if CORE.is_esp32:
        if CORE.using_arduino:
            cg.add_library("ESPmDNS", None)
            cg.add_library("WiFi", None)
            cg.add_library("NetworkClientSecure", None)
            cg.add_library("HTTPClient", None)
            cg.add_library("gilmaimon/ArduinoWebsockets", "0.5.4")
        elif not CORE.using_arduino:
            include_builtin_idf_component("esp_http_client")
            add_idf_component(
                name="espressif/esp_websocket_client",
                ref="1.8.0",
            )
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
