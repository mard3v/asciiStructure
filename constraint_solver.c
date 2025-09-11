#include "constraint_solver.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// =============================================================================
// DYNAMIC GRID CONSTRAINT SOLVER IMPLEMENTATION
// =============================================================================
// This module implements a dynamic constraint satisfaction algorithm for
// spatial layout generation. Unlike traditional fixed-grid approaches, this
// solver uses a growing grid that expands to accommodate components of varying
// sizes while maintaining proper spatial relationships.

/**
 * @brief Adds a component to the solver with parsed ASCII art representation
 * 
 * Parses ASCII art data and creates a structured component representation.
 * Components are defined by their ASCII tiles, dimensions, and metadata.
 * 
 * @param solver    The layout solver instance
 * @param name      Component name (used for constraint references)
 * @param ascii_data Raw ASCII art string (newline-separated rows)
 * 
 * Features:
 * - Manual ASCII parsing (avoids strtok state corruption)
 * - Automatic dimension calculation
 * - Space-initialized tile arrays for consistent processing
 */
void add_component(LayoutSolver *solver, const char *name,
                   const char *ascii_data) {
  if (solver->component_count >= MAX_COMPONENTS)
    return;

  Component *comp = &solver->components[solver->component_count];
  strcpy(comp->name, name);
  comp->is_placed = 0;
  comp->placed_x = -1;
  comp->placed_y = -1;
  comp->group_id = 0;

  // Parse ASCII tile data - avoid strtok to prevent interference with parsing
  comp->width = 0;
  comp->height = 0;

  // Initialize tile with spaces
  for (int i = 0; i < MAX_TILE_SIZE; i++) {
    for (int j = 0; j < MAX_TILE_SIZE; j++) {
      comp->ascii_tile[i][j] = ' ';
    }
  }

  // Parse manually without strtok to avoid state corruption
  const char *ptr = ascii_data;
  int row = 0, col = 0;
  int current_line_len = 0;

  while (*ptr && row < MAX_TILE_SIZE) {
    if (*ptr == '\n') {
      if (current_line_len > comp->width)
        comp->width = current_line_len;
      row++;
      col = 0;
      current_line_len = 0;
    } else {
      if (col < MAX_TILE_SIZE) {
        comp->ascii_tile[row][col] = *ptr;
        col++;
        current_line_len++;
      }
    }
    ptr++;
  }

  // Handle last line if no trailing newline
  if (current_line_len > 0) {
    if (current_line_len > comp->width)
      comp->width = current_line_len;
    row++;
  }

  comp->height = row;
  solver->component_count++;
}

/**
 * @brief Adds a DSL constraint to the solver
 * 
 * Parses constraint string in DSL format and adds it to the solver's constraint list.
 * Currently supports ADJACENT constraints with directional parameters.
 * 
 * @param solver         The layout solver instance
 * @param constraint_line DSL constraint string (e.g., "ADJACENT(Gatehouse, Courtyard, n)")
 * 
 * Format: CONSTRAINT_TYPE(component_a, component_b, direction)
 * Directions: 'n' (north), 's' (south), 'e' (east), 'w' (west), 'a' (any)
 */
void add_constraint(LayoutSolver *solver, const char *constraint_line) {
  if (solver->constraint_count >= MAX_CONSTRAINTS)
    return;

  DSLConstraint *constraint = &solver->constraints[solver->constraint_count];

  // Parse constraint format: ADJACENT(a, b, direction)
  char type_str[32], params[256];
  if (sscanf(constraint_line, "%31[^(](%255[^)])", type_str, params) != 2) {
    return; // Invalid format
  }

  // Only handle ADJACENT constraints
  if (strcmp(type_str, "ADJACENT") == 0) {
    constraint->type = DSL_ADJACENT;
    char dir_char;
    sscanf(params, "%63[^,], %63[^,], %c", constraint->component_a,
           constraint->component_b, &dir_char);
    constraint->direction = dir_char;
    solver->constraint_count++;
  }
  // Ignore all other constraint types
}

/**
 * @brief Finds a component by name in the solver's component list
 * 
 * Linear search through all registered components to find one matching the given name.
 * 
 * @param solver The layout solver instance
 * @param name   Component name to search for (case-sensitive)
 * @return       Pointer to matching Component, or NULL if not found
 */
Component *find_component(LayoutSolver *solver, const char *name) {
  for (int i = 0; i < solver->component_count; i++) {
    if (strcmp(solver->components[i].name, name) == 0) {
      return &solver->components[i];
    }
  }
  return NULL;
}

/**
 * @brief Builds adjacency matrix representing constraint relationships
 * 
 * Creates a bidirectional constraint graph where graph[i][j] = 1 indicates
 * that components i and j have constraints between them. Used for dependency
 * analysis and constraint ordering.
 * 
 * @param solver The layout solver instance
 */
void build_constraint_graph(LayoutSolver *solver) {
  // Initialize graph
  for (int i = 0; i < MAX_COMPONENTS; i++) {
    for (int j = 0; j < MAX_COMPONENTS; j++) {
      solver->constraint_graph[i][j] = 0;
    }
  }

  // Build dependency relationships
  for (int i = 0; i < solver->constraint_count; i++) {
    DSLConstraint *constraint = &solver->constraints[i];
    Component *comp_a = find_component(solver, constraint->component_a);
    Component *comp_b = find_component(solver, constraint->component_b);

    if (comp_a && comp_b) {
      int idx_a = comp_a - solver->components;
      int idx_b = comp_b - solver->components;

      // Create dependency based on constraint type
      switch (constraint->type) {
      case DSL_ADJACENT:
        solver->constraint_graph[idx_a][idx_b] = 1;
        solver->constraint_graph[idx_b][idx_a] = 1; // bidirectional
        break;
      default:
        solver->constraint_graph[idx_a][idx_b] = 1;
        break;
      }
    }
  }
}


// =============================================================================
// DEBUG FUNCTIONS
// =============================================================================

/**
 * @brief Initializes debug logging file for placement visualization
 * 
 * Creates placement_debug.log file with structured logging headers and
 * component/constraint summary. All subsequent debug operations write
 * to this file for tracing placement attempts.
 * 
 * @param solver The layout solver instance
 */
