# LuduScript Language Reference

LuduScript is a domain-specific language (DSL) designed for defining game logic in the LudusEngine. It provides a high-level, declarative syntax to define roles, game phases, and interactions.

## 1. Game Structure

A LuduScript file defines a single game using the `game` block.

```ludu
game GameName {
    // Declarations
}
```

### Configuration
Configure game parameters like player limits.

```ludu
config {
    min_players: 6
    max_players: 12
}
```

### Roles
Define the roles available in the game.

```ludu
role Werewolf "The bad guy"
role Villager "The good guy"
```

### Global Variables
Define state variables accessible across all actions.

```ludu
var is_night: bool = true
var kill_target: string = ""
```

## 2. Phases and Steps

The game flow is organized into Phases, which contain Steps.

```ludu
phase Night "Night Phase" {
    step "WerewolfAction" {
        roles: [Werewolf]
        action: WerewolfKill
    }
}
```

## 3. Actions

Actions define the logic executed during a step.

```ludu
action WerewolfKill "Werewolf Action" {
    description: "Werewolves choose a target"
    execute {
        // Logic here
    }
}
```

## 4. Syntax and Features

### Types
- **string**: `"text"`
- **number**: `123`
- **bool**: `true`, `false`
- **list**: `["a", "b"]`

### Control Flow
Standard `if-else` structures.

```ludu
if x > 0 {
    // ...
} else {
    // ...
}
```

### Built-in Functions

- **announce(message, [visibility])**
  Broadcasts a message. `visibility` is an optional list of player names.
  ```ludu
  announce("Night has fallen")
  announce("You are a wolf", wolves)
  ```

- **get_players(role, status)**
  Returns a list of player names.
  - `role`: A Role enum (e.g., `Role.Werewolf`) or `None` for all.
  - `status`: `"alive"` (default) or `"dead"`.
  ```ludu
  let wolves = get_players(Role.Werewolf, "alive")
  ```

- **vote(voters, candidates)**
  Initiates a voting process. Returns the selected target (string) or `None`.
  ```ludu
  let target = vote(wolves, all_players)
  ```

- **discussion(participants)**
  Starts a discussion phase.
  ```ludu
  discussion(all_players)
  ```

- **get_role(player_name)**
  Returns the role of a player.
  ```ludu
  let role = get_role(target)
  if role == Role.Werewolf { ... }
  ```

- **kill(player_name)**
  Sets a player's status to dead.
  ```ludu
  kill(target)
  ```

- **stop_game(message)**
  Ends the game with an optional message.
  ```ludu
  stop_game("Game Over")
  ```

- **len(list)**
  Returns the length of a list.
  ```ludu
  if len(wolves) == 0 { ... }
  ```

## 5. Example

```ludu
game Demo {
    role Player "A player"
    
    action Start "Start" {
        execute {
            announce("Hello World")
        }
    }
    
    phase Main "Main" {
        step "S1" {
            roles: [Player]
            action: Start
        }
    }
}
```
