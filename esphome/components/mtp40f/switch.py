import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import switch, uart
from esphome.const import CONF_ID

mtp40f_ns = cg.esphome_ns.namespace("mtp40f")
MTP40FComponent = mtp40f_ns.class_("MTP40FComponent", cg.PollingComponent, uart.UARTDevice)
MTP40FSelfCalibrationSwitch = mtp40f_ns.class_("MTP40FSelfCalibrationSwitch", switch.Switch, cg.Component)

CONFIG_SCHEMA = switch.switch_schema(MTP40FSelfCalibrationSwitch).extend({
    cv.GenerateID(): cv.declare_id(MTP40FSelfCalibrationSwitch),
    cv.Required("mtp40f_id"): cv.use_id(MTP40FComponent),
})

async def to_code(config):
    paren = await cg.get_variable(config["mtp40f_id"])
    var = await switch.new_switch(config)
    cg.add(var.set_parent(paren))
    await cg.register_component(var, config)