void init_debug_file(LayoutSolver *solver) {
  solver->debug_file = fopen("placement_debug.log", "w");
  if (solver->debug_file) {
    fprintf(solver->debug_file, "=============================================="
                                "===============================\n");
    fprintf(solver->debug_file, "DYNAMIC CONSTRAINT SOLVER - DEBUG LOG\n");
    fprintf(solver->debug_file, "=============================================="
                                "===============================\n\n");

    // Log initial component information
    fprintf(solver->debug_file, "COMPONENTS:\n");
    for (int i = 0; i < solver->component_count; i++) {
      Component *comp = &solver->components[i];
      fprintf(solver->debug_file, "  %d. %s (%dx%d)\n", i + 1, comp->name,
              comp->width, comp->height);
    }

    fprintf(solver->debug_file, "\nCONSTRAINTS:\n");
    for (int i = 0; i < solver->constraint_count; i++) {
      DSLConstraint *constraint = &solver->constraints[i];
      const char *dir_name;
      switch (constraint->direction) {
      case 'n':
        dir_name = "NORTH";
        break;
      case 's':
        dir_name = "SOUTH";
        break;
      case 'e':
        dir_name = "EAST";
        break;
      case 'w':
        dir_name = "WEST";
        break;
      case 'a':
        dir_name = "ANY";
        break;
      default:
        dir_name = "UNKNOWN";
        break;
      }
      fprintf(solver->debug_file, "  %d. ADJACENT(%s, %s, %s)\n", i + 1,
              constraint->component_a, constraint->component_b, dir_name);
    }

    fprintf(solver->debug_file, "\n============================================"
                                "=================================\n");
    fprintf(solver->debug_file, "PLACEMENT LOG:\n");
    fprintf(solver->debug_file, "=============================================="
                                "===============================\n\n");
    fflush(solver->debug_file);
  }
}

/**
 * @brief Closes debug file and finalizes log output
 * 
 * Writes completion markers and closes the debug file handle.
 * Should be called after constraint solving is complete.
 * 
 * @param solver The layout solver instance
 */
void close_debug_file(LayoutSolver *solver) {
  if (solver->debug_file) {
    fprintf(solver->debug_file, "\n============================================"
                                "=================================\n");
    fprintf(solver->debug_file, "DEBUG LOG COMPLETE\n");
    fprintf(solver->debug_file, "=============================================="
                                "===============================\n");
    fclose(solver->debug_file);
    solver->debug_file = NULL;
  }
}

/**
 * @brief Logs detailed information about component placement attempts
 * 
 * Records each placement attempt with component details, target relationships,
 * success/failure status, and ASCII grid visualization. Essential for debugging
 * constraint satisfaction issues.
 * 
 * @param solver     The layout solver instance
 * @param comp       Component being placed
 * @param target_comp Reference component for adjacency constraint
 * @param dir        Direction of adjacency ('n', 's', 'e', 'w')
 * @param x          Attempted x coordinate
 * @param y          Attempted y coordinate
 * @param success    Whether placement succeeded (1) or failed (0)
 */
void debug_log_placement_attempt(LayoutSolver *solver, Component *comp,
                                 Component *target_comp, Direction dir, int x,
                                 int y, int success) {
  if (!solver->debug_file)
    return;

  const char *dir_name;
  switch (dir) {
  case 'n':
    dir_name = "NORTH";
    break;
  case 's':
    dir_name = "SOUTH";
    break;
  case 'e':
    dir_name = "EAST";
    break;
  case 'w':
    dir_name = "WEST";
    break;
  case 'a':
    dir_name = "ANY";
    break;
  default:
    dir_name = "UNKNOWN";
    break;
  }

  fprintf(solver->debug_file, "PLACEMENT ATTEMPT: %s\n", comp->name);
  fprintf(solver->debug_file, "  Target: %s (at %d,%d)\n", target_comp->name,
          target_comp->placed_x, target_comp->placed_y);
  fprintf(solver->debug_file, "  Direction: %s (%c)\n", dir_name, dir);
  fprintf(solver->debug_file, "  Attempted position: (%d,%d)\n", x, y);
  fprintf(solver->debug_file, "  Component size: %dx%d\n", comp->width,
          comp->height);
  fprintf(solver->debug_file, "  Result: %s\n", success ? "SUCCESS" : "FAILED");

  if (success) {
    fprintf(solver->debug_file,
            "  Component '%s' successfully placed at (%d,%d)\n", comp->name, x,
            y);

    // Check overlap conditions
    if (dir == 'n' || dir == 's') {
      int has_h_overlap = has_horizontal_overlap(
          x, comp->width, target_comp->placed_x, target_comp->width);
      fprintf(solver->debug_file, "  Horizontal overlap with target: %s\n",
              has_h_overlap ? "YES" : "NO");
    } else if (dir == 'e' || dir == 'w') {
      int has_v_overlap = has_vertical_overlap(
          y, comp->height, target_comp->placed_y, target_comp->height);
      fprintf(solver->debug_file, "  Vertical overlap with target: %s\n",
              has_v_overlap ? "YES" : "NO");
    }

    // Show the grid after successful placement
    debug_log_ascii_grid(solver, "AFTER SUCCESSFUL PLACEMENT");

  } else {
    // Log why it failed
    int overlap_detected = has_overlap(solver, comp, x, y);
    fprintf(solver->debug_file, "  Failure reason: %s\n",
            overlap_detected ? "OVERLAP DETECTED"
                             : "ADJACENCY CONSTRAINT NOT SATISFIED");

    // For failed attempts, show what the grid would look like if we temporarily
    // placed the component Save current state
    int was_placed = comp->is_placed;
    int old_x = comp->placed_x;
    int old_y = comp->placed_y;

    // Temporarily place component to show the attempted layout
    comp->placed_x = x;
    comp->placed_y = y;
    comp->is_placed = 1;

    debug_log_ascii_grid(solver, "ATTEMPTED PLACEMENT (FAILED)");

    // Restore original state
    comp->is_placed = was_placed;
    comp->placed_x = old_x;
    comp->placed_y = old_y;
  }

  fprintf(solver->debug_file, "\n");
  fflush(solver->debug_file);
}

/**
 * @brief Logs ASCII art visualization of current grid state
 * 
 * Renders all placed components as ASCII art with coordinate references.
 * Automatically calculates grid bounds and limits output size for readability.
 * Uses spaces for empty areas to distinguish from component dots.
 * 
 * @param solver The layout solver instance
 * @param title  Descriptive title for this grid visualization
 */
