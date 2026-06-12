import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import select
from esphome.const import CONF_ID, CONF_OPTIONS

# 1. 获取命名空间 (必须与 C++ 中的 namespace esphome::pipsolar2 匹配)
pipsolar2_ns = cg.esphome_ns.namespace("pipsolar2")

# 2. 获取类 (必须与 C++ 中的 class PipsolarSelect 匹配)
PipsolarSelect = pipsolar2_ns.class_("PipsolarSelect", select.Select, cg.Component)

# 3. 定义配置 Schema
CONF_PIPSOLAR_ID = "pipsolar_id" # 或者是你定义的父组件 ID 名称

CONFIG_SCHEMA = select.SELECT_SCHEMA.extend(
    {
        cv.GenerateID(): cv.declare_id(PipsolarSelect),
        cv.Required(CONF_PIPSOLAR_ID): cv.use_id(pipsolar2_ns.class_("Pipsolar")), # 关联父组件
        # 这里根据你的需求添加 options 等配置...
    }
).extend(cv.COMPONENT_SCHEMA)

# 4. 代码生成函数
async def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    await select.register_select(var, config, options=config[CONF_OPTIONS])
    await cg.register_component(var, config)

    # 关联父组件
    parent = await cg.get_variable(config[CONF_PIPSOLAR_ID])
    cg.add(var.set_parent(parent))

    # 如果有 mapping 配置，在这里通过 cg.add(var.set_mapping(...)) 传递进去
