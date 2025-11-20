import esphome.codegen as cg

CODEOWNERS = ["@jtkm6"]
DEPENDENCIES = ["uart"]
AUTO_LOAD = ["select", "switch"]

daewoo_ac_ns = cg.esphome_ns.namespace("daewoo_ac")
DaewooAC = daewoo_ac_ns.class_("DaewooAC")
DaewooACVaneSelect = daewoo_ac_ns.class_("DaewooACVaneSelect")
DaewooACDisplaySwitch = daewoo_ac_ns.class_("DaewooACDisplaySwitch")
DaewooACUVLightSwitch = daewoo_ac_ns.class_("DaewooACUVLightSwitch")
DaewooACHorizontalSwingSwitch = daewoo_ac_ns.class_("DaewooACHorizontalSwingSwitch")