void debug_log_ascii_grid(LayoutSolver *solver, const char *title) {
  if (!solver->debug_file)
    return;

  // Find actual bounds of placed components
  int min_x = 0, max_x = 0, min_y = 0, max_y = 0;
  int first = 1;

  for (int i = 0; i < solver->component_count; i++) {
    Component *comp = &solver->components[i];
    if (comp->is_placed) {
      if (first) {
        min_x = comp->placed_x;
        max_x = comp->placed_x + comp->width - 1;
        min_y = comp->placed_y;
        max_y = comp->placed_y + comp->height - 1;
        first = 0;
      } else {
        if (comp->placed_x < min_x)
          min_x = comp->placed_x;
        if (comp->placed_x + comp->width - 1 > max_x)
          max_x = comp->placed_x + comp->width - 1;
        if (comp->placed_y < min_y)
          min_y = comp->placed_y;
        if (comp->placed_y + comp->height - 1 > max_y)
          max_y = comp->placed_y + comp->height - 1;
      }
    }
  }

  if (first) {
    fprintf(solver->debug_file, "  %s: (no components placed)\n", title);
    return;
  }

  fprintf(solver->debug_file, "  %s:\n", title);
  fprintf(solver->debug_file, "    Bounds: (%d,%d) to (%d,%d)\n", min_x, min_y,
          max_x, max_y);

  // Limit output size for debug file
  int max_width = 60;
  int max_height = 30;
  int display_width =
      (max_x - min_x + 1 > max_width) ? max_width : max_x - min_x + 1;
  int display_height =
      (max_y - min_y + 1 > max_height) ? max_height : max_y - min_y + 1;

  fprintf(solver->debug_file, "    ASCII Grid (%dx%d):\n", display_width,
          display_height);

  // Add column numbers for reference (every 5th column)
  fprintf(solver->debug_file, "      ");
  for (int x = 0; x < display_width; x++) {
    if (x % 5 == 0) {
      fprintf(solver->debug_file, "%d", (x / 10) % 10);
    } else {
      fprintf(solver->debug_file, " ");
    }
  }
  fprintf(solver->debug_file, "\n      ");
  for (int x = 0; x < display_width; x++) {
    if (x % 5 == 0) {
      fprintf(solver->debug_file, "%d", x % 10);
    } else {
      fprintf(solver->debug_file, " ");
    }
  }
  fprintf(solver->debug_file, "\n");

  // Display grid within bounds
  for (int y = min_y; y < min_y + display_height; y++) {
    fprintf(solver->debug_file, "   %2d ", y);
    for (int x = min_x; x < min_x + display_width; x++) {
      char ch = ' '; // Default background character (space)

      // Check each component for this position
      for (int i = 0; i < solver->component_count; i++) {
        Component *comp = &solver->components[i];
        if (comp->is_placed && x >= comp->placed_x &&
            x < comp->placed_x + comp->width && y >= comp->placed_y &&
            y < comp->placed_y + comp->height) {
          int local_x = x - comp->placed_x;
          int local_y = y - comp->placed_y;
          if (comp->ascii_tile[local_y][local_x] != ' ') {
            ch = comp->ascii_tile[local_y][local_x];
            break;
          }
        }
      }

      fprintf(solver->debug_file, "%c", ch);
    }
    fprintf(solver->debug_file, "\n");
  }

  if (max_x - min_x + 1 > max_width || max_y - min_y + 1 > max_height) {
    fprintf(solver->debug_file, "    (grid truncated - actual size: %dx%d)\n",
            max_x - min_x + 1, max_y - min_y + 1);
  }

  fprintf(solver->debug_file, "\n");
  fflush(solver->debug_file);
}

/**
 * @brief Logs comprehensive grid state information
 * 
 * Combines grid metadata (bounds, dimensions) with component placement status
 * and ASCII visualization. Used at key solver milestones for state tracking.
 * 
 * @param solver The layout solver instance
 * @param stage  Description of current solver stage
 */
void debug_log_grid_state(LayoutSolver *solver, const char *stage) {
  if (!solver->debug_file)
    return;

  fprintf(solver->debug_file, "GRID STATE - %s:\n", stage);
  fprintf(solver->debug_file, "  Grid bounds: (%d,%d) to (%d,%d)\n",
          solver->grid_min_x, solver->grid_min_y,
          solver->grid_min_x + solver->grid_width - 1,
          solver->grid_min_y + solver->grid_height - 1);
  fprintf(solver->debug_file, "  Grid size: %dx%d\n", solver->grid_width,
          solver->grid_height);

  // List all placed components
  fprintf(solver->debug_file, "  Placed components:\n");
  for (int i = 0; i < solver->component_count; i++) {
    Component *comp = &solver->components[i];
    if (comp->is_placed) {
      fprintf(solver->debug_file, "    %s: (%d,%d) [%dx%d] group_id=%d\n",
              comp->name, comp->placed_x, comp->placed_y, comp->width,
              comp->height, comp->group_id);
    }
  }

  // Show ASCII visualization
  debug_log_ascii_grid(solver, "ASCII LAYOUT");

  fprintf(solver->debug_file, "\n");
  fflush(solver->debug_file);
}

/**
 * @brief Logs detailed constraint analysis for a specific component
 * 
 * Analyzes all constraints involving the given component, calculates constraint
 * degree, and reports placement status of related components. Critical for
 * understanding why placement attempts succeed or fail.
 * 
 * @param solver The layout solver instance
 * @param comp   Component to analyze constraints for
 */
