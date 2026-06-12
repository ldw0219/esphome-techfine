import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import select
from esphome.const import CONF_ID, CONF_OPTIONS # 确保从这里导入基础常量

# 定义组件特有的常量
CONF_PIPSOLAR_ID = "pipsolar_id"

# 获取命名空间
pipsolar2_ns = cg.esphome_ns.namespace("pipsolar2")
PipsolarSelect = pipsolar2_ns.class_("PipsolarSelect", select.Select, cg.Component)

CONFIG_SCHEMA = select.SELECT_SCHEMA.extend(
    {
        cv.GenerateID(): cv.declare_id(PipsolarSelect),
        cv.Required(CONF_PIPSOLAR_ID): cv.use_id(pipsolar2_ns.class_("Pipsolar")),
        # 必须显式声明 options，否则 config[CONF_OPTIONS] 会报错
        cv.Required(CONF_OPTIONS): select.validate_options,
    }
).extend(cv.COMPONENT_SCHEMA)

async def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    # 注册 select 实体，这里会自动处理 options
    await select.register_select(var, config, options=config[CONF_OPTIONS])
    await cg.register_component(var, config)

    parent = await cg.get_variable(config[CONF_PIPSOLAR_ID])
    cg.add(var.set_parent(parent))
