import json
import esphome.codegen as cg
from esphome.components import text
import esphome.config_validation as cv
from esphome.const import CONF_PATH
from .. import signalk, signalk_ns, SIGNALK_META_SCHEMA, signalk_meta, CONF_UNIT, UNIT

CONF_MIN = "min"
CONF_MAX = "max"
CONF_STEP = "step"
CONF_PERIOD = "period"
CONF_FORMAT = "format"
CONF_POLICY = "policy"
CONF_MIN_PERIOD = "minperiod"
CONF_SIGNALK_PARENT_ID = "SignalK"


SignalkText = signalk_ns.class_("SignalkText", text.Text, cg.Component)


CONFIG_SCHEMA = (
    text.text_schema(
        SignalkText,
    )
    .extend(
        {
            cv.GenerateID(CONF_SIGNALK_PARENT_ID): cv.use_id(signalk),

            cv.Required(CONF_PATH): cv.string,
            cv.Optional(CONF_STEP, default=1.0): cv.float_,
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
    var = await text.new_text(config)
    await cg.register_component(var, config)
    cg.add(var.set_path(config[CONF_PATH]))
    cg.add(var.set_period(config[CONF_PERIOD]))
    cg.add(var.set_format(config[CONF_FORMAT]))
    cg.add(var.set_policy(config[CONF_POLICY]))
    cg.add(var.set_min_period(config[CONF_MIN_PERIOD]))
    await signalk_meta(config, var)
    # meta = config.get("meta", {})
    # if meta:
    #     for key, value in meta.items():
    #         # bool
    #         if isinstance(value, bool):
    #             cg.add(var.add_metadata(key, value))

    #         # number (converted to C++ double)
    #         elif isinstance(value, (int, float)):
    #             cg.add(var.add_metadata(key, float(value)))

    #         # string
    #         elif isinstance(value, str):
    #             cg.add(var.add_metadata(key, cg.std_string(value)))

    #         # list of strings
    #         elif isinstance(value, list):
    #             json_str = json.dumps(value)
    #             cg.add(var.add_metadata_from_json(key, cg.std_string(json_str)))
    #         # dictionary
    #         elif isinstance(value, dict):
    #             json_str = json.dumps(value)
    #             cg.add(var.add_metadata_from_json(key, cg.std_string(json_str)))

    #         else:
    #             raise cv.Invalid(f"Unsupported metadata type for '{key}': {value}")

    parent = await cg.get_variable(config[CONF_SIGNALK_PARENT_ID])
    cg.add(parent.subscribe(var))
    cg.add(var.set_parent(parent))
    cg.add(var.setup())