void debug_log_constraint_analysis(LayoutSolver *solver, Component *comp) {
  if (!solver->debug_file)
    return;

  fprintf(solver->debug_file, "CONSTRAINT ANALYSIS: %s\n", comp->name);
  fprintf(solver->debug_file, "  Constraint degree: %d\n",
          count_constraint_degree(solver, comp));

  fprintf(solver->debug_file, "  Related constraints:\n");
  for (int i = 0; i < solver->constraint_count; i++) {
    DSLConstraint *constraint = &solver->constraints[i];
    if (strcmp(constraint->component_a, comp->name) == 0 ||
        strcmp(constraint->component_b, comp->name) == 0) {

      const char *dir_name;
      switch (constraint->direction) {
      case 'n':
        dir_name = "NORTH";
        break;
      case 's':
        dir_name = "SOUTH";
        break;
      case 'e':
        dir_name = "EAST";
        break;
      case 'w':
        dir_name = "WEST";
        break;
      case 'a':
        dir_name = "ANY";
        break;
      default:
        dir_name = "UNKNOWN";
        break;
      }

      Component *other_comp = NULL;
      int is_comp_a = (strcmp(constraint->component_a, comp->name) == 0);

      if (is_comp_a) {
        other_comp = find_component(solver, constraint->component_b);
        fprintf(solver->debug_file,
                "    ADJACENT(%s, %s, %s) - %s is component A\n",
                constraint->component_a, constraint->component_b, dir_name,
                comp->name);
      } else {
        other_comp = find_component(solver, constraint->component_a);
        fprintf(solver->debug_file,
                "    ADJACENT(%s, %s, %s) - %s is component B\n",
                constraint->component_a, constraint->component_b, dir_name,
                comp->name);
      }

      if (other_comp) {
        fprintf(solver->debug_file, "      Other component: %s (placed: %s)\n",
                other_comp->name, other_comp->is_placed ? "YES" : "NO");
        if (other_comp->is_placed) {
          fprintf(solver->debug_file,
                  "      Other component position: (%d,%d)\n",
                  other_comp->placed_x, other_comp->placed_y);
        }
      }
    }
  }

  fprintf(solver->debug_file, "\n");
  fflush(solver->debug_file);
}

/**
 * @brief Initializes the layout solver with dynamic grid capabilities
 * 
 * Sets up solver state including component arrays, constraint storage,
 * dynamic grid system, and debug infrastructure. The initial grid size
 * serves as a starting point and will expand as needed during placement.
 * 
 * @param solver The layout solver instance to initialize
 * @param width  Initial grid width (will expand dynamically)
 * @param height Initial grid height (will expand dynamically)
 */
void init_solver(LayoutSolver *solver, int width, int height) {
  solver->component_count = 0;
  solver->constraint_count = 0;
  solver->grid_width = width;
  solver->grid_height = height;
  solver->grid_min_x = 0;
  solver->grid_min_y = 0;
  solver->next_group_id = 1;
  solver->debug_file = NULL;

  // Initialize grid with empty spaces
  for (int i = 0; i < MAX_GRID_SIZE; i++) {
    for (int j = 0; j < MAX_GRID_SIZE; j++) {
      solver->grid[i][j] = ' ';
    }
  }

  // Initialize constraint graph and placement attempts
  solver->total_iterations = 0;

  for (int i = 0; i < MAX_COMPONENTS; i++) {
    solver->placement_attempts[i] = 0;
    solver->components[i].group_id = 0; // No group initially
    for (int j = 0; j < MAX_COMPONENTS; j++) {
      solver->constraint_graph[i][j] = 0;
    }
  }
}

/**
 * @brief Checks if two rectangles have horizontal overlap
 * 
 * Determines if two horizontal spans [x1, x1+w1) and [x2, x2+w2) overlap.
 * Used for validating north/south adjacency constraints where components
 * must share horizontal space.
 * 
 * @param x1 First rectangle x position
 * @param w1 First rectangle width
 * @param x2 Second rectangle x position
 * @param w2 Second rectangle width
 * @return   1 if rectangles overlap horizontally, 0 otherwise
 */
int has_horizontal_overlap(int x1, int w1, int x2, int w2) {
  return !(x1 + w1 <= x2 || x2 + w2 <= x1);
}

/**
 * @brief Checks if two rectangles have vertical overlap
 * 
 * Determines if two vertical spans [y1, y1+h1) and [y2, y2+h2) overlap.
 * Used for validating east/west adjacency constraints where components
 * must share vertical space.
 * 
 * @param y1 First rectangle y position
 * @param h1 First rectangle height
 * @param y2 Second rectangle y position
 * @param h2 Second rectangle height
 * @return   1 if rectangles overlap vertically, 0 otherwise
 */
int has_vertical_overlap(int y1, int h1, int y2, int h2) {
  return !(y1 + h1 <= y2 || y2 + h2 <= y1);
}

/**
 * @brief Checks if placing a component would cause character overlap
 * 
 * Tests each non-space character in the component's ASCII art against the
 * current grid state. Only non-space characters count as occupied space,
 * allowing components with internal spaces to interleave properly.
 * 
 * @param solver The layout solver instance
 * @param comp   Component to test placement for
 * @param x      Proposed x coordinate for component
 * @param y      Proposed y coordinate for component
 * @return       1 if overlap detected, 0 if placement is clear
 */
int has_overlap(LayoutSolver *solver, Component *comp, int x, int y) {
  for (int row = 0; row < comp->height; row++) {
    for (int col = 0; col < comp->width; col++) {
      if (comp->ascii_tile[row][col] != ' ') {
        int grid_x = x + col;
        int grid_y = y + row;

        // Check bounds
        if (grid_x < solver->grid_min_x || grid_y < solver->grid_min_y ||
            grid_x >= solver->grid_min_x + solver->grid_width ||
            grid_y >= solver->grid_min_y + solver->grid_height) {
          continue; // Outside current grid bounds - will be expanded
        }

        // Check for actual overlap with non-space characters
        int grid_idx_x = grid_x - solver->grid_min_x;
        int grid_idx_y = grid_y - solver->grid_min_y;
        if (solver->grid[grid_idx_y][grid_idx_x] != ' ') {
          return 1; // Overlap detected
        }
      }
    }
  }
  return 0;
}

/**
 * @brief Expands the dynamic grid to accommodate component placement
 * 
 * Dynamically resizes the grid to include the specified component at the
 * given position. Handles negative coordinates by adjusting grid origin
 * and preserving existing component data through coordinate translation.
 * 
 * @param solver The layout solver instance
 * @param comp   Component requiring grid space
 * @param x      Component's target x coordinate
 * @param y      Component's target y coordinate
 */
