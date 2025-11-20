import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import climate, uart
from esphome.const import CONF_ID
from . import daewoo_ac_ns, DaewooAC

CONF_UPDATE_INTERVAL = "update_interval"
CONF_UART_ID = "uart_id"

DaewooAC = daewoo_ac_ns.class_("DaewooAC", climate.Climate, cg.Component)

CONFIG_SCHEMA = climate.climate_schema(DaewooAC).extend(
    {
        cv.Optional(CONF_UPDATE_INTERVAL, default="5s"): cv.positive_time_period_milliseconds,
        cv.Required(CONF_UART_ID): cv.use_id(uart.UARTComponent),
    }
).extend(cv.COMPONENT_SCHEMA)


async def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)
    await climate.register_climate(var, config)
    
    cg.add(var.set_update_interval(config[CONF_UPDATE_INTERVAL]))
    
    uart_component = await cg.get_variable(config[CONF_UART_ID])
    cg.add(var.set_uart(uart_component))

