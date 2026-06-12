import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import select
from esphome.const import CONF_ID, CONF_OPTIONS

# 1. 手动定义常量，不再依赖 const.py
CONF_PIPSOLAR_ID = "pipsolar_id" 

# 2. 获取命名空间 (注意：这里要和 C++ 的 namespace 一致，你之前的代码是 pipsolar2)
pipsolar2_ns = cg.esphome_ns.namespace("pipsolar2")
PipsolarSelect = pipsolar2_ns.class_("PipsolarSelect", select.Select, cg.Component)

# 3. 定义 Schema
CONFIG_SCHEMA = select.SELECT_SCHEMA.extend(
    {
        cv.GenerateID(): cv.declare_id(PipsolarSelect),
        cv.Required(CONF_PIPSOLAR_ID): cv.use_id(pipsolar2_ns.class_("Pipsolar")),
        cv.Required(CONF_OPTIONS): select.validate_options,
    }
)

async def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    await select.register_select(var, config, options=config[CONF_OPTIONS])
    await cg.register_component(var, config)

    parent = await cg.get_variable(config[CONF_PIPSOLAR_ID])
    cg.add(var.set_parent(parent))