void expand_grid_for_component(LayoutSolver *solver, Component *comp, int x,
                               int y) {
  int new_min_x = solver->grid_min_x;
  int new_min_y = solver->grid_min_y;
  int new_max_x = solver->grid_min_x + solver->grid_width - 1;
  int new_max_y = solver->grid_min_y + solver->grid_height - 1;

  // Expand bounds to include the new component
  if (x < new_min_x)
    new_min_x = x;
  if (y < new_min_y)
    new_min_y = y;
  if (x + comp->width - 1 > new_max_x)
    new_max_x = x + comp->width - 1;
  if (y + comp->height - 1 > new_max_y)
    new_max_y = y + comp->height - 1;

  int new_width = new_max_x - new_min_x + 1;
  int new_height = new_max_y - new_min_y + 1;

  // Check if we need to expand
  if (new_min_x != solver->grid_min_x || new_min_y != solver->grid_min_y ||
      new_width != solver->grid_width || new_height != solver->grid_height) {

    // Create new grid
    char new_grid[MAX_GRID_SIZE][MAX_GRID_SIZE];
    for (int i = 0; i < MAX_GRID_SIZE; i++) {
      for (int j = 0; j < MAX_GRID_SIZE; j++) {
        new_grid[i][j] = ' ';
      }
    }

    // Copy existing grid data to new position
    for (int y_old = 0; y_old < solver->grid_height; y_old++) {
      for (int x_old = 0; x_old < solver->grid_width; x_old++) {
        int new_y = (solver->grid_min_y + y_old) - new_min_y;
        int new_x = (solver->grid_min_x + x_old) - new_min_x;
        if (new_y >= 0 && new_y < MAX_GRID_SIZE && new_x >= 0 &&
            new_x < MAX_GRID_SIZE) {
          new_grid[new_y][new_x] = solver->grid[y_old][x_old];
        }
      }
    }

    // Update solver with new grid
    memcpy(solver->grid, new_grid, sizeof(solver->grid));
    solver->grid_min_x = new_min_x;
    solver->grid_min_y = new_min_y;
    solver->grid_width = new_width;
    solver->grid_height = new_height;

    // Update component positions to reflect new grid origin
    for (int i = 0; i < solver->component_count; i++) {
      if (solver->components[i].is_placed) {
        // Component positions remain the same in world coordinates
        // Grid indexing handles the translation
      }
    }
  }
}

/**
 * @brief Moves all components in a group by specified offset
 * 
 * Implements group-based component movement for constraint satisfaction.
 * Components sharing the same group_id move together to maintain their
 * relative spatial relationships during sliding placement operations.
 * 
 * @param solver   The layout solver instance
 * @param group_id ID of component group to move
 * @param dx       Horizontal offset for movement
 * @param dy       Vertical offset for movement
 */
void move_component_group(LayoutSolver *solver, int group_id, int dx, int dy) {
  // First, remove all components in the group from grid
  for (int i = 0; i < solver->component_count; i++) {
    Component *comp = &solver->components[i];
    if (comp->is_placed && comp->group_id == group_id) {
      // Clear from grid
      for (int row = 0; row < comp->height; row++) {
        for (int col = 0; col < comp->width; col++) {
          if (comp->ascii_tile[row][col] != ' ') {
            int grid_x = comp->placed_x + col - solver->grid_min_x;
            int grid_y = comp->placed_y + row - solver->grid_min_y;
            if (grid_x >= 0 && grid_x < solver->grid_width && grid_y >= 0 &&
                grid_y < solver->grid_height) {
              solver->grid[grid_y][grid_x] = ' ';
            }
          }
        }
      }
    }
  }

  // Update positions and re-place
  for (int i = 0; i < solver->component_count; i++) {
    Component *comp = &solver->components[i];
    if (comp->is_placed && comp->group_id == group_id) {
      comp->placed_x += dx;
      comp->placed_y += dy;

      // Expand grid if necessary
      expand_grid_for_component(solver, comp, comp->placed_x, comp->placed_y);

      // Place back on grid
      for (int row = 0; row < comp->height; row++) {
        for (int col = 0; col < comp->width; col++) {
          if (comp->ascii_tile[row][col] != ' ') {
            int grid_x = comp->placed_x + col - solver->grid_min_x;
            int grid_y = comp->placed_y + row - solver->grid_min_y;
            if (grid_x >= 0 && grid_x < solver->grid_width && grid_y >= 0 &&
                grid_y < solver->grid_height) {
              solver->grid[grid_y][grid_x] = comp->ascii_tile[row][col];
            }
          }
        }
      }
    }
  }
}

/**
 * @brief Counts the constraint degree (number of constraints) for a component
 * 
 * Calculates how many constraints involve the given component, either as
 * component A or component B. Used for most-constrained-first heuristic
 * to improve solver efficiency.
 * 
 * @param solver The layout solver instance
 * @param comp   Component to count constraints for
 * @return       Number of constraints involving this component
 */
int count_constraint_degree(LayoutSolver *solver, Component *comp) {
  int degree = 0;
  for (int i = 0; i < solver->constraint_count; i++) {
    DSLConstraint *constraint = &solver->constraints[i];
    if (strcmp(constraint->component_a, comp->name) == 0 ||
        strcmp(constraint->component_b, comp->name) == 0) {
      degree++;
    }
  }
  return degree;
}

/**
 * @brief Finds the unplaced component with the highest constraint degree
 * 
 * Implements the most-constrained-first heuristic for constraint satisfaction.
 * Components with more constraints are placed first to reduce backtracking
 * and improve overall solver performance.
 * 
 * @param solver The layout solver instance
 * @return       Most constrained unplaced component, or NULL if none found
 */
Component *find_most_constrained_unplaced(LayoutSolver *solver) {
  Component *most_constrained = NULL;
  int max_degree = -1;

  for (int i = 0; i < solver->component_count; i++) {
    Component *comp = &solver->components[i];
    if (!comp->is_placed) {
      int degree = count_constraint_degree(solver, comp);
      if (degree > max_degree) {
        max_degree = degree;
        most_constrained = comp;
      }
    }
  }

  return most_constrained;
}

/**
 * @brief Attempts component placement using sliding algorithm
 * 
 * Core placement algorithm that tries to satisfy adjacency constraints by
 * sliding the new component along the perpendicular axis to the constraint
 * direction. For north/south constraints, slides east-west; for east/west
 * constraints, slides north-south.
 * 
 * @param solver      The layout solver instance
 * @param new_comp    Component to be placed
 * @param placed_comp Already placed reference component
 * @param dir         Direction of adjacency constraint ('n', 's', 'e', 'w')
 * @return            1 if placement successful, 0 if failed
 */
