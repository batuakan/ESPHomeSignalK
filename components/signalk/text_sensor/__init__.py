import esphome.codegen as cg
from esphome.components import text_sensor
import esphome.config_validation as cv
from esphome.const import CONF_PATH

from .. import SIGNALK_FORMAT, SIGNALK_PERIOD, SIGNALK_POLICY, signalk, signalk_ns, validate_signalk_path

CONF_PERIOD = "period"
CONF_FORMAT = "format"
CONF_POLICY = "policy"
CONF_MIN_PERIOD = "minperiod"
CONF_SIGNALK_PARENT_ID = "SignalK"

SignalkTextSensor = signalk_ns.class_("SignalkTextSensor", text_sensor.TextSensor, cg.Component)

CONFIG_SCHEMA = (
    text_sensor.text_sensor_schema(
        SignalkTextSensor,
    )
    .extend(
        {
            # cv.GenerateID(): cv.declare_id(SignalkTextSensor),
            cv.GenerateID(CONF_SIGNALK_PARENT_ID): cv.use_id(signalk),
            cv.Required(CONF_PATH): validate_signalk_path,
            cv.Optional(CONF_PERIOD, default=1000): SIGNALK_PERIOD,
            cv.Optional(CONF_FORMAT, default="delta"): SIGNALK_FORMAT,
            cv.Optional(CONF_POLICY, default="instant"): SIGNALK_POLICY,
            cv.Optional(CONF_MIN_PERIOD, default=200): SIGNALK_PERIOD,
        }
    )
    .extend(cv.COMPONENT_SCHEMA)
)


async def to_code(config):
    var = await text_sensor.new_text_sensor(config)
    await cg.register_component(var, config)
    cg.add(var.set_path(config[CONF_PATH]))
    cg.add(var.set_period(config[CONF_PERIOD]))
    cg.add(var.set_format(config[CONF_FORMAT]))
    cg.add(var.set_policy(config[CONF_POLICY]))
    cg.add(var.set_min_period(config[CONF_MIN_PERIOD]))

    parent = await cg.get_variable(config[CONF_SIGNALK_PARENT_ID])
    cg.add(parent.subscribe(var))
