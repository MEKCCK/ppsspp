# MHP3rd 内存标记表 / P3 Memory Map

> 来源：mhp3reload（Kurogami2134/mhp3reload）的 loader 汇编源码
> （`src/main.asm`、`src/no_hd.asm`、`src/hd_ver.asm`、`src/preload*.asm`）
> 与 p3rdml_modman（Kurogami2134/p3rdml_modman）的部署配置。
>
> 用途：给 MH HP Overlay（和任何想逆 PSP 版 MHP3 内存的人）作参考，
> 标注已知的内存区域用途。所有地址均为 **PSP 虚拟地址**（overlay 直接读写的那套）。

## PSP 内存总览（与本项目相关部分）

| 范围 | 含义 |
|---|---|
| `0x08800000` | **用户内存基址**（`PSP_GetUserMemoryBase()`），overlay 的 `0x08800000 + initial_pointer` 从这里开始 |
| `0x08800000 – 0x09FFFFFF` | 游戏主 RAM（SLIM/64MB 模式含扩展区） |
| `0x09F00000 – 0x0BFFFFFF` | 扩展 RAM（PSP Slim 额外 32MB），loader/es 工具常用 |
| `0x08960A00` 附近 | 游戏内的 sceIo 导入表（自实现 syscall 跳转） |

## P3 原版 ULJM05800（noHD，P3RDML）

loader（no_hd.asm）标注的钩子/入口：

| 名称 | 地址 | 说明 |
|---|---|---|
| EBOOT_LOAD | `0x0880134C` | 引导装载入口 |
| PRELOAD_HOOK | `0x088215D4` | 启动预加载挂钩 |
| PRELOAD_INIT | `0x089E02A0` | 预加载初始化（数据段 0x089Exxxx） |
| MOD_ENTRY_ADD | `0x09FA2100` | loader 模组表工作区（扩展 RAM） |
| MOD_LOAD_ADD | `0x09FA2100 + 0x800` | 模组装载表（预留 256 项） |
| SIZE_LOAD_HOOK | `0x08863CB8` | 文件大小读取（文件替换拦截点） |
| READ_HOOK | `0x0886242C` | 文件读取（解密后数据）拦截点 |
| SEEK_HOOK | `0x08864390` | 文件定位拦截点 |
| CRYPTO_HOOK | `0x088641F0` | 文件解密钩子 |
| CRYPTO_CONT | `0x08863998` | 解密流程续 |
| SIZE_CHECK_SKIP | `0x088642E8` | 尺寸校验跳过 |
| CONT_SEEK_PATCH | `0x08864374` | 连续 seek 补丁 |

sceIo 导入表（`sceIoWrite`…`sceIoOpen` 等）：`0x08960A00` 起每项 8 字节。

动画数据区起始：`0x099C0000`（p3rdml_modman 的 `anim_start_offset`，NOHD）。

## P3 HD NPJB40001（P3HDML）

| 名称 | 地址 |
|---|---|
| EBOOT_LOAD | `0x0880134C` |
| PRELOAD_HOOK | `0x08821818` |
| PRELOAD_INIT | `0x089DFE60` |
| MOD_ENTRY_ADD | `0x083B5600`（HD 布局差异大，注意） |
| MOD_LOAD_ADD | `0x083B5600 + 0x800` |
| SIZE_LOAD_HOOK | `0x08864EE8` |
| READ_HOOK | `0x0886365C` |
| SEEK_HOOK | `0x088655C0` |
| CRYPTO_HOOK | `0x08865420` |
| CRYPTO_CONT | `0x08864BC8` |
| SIZE_CHECK_SKIP | `0x08865518` |
| CONT_SEEK_PATCH | `0x088655A4` |

sceIo 导入表：`0x08965690` 起。

## 文件替换机制（与文件表的关系）

- 替换文件放在 `ms0:/P3RDML/FILES/`（HD：`ms0:/P3RDHDML/FILES/`；FUC：`PSP/SAVEDATA/FUCDAT/FILES/`）。
- modelloader.asm 把文件 id 以 **十六进制字符串** 命名 → 文件 id 是游戏文件索引表的下标。
- loader 通过 READ_HOOK / SEEK_HOOK / CRYPTO_HOOK 拦"读–寻址–解密"管线，命中 id 即返回替换内容。
- 意义：**游戏的资源以索引表方式组织**，顺着文件表可以定位怪物模型/数值/肉质等数据文件（比纯内存扫描更根本）。

## 模组二进制格式（mhp3reload 消费的，补丁生态的核心）

`ms0:/P3RDML/mods.bin`：路径表，每项 `U8 长度 + "/名字"`（0 结尾），末尾 `0xFF`。
模组文件：`Int 版本` + 块序列：
- `块-1` 结束
- `块 0` Patch：`Load 地址 + 长度(最高位=边载边运行) + 内容`
- `块 1` Main：`长度 + 4 字节 ModID + 内容`
- `块 2` Hook：`Hook 地址 + U16 偏移 + hook op(0x08 j / 0x0C jal) + nop`
- `块 3` Init：`长度 + 内容`

Patch 块的 Load 地址就是作者"标好"的数据/代码区入口 —— 社区 code 模组等于现成的内存标注，可反推更多数据结构。

## 与本 overlay 的关系

- overlay 直接读取 `0x08800000 + initial_pointer[game_id]` 的怪物链表（指针 → 结构体：名字/H P/尺寸/异常状态）。
- 上表钩子区（`0x0886xxxx`、`0x089Exxxx`）是游戏代码/数据段边界，逆向怪物坐标、疲劳值、任务数据时从这些边界内搜更快。
- 已知钩子地址（PRELOAD_INIT 数据段等）可作为扫描怪物结构时的锚点。