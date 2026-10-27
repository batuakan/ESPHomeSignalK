import esphome.codegen as cg
from esphome.components import switch
import esphome.config_validation as cv
from esphome.const import CONF_PATH, DEVICE_CLASS_SWITCH

from .. import (
    SIGNALK_FORMAT,
    SIGNALK_META_SCHEMA,
    SIGNALK_PERIOD,
    SIGNALK_POLICY,
    signalk,
    signalk_meta,
    signalk_ns,
    validate_signalk_path,
)

CONF_PERIOD = "period"
CONF_FORMAT = "format"
CONF_POLICY = "policy"
CONF_MIN_PERIOD = "minperiod"
CONF_SIGNALK_PARENT_ID = "SignalK"


SignalkSwitch = signalk_ns.class_("SignalkSwitch", switch.Switch, cg.Component)


CONFIG_SCHEMA = (
    switch.switch_schema(
        SignalkSwitch,
        device_class=DEVICE_CLASS_SWITCH,
    )
    .extend(
        {
            cv.GenerateID(CONF_SIGNALK_PARENT_ID): cv.use_id(signalk),
            cv.Required(CONF_PATH): validate_signalk_path,
            cv.Optional(CONF_PERIOD, default=1000): SIGNALK_PERIOD,
            cv.Optional(CONF_FORMAT, default="delta"): SIGNALK_FORMAT,
            cv.Optional(CONF_POLICY, default="instant"): SIGNALK_POLICY,
            cv.Optional(CONF_MIN_PERIOD, default=200): SIGNALK_PERIOD,
            cv.Optional("meta"): SIGNALK_META_SCHEMA,
        }
    )
    .extend(cv.COMPONENT_SCHEMA)
)


async def to_code(config):
    var = await switch.new_switch(config)
    await cg.register_component(var, config)
    cg.add(var.set_path(config[CONF_PATH]))
    cg.add(var.set_period(config[CONF_PERIOD]))
    cg.add(var.set_format(config[CONF_FORMAT]))
    cg.add(var.set_policy(config[CONF_POLICY]))
    cg.add(var.set_min_period(config[CONF_MIN_PERIOD]))
    await signalk_meta(config, var, defaults={"units": "bool", "supportsPut": True})

    parent = await cg.get_variable(config[CONF_SIGNALK_PARENT_ID])
    cg.add(parent.subscribe(var))
    cg.add(var.set_parent(parent))
    cg.add(var.setup())
