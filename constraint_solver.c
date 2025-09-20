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

    // Show the grid after successful placement with highlighting
    debug_log_placement_grid(solver, "AFTER SUCCESSFUL PLACEMENT", comp, x, y);

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

    debug_log_placement_grid(solver, "ATTEMPTED PLACEMENT (FAILED)", comp, x, y);

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

// Debug function specifically for placement attempts with highlighting
void debug_log_placement_grid(LayoutSolver *solver, const char *title, Component* highlight_comp, int highlight_x, int highlight_y) {
  if (!solver->debug_file)
    return;

  // Find actual bounds including the highlight component at its attempted position
  int min_x = 0, max_x = 0, min_y = 0, max_y = 0;
  int first = 1;

  // Include all placed components
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
        if (comp->placed_x < min_x) min_x = comp->placed_x;
        if (comp->placed_x + comp->width - 1 > max_x) max_x = comp->placed_x + comp->width - 1;
        if (comp->placed_y < min_y) min_y = comp->placed_y;
        if (comp->placed_y + comp->height - 1 > max_y) max_y = comp->placed_y + comp->height - 1;
      }
    }
  }

  // Include the highlight component bounds
  if (highlight_comp) {
    if (first) {
      min_x = highlight_x;
      max_x = highlight_x + highlight_comp->width - 1;
      min_y = highlight_y;
      max_y = highlight_y + highlight_comp->height - 1;
      first = 0;
    } else {
      if (highlight_x < min_x) min_x = highlight_x;
      if (highlight_x + highlight_comp->width - 1 > max_x) max_x = highlight_x + highlight_comp->width - 1;
      if (highlight_y < min_y) min_y = highlight_y;
      if (highlight_y + highlight_comp->height - 1 > max_y) max_y = highlight_y + highlight_comp->height - 1;
    }
  }

  if (first) {
    fprintf(solver->debug_file, "  %s: (no components to display)\n", title);
    return;
  }

  fprintf(solver->debug_file, "  %s:\n", title);
  fprintf(solver->debug_file, "    Bounds: (%d,%d) to (%d,%d)\n", min_x, min_y, max_x, max_y);

  // Limit output size for debug file
  int max_width = 60;
  int max_height = 30;
  int display_width = (max_x - min_x + 1 > max_width) ? max_width : max_x - min_x + 1;
  int display_height = (max_y - min_y + 1 > max_height) ? max_height : max_y - min_y + 1;

  fprintf(solver->debug_file, "    ASCII Grid (%dx%d):\n", display_width, display_height);

  // Add column numbers for reference
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

  // Display grid with highlighting
  for (int y = min_y; y < min_y + display_height; y++) {
    fprintf(solver->debug_file, "   %2d ", y);
    for (int x = min_x; x < min_x + display_width; x++) {
      char ch = ' '; // Default background character
      int is_highlight = 0;

      // Check if this position is part of the highlighted component (draw it on top)
      if (highlight_comp && x >= highlight_x && x < highlight_x + highlight_comp->width &&
          y >= highlight_y && y < highlight_y + highlight_comp->height) {
        int local_x = x - highlight_x;
        int local_y = y - highlight_y;
        if (highlight_comp->ascii_tile[local_y][local_x] != ' ') {
          ch = highlight_comp->ascii_tile[local_y][local_x];
          is_highlight = 1;
        }
      }

      // If not highlighted, check other placed components
      if (!is_highlight) {
        for (int i = 0; i < solver->component_count; i++) {
          Component *comp = &solver->components[i];
          if (comp->is_placed && x >= comp->placed_x && x < comp->placed_x + comp->width &&
              y >= comp->placed_y && y < comp->placed_y + comp->height) {
            int local_x = x - comp->placed_x;
            int local_y = y - comp->placed_y;
            if (comp->ascii_tile[local_y][local_x] != ' ') {
              ch = comp->ascii_tile[local_y][local_x];
              break;
            }
          }
        }
      }

      // Current room is drawn on top - just output the character
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
  solver->solver_type = SOLVER_RECURSIVE_TREE; // Default to recursive tree solver

  // Initialize backtracking stack
  clear_backtrack_stack(solver);

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
    solver->failed_counts[i] = 0; // Initialize failed position tracking
    for (int j = 0; j < 200; j++) {
      solver->failed_positions[i][j].valid = 0; // Mark all failed positions as invalid initially
    }
  }
}