int try_place_with_sliding(LayoutSolver *solver, Component *new_comp,
                           Component *placed_comp, Direction dir) {
  int base_x, base_y;
  int slide_min, slide_max;
  int dx, dy;

  switch (dir) {
  case 'n': // new_comp north of placed_comp
    base_x = placed_comp->placed_x;
    base_y = placed_comp->placed_y - new_comp->height;
    // Try sliding east-west
    slide_min = -placed_comp->width;
    slide_max = placed_comp->width;
    dx = 1;
    dy = 0;
    break;

  case 's': // new_comp south of placed_comp
    base_x = placed_comp->placed_x;
    base_y = placed_comp->placed_y + placed_comp->height;
    // Try sliding east-west
    slide_min = -placed_comp->width;
    slide_max = placed_comp->width;
    dx = 1;
    dy = 0;
    break;

  case 'e': // new_comp east of placed_comp
    base_x = placed_comp->placed_x + placed_comp->width;
    base_y = placed_comp->placed_y;
    // Try sliding north-south
    slide_min = -placed_comp->height;
    slide_max = placed_comp->height;
    dx = 0;
    dy = 1;
    break;

  case 'w': // new_comp west of placed_comp
    base_x = placed_comp->placed_x - new_comp->width;
    base_y = placed_comp->placed_y;
    // Try sliding north-south
    slide_min = -placed_comp->height;
    slide_max = placed_comp->height;
    dx = 0;
    dy = 1;
    break;

  default:
    debug_log_placement_attempt(solver, new_comp, placed_comp, dir, 0, 0, 0);
    return 0;
  }

  // Try different sliding positions
  for (int slide = slide_min; slide <= slide_max; slide++) {
    int try_x = base_x + slide * dx;
    int try_y = base_y + slide * dy;

    // Expand grid for this position
    expand_grid_for_component(solver, new_comp, try_x, try_y);

    // Check if placement is valid (no overlap with non-space characters)
    if (!has_overlap(solver, new_comp, try_x, try_y)) {
      // Check adjacency constraint
      if ((dir == 'n' || dir == 's') &&
          has_horizontal_overlap(try_x, new_comp->width, placed_comp->placed_x,
                                 placed_comp->width)) {
        // Valid north/south placement with horizontal overlap
        new_comp->placed_x = try_x;
        new_comp->placed_y = try_y;
        new_comp->is_placed = 1;

        // Place on grid
        for (int row = 0; row < new_comp->height; row++) {
          for (int col = 0; col < new_comp->width; col++) {
            if (new_comp->ascii_tile[row][col] != ' ') {
              int grid_x = try_x + col - solver->grid_min_x;
              int grid_y = try_y + row - solver->grid_min_y;
              if (grid_x >= 0 && grid_x < solver->grid_width && grid_y >= 0 &&
                  grid_y < solver->grid_height) {
                solver->grid[grid_y][grid_x] = new_comp->ascii_tile[row][col];
              }
            }
          }
        }

        // Log successful placement
        debug_log_placement_attempt(solver, new_comp, placed_comp, dir, try_x,
                                    try_y, 1);
        return 1; // Success
      } else if ((dir == 'e' || dir == 'w') &&
                 has_vertical_overlap(try_y, new_comp->height,
                                      placed_comp->placed_y,
                                      placed_comp->height)) {
        // Valid east/west placement with vertical overlap
        new_comp->placed_x = try_x;
        new_comp->placed_y = try_y;
        new_comp->is_placed = 1;

        // Place on grid
        for (int row = 0; row < new_comp->height; row++) {
          for (int col = 0; col < new_comp->width; col++) {
            if (new_comp->ascii_tile[row][col] != ' ') {
              int grid_x = try_x + col - solver->grid_min_x;
              int grid_y = try_y + row - solver->grid_min_y;
              if (grid_x >= 0 && grid_x < solver->grid_width && grid_y >= 0 &&
                  grid_y < solver->grid_height) {
                solver->grid[grid_y][grid_x] = new_comp->ascii_tile[row][col];
              }
            }
          }
        }

        // Log successful placement
        debug_log_placement_attempt(solver, new_comp, placed_comp, dir, try_x,
                                    try_y, 1);
        return 1; // Success
      } else {
        // Log failed placement attempt (adjacency not satisfied)
        debug_log_placement_attempt(solver, new_comp, placed_comp, dir, try_x,
                                    try_y, 0);
      }
    } else {
      // Log failed placement attempt (overlap detected)
      debug_log_placement_attempt(solver, new_comp, placed_comp, dir, try_x,
                                  try_y, 0);
    }
  }

  return 0; // Could not place
}

/**
 * @brief Normalizes grid coordinates to ensure all components are in positive space
 * 
 * Translates the entire layout so that the minimum x,y coordinates become (0,0).
 * This final step ensures the output uses standard positive coordinate system
 * regardless of negative coordinates used during dynamic placement.
 * 
 * @param solver The layout solver instance
 */
void normalize_grid_coordinates(LayoutSolver *solver) {
  if (solver->grid_min_x >= 0 && solver->grid_min_y >= 0) {
    return; // Already normalized
  }

  int dx = -solver->grid_min_x;
  int dy = -solver->grid_min_y;

  // Update all component positions
  for (int i = 0; i < solver->component_count; i++) {
    if (solver->components[i].is_placed) {
      solver->components[i].placed_x += dx;
      solver->components[i].placed_y += dy;
    }
  }

  // Rebuild grid with normalized coordinates
  char new_grid[MAX_GRID_SIZE][MAX_GRID_SIZE];
  for (int i = 0; i < MAX_GRID_SIZE; i++) {
    for (int j = 0; j < MAX_GRID_SIZE; j++) {
      new_grid[i][j] = ' ';
    }
  }

  // Place all components on normalized grid
  for (int i = 0; i < solver->component_count; i++) {
    Component *comp = &solver->components[i];
    if (comp->is_placed) {
      for (int row = 0; row < comp->height; row++) {
        for (int col = 0; col < comp->width; col++) {
          if (comp->ascii_tile[row][col] != ' ') {
            int grid_x = comp->placed_x + col;
            int grid_y = comp->placed_y + row;
            if (grid_x >= 0 && grid_x < MAX_GRID_SIZE && grid_y >= 0 &&
                grid_y < MAX_GRID_SIZE) {
              new_grid[grid_y][grid_x] = comp->ascii_tile[row][col];
            }
          }
        }
      }
    }
  }

  memcpy(solver->grid, new_grid, sizeof(solver->grid));
  solver->grid_min_x = 0;
  solver->grid_min_y = 0;
}

