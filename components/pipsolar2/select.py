# pipsolar2/select.py
import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import select
from esphome.const import CONF_ID

from . import Pipsolar2, pipsolar2_ns  # <--- 注意这里引用的是 pipsolar2_ns

CONF_PIPSOLAR_SELECT = "pipsolar_select"

PipsolarSelect = pipsolar2_ns.class_("PipsolarSelect", select.Select, cg.Component)

CONFIG_SCHEMA = select.select_schema(PipsolarSelect).extend(
    {
        cv.GenerateID(): cv.declare_id(PipsolarSelect),
    }
)

async def to_code(config):
    var = await select.new_select(config, options=[]) # 这里的 options 可能需要动态获取，视具体逻辑而定
    await cg.register_component(var, config)
