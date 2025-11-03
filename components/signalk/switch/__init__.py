import esphome.codegen as cg
from esphome.components import switch
import esphome.config_validation as cv
from esphome.const import CONF_PATH, DEVICE_CLASS_SWITCH
from .. import signalk, signalk_ns, SIGNALK_META_SCHEMA

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
            cv.Required(CONF_PATH): cv.string,
            cv.Optional(CONF_PERIOD, default=1000): cv.int_,
            cv.Optional(CONF_FORMAT, default="delta"): cv.string,
            cv.Optional(CONF_POLICY, default="instant"): cv.string,
            cv.Optional(CONF_MIN_PERIOD, default=200): cv.int_,

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
    meta = config.get("meta", {})
    if meta:
        for key, value in meta.items():
            # bool
            if isinstance(value, bool):
                cg.add(var.add_metadata(key, value))

            # number (converted to C++ double)
            elif isinstance(value, (int, float)):
                cg.add(var.add_metadata(key, float(value)))

            # string
            elif isinstance(value, str):
                cg.add(var.add_metadata(key, cg.std_string(value)))

            # list of strings
            elif isinstance(value, list):
                vec = cg.std_vector.template(cg.std_string)()
                for item in value:
                    cg.add(vec.push_back(cg.std_string(item)))
                cg.add(var.add_metadata(key, vec))
            else:
                raise cv.Invalid(f"Unsupported metadata type for '{key}': {value}")

    parent = await cg.get_variable(config[CONF_SIGNALK_PARENT_ID])
    cg.add(parent.subscribe(var))
    cg.add(var.set_parent(parent))
    cg.add(var.setup())
