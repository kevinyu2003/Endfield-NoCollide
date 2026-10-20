# Endfield No-Collide

**English** | [简体中文](README.zh-CN.md)

A small plugin for **Arknights: Endfield** that lets your own operators walk through each other and stand close together, so you can line them up for photo mode. **Enemies are not affected.** They still block you, push you and hit you.

> ⚠️ This is an unofficial fan-made tool and is not affiliated with Hypergryph or GRYPHLINE. Read the [disclaimer](#disclaimer) before you use it.

## Features

- Your operators no longer block each other while moving, and no longer get pushed apart when they overlap.
- Only operator-to-operator collision changes. Enemies, NPCs, terrain, hit boxes and projectiles all work as normal.
- It is **off every time the game starts**. Press **F9** to turn it on or off. A high beep means on and a low beep means off.
- It finds the game code it needs by name each time the game starts, so small game updates usually don't break it. If an update removes what it needs, it writes that to its log and does nothing. It does not crash the game.
- It is lightweight: no overlay, no scene scanning, and its log is cleared at each launch.

## Requirements

- The Windows PC version of Arknights: Endfield.
- A plugin loader that loads `<game>\plugin\*.dll`. The `d3dcompiler_47.dll` proxy from [Endfield Poser](https://github.com/honxi1/Endfield-Poser) does this. If Poser is already installed, you already have the loader.

## Install

1. **Install the loader** (skip this step if Poser is installed). Download the latest [Endfield Poser release](https://github.com/honxi1/Endfield-Poser/releases/latest). Back up your game's own `d3dcompiler_47.dll`, then copy Poser's `d3dcompiler_47.dll` into the game root, next to `Endfield.exe`. If you run the game with Vulkan, also copy its `vulkan-1.dll`.
2. Download `endfield_nocol.dll` from [Releases](../../releases/latest) and put it in `<game>\plugin\`. Create the folder if it doesn't exist.
   > 📁 The folder is **`plugin\`** (singular), not the game's own `plugins\` folder.
3. Start the game with the official launcher. The first launch creates `plugin\nocollide_config.txt` and `plugin\nocollide_log.txt`.

The game is usually installed somewhere like `D:\Hypergryph Launcher\games\Endfield Game\`.

## Use

1. Go into the game and press **F9**. A high beep means the mod is on.
2. Walk your operator into a teammate. You'll pass through instead of bumping or being pushed away.
3. Press **F9** again to turn it off (low beep).

To check that it worked, open `plugin\nocollide_log.txt`. It should contain:

```
[OK] hooked _CheckIfColliderValidForCollisions
[OK] hooked _CheckCollisionForPush
[RUN] first character-vs-character block ignored - mod is working
```

## Configuration

The settings are in `plugin\nocollide_config.txt`:

| Key | Default | Meaning |
|---|---|---|
| `enabled` | `0` | Whether the mod is on when the game starts. `0` = off (turn on with the hotkey), `1` = on. |
| `toggle_key` | `0x78` | [Virtual-key code](https://learn.microsoft.com/windows/win32/inputdev/virtual-key-codes) of the on/off key. `0x78` is F9 and `0x77` is F8. `0` disables the hotkey. |
| `ignore_block` | `1` | `1` = operators don't block each other while moving. |
| `ignore_push` | `1` | `1` = overlapping operators aren't pushed apart. |
| `beep` | `1` | Plays a short sound when the mod is toggled. |

## Uninstall

Delete `plugin\endfield_nocol.dll`, plus `nocollide_config.txt` and `nocollide_log.txt` if you want. If you only installed the loader for this mod, also restore the game's original `d3dcompiler_47.dll`.

## How it works

Endfield doesn't move characters with Unity's `CharacterController` or ordinary collider flags. That's why turning colliders off has no effect. The game's own `Beyond.Gameplay.Core.MovementComponent` checks collisions against ECS collider proxies, and asks two questions about each nearby collider:

| Game method | When it's used | This mod's change |
|---|---|---|
| `_CheckIfColliderValidForCollisions(proxy, ignoreInterEnemy)` | Movement sweeps, overlaps and raycasts: "does this collider block me?" | Returns `false` (no) |
| `_CheckCollisionForPush(proxy, checkCantSwitch)` | Pushing overlapping characters apart: "should I be pushed away from this?" | Returns `false` (no) |

The mod changes the answer **only when both sides are `ObjectType.Character`**. It identifies the other side with the game's own `_GetCollisionOwnerEntityFromCollider`. In every other case, including enemies (`ObjectType.Enemy`), the game's original code runs. Damage uses separate hit boxes, so it is not affected. The game already has a built-in rule that ignores physical collision between enemies and characters, and this mod works the same way.

Classes, methods, field offsets and enum values are all looked up by name at runtime through the IL2CPP API, so the mod has no hard-coded addresses. The hooks use [MinHook](https://github.com/TsudaKageyu/minhook).

## Build

**Windows** (Visual Studio 2022 with C++ and CMake):

```bat
build.bat
```

The output is `build\Release\endfield_nocol.dll`.

**Linux / WSL** (mingw-w64 and CMake):

```sh
sh build_mingw.sh
```

The output is `build-mingw/endfield_nocol.dll`.

## FAQ

**Will enemies still hit me?** Yes. Only collision between your own operators changes.

**Does this disable or bypass the anti-cheat (ACE)?** No. The mod doesn't touch ACE at all. It also doesn't hide from it. It is a DLL loaded into the game that changes two game functions, so read the disclaimer below.

**The game updated and the mod stopped working.** Check `nocollide_log.txt`. If it shows `[ERR] ... not found`, the game code changed, so please open an issue and attach the log.

## Disclaimer

- This is an unofficial third-party mod, provided as-is with no warranty.
- Changing the game client may break the game's Terms of Service. You use it at your own risk, which includes any action taken against your account. Don't use it to gain an advantage in online or competitive content.
- All game names and assets belong to their respective owners.

## Credits

- [Endfield Poser](https://github.com/honxi1/Endfield-Poser): the reference for the plugin loading layout and the runtime-ready check.
- [MinHook](https://github.com/TsudaKageyu/minhook) (BSD-2-Clause) by Tsuda Kageyu.
- [DeftSolutions-dev/IL2CPP-Dumper](https://github.com/DeftSolutions-dev/IL2CPP-Dumper): its public Endfield dump helped locate the movement code.

## License

[MIT](LICENSE). MinHook in `third_party/minhook` uses its own BSD-2-Clause license.