/**
 * @brief Main dynamic constraint satisfaction algorithm
 * 
 * Implements the complete constraint solving workflow:
 * 1. Places most constrained component at origin
 * 2. Iteratively places remaining components using sliding algorithm
 * 3. Maintains debug logging throughout process
 * 4. Normalizes coordinates for final output
 * 
 * Features:
 * - Most-constrained-first heuristic
 * - Dynamic grid expansion
 * - Sliding placement with overlap validation
 * - Component grouping for spatial relationships
 * - Comprehensive debug visualization
 * - Iteration limits to prevent infinite loops
 * 
 * @param solver The layout solver instance
 * @return       1 if all components placed successfully, 0 if failed
 */
int solve_dynamic_constraints(LayoutSolver *solver) {
  if (solver->component_count == 0) {
    return 1; // Nothing to solve
  }

  // Initialize debug file
  init_debug_file(solver);

  printf("🧠 Starting dynamic constraint solver...\n");

  // Find most constrained component and place it at origin
  Component *first_comp = find_most_constrained_unplaced(solver);
  if (!first_comp) {
    close_debug_file(solver);
    return 0; // No components found
  }

  printf("📍 Placing first component '%s' at origin (0,0)\n", first_comp->name);

  // Log constraint analysis for first component
  debug_log_constraint_analysis(solver, first_comp);

  // Initialize grid to fit first component
  solver->grid_width = first_comp->width;
  solver->grid_height = first_comp->height;
  solver->grid_min_x = 0;
  solver->grid_min_y = 0;

  // Place first component
  first_comp->placed_x = 0;
  first_comp->placed_y = 0;
  first_comp->is_placed = 1;
  first_comp->group_id = solver->next_group_id++;

  // Place on grid
  for (int row = 0; row < first_comp->height; row++) {
    for (int col = 0; col < first_comp->width; col++) {
      if (first_comp->ascii_tile[row][col] != ' ') {
        solver->grid[row][col] = first_comp->ascii_tile[row][col];
      }
    }
  }

  // Log initial grid state
  debug_log_grid_state(solver, "AFTER FIRST COMPONENT PLACEMENT");

  // Place remaining components
  while (1) {
    solver->total_iterations++;
    if (solver->total_iterations > MAX_SOLVER_ITERATIONS) {
      printf("❌ Maximum iterations reached\n");

      if (solver->debug_file) {
        fprintf(solver->debug_file,
                "❌ SOLVER TERMINATED: Maximum iterations (%d) reached\n",
                MAX_SOLVER_ITERATIONS);
      }

      close_debug_file(solver);
      return 0;
    }

    Component *next_comp = find_most_constrained_unplaced(solver);
    if (!next_comp) {
      break; // All components placed
    }

    printf("🔧 Placing component '%s'\n", next_comp->name);

    // Log constraint analysis for this component
    debug_log_constraint_analysis(solver, next_comp);

    // Find constraints involving this component
    int placed = 0;
    for (int i = 0; i < solver->constraint_count && !placed; i++) {
      DSLConstraint *constraint = &solver->constraints[i];
      Component *other_comp = NULL;
      Direction dir;

      if (strcmp(constraint->component_a, next_comp->name) == 0) {
        other_comp = find_component(solver, constraint->component_b);
        dir = constraint->direction;
      } else if (strcmp(constraint->component_b, next_comp->name) == 0) {
        other_comp = find_component(solver, constraint->component_a);
        // Reverse direction for component_b
        switch (constraint->direction) {
        case 'n':
          dir = 's';
          break;
        case 's':
          dir = 'n';
          break;
        case 'e':
          dir = 'w';
          break;
        case 'w':
          dir = 'e';
          break;
        default:
          dir = constraint->direction;
          break;
        }
      }

      if (other_comp && other_comp->is_placed) {
        printf("  💡 Trying to satisfy constraint with '%s' (direction: %c)\n",
               other_comp->name, dir);

        if (solver->debug_file) {
          fprintf(solver->debug_file,
                  "ATTEMPTING TO PLACE: %s relative to %s (direction: %c)\n",
                  next_comp->name, other_comp->name, dir);
          fflush(solver->debug_file);
        }

        if (try_place_with_sliding(solver, next_comp, other_comp, dir)) {
          next_comp->group_id = other_comp->group_id; // Join the group
          printf("  ✅ Successfully placed '%s' at (%d, %d)\n", next_comp->name,
                 next_comp->placed_x, next_comp->placed_y);
          placed = 1;

          // Log updated grid state
          debug_log_grid_state(solver, "AFTER SUCCESSFUL PLACEMENT");
        }
      }
    }

    if (!placed) {
      printf("  ❌ Could not place component '%s'\n", next_comp->name);

      if (solver->debug_file) {
        fprintf(solver->debug_file,
                "❌ FAILED TO PLACE: %s - No valid position found\n",
                next_comp->name);
        fflush(solver->debug_file);
      }

      close_debug_file(solver);
      return 0; // Failed to place component
    }
  }

  // Normalize coordinates
  normalize_grid_coordinates(solver);

  // Log final grid state
  debug_log_grid_state(solver, "FINAL - AFTER NORMALIZATION");

  printf("🎉 All components placed successfully!\n");

  // Close debug file
  close_debug_file(solver);
  return 1;
}

/**
 * @brief Primary constraint solving interface
 * 
 * Wrapper function that delegates to the dynamic constraint solver.
 * Provides backwards compatibility while using the advanced dynamic
 * placement algorithm internally.
 * 
 * @param solver The layout solver instance
 * @return       1 if solution found, 0 if failed
 */
int solve_constraints(LayoutSolver *solver) {
  return solve_dynamic_constraints(solver);
}

// =============================================================================
// DISPLAY FUNCTIONS
// =============================================================================

/**
 * @brief Displays all parsed components with their ASCII art
 * 
 * Console output function that shows component names, dimensions, and
 * ASCII tile representations. Limited output size for readability.
 * 
 * @param solver The layout solver instance
 */
void display_components(LayoutSolver *solver) {
  printf("\n📦 PARSED COMPONENTS:\n");
  printf("=====================\n");

  for (int i = 0; i < solver->component_count; i++) {
    Component *comp = &solver->components[i];
    printf("\n**%s** (%dx%d):\n", comp->name, comp->width, comp->height);

    for (int row = 0; row < comp->height && row < 10; row++) {  // Limit height
      for (int col = 0; col < comp->width && col < 30; col++) { // Limit width
        printf("%c", comp->ascii_tile[row][col]);
      }
      printf("\n");
    }
  }
}

