# MHP3rd 内存标记表 / P3 Memory Map（带来源标注）

所有地址均为 **PSP 虚拟地址**（PPSSPP/overlay 直接读写的地址体系）。

## 来源标注（Source Tags）

| 标记 | 来源 | 说明 |
|---|---|---|
| **[orig]** | Alexander-Lancellott/**MH-HP-Overlay-For-PSP-Emulator**（Python 原版 overlay 项目） | 指针表与结构偏移的真正来源；本项目仅移植/集成，无原创 |
| **[load]** | Kurogami2134/mhp3reload loader 汇编（src/*.asm） | 模组加载器钩子 |
| **[modman]** | Kurogami2134/p3rdml_modman | 模组管理器部署配置 |
| **[hpbar]** | Kurogami2134/mhp3rd_monster_hp_bar | 怪物血条 mod |
| **[dmg]** | Kurogami2134/mhp3rd_dmg_numbers | 伤害数字 mod |
| **[sharp]** | Kurogami2134/p3rd_sharpness_indicator | 斩味指示 mod |
| **[item]** | Kurogami2134/p3rd_item_sets | 道具套装 mod |
| **[val]** | 本项目（MEKCCK/ppsspp 内置 overlay） | 仅做**交叉验证/集成**，不产生新地址 |

> 声明：本表所有内存知识均来自上述两个原作者的**公开项目**，本项目（PPSSPP 内置
> overlay）只是把这些已公开的数据**移植进模拟器并交叉核对**，没有一条地址是本项目
> 原创发现。伤害数字、血条等能力的归属均为 Kurogami2134 的 mhp3rd_dmg_numbers /
> mhp3rd_monster_hp_bar。

---

## 0. 地址性质与偏移说明（重要）

- 本表全部为 **PSP 虚拟地址**。PSP 内存映射由硬件/系统固定（用户 RAM 基址
  `0x08800000`），**实体机与 PPSSPP 一致** —— mhp3reload 等 mod 的 MIPS 补丁
  就是在真机上用这些地址打进去的。
- **"偏移问题"只存在于"外部工具读模拟器进程内存"**：PPSSPP 把 PSP 内存映射到
  宿主进程的任意地址，外部工具（原版 Python overlay、Cheat Engine 等）必须再加
  一个 `base_address` 换算（原版 overlay 用 `SendMessageW(0xB118)` 拿它）。
- **本项目（内置 overlay）直接以 PSP 虚拟地址读写 `Memory::ReadUnchecked_*`**，
  PPSSPP 内部完成映射，**不存在任何偏移**。
- 需要按"游戏版本"区分的：
  - 原版 `ULJM05800` vs HD `NPJB40001`：EBOOT 布局不同 → 地址不同（本表已双列，
    "未提供"= 来源 mod 只有该版本实现）。
  - 同一游戏同版本：实体机 / PPSSPP 地址相同。

## 1. 怪物数据（最核心）

### 怪物列表指针（本 overlay 用法）

| 游戏 | 列表指针（绝对地址） | 等价写法 | 来源 |
|---|---|---|---|
| MHP3rd ULJM05800 | `0x09DA9860` | `0x08800000 + 0x15A9860`（initial_pointer） | **[orig]** + **[hpbar]**（`lw a0,(0x9DA9860)` 互证 ✓） |
| MHP3rd HD NPJB40001 | `0x0A2B0AE0` | `0x08800000 + 0x19B0AE0` | **[orig]** |
| MHP2G/MHFU etc. | 见下"原版项目表" | — | **[orig]** |

怪物列表：`指针[0..9]`（每项 4 字节），非零即现存怪物 → 怪物结构体。

### 怪物结构体偏移（MHP3rd，与 hpbar mod 互证）

| 偏移 | 字段 | 来源 |
|---|---|---|
| `+0x62` | 怪物名（1 字节，索引到怪表） | **[orig]** |
| `+0x246` | **当前 HP（u16/u32）** | **[orig]** + **[hpbar]**（`lh a1,0x246(a0)` ✓） |
| `+0x288` | **最大 HP** | **[orig]** + **[hpbar]**（`lh a2,0x288(a0)` ✓） |
| `+0xD4` | 尺寸倍率（u16） | **[orig]** |
| `+0x23C/+0x252` | 毒 当前/阈值 | **[orig]** |
| `+0x24E/+0x24C` | 眠 当前/阈值 | **[orig]** |
| `+0x25A/+0x258` | 麻痹 当前/阈值 | **[orig]** |
| `+0xC5C/+0xC5E` | 眩晕 当前/阈值 | **[orig]** |
| `+0xBC8` | 怒计时（u16，/60=秒） | **[orig]** |

> 待挖：怪物坐标 x/y/z（伤害飘字、小地图需要）—— 从结构体附近或 `0x0886xxxx` 代码区引用它的地方找。

### 怪表（名字字节 → 怪物，137 条）

见 `UI/monster_tables.inc`（MHF/MHFU/MHP3RD 三代，来自原版项目生成）。

### 其他游戏版本指针表（[orig]，Alexander-Lancellott 的 Python 原版 overlay 项目，本项目仅移植）

| 游戏/Disc | initial_pointer | 名/HP/最大HP/尺寸 偏移 | 状态 毒/眠/麻/晕/怒 |
|---|---|---|---|
| MHF ULES00318 / ULUS10084 / ULJM05066 | `0x1254D70/0x1253F70/0x1253570` | `+0x210 / +0x312 / +0x43C / +0x2A4` | `+0x3A8/+0x46C, +0x462/+0x460, +0x474/+0x472, —, +0x580` |
| MHF2 ULES00851 / ULUS10266 / ULJM05156 | `0x127AD70/0x12799F0/0x1278E70` | `+0x1E8 / +0x2E2 / +0x41E / +0x274` | `+0x388/+0x450, +0x446/+0x444, +0x458/+0x456, +0x440/+0x55E, +0x564` |
| MHFU ULES01213 / ULUS10391 / ULJM05500 | `0x1412140/0x1412240/0x140D3C0` | `+0x1E8 / +0x2E4 / +0x41E / +0x274` | `+0x388/+0x450, +0x446/+0x444, +0x458/+0x456, +0x440/+0x566, +0x56C` |
| MHP3RD ULJM05800 / NPJB40001 | `0x15A9860 / 0x19B0AE0` | `+0x62 / +0x246 / +0x288 / +0xD4` | `+0x23C/+0x252, +0x24E/+0x24C, +0x25A/+0x258, +0xC5C/+0xC5E, +0xBC8` |

---

## 2. 任务 / 游戏状态

| 含义 | ULJM05800（原版/NOHD） | NPJB40001（HD） | 来源 |
|---|---|---|---|
| 任务状态（in-quest 标志，0x656D6167 魔数附近） | `0x09C57CA0` | `0x0A05E620` | **[hpbar]**(仅原版) **[dmg]** |
| 返回中/任务阶段字节（<3 判定） | `0x09BAC044` | 未提供 | **[hpbar]** |
| 加载画面标志 | `0x08AB49EC` | 未提供 | **[hpbar]** |

## 3. 玩家数据

| 含义 | ULJM05800（原版/NOHD） | NPJB40001（HD） | 来源 |
|---|---|---|---|
| player_area（玩家状态区） | `0x08B24979` | `0x08B2B139` | **[dmg]** |
| 手持武器结构体（EQUIPPED_WEAPON） | `0x09B49234` | `0x09F4FCE4` | **[sharp]** |
| 斩味当前值 | `0x09B49234 + 0x5CC` | `0x09F4FCE4 + 0x5DC` | **[sharp]** |
| 斩味表（武器各色斩味数据） | `0x0897D728` | `0x08983060` | **[sharp]** |
| 精灵/纹理显示信息 | `0x08B268DC` | `0x08B2D09C` | **[sharp]** |

## 4. 物品

| 含义 | ULJM05800（原版/NOHD） | NPJB40001（HD） | 来源 |
|---|---|---|---|
| 箱子 ITEM_BOX | `0x09B4C244` | `0x09F52CF4` | **[item]** |
| 口袋 1 ITEM_POUCH1 | `0x09BA8D4A` | `0x09FAF7FE` | **[item]** |
| 口袋 2 ITEM_POUCH2 | `0x09B4D9B4` | `0x09F54464` | **[item]** |
| 给道具函数 GIVE_ITEM | `0x09CD0440` | `0x0A0D20A8` | **[item]** |
| 按键保持检测 CONTROL_HOLD | `0x09BB7A64` | `0x09FBE764` | **[item]** |
| 套装 hook | `0x09D48EE0` | `0x0A14AAE0` | **[item]** |

## 5. 渲染 / 相机（伤害飘字、小地图、自由视角用）

| 含义 | ULJM05800（原版/NOHD） | NPJB40001（HD） | 来源 |
|---|---|---|---|
| ViewMatrix（视图矩阵，世界→屏幕投影） | `0x09B486B0` | `0x09F4F120` | **[dmg]** |
| 主渲染 hook / 返回 | `0x088E6D64 / 0x088EBAB8` | `0x088E881C / 0x088EE410` | **[dmg]** |
| printf（画面打印文字） | `0x088EAA64` | `0x088EC51C` | **[dmg]** |
| 伤害数字 ADD hook/返回 | `0x09C750FC / 0x09C953E0` | `0x0A07BA7C / 0x0A09BD60` | **[dmg]** |
| 打印设置区 PRINT_SETTINGS | `0x09ADB910` | `0x09EE2350` | **[dmg]** |
| 伤害判定 CHECK | `0x09C1EC70` | `0x0A025608` | **[dmg]** |
| sceGeListEnQueue（绘制队列） | `0x08960CF8` | 未提供 | **[hpbar]** |
| 血条 mod 自用绘制区 | `0x08800FF0` | 未提供 | **[hpbar]** |

> ViewMatrix + player_area 是之前"伤害数字"需求的两个关键锚点（都有原版/HD 两套）—— 想做随时能继续。

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

## 7. 交叉验证记录（[val] 本项目所做工作的全部内容）

- `0x09DA9860`（hpbar 怪物列表指针）== 本项目 `0x08800000 + 0x15A9860` ✅
- 怪物 HP 偏移 `+0x246`、最大 HP `+0x288`（hpbar）== 本项目 MHP3RD 表 ✅
- sceIo 导入表地址（load/item 两处仓库一致）✅

## 8. 待挖（TODO）

- 怪物坐标 x/y/z（结构体内/附近）
- 任务信息区（任务名/时限/报酬）
- 玩家属性（攻防/技能）
- 肉质/疲劳表（file id 体系，配合 `MHP3rd-Game-FIle-List`）
- 其他游戏（MHF2/MHFU）的玩家/物品区（目前只有怪物数据）