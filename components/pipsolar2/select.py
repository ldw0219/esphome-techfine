import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import select
from esphome.const import CONF_ID, CONF_OPTIONS

# 1. 获取命名空间 (必须与 C++ 中的 namespace esphome::pipsolar2 匹配)
pipsolar2_ns = cg.esphome_ns.namespace("pipsolar2")

# 2. 获取类 (必须与 C++ 中的 class PipsolarSelect 匹配)
PipsolarSelect = pipsolar2_ns.class_("PipsolarSelect", select.Select, cg.Component)

# 3. 定义父组件 ID 常量
CONF_PIPSOLAR_ID = "pipsolar_id"

# 4. 定义配置 Schema
CONFIG_SCHEMA = select.SELECT_SCHEMA.extend(
    {
        cv.GenerateID(): cv.declare_id(PipsolarSelect),
        # 关联父组件 Pipsolar
        cv.Required(CONF_PIPSOLAR_ID): cv.use_id(pipsolar2_ns.class_("Pipsolar")),
    }
).extend(cv.COMPONENT_SCHEMA)

# 5. 代码生成函数
async def to_code(config):
    # 创建变量
    var = cg.new_Pvariable(config[CONF_ID])

    # 注册 Select 组件 (这里会自动处理 options，前提是 YAML 里写了 options)
    await select.register_select(var, config, options=config.get(CONF_OPTIONS, []))

    # 注册通用组件 (处理 update_interval 等)
    await cg.register_component(var, config)

    # 设置父组件指针
    parent = await cg.get_variable(config[CONF_PIPSOLAR_ID])
    cg.add(var.set_parent(parent))

    # 注意：如果你的 C++ 类里有 set_mapping 方法，需要在这里调用
    # 例如：
    # if CONF_MAPPING in config:
    #     mapping = config[CONF_MAPPING]
    #     cg.add(var.set_mapping(mapping))
