import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import switch
from esphome.const import CONF_ENTITY_CATEGORY
from . import DaewooAC, daewoo_ac_ns

CONF_DAEWOO_AC_ID = "daewoo_ac_id"
CONF_DISPLAY = "display"
CONF_UV_LIGHT = "uv_light"
CONF_HORIZONTAL_SWING = "horizontal_swing"

DaewooACDisplaySwitch = daewoo_ac_ns.class_("DaewooACDisplaySwitch", switch.Switch, cg.Component)
DaewooACUVLightSwitch = daewoo_ac_ns.class_("DaewooACUVLightSwitch", switch.Switch, cg.Component)
DaewooACHorizontalSwingSwitch = daewoo_ac_ns.class_(
    "DaewooACHorizontalSwingSwitch", switch.Switch, cg.Component
)


CONFIG_SCHEMA = cv.Schema(
    {
        cv.GenerateID(CONF_DAEWOO_AC_ID): cv.use_id(DaewooAC),
        cv.Required(CONF_DISPLAY): switch.switch_schema(DaewooACDisplaySwitch),
        cv.Optional(CONF_UV_LIGHT): switch.switch_schema(DaewooACUVLightSwitch),
        cv.Optional(CONF_HORIZONTAL_SWING): switch.switch_schema(DaewooACHorizontalSwingSwitch),
    }
)


async def to_code(config):
    parent = await cg.get_variable(config[CONF_DAEWOO_AC_ID])
    display_config = config[CONF_DISPLAY]

    if CONF_ENTITY_CATEGORY in display_config:
        display_config = display_config.copy()
        del display_config[CONF_ENTITY_CATEGORY]

    display_switch = await switch.new_switch(display_config)
    await cg.register_component(display_switch, display_config)
    cg.add(display_switch.set_parent(parent))
    cg.add(parent.set_display_switch(display_switch))

    uv_config = config.get(CONF_UV_LIGHT)
    if uv_config is not None:
        if CONF_ENTITY_CATEGORY in uv_config:
            uv_config = uv_config.copy()
            del uv_config[CONF_ENTITY_CATEGORY]

        uv_switch = await switch.new_switch(uv_config)
        await cg.register_component(uv_switch, uv_config)
        cg.add(uv_switch.set_parent(parent))
        cg.add(parent.set_uv_light_switch(uv_switch))

    horizontal_swing_config = config.get(CONF_HORIZONTAL_SWING)
    if horizontal_swing_config is not None:
        if CONF_ENTITY_CATEGORY in horizontal_swing_config:
            horizontal_swing_config = horizontal_swing_config.copy()
            del horizontal_swing_config[CONF_ENTITY_CATEGORY]

        horizontal_swing_switch = await switch.new_switch(horizontal_swing_config)
        await cg.register_component(horizontal_swing_switch, horizontal_swing_config)
        cg.add(horizontal_swing_switch.set_parent(parent))


