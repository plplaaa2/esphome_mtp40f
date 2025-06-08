from esphome import automation
from esphome.automation import maybe_simple_id
import esphome.codegen as cg
from esphome.components import sensor, uart
import esphome.config_validation as cv
from esphome.const import (
    CONF_CO2,
    CONF_ID,
    DEVICE_CLASS_CARBON_DIOXIDE,
    ICON_MOLECULE_CO2,
    STATE_CLASS_MEASUREMENT,
    UNIT_PARTS_PER_MILLION,
    UNIT_HECTOPASCAL,  # 대기압 단위 hPa
)

DEPENDENCIES = ["uart"]

CONF_SELF_CALIBRATION = "self_calibration"
CONF_WARMUP_TIME = "warmup_time"
CONF_AIR_PRESSURE_REFERENCE = "air_pressure_reference"
CONF_EXTERNAL_AIR_PRESSURE = "external_air_pressure"

mtp40f_ns = cg.esphome_ns.namespace("mtp40f")
MTP40FComponent = mtp40f_ns.class_("MTP40FComponent", cg.PollingComponent, uart.UARTDevice)
MTP40FCalibrate400ppmAction = mtp40f_ns.class_("MTP40FCalibrate400ppmAction", automation.Action)

CONFIG_SCHEMA = (
    cv.Schema(
        {
            cv.GenerateID(): cv.declare_id(MTP40FComponent),
            cv.Required(CONF_CO2): sensor.sensor_schema(
                unit_of_measurement=UNIT_PARTS_PER_MILLION,
                icon=ICON_MOLECULE_CO2,
                accuracy_decimals=0,
                device_class=DEVICE_CLASS_CARBON_DIOXIDE,
                state_class=STATE_CLASS_MEASUREMENT,
            ),
            cv.Optional(CONF_EXTERNAL_AIR_PRESSURE): cv.use_id(sensor.Sensor),
        
            ),
            cv.Optional(CONF_AIR_PRESSURE_REFERENCE): sensor.sensor_schema(
                unit_of_measurement=UNIT_HECTOPASCAL,
                icon="mdi:gauge",
                accuracy_decimals=0,
                device_class="pressure",
                state_class=STATE_CLASS_MEASUREMENT,
            ),
            cv.Optional(CONF_SELF_CALIBRATION, default=True): cv.boolean,
            cv.Optional(CONF_WARMUP_TIME, default="60s"): cv.positive_time_period_seconds,
        }
    )
    .extend(cv.polling_component_schema("60s"))
    .extend(uart.UART_DEVICE_SCHEMA)
)

async def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)
    await uart.register_uart_device(var, config)

    if CONF_CO2 in config:
        sens = await sensor.new_sensor(config[CONF_CO2])
        cg.add(var.set_co2_sensor(sens))

    if CONF_AIR_PRESSURE_REFERENCE in config:
        sens = await sensor.new_sensor(config[CONF_AIR_PRESSURE_REFERENCE])
        cg.add(var.set_air_pressure_reference_sensor(sens))
     if CONF_EXTERNAL_AIR_PRESSURE in config:
        sens = await cg.get_variable(config[CONF_EXTERNAL_AIR_PRESSURE])

    cg.add(var.set_external_air_pressure_sensor(sens))
    cg.add(var.set_self_calibration_enabled(config[CONF_SELF_CALIBRATION]))
    cg.add(var.set_warmup_seconds(config[CONF_WARMUP_TIME].total_seconds))

# === 400ppm zero calibration 액션 ===
CALIBRATE_400PPM_ACTION_SCHEMA = maybe_simple_id(
    {
        cv.Required(CONF_ID): cv.use_id(MTP40FComponent),
    }
)

@automation.register_action(
    "mtp40f.calibrate_400ppm", MTP40FCalibrate400ppmAction, CALIBRATE_400PPM_ACTION_SCHEMA
)
async def mtp40f_calibrate_400ppm_to_code(config, action_id, template_arg, args):
    paren = await cg.get_variable(config[CONF_ID])
    return cg.new_Pvariable(action_id, template_arg, paren)
