# Wolf DSL 语法规范手册

Wolf DSL 是一种专为描述棋牌及角色扮演类游戏（如狼人杀）流程而设计的领域特定语言。它采用声明式与命令式混合的风格，支持游戏逻辑的高度定制化。

---

## 1. 结构化定义

游戏逻辑必须包含在一个 `game` 块中，顶层仅允许 `game` 定义。

### 1.1 游戏主体 (Game)
所有游戏逻辑的根容器。
```wolf
game 游戏名称 {
    // 游戏内容
}
```

### 1.2 角色枚举 (Enum)
定义游戏中参与的角色类型。
```wolf
enum {
    角色1,
    角色2,
    ...
}
```

### 1.3 动作定义 (Action)
定义角色可以执行的具体行为，支持参数传递。
```wolf
action 动作名(参数1, 参数2) {
    // 动作逻辑，例如：
    println("执行了动作");
}
```

### 1.4 阶段与步骤 (Phase && Step)
定义游戏的宏观流程。一个 `phase` 包含多个 `step`。

- **Phase**: 逻辑阶段（如“夜晚”、“白天”）。
- **Step**: 具体的执行步骤，包含执行者、关联动作及触发条件。支持多角色参与。

```wolf
phase 阶段名 {
    // 单角色
    step "步骤1" for werewolf with kill { ... }
    
    // 多角色（用逗号分隔）
    step "步骤2" for werewolf, villager with vote if (day_count > 1) { ... }
}
```

### 1.5 方法定义 (Method)
自定义可复用的逻辑块。
```wolf
def 方法名(参数列表) {
    // 逻辑代码
}
```

### 1.6 初始化块 (Setup)
游戏启动时执行的一次性配置。
```wolf
setup {
    println("游戏初始化开始");
}
```

---

## 2. 变量与数据类型

### 2.1 数据类型
- **num**: 数值型（支持整数和浮点数）。
- **str**: 字符串型（需用双引号 `""` 包裹）。
- **bool**: 布尔型（`true` 或 `false`）。
- **obj**: 对象/引用类型（用于复杂实体引用）。

### 2.2 变量声明
支持两种声明语法，分号 `;` 为可选。

1. **函数式声明**:
   ```wolf
   num(count) = 1;
   str(name) = "Wolf";
   ```
2. **命令式声明 (推荐)**:
   ```wolf
   num count = 1
   bool is_active = true
   ```

---

## 3. 控制流

### 3.1 条件分支 (If/Elif/Else)
```wolf
if (condition) {
    // 逻辑
} elif (other_condition) {
    // 逻辑
} else {
    // 逻辑
}
```

### 3.2 循环 (For)
遍历或重复执行逻辑。
```wolf
for (item, list_expression) {
    // 逻辑
}
```

---

## 4. 运算符与表达式

### 4.1 运算符
代码支持以下运算符（按优先级排序）：

1. **逻辑非**: `!`
2. **乘除取模**: `*`, `/`, `%`
3. **加减**: `+`, `-`
4. **关系比较**: `==`, `!=`, `<`, `>`, `<=`, `>=`
5. **逻辑与**: `&&`
6. **逻辑或**: `||`
7. **赋值**: `=`

### 4.2 表达式与拼接
- **算术运算**: 支持基础数学运算。
- **字符串拼接**: 支持使用 `+` 运算符拼接字符串与变量。
- **条件表达式**: 可用于 `if`、`step` 的条件判断。
```wolf
println("当前天数: " + day_count)
bool is_night = (hour > 18 || hour < 6)
```

---

## 5. 内置函数

| 函数名 | 说明 | 示例 |
| :--- | :--- | :--- |
| `println(msg)` | 向控制台输出信息并换行 | `println("Hello World")` |

---

## 6. 词法规范

- **注释**: 使用 `//` 进行单行注释。
- **标识符**: 字母、数字或下划线组成，不能以数字开头。
- **关键字**: `game`, `enum`, `action`, `phase`, `step`, `def`, `setup`, `num`, `str`, `bool`, `obj`, `if`, `elif`, `else`, `for`。
- **大小写**: 关键字必须小写，标识符区分大小写。

---

## 7. 完整示例

```wolf
game MyWerewolf {
    enum { werewolf, villager }

    num day_count = 1
    bool game_over = false

    action kill(target) {
        println("狼人选择了目标: " + target)
    }

    phase night {
        step "狼人行动" for werewolf with kill if (!game_over) {
            println("黑夜降临...")
        }
    }

    setup {
        println("游戏开始！")
    }
}
```
