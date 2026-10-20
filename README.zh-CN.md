# Endfield No-Collide（终末地 角色防碰撞插件）

[English](README.md) | **简体中文**

《明日方舟：终末地》的小插件：让我方干员之间可以互相穿过、贴近站位，方便在拍照模式里摆合影。**敌人不受影响**：敌人仍然会阻挡你、推开你、打到你。

> ⚠️ 非官方粉丝制作的工具，与鹰角网络 / GRYPHLINE 无关。使用前请先阅读文末的[免责声明](#免责声明)。

## 功能

- 我方干员之间移动时不再互相阻挡，重叠时也不会被互相推开。
- 只改变"干员与干员"之间的碰撞；敌人、NPC、地形、受击判定和子弹全部照常工作。
- **每次启动游戏默认关闭**，按 **F9** 开启 / 关闭（高音"嘀"= 开，低音 = 关）。
- 每次启动时按名字查找所需的游戏代码，游戏小更新通常不会失效；如果更新后找不到，会在日志里写明并且不做任何修改，不会让游戏崩溃。
- 轻量：没有悬浮面板，不扫描场景，日志每次启动时清空。

## 前置要求

- 《明日方舟：终末地》Windows PC 版。
- 一个会加载 `<游戏目录>\plugin\*.dll` 的插件加载器。[Endfield Poser](https://github.com/honxi1/Endfield-Poser) 的 `d3dcompiler_47.dll` 代理就可以；如果你已经装了 Poser，加载器就已经有了。

## 安装

1. **安装加载器**（已装 Poser 可跳过）：下载最新的 [Endfield Poser 发布包](https://github.com/honxi1/Endfield-Poser/releases/latest)。**先备份**游戏自带的 `d3dcompiler_47.dll`，再把 Poser 的 `d3dcompiler_47.dll` 复制到游戏根目录（`Endfield.exe` 所在目录）。如果游戏用 Vulkan 运行，把它的 `vulkan-1.dll` 也一起复制过去。
2. 从 [Releases](../../releases/latest) 下载 `endfield_nocol.dll`，放进 `<游戏目录>\plugin\`（没有这个文件夹就新建一个）。
   > 📁 是 **`plugin\`（单数）**，不是游戏自带的 `plugins\`（复数）文件夹。
3. 用官方启动器启动游戏。第一次启动会自动生成 `plugin\nocollide_config.txt` 和 `plugin\nocollide_log.txt`。

游戏目录一般类似：`D:\Hypergryph Launcher\games\Endfield Game\`。

## 使用

1. 进入游戏后按 **F9**，听到高音"嘀"表示已开启。
2. 控制角色走向队友，会直接穿过去，不会撞上或被推开。
3. 再按一次 **F9** 关闭（低音）。

想确认是否生效，可以打开 `plugin\nocollide_log.txt`，应该能看到：

```
[OK] hooked _CheckIfColliderValidForCollisions
[OK] hooked _CheckCollisionForPush
[RUN] first character-vs-character block ignored - mod is working
```

## 配置

设置保存在 `plugin\nocollide_config.txt`：

| 选项 | 默认值 | 说明 |
|---|---|---|
| `enabled` | `0` | 启动游戏时是否开启。`0` = 关闭（用热键开启），`1` = 开启。 |
| `toggle_key` | `0x78` | 开关热键的[虚拟键码](https://learn.microsoft.com/zh-cn/windows/win32/inputdev/virtual-key-codes)。`0x78` = F9，`0x77` = F8；填 `0` 表示不使用热键。 |
| `ignore_block` | `1` | `1` = 干员移动时不再互相阻挡。 |
| `ignore_push` | `1` | `1` = 干员重叠时不再被互相推开。 |
| `beep` | `1` | 开关时播放提示音。 |

## 卸载

删除 `plugin\endfield_nocol.dll`（也可以一并删除 `nocollide_config.txt` 和 `nocollide_log.txt`）。如果加载器只是为本插件装的，再把游戏原本的 `d3dcompiler_47.dll` 还原回去。

## 工作原理

终末地移动角色时不使用 Unity 的 `CharacterController` 或普通的碰撞体开关，所以直接关闭碰撞体是没有效果的。游戏自己的 `Beyond.Gameplay.Core.MovementComponent` 会对 ECS 碰撞体代理做检测，并对每个附近的碰撞体问两个问题：

| 游戏方法 | 使用场景 | 本插件的改动 |
|---|---|---|
| `_CheckIfColliderValidForCollisions(proxy, ignoreInterEnemy)` | 移动扫掠、重叠、射线检测："这个碰撞体会挡住我吗？" | 返回 `false`（不会） |
| `_CheckCollisionForPush(proxy, checkCantSwitch)` | 把重叠的角色推开："我要被它推开吗？" | 返回 `false`（不要） |

**只有当双方都是 `ObjectType.Character`（干员）时**才改变答案；另一方是谁，用游戏自带的 `_GetCollisionOwnerEntityFromCollider` 判断。其他情况，包括敌人（`ObjectType.Enemy`），都照常执行游戏原本的代码。伤害走的是独立的受击判定，不受影响。游戏本身就有一条"敌人与干员之间忽略物理碰撞"的内置规则，本插件也是同样的做法。

类、方法、字段偏移和枚举值都在运行时通过 IL2CPP 接口按名字查找，没有写死任何地址。挂钩使用 [MinHook](https://github.com/TsudaKageyu/minhook)。

## 编译

**Windows**（Visual Studio 2022，需安装 C++ 和 CMake）：

```bat
build.bat
```

生成文件：`build\Release\endfield_nocol.dll`

**Linux / WSL**（需要 mingw-w64 和 CMake）：

```sh
sh build_mingw.sh
```

生成文件：`build-mingw/endfield_nocol.dll`

## 常见问题

**敌人还能打到我吗？** 能。只有我方干员之间的碰撞会改变。

**会关闭或绕过反作弊（ACE）吗？** 不会。插件完全不碰 ACE，但也不会对它隐藏自己：它是一个加载进游戏、修改了两个游戏函数的 DLL。请阅读下方免责声明。

**游戏更新后插件失效了？** 查看 `nocollide_log.txt`。如果出现 `[ERR] ... not found`，说明游戏代码变了，请提交 issue 并附上日志。

## 免责声明

- 本插件为非官方第三方 Mod，按"现状"提供，不提供任何担保。
- 修改游戏客户端可能违反游戏的用户协议。使用风险由你自行承担，包括账号可能受到的处理。请勿用于在线或竞技内容中获取优势。
- 游戏名称及相关素材的权利归各自所有者所有。

## 致谢

- [Endfield Poser](https://github.com/honxi1/Endfield-Poser)：插件加载方式和运行时就绪检测的参考。
- [MinHook](https://github.com/TsudaKageyu/minhook)（BSD-2-Clause），作者 Tsuda Kageyu。
- [DeftSolutions-dev/IL2CPP-Dumper](https://github.com/DeftSolutions-dev/IL2CPP-Dumper)：其公开的终末地 dump 帮助定位了移动代码。

## 许可

[MIT](LICENSE)。`third_party/minhook` 中的 MinHook 使用其自己的 BSD-2-Clause 许可。
