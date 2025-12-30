# LuduScript-refactor 语法规范手册

---

## 1. 顶层结构 (Top-level Structure)

所有的游戏逻辑必须封装在 `game` 块中。

```ludu
game [GameName] {
    // 1. 配置
    config { ... }

    // 2. 角色定义
    role [RoleID] "[Description]"

    // 3. 全局变量
    var [name]: [type] = [value]

    // 4. 初始化
    setup { ... }

    // 5. 行动定义
    action [ActionID] "[Title]" { ... }

    // 6. 流程定义
    phase [PhaseID] "[Title]" { ... }
}
```

---

## 2. 核心模块规范

### 2.1 配置 (Config)
定义游戏的基础约束。
```ludu
config {
    min_players: 6
    max_players: 12
}
```

### 2.2 角色 (Roles)
使用 `role` 关键字。角色 ID 通常使用大驼峰命名。
```ludu
role Werewolf "狼人"
role Villager "村民"
```

### 2.3 全局变量 (Global Variables)
在 `game` 作用域内声明，必须指定类型。支持类型：`number`, `string`, `bool`, `list`。
```ludu
var killed_player: string = ""
var day_count: number = 1
var is_ended: bool = false
```

### 2.4 初始化块 (Setup)
在游戏开始前执行一次，用于分配身份等。
```ludu
setup {
    let players = get_players(None, "all")
    let shuffled = shuffle(players)
    // 分配身份逻辑...
}
```

### 2.5 行动 (Actions)
定义具体环节的执行逻辑。
- `description`: 描述信息。
- `execute`: 逻辑代码块。
```ludu
action KillAction "狼人杀人" {
    description: "狼人夜晚讨论并选择一名玩家袭击"
    execute {
        let wolves = get_players(Role.Werewolf, "alive")
        let target = vote(wolves, get_players(None, "alive"))
        if target != None {
            game.killed_player = target
        }
    }
}
```

### 2.6 阶段与步骤 (Phases & Steps)
定义游戏的循环流程。
- `phase`: 大阶段。
- `step`: 阶段内的具体环节。
- `roles`: 参与该步骤的角色列表（空列表表示全员参与）。
- `action`: 该步骤触发的行动。
```ludu
phase Night "夜晚" {
    step "WolfKill" {
        roles: [Werewolf]
        action: KillAction
    }
}
```

---

## 3. 语句与表达式规范

### 3.1 变量访问规则 (重要)
- **局部变量**: 使用 `let` 声明，直接使用名称访问。
- **全局变量**: 必须通过 `game.` 前缀访问（无论是读取还是赋值）。
```ludu
let target = "Player1"       // 局部变量
game.killed_player = target  // 赋值给全局变量
if game.killed_player == ""  // 读取全局变量
```

### 3.2 控制流
- **条件**: `if (condition) { ... } else { ... }` (括号可选，但推荐)。
- **循环**: `for (item in list) { ... }`。
```ludu
if game.is_night {
    announce("天黑了")
}

for (p in get_players(None, "alive")) {
    announce("存活玩家: " + p)
}
```

### 3.3 特殊常量
- `Role.[RoleID]`: 引用角色枚举（如 `Role.Werewolf`）。
- `None`: 表示空值或无。
- `true`, `false`: 布尔值。

---

## 4. 内置函数库

| 函数名 | 参数 | 说明 |
| :--- | :--- | :--- |
| `announce` | `(msg, [visibility])` | 发送公告。`visibility` 可选（玩家列表）。 |
| `get_players` | `(role, status)` | 获取玩家。`role` 可为 `Role.ID` 或 `None`；`status` 可为 `"alive"`, `"dead"`, `"all"`。 |
| `vote` | `(voters, candidates)` | 发起投票。返回选中的玩家名或 `None`。 |
| `kill` | `(player_name)` | 标记玩家死亡。 |
| `get_role` | `(player_name)` | 返回玩家的角色枚举值。 |
| `len` | `(list)` | 返回列表长度。 |
| `shuffle` | `(list)` | 返回打乱顺序后的新列表。 |
| `discussion` | `(participants)` | 开启讨论环节。 |
| `set_data` | `(player, key, value)`| 给玩家绑定自定义数据（如分配角色）。 |
| `stop_game` | `(message)` | 结束游戏并显示结果。 |

---

## 5. 命名建议
1. **RoleID**: 大驼峰 (e.g., `Werewolf`)。
2. **ActionID**: 大驼峰 (e.g., `NightAction`)。
3. **变量名**: 小蛇形 (e.g., `killed_player`)。
4. **Phase/Step ID**: 大驼峰或小蛇形。
