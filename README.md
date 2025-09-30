# ASCII Structure System

A constraint-based spatial layout system for generating ASCII art structures with configurable relationships. Features a tree-based solver with modular constraint system and interactive testing tools.

## Table of Contents

- [Overview](#overview)
- [System Components](#system-components)
- [Installation](#installation)
- [Usage](#usage)
- [Constraint System](#constraint-system)
- [Testing & Development](#testing--development)
- [Architecture](#architecture)
- [DSL Format](#dsl-format)
- [API Reference](#api-reference)

## Overview

The ASCII Structure System provides two main capabilities:

1. **Structure Generation**: Generate procedural ASCII structures using LLM integration or load from files
2. **Constraint Testing**: Interactive testing environment for developing and debugging spatial constraints

The system uses a modular constraint architecture where constraints are defined in a separate file and called directly by the solver, allowing for easy extension and testing of new constraint types.

## System Components

### Main System (`ascii_structure_system`)

The primary constraint solver with LLM integration for generating complete structures:

- Tree-based constraint solver with conflict resolution
- OpenAI API integration for structure specification generation
- Support for multiple structure types (castle, village, dungeon, cathedral, tower)
- File-based DSL parsing and processing
- Visual ASCII output with Unicode box drawing

### Constraint Testing System (`constraint_test`)

Interactive tool for testing individual constraints:

- Two predefined test rooms of different sizes
- Visual display of all placement options with preference scoring
- Results logged with ASCII visualization using dot grid
- Support for testing ADJACENT constraints in all directions
- Priority-based ordering showing constraint behavior

## Installation

### Prerequisites

Install required dependencies:

```bash
# Ubuntu/Debian
sudo apt-get install libcurl4-openssl-dev libcjson-dev

# CentOS/RHEL
sudo yum install libcurl-devel cjson-devel

# macOS with Homebrew
brew install curl cjson
```

### Building

Build all components with the unified build system:

```bash
./build.sh
```

This creates two executables:
- `ascii_structure_system` - Main system
- `constraint_test` - Constraint testing tool

### Environment Setup

For LLM features, set your OpenAI API key:

```bash
export OPENAI_API_KEY='your-api-key-here'
```

## Usage

### Main System

Run the interactive menu:

```bash
./ascii_structure_system
```

**Menu Options:**
1. **Generate Castle** - LLM-generated castle layout
2. **Generate Village** - LLM-generated village layout
3. **Generate Dungeon** - LLM-generated dungeon layout
4. **Generate Cathedral** - LLM-generated cathedral layout
5. **Generate Tower** - LLM-generated tower layout
6. **Load test file** - Select from test files in `tests/` directory
7. **Load custom file and solve** - Load your own DSL file
8. **Test string parsing** - Test built-in parsing with sample data
0. **Exit**

### Test Files

Test specifications are stored in the `tests/` directory:

- **simple_castle.txt** - Basic castle with 6 components and 6 constraints
- **palace.txt** - Complex palace with 9 components and 9 constraints (demonstrates backtracking)

Add your own test files to this directory and load them via menu option 6.

### Constraint Testing

Test individual constraints interactively:

```bash
./constraint_test
```

**Example Session:**
```
Enter constraint type (ADJACENT) or 'quit': ADJACENT
Enter direction parameter (N/S/E/W/*): E
Testing ADJACENT constraint with direction E...
Check constraint_test.log for visual results.
```

Results are saved to `constraint_test.log` showing all placement options with visual ASCII representations.

## Constraint System

### Current Constraints

**ADJACENT(ComponentA, ComponentB, direction)**
- Places ComponentA adjacent to ComponentB in the specified direction
- Directions: `n` (north), `s` (south), `e` (east), `w` (west), `a` (any)
- Supports priority-based placement with edge alignment preference

### Constraint Priority System

The ADJACENT constraint uses a sophisticated priority system:

1. **Edge Alignment (Score: 100)**: Perfect edge alignment (left/right for N/S, top/bottom for E/W)
2. **Centered Placement (Score: 90)**: When both components have matching parity, perfect centering
3. **Overlapping - Wall Hugging (Score: 60-89)**: Overlapping positions prioritizing edge proximity
4. **Overlapping - More Centered (Score: 50-89)**: Overlapping positions with more centered alignment
5. **Non-overlapping (Score: 1-49)**: Non-overlapping positions ordered by proximity

### Adding New Constraints

The modular constraint system allows easy extension:

1. **Add constraint logic** to `constraints.c` in the appropriate functions:
   - `generate_constraint_placements()` - Add case for new constraint type
   - `calculate_constraint_score()` - Add scoring logic
   - `validate_constraint()` - Add validation logic

2. **Add constraint type** to `constraint_solver.h` in the `DSLConstraintType` enum

3. **Test with constraint_test** tool to verify behavior

## Testing & Development

### Constraint Testing Workflow

1. **Run constraint tester**: `./constraint_test`
2. **Test constraint**: Enter type and parameters
3. **Analyze results**: Check `constraint_test.log` for visual output
4. **Iterate**: Modify constraint logic and retest

### Test Rooms

The constraint tester uses two predefined rooms:

**RoomA (Large)**: 7×5 rectangular room
```
+-----+
|     |
|     |
|     |
+-----+
```

**RoomB (Small)**: 4×3 rectangular room
```
+--+
|  |
+--+
```

### Visual Output

Results show placement options with:
- **Dots (.)** for empty grid cells
- **Original ASCII** for placed components
- **Modified characters** for test placements (`#`, `=`, `:` instead of `+`, `-`, `|`)
- **Priority ordering** from highest to lowest preference score

### Example Output

```
--- Option 1: Position (12, 3) Score: 100 [OK] ---
...........................................
...........................................
...........................................
.....+-----+........................
.....| . . |........................
.....| . . |================........
.....| . . |........................
.....+-----+........................
...........................................
```

## Architecture

### Core Files

**constraint_solver.c/h**
- Main constraint solver with tree-based search
- Component and constraint management
- Grid rendering and coordinate handling
- Conflict detection and resolution

**constraints.c/h**
- Modular constraint implementation
- Direct function dispatch (no registry layer)
- ADJACENT constraint with priority scoring
- Extensible architecture for new constraint types

**main.c**
- Interactive menu system
- DSL parsing and file handling
- LLM integration workflow
- Error handling and user interface

**llm_integration.c/h**
- OpenAI API integration
- Prompt engineering for structure generation
- JSON response parsing
- HTTP communication via libcurl

**constraint_test.c**
- Interactive constraint testing environment
- Visual result logging
- Test room setup and management
- Priority analysis tools

**tree_debug.c/h**
- Debug logging infrastructure
- Visual tree traversal output
- Performance metrics tracking

### Data Flow

```
User Input → DSL Parsing → Component/Constraint Setup → Solver → ASCII Output
                                    ↓
                           Constraint Testing ← constraints.c
```

### Constraint Architecture

The constraint system uses direct function calls instead of a registry pattern:

```c
// Solver calls constraints directly:
generate_constraint_placements(solver, constraint, unplaced_comp, placed_comp, options, max_options);

// Which dispatches to specific implementations:
switch (constraint->type) {
    case DSL_ADJACENT:
        return adjacent_generate_placements(...);
    // More constraint types here
}
```

## DSL Format

### Structure

DSL files use markdown-style format with three sections:

```markdown
## Components

**ComponentName** - Description of the component

## Constraints

ADJACENT(ComponentA, ComponentB, direction)

## Component Tiles

**ComponentName:**
```
ASCII art representation
```
```

### Example DSL

```markdown
## Components

**Gatehouse** - Main entrance with defensive features
**Courtyard** - Large open central area for gatherings

## Constraints

ADJACENT(Gatehouse, Courtyard, n)

## Component Tiles

**Gatehouse:**
```
XXXXXXX
X.....X
X..D..X
X.....X
XXXXXXX
```

**Courtyard:**
```
...........
...........
.....:.....
...........
...........
```
```

### Constraint Syntax

**ADJACENT(A, B, direction)**
- `A`, `B`: Component names (must match names in Components section)
- `direction`: Single character - `n`, `s`, `e`, `w`, or `a` (any)

## API Reference

### Core Solver Functions

```c
// Initialize solver
void init_solver(LayoutSolver *solver, int width, int height);

// Add components and constraints
void add_component(LayoutSolver *solver, const char *name, const char *ascii_data);
void add_constraint(LayoutSolver *solver, const char *constraint_line);

// Solve and display
int solve_constraints(LayoutSolver *solver);
void display_grid(LayoutSolver *solver);
```

### Constraint System Functions

```c
// Direct constraint interface
int generate_constraint_placements(struct LayoutSolver* solver,
                                  struct DSLConstraint* constraint,
                                  struct Component* unplaced_comp,
                                  struct Component* placed_comp,
                                  struct TreePlacementOption* options,
                                  int max_options);

int calculate_constraint_score(struct LayoutSolver* solver,
                              struct Component* comp,
                              int x, int y,
                              struct DSLConstraint* constraint,
                              struct Component* placed_comp);

int validate_constraint(struct LayoutSolver* solver,
                       struct DSLConstraint* constraint);
```

### Data Structures

```c
typedef struct Component {
    char name[64];
    char ascii_tile[MAX_TILE_SIZE][MAX_TILE_SIZE];
    int width, height;
    int placed_x, placed_y;
    int is_placed;
    int group_id;
} Component;

typedef struct DSLConstraint {
    DSLConstraintType type;
    char component_a[64];
    char component_b[64];
    Direction direction;  // char type: 'n', 's', 'e', 'w', 'a'
} DSLConstraint;

typedef struct TreePlacementOption {
    int x, y;
    int preference_score;
    ConflictInfo conflicts;
    int has_conflict;
} TreePlacementOption;
```

### LLM Integration

```c
// Generate structure specifications via OpenAI API
int generate_structure_specification(const char* structure_type,
                                   char* output_buffer,
                                   size_t buffer_size);

// Prompt generation for different structure types
void generate_dsl_prompt(const char* structure_type,
                        char* system_prompt,
                        char* user_prompt);
```

## Development

### Build System

Single build script handles all components:
- Dependency checking (libcurl, libcjson)
- Compilation with appropriate flags
- Clear success/failure reporting
- Usage instructions

### Testing New Constraints

1. **Implement** constraint logic in `constraints.c`
2. **Test** with `./constraint_test` tool
3. **Verify** priority ordering and scoring
4. **Debug** with visual log output
5. **Integrate** with main solver

### Debug Analysis

Check `constraint_test.log` for:
- Visual placement grids with dot backgrounds
- Priority scores for each option
- Conflict status indicators
- Edge alignment and overlap analysis

### Performance Considerations

- **Static allocation** for predictable memory usage
- **Direct function calls** instead of function pointers
- **Efficient constraint evaluation** with early termination
- **Visual feedback** for debugging complex layouts

---

*Documentation for ASCII Structure System v2.0 - Modular Constraint Architecture with Interactive Testing*