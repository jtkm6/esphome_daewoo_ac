import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import select
from esphome.const import CONF_ENTITY_CATEGORY, ENTITY_CATEGORY_NONE
from . import daewoo_ac_ns, DaewooAC

CONF_DAEWOO_AC_ID = "daewoo_ac_id"
CONF_VERTICAL_VANE = "vertical_vane"
CONF_OPTIONS = "options"

DaewooACVaneSelect = daewoo_ac_ns.class_("DaewooACVaneSelect", select.Select, cg.Component)

DEFAULT_VERTICAL_VANE_OPTIONS = [
    "Swing",
    "Up",
    "Up & Medium",
    "Medium",
    "Medium & Down",
    "Down",
    "Static",
]


def vane_select_schema(default_options):
    """Create schema for vane select with optional custom options."""
    # Use ENTITY_CATEGORY_NONE to make these appear in Controls section
    schema = select.select_schema(DaewooACVaneSelect)
    schema = schema.extend({
        cv.Optional(CONF_OPTIONS, default=default_options): cv.All(
            cv.ensure_list(cv.string_strict),
            cv.Length(min=7, max=7)
        ),
    })
    return schema


CONFIG_SCHEMA = cv.Schema(
    {
        cv.GenerateID(CONF_DAEWOO_AC_ID): cv.use_id(DaewooAC),
        cv.Optional(CONF_VERTICAL_VANE): vane_select_schema(DEFAULT_VERTICAL_VANE_OPTIONS),
    }
)


async def to_code(config):
    parent = await cg.get_variable(config[CONF_DAEWOO_AC_ID])
    
    if CONF_VERTICAL_VANE in config:
        vane_config = config[CONF_VERTICAL_VANE]
        options = vane_config.get(CONF_OPTIONS, DEFAULT_VERTICAL_VANE_OPTIONS)
        # Remove entity_category if it exists to make it appear in Controls
        if CONF_ENTITY_CATEGORY in vane_config:
            vane_config = vane_config.copy()
            del vane_config[CONF_ENTITY_CATEGORY]
        var = await select.new_select(vane_config, options=options)
        await cg.register_component(var, vane_config)
        cg.add(var.set_parent(parent))
        cg.add(var.set_vane_type(True))  # True for vertical
        cg.add(parent.set_vertical_vane_select(var))

