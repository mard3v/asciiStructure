# ASCII Structure System - Dynamic Constraint Solver

A constraint satisfaction system for generating ASCII art structure layouts in roguelike games. The system combines LLM-powered structure specification generation with a dynamic constraint solver that uses sliding placement algorithms.

## Table of Contents

- [Overview](#overview)
- [Features](#features)
- [Installation](#installation)
- [Usage](#usage)
- [DSL Format](#dsl-format)
- [Architecture](#architecture)
- [API Reference](#api-reference)
- [Examples](#examples)
- [Development](#development)
- [Contributing](#contributing)

## Overview

The ASCII Structure System implements a two-phase approach to procedural structure generation:

1. **Phase 1**: Generate or load DSL (Domain Specific Language) specifications that describe structure components and spatial relationships
2. **Phase 2**: Solve spatial constraints using a dynamic grid system with sliding placement algorithms

The system is designed for roguelike game developers who need procedural generation of complex structures like castles, dungeons, villages, and cathedrals with proper spatial relationships maintained.

## Features

### Dynamic Constraint Satisfaction

- **Sliding Placement Algorithm**: Components slide along perpendicular axes to satisfy adjacency constraints
- **Dynamic Grid Expansion**: Grid grows automatically to accommodate negative coordinates and varying component sizes
- **Component Grouping**: Related components move together to maintain spatial relationships
- **Most-Constrained-First Heuristic**: Improves solver efficiency by placing highly constrained components first

### Comprehensive Debug System

- **ASCII Grid Visualization**: Real-time visualization of placement attempts and grid states
- **Detailed Logging**: Complete trace of solver decisions and constraint analysis
- **Placement Debug**: Visual feedback for successful and failed placement attempts
- **Constraint Verification**: Post-solving validation of all spatial relationships

### LLM Integration

- **OpenAI API Integration**: Generate structure specifications using GPT models
- **Flexible Input**: Support for both file-based and string-based DSL specifications
- **Multiple Structure Types**: Built-in templates for castles, villages, dungeons, towers, and cathedrals

### Modular Architecture

- **Clean Separation**: Distinct modules for constraint solving, LLM integration, and parsing
- **Extensible Design**: Easy to add new constraint types and structure generators
- **Memory Safe**: Proper memory management with bounds checking throughout

## Installation

### Prerequisites

The system requires the following libraries:

```bash
# Ubuntu/Debian
sudo apt-get install libcurl4-openssl-dev libcjson-dev

# CentOS/RHEL
sudo yum install libcurl-devel cjson-devel

# macOS with Homebrew
brew install curl cjson
```

### Building

1. Clone or extract the source code
2. Navigate to the project directory
3. Run the build script:

```bash
chmod +x build.sh
./build.sh
```

The build script will:

- Check for required dependencies
- Compile all source files with appropriate flags
- Create the `ascii_structure_system` executable

### Environment Setup

For LLM integration, set your OpenAI API key:

```bash
export OPENAI_API_KEY='your-api-key-here'
```

## Usage

### Interactive Mode

Run the system with the interactive menu:

```bash
./ascii_structure_system
```

Menu options:

- **1-5**: Generate structures using LLM (castle, village, dungeon, cathedral, tower)
- **6**: Load and solve the test castle specification
- **7**: Load and solve a custom DSL file
- **8**: Test built-in string parsing with sample data
- **0**: Exit

### File-Based Usage

Load a DSL specification from a file:

```bash
# The system will automatically detect .txt files
./ascii_structure_system
# Choose option 7, enter: test_castle.txt
```

### Programmatic Integration

Include the header files in your C project:

```c
#include "constraint_solver.h"
#include "llm_integration.h"

// Initialize solver
LayoutSolver solver;
init_solver(&solver, 60, 40);

// Add components and constraints
add_component(&solver, "Gatehouse", gatehouse_ascii);
add_constraint(&solver, "ADJACENT(Gatehouse, Courtyard, n)");

// Solve constraints
if (solve_constraints(&solver)) {
    display_grid(&solver);
}
```

## DSL Format

The DSL (Domain Specific Language) uses a markdown-style format with three main sections:

### Components Section

```markdown
## Components

**Gatehouse** - The main entrance with defensive features. Medium scale fortified entry point with portcullis and guard posts.

**Courtyard** - Large open central area for gatherings and movement. Large scale open space that serves as the heart of the castle.
```

### Constraints Section

```markdown
## Constraints

ADJACENT(Gatehouse, Courtyard, n)
ADJACENT(Courtyard, Keep, n)
ADJACENT(Barracks, Armory, e)
```

### Component Tiles Section

```markdown
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
.....:....
...........
...........

```

```

### Constraint Types

Currently supported constraint:

- **ADJACENT(A, B, direction)**: Component A and B must be adjacent in the specified direction
  - Directions: `n` (north), `s` (south), `e` (east), `w` (west), `a` (any)

## Architecture

### Core Components

#### constraint_solver.c/h

The heart of the system implementing:

- **Dynamic Grid System**: Expandable grid supporting negative coordinates
- **Sliding Placement**: Algorithm for satisfying adjacency constraints
- **Component Management**: Storage and manipulation of ASCII art components
- **Debug Infrastructure**: Comprehensive logging and visualization

#### main.c

Application entry point providing:

- **Menu System**: Interactive interface for structure generation
- **DSL Parsing**: Markdown-style specification parser
- **Workflow Coordination**: Integration between LLM and solver phases

#### llm_integration.c/h

OpenAI API integration featuring:

- **HTTP Communication**: libcurl-based API requests
- **JSON Processing**: cJSON-based response parsing
- **Prompt Engineering**: Structure-specific prompt generation

### Data Structures

#### Component

```c
typedef struct Component {
    char name[64];                                    // Component identifier
    char ascii_tile[MAX_TILE_SIZE][MAX_TILE_SIZE];   // ASCII art representation
    int width, height;                               // Dimensions
    int placed_x, placed_y;                          // World coordinates
    int is_placed;                                   // Placement status
    int group_id;                                    // Group for collective movement
} Component;
```

#### LayoutSolver

```c
typedef struct LayoutSolver {
    Component components[MAX_COMPONENTS];            // Component storage
    DSLConstraint constraints[MAX_CONSTRAINTS];     // Constraint storage
    char grid[MAX_GRID_SIZE][MAX_GRID_SIZE];        // ASCII grid
    int grid_width, grid_height;                    // Current grid dimensions
    int grid_min_x, grid_min_y;                     // Grid origin coordinates
    // ... additional fields for debugging and state management
} LayoutSolver;
```

### Algorithm Overview

#### Constraint Solving Process

1. **Initialization**: Load components and constraints from DSL specification
2. **First Placement**: Place most constrained component at grid origin
3. **Iterative Placement**: For each remaining component:
   - Find constraints involving this component
   - Try sliding placement relative to placed components
   - Expand grid dynamically as needed
   - Validate against overlap and adjacency requirements
4. **Normalization**: Translate all coordinates to positive space
5. **Verification**: Validate that all constraints are satisfied

#### Sliding Algorithm

For adjacency constraint `ADJACENT(A, B, direction)`:

- Calculate base position based on direction (north/south/east/west)
- Slide component along perpendicular axis within reasonable range
- Check for overlaps with existing components (non-space characters only)
- Verify adjacency requirement (horizontal overlap for N/S, vertical overlap for E/W)

## API Reference

### Core Functions

#### `init_solver(LayoutSolver* solver, int width, int height)`

Initialize layout solver with specified grid dimensions.

#### `add_component(LayoutSolver* solver, const char* name, const char* ascii_data)`

Add ASCII art component to solver with automatic dimension calculation.

#### `add_constraint(LayoutSolver* solver, const char* constraint_line)`

Parse and add DSL constraint to solver.

#### `solve_constraints(LayoutSolver* solver)`

Execute dynamic constraint satisfaction algorithm. Returns 1 on success, 0 on failure.

#### `display_grid(LayoutSolver* solver)`

Render final layout as ASCII art to console.

### Utility Functions

#### `has_horizontal_overlap(int x1, int w1, int x2, int w2)`

Check if two horizontal spans overlap (for N/S adjacency validation).

#### `has_vertical_overlap(int y1, int h1, int y2, int h2)`

Check if two vertical spans overlap (for E/W adjacency validation).

#### `normalize_grid_coordinates(LayoutSolver* solver)`

Translate entire layout to ensure positive coordinates.

### Debug Functions

#### `init_debug_file(LayoutSolver* solver)`

Initialize placement_debug.log for detailed solver tracing.

#### `debug_log_ascii_grid(LayoutSolver* solver, const char* title)`

Log current grid state with ASCII visualization.

## Examples

### Simple Castle

```markdown
## Components

**Gatehouse** - Main entrance
**Courtyard** - Central area

## Constraints

ADJACENT(Gatehouse, Courtyard, n)

## Component Tiles

**Gatehouse:**
```

XXXXX
X.D.X
XXXXX

```

**Courtyard:**
```

.......
.......
.......

```

```

### Complex Structure with Multiple Constraints

See `test_castle.txt` for a complete example with 6 components and 7 constraints including:

- Gatehouse, Courtyard, Keep, Barracks, Armory, Kitchen
- Multiple adjacency relationships forming a realistic castle layout

## Development

### Adding New Constraint Types

1. Add constraint type to `DSLConstraintType` enum in `constraint_solver.h`
2. Extend constraint parsing in `add_constraint()` function
3. Implement constraint validation logic
4. Add solving algorithm for the new constraint type

### Debug Output

The system generates `placement_debug.log` with:

- Component and constraint summaries
- Step-by-step placement attempts
- ASCII grid visualization at each stage
- Constraint satisfaction analysis

### Memory Management

- All dynamic allocations are paired with appropriate `free()` calls
- String operations use safe functions (`strncpy`, `snprintf`)
- Buffer sizes are checked before operations
- File handles are properly closed

### Error Handling

- Comprehensive validation of input parameters
- Graceful handling of API failures
- Memory allocation failure detection
- File I/O error reporting

## Contributing

### Code Style

- Use comprehensive function documentation with `@brief`, `@param`, `@return`
- Maintain modular architecture with clear separation of concerns
- Follow existing naming conventions
- Add appropriate debug logging for new features

### Testing

- Test new constraint types with various component sizes
- Verify memory safety with valgrind
- Validate constraint satisfaction with complex specifications
- Check edge cases (empty inputs, malformed DSL, API failures)

### Pull Requests

- Include detailed description of changes
- Add test cases for new functionality
- Update documentation for API changes
- Ensure all existing tests pass

## License

This project is released under [your chosen license]. See LICENSE file for details.

## Troubleshooting

### Common Issues

**Build Failures**

- Ensure libcurl and libcjson development packages are installed
- Check pkg-config can find the libraries: `pkg-config --libs libcurl libcjson`

**API Failures**

- Verify OPENAI_API_KEY environment variable is set
- Check internet connectivity and OpenAI API status
- Review API response in debug output

**Constraint Solving Failures**

- Check DSL format matches expected markdown structure
- Verify component names match between constraints and tiles sections
- Enable debug logging to trace placement attempts

**Memory Issues**

- Monitor memory usage with complex structures
- Check for proper cleanup of temporary allocations
- Use valgrind for memory leak detection