/**
 * @brief Displays all parsed constraints in human-readable format
 * 
 * Console output function showing constraint types, components involved,
 * and directional relationships in expanded format.
 * 
 * @param solver The layout solver instance
 */
void display_constraints(LayoutSolver *solver) {
  printf("\n📋 PARSED CONSTRAINTS:\n");
  printf("======================\n");

  for (int i = 0; i < solver->constraint_count; i++) {
    DSLConstraint *c = &solver->constraints[i];

    const char *type_name;
    switch (c->type) {
    case DSL_ADJACENT:
      type_name = "ADJACENT";
      break;
    default:
      type_name = "UNKNOWN";
      break;
    }

    const char *dir_name;
    switch (c->direction) {
    case 'n':
      dir_name = "NORTH";
      break;
    case 'e':
      dir_name = "EAST";
      break;
    case 's':
      dir_name = "SOUTH";
      break;
    case 'w':
      dir_name = "WEST";
      break;
    case 'a':
      dir_name = "ANY";
      break;
    default:
      dir_name = "NONE";
      break;
    }

    printf("  %d. %s(%s, %s, %s)\n", i + 1, type_name, c->component_a,
           c->component_b, dir_name);
  }
}

/**
 * @brief Displays the final solved layout as ASCII art
 * 
 * Renders the complete structure layout by compositing all placed components.
 * Automatically calculates display bounds and limits output size for console
 * viewing. This is the primary visual output of the solver.
 * 
 * @param solver The layout solver instance
 */
void display_grid(LayoutSolver *solver) {
  printf("\n🏗️  Generated Structure Layout:\n");
  printf("=================================\n");

  // Find actual bounds of placed components
  int min_x = 0, max_x = 0, min_y = 0, max_y = 0;
  int first = 1;

  for (int i = 0; i < solver->component_count; i++) {
    Component *comp = &solver->components[i];
    if (comp->is_placed) {
      if (first) {
        min_x = comp->placed_x;
        max_x = comp->placed_x + comp->width - 1;
        min_y = comp->placed_y;
        max_y = comp->placed_y + comp->height - 1;
        first = 0;
      } else {
        if (comp->placed_x < min_x)
          min_x = comp->placed_x;
        if (comp->placed_x + comp->width - 1 > max_x)
          max_x = comp->placed_x + comp->width - 1;
        if (comp->placed_y < min_y)
          min_y = comp->placed_y;
        if (comp->placed_y + comp->height - 1 > max_y)
          max_y = comp->placed_y + comp->height - 1;
      }
    }
  }

  // Display grid within bounds
  for (int y = min_y; y <= max_y && (y - min_y) < MAX_OUTPUT_LINES; y++) {
    for (int x = min_x; x <= max_x && (x - min_x) < MAX_OUTPUT_WIDTH; x++) {
      char ch = ' ';

      // Check each component for this position
      for (int i = 0; i < solver->component_count; i++) {
        Component *comp = &solver->components[i];
        if (comp->is_placed && x >= comp->placed_x &&
            x < comp->placed_x + comp->width && y >= comp->placed_y &&
            y < comp->placed_y + comp->height) {
          int local_x = x - comp->placed_x;
          int local_y = y - comp->placed_y;
          if (comp->ascii_tile[local_y][local_x] != ' ') {
            ch = comp->ascii_tile[local_y][local_x];
            break;
          }
        }
      }

      printf("%c", ch);
    }
    printf("\n");
  }
  printf("=================================\n");
}

/**
 * @brief Verifies that all constraints are satisfied in the final solution
 * 
 * Post-solving validation that checks each constraint against the actual
 * component placements. Reports satisfaction status for debugging and
 * solution verification.
 * 
 * @param solver The layout solver instance
 */
void verify_solution(LayoutSolver *solver) {
  printf("\n🔍 CONSTRAINT VERIFICATION:\n");
  printf("===========================\n");

  for (int i = 0; i < solver->constraint_count; i++) {
    DSLConstraint *c = &solver->constraints[i];
    Component *comp_a = find_component(solver, c->component_a);
    Component *comp_b = find_component(solver, c->component_b);

    if (!comp_a || !comp_b || !comp_a->is_placed || !comp_b->is_placed) {
      printf("  ❌ %s(%s, %s): Components not found or not placed\n",
             (c->type == DSL_ADJACENT ? "ADJACENT" : "OTHER"), c->component_a,
             c->component_b);
      continue;
    }

    const char *type_name = (c->type == DSL_ADJACENT ? "ADJACENT" : "UNKNOWN");

    // Check if constraint is satisfied based on direction
    int satisfied = 0;
    switch (c->direction) {
    case 'n': // comp_b north of comp_a
      satisfied = (comp_b->placed_y + comp_b->height == comp_a->placed_y) &&
                  has_horizontal_overlap(comp_a->placed_x, comp_a->width,
                                         comp_b->placed_x, comp_b->width);
      break;
    case 's': // comp_b south of comp_a
      satisfied = (comp_a->placed_y + comp_a->height == comp_b->placed_y) &&
                  has_horizontal_overlap(comp_a->placed_x, comp_a->width,
                                         comp_b->placed_x, comp_b->width);
      break;
    case 'e': // comp_b east of comp_a
      satisfied = (comp_a->placed_x + comp_a->width == comp_b->placed_x) &&
                  has_vertical_overlap(comp_a->placed_y, comp_a->height,
                                       comp_b->placed_y, comp_b->height);
      break;
    case 'w': // comp_b west of comp_a
      satisfied = (comp_b->placed_x + comp_b->width == comp_a->placed_x) &&
                  has_vertical_overlap(comp_a->placed_y, comp_a->height,
                                       comp_b->placed_y, comp_b->height);
      break;
    default:
      satisfied = 1; // Unknown direction - assume satisfied
      break;
    }

    printf("  %s(%s, %s): %s\n", type_name, c->component_a, c->component_b,
           satisfied ? "✅ SATISFIED" : "❌ VIOLATED");
    printf("    %s at (%d,%d), %s at (%d,%d)\n\n", comp_a->name,
           comp_a->placed_x, comp_a->placed_y, comp_b->name, comp_b->placed_x,
           comp_b->placed_y);
  }
}
