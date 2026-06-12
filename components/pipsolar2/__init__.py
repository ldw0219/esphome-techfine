import logging
import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.const import CONF_ID
from esphome.components import uart

_LOGGER = logging.getLogger(__name__)

# 兼容旧配置的字段重命名工具函数（来自第二份代码）
def deprecated_renames(renames: dict[str, str | tuple[str, str]]):
    """Accept old→new or old→(new, custom_message) pairs."""

    def validator(config):
        config = config.copy()
        for old, spec in renames.items():
            if old not in config:
                continue
            if isinstance(spec, tuple):
                new, msg = spec
            else:
                new = spec
                msg = f"'{old}' is deprecated, use '{new}' instead. Will be removed in a future release."
            _LOGGER.warning(msg)
            config[new] = config.pop(old)
        return config

    return validator

# 全局配置：依赖、作者、自动加载、多实例
DEPENDENCIES = ["uart"]
CODEOWNERS = ["@andreashergert1984"]
# 合并两边 AUTO_LOAD，新增 select
AUTO_LOAD = ["binary_sensor", "text_sensor", "sensor", "switch", "output", "select"]
MULTI_CONF = True

# 通用ID常量
CONF_PIPSOLAR_ID = "pipsolar_id"

# 命名空间 + 组件类（使用第二份 pipsolar 命名空间）
pipsolar_ns = cg.esphome_ns.namespace("pipsolar")
PipsolarComponent = pipsolar_ns.class_("Pipsolar", cg.Component)

# 子组件通用引用 Schema
PIPSOLAR_COMPONENT_SCHEMA = cv.Schema(
    {
        cv.Required(CONF_PIPSOLAR_ID): cv.use_id(PipsolarComponent),
    }
)

# 主配置 Schema：合并版本校验、轮询、UART
CONFIG_SCHEMA = cv.All(
    cv.require_esphome_version(2026, 3, 0),
    cv.Schema({cv.GenerateID(): cv.declare_id(PipsolarComponent)})
    .extend(cv.polling_component_schema("1s"))
    .extend(uart.UART_DEVICE_SCHEMA),
)

# 代码生成逻辑（使用新版 async/await 语法）
async def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)
    await uart.register_uart_device(var, config)
