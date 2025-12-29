# LuduScript Refactor

LuduScript 的新一代编译器，使用 C++17 编写，旨在提供更简洁、高性能且易于扩展的游戏脚本编写体验。

## 特性

*   **简洁语法**: 类似于 Rust/Kotlin 的声明式语法，支持可选括号。
*   **强类型结构**: 明确的角色 (Role)、阶段 (Phase) 和动作 (Action) 定义。
*   **Python 转译**: 编译为 LudusEngine 原生支持的 Python 代码 (基于 `src.Game` 框架)。
*   **无依赖**: 仅使用 C++ 标准库，无需外部依赖，易于构建。

## 构建指南

确保已安装 CMake (3.10+) 和支持 C++17 的编译器 (MSVC, GCC, Clang)。

```powershell
mkdir build
cd build
cmake ..
cmake --build .
```

## 使用方法

编译完成后，使用 `ludu` 可执行文件编译 `.ludu` 脚本：

```powershell
./Debug/ludu.exe path/to/your_game.ludu
```

这将会在同目录下生成 `path/to/your_game.py`。

## 示例 (Werewolf)

请参考 `examples/werewolf.ludu`。

```ludu
game Werewolf {
    role Werewolf "狼人"
    role Villager "村民"

    action NightAction "夜晚行动" {
        execute {
            let wolves = get_players(Role.Werewolf, "alive")
            announce("天黑请闭眼", wolves)
        }
    }
    
    phase Night "夜晚" {
        step "WerewolfStep" {
            roles: [Werewolf]
            action: NightAction
        }
    }
}
```
