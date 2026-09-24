from esphome import automation
from esphome.automation import maybe_simple_id
import esphome.codegen as cg
from esphome.components import i2c
from esphome.components.audio_dac import AudioDac
import esphome.config_validation as cv
from esphome.const import CONF_ID
from esphome.types import ConfigType

CODEOWNERS = ["@mojansch"]
DEPENDENCIES = ["i2c"]

CONF_ROUTE = "route"

es8388_luxe_ns = cg.esphome_ns.namespace("es8388_luxe")

ES8388Luxe = es8388_luxe_ns.class_(
    "ES8388Luxe", AudioDac, cg.Component, i2c.I2CDevice
)

OutputRoute = es8388_luxe_ns.enum("OutputRoute")
OUTPUT_ROUTES = {
    "speaker": OutputRoute.OUTPUT_ROUTE_SPEAKER,
    "headphone": OutputRoute.OUTPUT_ROUTE_HEADPHONE,
}

ReinitializeAction = es8388_luxe_ns.class_("ReinitializeAction", automation.Action)
SetOutputRouteAction = es8388_luxe_ns.class_("SetOutputRouteAction", automation.Action)

CONFIG_SCHEMA = (
    cv.Schema({cv.GenerateID(): cv.declare_id(ES8388Luxe)})
    .extend(cv.COMPONENT_SCHEMA)
    .extend(i2c.i2c_device_schema(0x10))
)


async def to_code(config: ConfigType) -> None:
    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)
    await i2c.register_i2c_device(var, config)


@automation.register_action(
    "es8388_luxe.reinitialize",
    ReinitializeAction,
    maybe_simple_id({cv.GenerateID(): cv.use_id(ES8388Luxe)}),
    synchronous=True,
)
async def reinitialize_action_to_code(config, action_id, template_arg, args):
    var = cg.new_Pvariable(action_id, template_arg)
    await cg.register_parented(var, config[CONF_ID])
    return var


@automation.register_action(
    "es8388_luxe.set_output_route",
    SetOutputRouteAction,
    cv.maybe_simple_value(
        {
            cv.GenerateID(): cv.use_id(ES8388Luxe),
            cv.Required(CONF_ROUTE): cv.templatable(cv.enum(OUTPUT_ROUTES, lower=True)),
        },
        key=CONF_ROUTE,
    ),
    synchronous=True,
)
async def set_output_route_action_to_code(config, action_id, template_arg, args):
    var = cg.new_Pvariable(action_id, template_arg)
    await cg.register_parented(var, config[CONF_ID])
    template_ = await cg.templatable(config[CONF_ROUTE], args, OutputRoute)
    cg.add(var.set_route(template_))
    return var