/**
 * @brief Sets the constraint solver strategy type
 */
void set_solver_type(LayoutSolver* solver, ConstraintSolverType type) {
    if (type < SOLVER_COUNT) {
        solver->solver_type = type;
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
 * and improve overall solver performance. Components with previous placement
 * failures are deprioritized to allow alternative orderings.
 *
 * @param solver The layout solver instance
 * @return       Most constrained unplaced component, or NULL if none found
 */
Component *find_most_constrained_unplaced(LayoutSolver *solver) {
  Component *most_constrained = NULL;
  int max_degree = -1;

  // First pass: try components with no previous placement attempts
  for (int i = 0; i < solver->component_count; i++) {
    Component *comp = &solver->components[i];
    if (!comp->is_placed && solver->placement_attempts[i] == 0) {
      int degree = count_constraint_degree(solver, comp);
      if (degree > max_degree) {
        max_degree = degree;
        most_constrained = comp;
      }
    }
  }

  // If no components with zero attempts, try any unplaced component
  if (!most_constrained) {
    max_degree = -1;
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
  }

  return most_constrained;
}

/**
 * @brief Attempts component placement using wall-aligned sliding algorithm
 *
 * Improved placement algorithm that prioritizes wall alignment and centered placement:
 * 1. First tries wall alignment (left/right walls for N/S, top/bottom walls for E/W)
 * 2. If wall alignment fails, slides from center outward in both directions
 * 3. This reduces diagonal connections by preferring aligned walls and centered placement
 *
 * @param solver      The layout solver instance
 * @param new_comp    Component to be placed
 * @param placed_comp Already placed reference component
 * @param dir         Direction of adjacency constraint ('n', 's', 'e', 'w')
 * @return            1 if placement successful, 0 if failed
 */
int solve_constraints(LayoutSolver *solver) {
  // Dispatch to the appropriate solver based on configuration
  switch (solver->solver_type) {
    case SOLVER_RECURSIVE_TREE:
      printf("🌳 Using recursive search tree with backtracking\n");
      return solve_recursive_tree(solver);
    default:
      printf("❌ Unknown solver type: %d\n", solver->solver_type);
      return 0;
  }
}

// BACKTRACKING IMPLEMENTATION
// =============================================================================

/**
 * @brief Saves the current solver state to the backtracking stack
 *
 * Creates a snapshot of the current solver state including component placements,
 * grid state, and search parameters. This allows restoration if backtracking
 * is needed due to placement conflicts.
 *
 * @param solver            The layout solver instance
 * @param component_index   Current component being placed
 * @param constraint_index  Current constraint being satisfied
 * @param placement_option  Current placement attempt number
 */
void save_solver_state(LayoutSolver* solver, int component_index, int constraint_index, int placement_option) {
    if (solver->backtrack_depth >= MAX_BACKTRACK_DEPTH - 1) {
        if (solver->debug_file) {
            fprintf(solver->debug_file, "⚠️  BACKTRACK STACK OVERFLOW - depth %d\n", solver->backtrack_depth);
        }
        return; // Stack overflow protection
    }

    BacktrackState* state = &solver->backtrack_stack[solver->backtrack_depth];

    // Save component states
    memcpy(state->components, solver->components, sizeof(solver->components));

    // Save grid state
    memcpy(state->grid, solver->grid, sizeof(solver->grid));
    state->grid_width = solver->grid_width;
    state->grid_height = solver->grid_height;
    state->grid_min_x = solver->grid_min_x;
    state->grid_min_y = solver->grid_min_y;

    // Save search parameters
    state->next_group_id = solver->next_group_id;
    state->component_index = component_index;
    state->constraint_index = constraint_index;
    state->placement_option = placement_option;

    solver->backtrack_depth++;

    if (solver->debug_file) {
        fprintf(solver->debug_file, "💾 SAVED STATE: depth=%d, component=%d, constraint=%d, option=%d\n",
                solver->backtrack_depth - 1, component_index, constraint_index, placement_option);
    }
}

/**
 * @brief Restores the solver state from the backtracking stack
 *
 * Pops the most recent state from the backtracking stack and restores
 * the solver to that state. Returns 1 if restoration successful, 0 if
 * the stack is empty.
 *
 * @param solver The layout solver instance
 * @return       1 if state restored, 0 if stack empty
 */
int restore_solver_state(LayoutSolver* solver) {
    if (solver->backtrack_depth <= 0) {
        if (solver->debug_file) {
            fprintf(solver->debug_file, "❌ BACKTRACK STACK EMPTY - cannot restore\n");
        }
        return 0; // Nothing to restore
    }

    solver->backtrack_depth--;
    BacktrackState* state = &solver->backtrack_stack[solver->backtrack_depth];

    // Restore component states
    memcpy(solver->components, state->components, sizeof(solver->components));

    // Restore grid state
    memcpy(solver->grid, state->grid, sizeof(solver->grid));
    solver->grid_width = state->grid_width;
    solver->grid_height = state->grid_height;
    solver->grid_min_x = state->grid_min_x;
    solver->grid_min_y = state->grid_min_y;

    // Restore search parameters
    solver->next_group_id = state->next_group_id;

    if (solver->debug_file) {
        fprintf(solver->debug_file, "🔄 RESTORED STATE: depth=%d, component=%d, constraint=%d, option=%d\n",
                solver->backtrack_depth, state->component_index, state->constraint_index, state->placement_option);
    }

    return 1;
}

/**
 * @brief Clears the backtracking stack
 *
 * Resets the backtracking stack to empty state. Used during solver
 * initialization and cleanup.
 *
 * @param solver The layout solver instance
 */
void clear_backtrack_stack(LayoutSolver* solver) {
    solver->backtrack_depth = 0;
    if (solver->debug_file) {
        fprintf(solver->debug_file, "🗑️  CLEARED BACKTRACK STACK\n");
    }
}

/**
 * @brief Attempts alternative placements for a component using backtracking
 *
 * When a component cannot be placed, this function tries to find alternative
 * placements by backtracking to previous decisions and trying different options.
 *
 * @param solver The layout solver instance
 * @param comp   Component that failed to place
 * @return       1 if alternative placement found, 0 if no more options
 */
int try_alternative_placement(LayoutSolver* solver, Component* comp) {
    if (solver->debug_file) {
        fprintf(solver->debug_file, "🔍 TRYING ALTERNATIVES for %s (backtrack depth: %d)\n",
                comp->name, solver->backtrack_depth);
    }

    // Try to backtrack and find alternative placements
    while (solver->backtrack_depth > 0) {
        BacktrackState* state = &solver->backtrack_stack[solver->backtrack_depth - 1];

        if (solver->debug_file) {
            fprintf(solver->debug_file, "⏪ BACKTRACKING: trying alternative for component %d\n",
                    state->component_index);
        }

        // Restore previous state
        if (!restore_solver_state(solver)) {
            break;
        }

        // TODO: Here we would try alternative constraint/placement combinations
        // For now, we'll just continue backtracking until we find a solution
        // or exhaust all possibilities

        // If we've backtracked to a different component, we can try continuing from there
        if (state->component_index != comp - solver->components) {
            if (solver->debug_file) {
                fprintf(solver->debug_file, "✅ BACKTRACKED to different component - continuing search\n");
            }
            return 1; // Continue search from this point
        }
    }

    if (solver->debug_file) {
        fprintf(solver->debug_file, "❌ NO MORE ALTERNATIVES - backtrack exhausted\n");
    }

    return 0; // No more alternatives
}

// =============================================================================
// RECURSIVE SEARCH TREE BACKTRACKING
// =============================================================================

/**
 * @brief Main entry point for recursive backtracking search
 *
 * Replaces the linear placement approach with a proper search tree that can
 * backtrack to any previous decision point and explore alternative branches.
 *
 * @param solver The layout solver instance
 * @return       1 if solution found, 0 if no solution exists
 */
int solve_recursive_tree(LayoutSolver* solver) {
    if (solver->component_count == 0) {
        return 1; // Nothing to solve
    }

    // Initialize debug file
    init_debug_file(solver);
    printf("🌳 Starting recursive backtracking search...\n");

    // Place first component at origin (this is our root node)
    Component* first_comp = find_most_constrained_unplaced(solver);
    if (!first_comp) {
        close_debug_file(solver);
        return 0;
    }

    printf("📍 Placing root component '%s' at origin (0,0)\n", first_comp->name);

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

    if (solver->debug_file) {
        debug_log_grid_state(solver, "TREE SEARCH ROOT PLACEMENT");
    }

    // Start recursive search from depth 1
    int result = place_component_recursive(solver, 1);

    if (result) {
        printf("🎉 Recursive search found solution!\n");
        // Normalize coordinates
        normalize_grid_coordinates(solver);
        debug_log_grid_state(solver, "TREE SEARCH FINAL SOLUTION");
    } else {
        printf("❌ Recursive search exhausted - no solution found\n");
    }

    close_debug_file(solver);
    return result;
}

/**
 * @brief Recursive function to place components using backtracking
 *
 * This is the core of the search tree. Each call represents a node in the tree,
 * and each placement option represents a branch to explore.
 *
 * @param solver The layout solver instance
 * @param depth  Current search depth (0 = root)
 * @return       1 if solution found from this node, 0 if dead end
 */
int place_component_recursive(LayoutSolver* solver, int depth) {
    // Check for solution: all components placed
    Component* next_comp = find_most_constrained_unplaced(solver);
    if (!next_comp) {
        return 1; // Success - all components placed!
    }

    // Iteration limit safety check
    solver->total_iterations++;
    if (solver->total_iterations > MAX_SOLVER_ITERATIONS) {
        if (solver->debug_file) {
            fprintf(solver->debug_file, "🛑 TREE SEARCH ITERATION LIMIT REACHED at depth %d\n", depth);
        }
        return 0;
    }

    if (solver->debug_file) {
        fprintf(solver->debug_file, "🌿 TREE NODE depth=%d: trying to place '%s'\n",
                depth, next_comp->name);
    }

    // Try all placement options for this component
    return try_placement_options(solver, next_comp, depth);
}

/**
 * @brief Tries all placement options for a component (all branches from this node)
 *
 * Each placement option represents a branch in the search tree. We try each
 * branch, and if it leads to a dead end, we backtrack and try the next branch.
 *
 * @param solver The layout solver instance
 * @param comp   Component to place at this node
 * @param depth  Current search depth
 * @return       1 if any branch leads to solution, 0 if all branches fail
 */
int try_placement_options(LayoutSolver* solver, Component* comp, int depth) {
    // Get all possible placement options for this component
    PlacementOption options[MAX_CONSTRAINTS * 10]; // Allow multiple positions per constraint
    int option_count = 0;

    get_placement_options(solver, comp, options, &option_count);

    if (solver->debug_file) {
        fprintf(solver->debug_file, "🔀 Found %d placement options for '%s'\n",
                option_count, comp->name);
    }

    // Try each placement option (branch)
    for (int i = 0; i < option_count; i++) {
        if (solver->debug_file) {
            fprintf(solver->debug_file, "🌱 BRANCH %d/%d: trying placement at (%d,%d)\n",
                    i + 1, option_count, options[i].x, options[i].y);
        }

        // Save current state before trying this branch
        save_solver_state(solver, comp - solver->components, i, i);

        // Log detailed placement attempt
        debug_log_placement_attempt(solver, comp, options[i].other_comp, options[i].dir,
                                   options[i].x, options[i].y, 0);

        // Try this placement
        expand_grid_for_component(solver, comp, options[i].x, options[i].y);

        if (!has_overlap(solver, comp, options[i].x, options[i].y)) {
            // Check constraint satisfaction
            int satisfies = 0;
            if ((options[i].dir == 'n' || options[i].dir == 's') &&
                has_horizontal_overlap(options[i].x, comp->width,
                                     options[i].other_comp->placed_x,
                                     options[i].other_comp->width)) {
                satisfies = 1;
            } else if ((options[i].dir == 'e' || options[i].dir == 'w') &&
                       has_vertical_overlap(options[i].y, comp->height,
                                          options[i].other_comp->placed_y,
                                          options[i].other_comp->height)) {
                satisfies = 1;
            }

            if (satisfies) {
                // Valid placement - place component and update grid
                comp->placed_x = options[i].x;
                comp->placed_y = options[i].y;
                comp->is_placed = 1;
                comp->group_id = options[i].other_comp->group_id;

                // Log successful placement
                debug_log_placement_attempt(solver, comp, options[i].other_comp, options[i].dir,
                                           options[i].x, options[i].y, 1);

                // Update grid
                for (int row = 0; row < comp->height; row++) {
                    for (int col = 0; col < comp->width; col++) {
                        if (comp->ascii_tile[row][col] != ' ') {
                            int grid_x = options[i].x + col - solver->grid_min_x;
                            int grid_y = options[i].y + row - solver->grid_min_y;
                            if (grid_x >= 0 && grid_x < solver->grid_width &&
                                grid_y >= 0 && grid_y < solver->grid_height) {
                                solver->grid[grid_y][grid_x] = comp->ascii_tile[row][col];
                            }
                        }
                    }
                }

                if (solver->debug_file) {
                    fprintf(solver->debug_file, "✅ PLACED '%s' at (%d,%d) - exploring subtree\n",
                            comp->name, options[i].x, options[i].y);
                }

                // Log grid state after placement
                debug_log_ascii_grid(solver, "After placement");

                // Recursively try to place remaining components
                if (place_component_recursive(solver, depth + 1)) {
                    return 1; // Solution found in this subtree!
                }

                // This branch didn't lead to solution - backtrack
                if (solver->debug_file) {
                    fprintf(solver->debug_file, "🔙 BACKTRACK: branch failed, trying next option\n");
                }
            } else {
                // Constraint not satisfied
                if (solver->debug_file) {
                    fprintf(solver->debug_file, "❌ FAILED: constraint not satisfied (%c direction)\n",
                            options[i].dir);
                }
            }
        } else {
            // Overlap detected
            if (solver->debug_file) {
                fprintf(solver->debug_file, "❌ FAILED: overlap detected at (%d,%d)\n",
                        options[i].x, options[i].y);
            }
        }

        // Record this failed position for future pruning
        int comp_index = comp - solver->components;
        record_failed_position(solver, comp_index, options[i].x, options[i].y);

        // Restore state and try next option
        restore_solver_state(solver);
    }

    // All branches from this node failed
    if (solver->debug_file) {
        fprintf(solver->debug_file, "💀 DEAD END: all %d branches failed for '%s'\n",
                option_count, comp->name);
    }
    return 0;
}

/**
 * @brief Generates all possible placement options for a component
 *
 * This expands the search space by considering multiple sliding positions
 * for each constraint, rather than just one placement per constraint.
 *
 * @param solver       The layout solver instance
 * @param comp         Component to generate options for
 * @param options      Output array for placement options
 * @param option_count Output count of placement options
 */
void get_placement_options(LayoutSolver* solver, Component* comp, PlacementOption* options, int* option_count) {
    *option_count = 0;

    for (int i = 0; i < solver->constraint_count; i++) {
        DSLConstraint* constraint = &solver->constraints[i];
        Component* other_comp = NULL;
        Direction dir;

        if (strcmp(constraint->component_a, comp->name) == 0) {
            other_comp = find_component(solver, constraint->component_b);
            // Reverse direction for component_a
            switch (constraint->direction) {
            case 'n': dir = 's'; break;
            case 's': dir = 'n'; break;
            case 'e': dir = 'w'; break;
            case 'w': dir = 'e'; break;
            default: dir = constraint->direction; break;
            }
        } else if (strcmp(constraint->component_b, comp->name) == 0) {
            other_comp = find_component(solver, constraint->component_a);
            dir = constraint->direction; // Use direction as-is for component_b
        }

        if (other_comp && other_comp->is_placed && *option_count < MAX_CONSTRAINTS * 10) {
            // Generate multiple sliding positions for this constraint
            int base_x, base_y, dx, dy, slide_min, slide_max;

            switch (dir) {
            case 'n':
                base_x = other_comp->placed_x;
                base_y = other_comp->placed_y - comp->height;
                dx = 1; dy = 0;
                slide_min = -other_comp->width;
                slide_max = other_comp->width;
                break;
            case 's':
                base_x = other_comp->placed_x;
                base_y = other_comp->placed_y + other_comp->height;
                dx = 1; dy = 0;
                slide_min = -other_comp->width;
                slide_max = other_comp->width;
                break;
            case 'e':
                base_x = other_comp->placed_x + other_comp->width;
                base_y = other_comp->placed_y;
                dx = 0; dy = 1;
                slide_min = -other_comp->height;
                slide_max = other_comp->height;
                break;
            case 'w':
                base_x = other_comp->placed_x - comp->width;
                base_y = other_comp->placed_y;
                dx = 0; dy = 1;
                slide_min = -other_comp->height;
                slide_max = other_comp->height;
                break;
            default:
                continue;
            }

            // Use wall alignment logic with random preference
            int prefer_left_or_top = rand() % 2;
            int positions_added = 0;

            // Phase 1: Try wall alignment positions first (ensuring adjacency)
            if ((dir == 'n' || dir == 's') && *option_count < MAX_CONSTRAINTS * 10) {
                // For north/south: try left edge, right edge alignment with proper adjacency
                int align_left = other_comp->placed_x;
                int align_right = other_comp->placed_x + other_comp->width - comp->width;

                // Ensure alignment positions are within valid sliding range for adjacency
                if (prefer_left_or_top) {
                    // Try left alignment first
                    if (align_left >= base_x + slide_min && align_left <= base_x + slide_max) {
                        int comp_index = comp - solver->components;
                        if (!is_position_failed(solver, comp_index, align_left, base_y)) {
                            options[*option_count].constraint = constraint;
                            options[*option_count].other_comp = other_comp;
                            options[*option_count].dir = dir;
                            options[*option_count].x = align_left;
                            options[*option_count].y = base_y;
                            (*option_count)++;
                            positions_added++;
                        }
                    }
                    if (align_right >= base_x + slide_min && align_right <= base_x + slide_max) {
                        options[*option_count].constraint = constraint;
                        options[*option_count].other_comp = other_comp;
                        options[*option_count].dir = dir;
                        options[*option_count].x = align_right;
                        options[*option_count].y = base_y;
                        (*option_count)++;
                        positions_added++;
                    }
                } else {
                    // Try right alignment first
                    if (align_right >= base_x + slide_min && align_right <= base_x + slide_max) {
                        options[*option_count].constraint = constraint;
                        options[*option_count].other_comp = other_comp;
                        options[*option_count].dir = dir;
                        options[*option_count].x = align_right;
                        options[*option_count].y = base_y;
                        (*option_count)++;
                        positions_added++;
                    }
                    if (align_left >= base_x + slide_min && align_left <= base_x + slide_max) {
                        options[*option_count].constraint = constraint;
                        options[*option_count].other_comp = other_comp;
                        options[*option_count].dir = dir;
                        options[*option_count].x = align_left;
                        options[*option_count].y = base_y;
                        (*option_count)++;
                        positions_added++;
                    }
                }
            } else if ((dir == 'e' || dir == 'w') && *option_count < MAX_CONSTRAINTS * 10) {
                // For east/west: try top edge, bottom edge alignment with proper adjacency
                int align_top = other_comp->placed_y;
                int align_bottom = other_comp->placed_y + other_comp->height - comp->height;

                // Ensure alignment positions are within valid sliding range for adjacency
                if (prefer_left_or_top) {
                    // Try top alignment first
                    if (align_top >= base_y + slide_min && align_top <= base_y + slide_max) {
                        options[*option_count].constraint = constraint;
                        options[*option_count].other_comp = other_comp;
                        options[*option_count].dir = dir;
                        options[*option_count].x = base_x;
                        options[*option_count].y = align_top;
                        (*option_count)++;
                        positions_added++;
                    }
                    if (align_bottom >= base_y + slide_min && align_bottom <= base_y + slide_max) {
                        options[*option_count].constraint = constraint;
                        options[*option_count].other_comp = other_comp;
                        options[*option_count].dir = dir;
                        options[*option_count].x = base_x;
                        options[*option_count].y = align_bottom;
                        (*option_count)++;
                        positions_added++;
                    }
                } else {
                    // Try bottom alignment first
                    if (align_bottom >= base_y + slide_min && align_bottom <= base_y + slide_max) {
                        options[*option_count].constraint = constraint;
                        options[*option_count].other_comp = other_comp;
                        options[*option_count].dir = dir;
                        options[*option_count].x = base_x;
                        options[*option_count].y = align_bottom;
                        (*option_count)++;
                        positions_added++;
                    }
                    if (align_top >= base_y + slide_min && align_top <= base_y + slide_max) {
                        options[*option_count].constraint = constraint;
                        options[*option_count].other_comp = other_comp;
                        options[*option_count].dir = dir;
                        options[*option_count].x = base_x;
                        options[*option_count].y = align_top;
                        (*option_count)++;
                        positions_added++;
                    }
                }
            }

            // Phase 2: Add smart sliding positions based on overlap requirements
            // Calculate the range of slides that would create ANY overlap
            int min_overlap_slide, max_overlap_slide;

            if (dir == 'n' || dir == 's') {
                // For north/south: need horizontal overlap
                // Component at slide_x must overlap with other_comp at other_comp->placed_x
                // Overlap exists when: slide_x < other_comp->placed_x + other_comp->width AND
                //                     slide_x + comp->width > other_comp->placed_x
                min_overlap_slide = other_comp->placed_x - comp->width + 1 - base_x;
                max_overlap_slide = other_comp->placed_x + other_comp->width - 1 - base_x;
            } else { // 'e' or 'w'
                // For east/west: need vertical overlap
                min_overlap_slide = other_comp->placed_y - comp->height + 1 - base_y;
                max_overlap_slide = other_comp->placed_y + other_comp->height - 1 - base_y;
            }

            // Clamp to reasonable bounds to avoid excessive positions
            if (min_overlap_slide < slide_min) min_overlap_slide = slide_min;
            if (max_overlap_slide > slide_max) max_overlap_slide = slide_max;

            // Create sorted list of positions by overlap amount (highest first)
            typedef struct {
                int slide_value;
                int overlap_amount;
                int x, y;
            } OverlapPosition;

            OverlapPosition overlap_positions[100]; // Reasonable limit
            int overlap_count = 0;

            for (int slide = min_overlap_slide; slide <= max_overlap_slide && overlap_count < 100; slide++) {
                int slide_x = base_x + slide * dx;
                int slide_y = base_y + slide * dy;

                // Calculate overlap amount for this position
                int overlap = 0;
                if (dir == 'n' || dir == 's') {
                    // Horizontal overlap amount
                    int overlap_start = (slide_x > other_comp->placed_x) ? slide_x : other_comp->placed_x;
                    int overlap_end = ((slide_x + comp->width) < (other_comp->placed_x + other_comp->width))
                                        ? (slide_x + comp->width) : (other_comp->placed_x + other_comp->width);
                    overlap = overlap_end - overlap_start;
                } else {
                    // Vertical overlap amount
                    int overlap_start = (slide_y > other_comp->placed_y) ? slide_y : other_comp->placed_y;
                    int overlap_end = ((slide_y + comp->height) < (other_comp->placed_y + other_comp->height))
                                        ? (slide_y + comp->height) : (other_comp->placed_y + other_comp->height);
                    overlap = overlap_end - overlap_start;
                }

                if (overlap > 0) {
                    overlap_positions[overlap_count].slide_value = slide;
                    overlap_positions[overlap_count].overlap_amount = overlap;
                    overlap_positions[overlap_count].x = slide_x;
                    overlap_positions[overlap_count].y = slide_y;
                    overlap_count++;
                }
            }

            // Sort by overlap amount (highest first)
            for (int i = 0; i < overlap_count - 1; i++) {
                for (int j = i + 1; j < overlap_count; j++) {
                    if (overlap_positions[j].overlap_amount > overlap_positions[i].overlap_amount) {
                        OverlapPosition temp = overlap_positions[i];
                        overlap_positions[i] = overlap_positions[j];
                        overlap_positions[j] = temp;
                    }
                }
            }

            // Add sorted positions, avoiding duplicates
            for (int i = 0; i < overlap_count && *option_count < MAX_CONSTRAINTS * 10; i++) {
                // Skip if this position was already added as wall alignment
                int already_added = 0;
                for (int check = *option_count - positions_added; check < *option_count; check++) {
                    if (options[check].x == overlap_positions[i].x && options[check].y == overlap_positions[i].y) {
                        already_added = 1;
                        break;
                    }
                }

                if (!already_added) {
                    // Check if this position has failed before for this component
                    int comp_index = comp - solver->components;
                    if (!is_position_failed(solver, comp_index, overlap_positions[i].x, overlap_positions[i].y)) {
                        options[*option_count].constraint = constraint;
                        options[*option_count].other_comp = other_comp;
                        options[*option_count].dir = dir;
                        options[*option_count].x = overlap_positions[i].x;
                        options[*option_count].y = overlap_positions[i].y;
                        (*option_count)++;
                    } else if (solver->debug_file) {
                        fprintf(solver->debug_file, "🚫 SKIPPED FAILED: (%d,%d) previously failed for '%s'\n",
                                overlap_positions[i].x, overlap_positions[i].y, comp->name);
                    }
                }
            }
        }
    }
}

// =============================================================================
// FAILED POSITION TRACKING FUNCTIONS
// =============================================================================

/**
 * @brief Records a failed placement position for a component to avoid retrying
 */
void record_failed_position(LayoutSolver* solver, int component_index, int x, int y) {
    if (component_index < 0 || component_index >= MAX_COMPONENTS) return;

    int count = solver->failed_counts[component_index];
    if (count >= 200) return; // Avoid overflow

    // Check if position already recorded
    for (int i = 0; i < count; i++) {
        if (solver->failed_positions[component_index][i].valid &&
            solver->failed_positions[component_index][i].x == x &&
            solver->failed_positions[component_index][i].y == y) {
            return; // Already recorded
        }
    }

    // Add new failed position
    solver->failed_positions[component_index][count].x = x;
    solver->failed_positions[component_index][count].y = y;
    solver->failed_positions[component_index][count].valid = 1;
    solver->failed_counts[component_index]++;

    if (solver->debug_file) {
        fprintf(solver->debug_file, "📝 RECORDED FAILURE: component %d at (%d,%d) - total failures: %d\n",
                component_index, x, y, solver->failed_counts[component_index]);
    }
}

/**
 * @brief Checks if a position has previously failed for this component
 */
int is_position_failed(LayoutSolver* solver, int component_index, int x, int y) {
    if (component_index < 0 || component_index >= MAX_COMPONENTS) return 0;

    int count = solver->failed_counts[component_index];
    for (int i = 0; i < count; i++) {
        if (solver->failed_positions[component_index][i].valid &&
            solver->failed_positions[component_index][i].x == x &&
            solver->failed_positions[component_index][i].y == y) {
            return 1; // Position has failed before
        }
    }
    return 0; // Position not tried or succeeded before
}

/**
 * @brief Clears failed positions for a component (when backtracking to earlier state)
 */
void clear_failed_positions(LayoutSolver* solver, int component_index) {
    if (component_index < 0 || component_index >= MAX_COMPONENTS) return;

    solver->failed_counts[component_index] = 0;

    if (solver->debug_file) {
        fprintf(solver->debug_file, "🧹 CLEARED FAILURES: component %d failure history reset\n", component_index);
    }
}

// =============================================================================
// GRID NORMALIZATION AND DISPLAY FUNCTIONS
// =============================================================================

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
