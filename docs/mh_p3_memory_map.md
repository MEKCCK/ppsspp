# MHP3rd 内存标记表 / P3 Memory Map（带来源标注）

所有地址均为 **PSP 虚拟地址**（PPSSPP/overlay 直接读写的地址体系）。

## 来源标注（Source Tags）

| 标记 | 来源 | 说明 |
|---|---|---|
| **[本]** | 本项目（移植自 Alexander-Lancellott/MH-HP-Overlay-For-PSP-Emulator 并经作者 mod 交叉验证） | 我们自己使用的指针表/偏移 |
| **[load]** | Kurogami2134/mhp3reload loader 汇编（src/*.asm） | 模组加载器钩子 |
| **[modman]** | Kurogami2134/p3rdml_modman | 模组管理器部署配置 |
| **[hpbar]** | Kurogami2134/mhp3rd_monster_hp_bar | 怪物血条 mod |
| **[dmg]** | Kurogami2134/mhp3rd_dmg_numbers | 伤害数字 mod |
| **[sharp]** | Kurogami2134/p3rd_sharpness_indicator | 斩味指示 mod |
| **[item]** | Kurogami2134/p3rd_item_sets | 道具套装 mod |

---

## 1. 怪物数据（最核心）

### 怪物列表指针（本 overlay 用法）

| 游戏 | 列表指针（绝对地址） | 等价写法 | 来源 |
|---|---|---|---|
| MHP3rd ULJM05800 | `0x09DA9860` | `0x08800000 + 0x15A9860`（initial_pointer） | **[本]** + **[hpbar]**（`lw a0,(0x9DA9860)` 互证 ✓） |
| MHP3rd HD NPJB40001 | `0x0A2B0AE0` | `0x08800000 + 0x19B0AE0` | **[本]** |
| MHP2G/MHFU etc. | 见下"原版项目表" | — | **[本]** |

怪物列表：`指针[0..9]`（每项 4 字节），非零即现存怪物 → 怪物结构体。

### 怪物结构体偏移（MHP3rd，与 hpbar mod 互证）

| 偏移 | 字段 | 来源 |
|---|---|---|
| `+0x62` | 怪物名（1 字节，索引到怪表） | **[本]** |
| `+0x246` | **当前 HP（u16/u32）** | **[本]** + **[hpbar]**（`lh a1,0x246(a0)` ✓） |
| `+0x288` | **最大 HP** | **[本]** + **[hpbar]**（`lh a2,0x288(a0)` ✓） |
| `+0xD4` | 尺寸倍率（u16） | **[本]** |
| `+0x23C/+0x252` | 毒 当前/阈值 | **[本]** |
| `+0x24E/+0x24C` | 眠 当前/阈值 | **[本]** |
| `+0x25A/+0x258` | 麻痹 当前/阈值 | **[本]** |
| `+0xC5C/+0xC5E` | 眩晕 当前/阈值 | **[本]** |
| `+0xBC8` | 怒计时（u16，/60=秒） | **[本]** |

> 待挖：怪物坐标 x/y/z（伤害飘字、小地图需要）—— 从结构体附近或 `0x0886xxxx` 代码区引用它的地方找。

### 怪表（名字字节 → 怪物，137 条）

见 `UI/monster_tables.inc`（MHF/MHFU/MHP3RD 三代，来自原版项目生成）。

### 其他游戏版本指针表（[本]，来自 MH-HP-Overlay 项目）

| 游戏/Disc | initial_pointer | 名/HP/最大HP/尺寸 偏移 | 状态 毒/眠/麻/晕/怒 |
|---|---|---|---|
| MHF ULES00318 / ULUS10084 / ULJM05066 | `0x1254D70/0x1253F70/0x1253570` | `+0x210 / +0x312 / +0x43C / +0x2A4` | `+0x3A8/+0x46C, +0x462/+0x460, +0x474/+0x472, —, +0x580` |
| MHF2 ULES00851 / ULUS10266 / ULJM05156 | `0x127AD70/0x12799F0/0x1278E70` | `+0x1E8 / +0x2E2 / +0x41E / +0x274` | `+0x388/+0x450, +0x446/+0x444, +0x458/+0x456, +0x440/+0x55E, +0x564` |
| MHFU ULES01213 / ULUS10391 / ULJM05500 | `0x1412140/0x1412240/0x140D3C0` | `+0x1E8 / +0x2E4 / +0x41E / +0x274` | `+0x388/+0x450, +0x446/+0x444, +0x458/+0x456, +0x440/+0x566, +0x56C` |
| MHP3RD ULJM05800 / NPJB40001 | `0x15A9860 / 0x19B0AE0` | `+0x62 / +0x246 / +0x288 / +0xD4` | `+0x23C/+0x252, +0x24E/+0x24C, +0x25A/+0x258, +0xC5C/+0xC5E, +0xBC8` |

---

## 2. 任务 / 游戏状态

| 地址 | 含义 | 来源 |
|---|---|---|
| `0x09C57CA0` | 任务状态（in-quest 标志，0x656D6167 魔数附近） | **[hpbar]** **[dmg]** |
| `0x09BAC044` | 返回中/任务阶段字节（<3 判定） | **[hpbar]** |
| `0x08AB49EC` | 加载画面标志 | **[hpbar]** |

## 3. 玩家数据

| 地址 | 含义 | 来源 |
|---|---|---|
| `0x08B24979` | player_area（玩家状态区） | **[dmg]** |
| `0x09B49234` | 手持武器结构体（EQUIPPED_WEAPON） | **[sharp]** |
| `0x09B49234 + 0x5CC` | 斩味（sharpness）当前值 | **[sharp]** |
| `0x0897D728` | 斩味表（武器各色斩味数据） | **[sharp]** |

## 4. 物品

| 地址 | 含义 | 来源 |
|---|---|---|
| `0x09B4C244` | 箱子 ITEM_BOX | **[item]** |
| `0x09BA8D4A` | 口袋 1 ITEM_POUCH1 | **[item]** |
| `0x09B4D9B4` | 口袋 2 ITEM_POUCH2 | **[item]** |
| `0x09CD0440` | 给道具函数 GIVE_ITEM | **[item]** |
| `0x09BB7A64` | 按键保持检测 CONTROL_HOLD | **[item]** |

## 5. 渲染 / 相机（伤害飘字、小地图、自由视角用）

| 地址 | 含义 | 来源 |
|---|---|---|
| `0x09B486B0` | ViewMatrix（视图矩阵，世界→屏幕投影） | **[dmg]** |
| `0x088E6D64 / 0x088EBAB8` | 主渲染 hook / 返回 | **[dmg]** |
| `0x088EAA64` | printf（画面打印文字） | **[dmg]** |
| `0x09C750FC` 系列 | 伤害数字 ADD hook/返回 | **[dmg]** |
| `0x09ADB910` | 打印设置区 PRINT_SETTINGS | **[dmg]** |
| `0x09C1EC70` | 伤害判定 CHECK | **[dmg]** |
| `0x08960CF8` | sceGeListEnQueue（绘制队列） | **[hpbar]** |
| `0x08800FF0` | 血条 mod 自用绘制区 | **[hpbar]** |

> ViewMatrix + player_area 是之前"伤害数字"需求的两个关键锚点 —— 想做随时能继续。

## 6. 文件系统 / 模组加载（loader 管线）

| 名称 | P3 原版 | P3HD | 来源 |
|---|---|---|---|
| EBOOT_LOAD | `0x0880134C` | `0x0880134C` | **[load]** |
| PRELOAD_HOOK | `0x088215D4` | `0x08821818` | **[load]** |
| PRELOAD_INIT | `0x089E02A0` | `0x089DFE60` | **[load]** |
| 文件读 hook | `0x0886242C` | `0x0886365C` | **[load]** |
| 文件寻址 hook | `0x08864390` | `0x088655C0` | **[load]** |
| 解密 hook | `0x088641F0` | `0x08865420` | **[load]** |
| 模组表工作区 | `0x09FA2100`（+0x800 装载表） | `0x083B5600` | **[load]** |
| sceIo 导入表 | `0x08960A00` 起 | `0x08965690` 起 | **[load]** **[item]** |
| 替换文件目录 | `ms0:/P3RDML/FILES/` | `ms0:/P3RDHDML/FILES/` | **[load]** **[modman]** |
| 动画数据区 | `0x099C0000`（anim offset） | — | **[modman]** |

---

## 7. 交叉验证记录

- `0x09DA9860`（hpbar 怪物列表指针）== 本项目 `0x08800000 + 0x15A9860` ✅
- 怪物 HP 偏移 `+0x246`、最大 HP `+0x288`（hpbar）== 本项目 MHP3RD 表 ✅
- sceIo 导入表地址（load/item 两处仓库一致）✅

## 8. 待挖（TODO）

- 怪物坐标 x/y/z（结构体内/附近）
- 任务信息区（任务名/时限/报酬）
- 玩家属性（攻防/技能）
- 肉质/疲劳表（file id 体系，配合 `MHP3rd-Game-FIle-List`）
- 其他游戏（MHF2/MHFU）的玩家/物品区（目前只有怪物数据）