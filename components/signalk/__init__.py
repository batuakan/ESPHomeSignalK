import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.const import CONF_ID, CONF_PORT

CODEOWNERS = ["@batuakan"]
DEPENDENCIES = ["wifi"]

CONF_HOST = "host"

signalk_ns = cg.esphome_ns.namespace("signalk")
signalk = signalk_ns.class_("SignalK", cg.PollingComponent)

CONFIG_SCHEMA = cv.Schema(
    {
        cv.GenerateID(): cv.declare_id(signalk),
        cv.Required(CONF_HOST): cv.string,
        cv.Optional(CONF_PORT, default=3000): cv.int_,
    }
)


async def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)
    cg.add(var.set_host(config[CONF_HOST]))
    cg.add(var.set_port(config[CONF_PORT]))

    cg.add(var.set_setup_priority(100))
    cg.add_library("WiFiClientSecure", None)
    cg.add_library("HTTPClient", None)
    cg.add_library("gilmaimon/ArduinoWebsockets", "0.5.4")
    cg.add_library("bblanchon/ArduinoJson", "7.3.1")
