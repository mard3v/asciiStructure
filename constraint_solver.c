#include "constraint_solver.h"
#include "constraints.h"
#include "tree_debug.h"
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
 * Parses constraint string in DSL format and adds it to the solver's constraint
 * list. Currently supports ADJACENT constraints with directional parameters.
 *
 * @param solver         The layout solver instance
 * @param constraint_line DSL constraint string (e.g., "ADJACENT(Gatehouse,
 * Courtyard, n)")
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
 * Linear search through all registered components to find one matching the
 * given name.
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
 * @brief Check if a component placement is valid at given coordinates
 *
 * Validates placement by checking:
 * - Grid bounds (components must fit within grid)
 * - No character overlap with existing placed components
 * - Basic spatial constraints
 *
 * @param solver The layout solver instance
 * @param comp   The component to place
 * @param x      X coordinate for placement
 * @param y      Y coordinate for placement
 * @return       1 if placement is valid, 0 if invalid
 */
int is_placement_valid(LayoutSolver *solver, Component *comp, int x, int y) {
  if (!comp)
    return 0;

  // Expand grid if needed
  expand_grid_for_component(solver, comp, x, y);

  // Check for overlap with other placed components
  for (int i = 0; i < solver->component_count; i++) {
    Component *other = &solver->components[i];
    if (other == comp || !other->is_placed)
      continue;

    if (has_character_overlap(solver, comp, x, y, other, other->placed_x,
                              other->placed_y)) {
      return 0; // Overlap detected
    }
  }

  return 1; // Placement is valid
}

/**
 * @brief Place a component at specified coordinates
 *
 * Places the component on the grid, updating:
 * - Component placement status and coordinates
 * - Grid character data
 * - Grid bounds if necessary
 *
 * @param solver The layout solver instance
 * @param comp   The component to place
 * @param x      X coordinate for placement
 * @param y      Y coordinate for placement
 */
void place_component(LayoutSolver *solver, Component *comp, int x, int y) {
  if (!comp)
    return;

  // Expand grid if needed
  expand_grid_for_component(solver, comp, x, y);

  // Update component state
  comp->is_placed = 1;
  comp->placed_x = x;
  comp->placed_y = y;

  // Place component tiles on grid
  for (int dy = 0; dy < comp->height; dy++) {
    for (int dx = 0; dx < comp->width; dx++) {
      int grid_x = x + dx;
      int grid_y = y + dy;

      if (grid_x >= 0 && grid_x < MAX_GRID_SIZE && grid_y >= 0 &&
          grid_y < MAX_GRID_SIZE) {
        char tile_char = comp->ascii_tile[dy][dx];
        if (tile_char != ' ') { // Only place non-space characters
          solver->grid[grid_y][grid_x] = tile_char;
        }
      }
    }
  }

  printf("  ✅ Placed %s at (%d,%d)\n", comp->name, x, y);
}

/**
 * @brief Remove a component from the grid
 *
 * Removes component from grid, updating:
 * - Component placement status
 * - Grid character data (restores to spaces)
 * - Does NOT update grid bounds (leaves them expanded)
 *
 * @param solver The layout solver instance
 * @param comp   The component to remove
 */
void remove_component(LayoutSolver *solver, Component *comp) {
  if (!comp || !comp->is_placed)
    return;

  // Remove component tiles from grid (restore to spaces)
  for (int dy = 0; dy < comp->height; dy++) {
    for (int dx = 0; dx < comp->width; dx++) {
      int grid_x = comp->placed_x + dx;
      int grid_y = comp->placed_y + dy;

      if (grid_x >= 0 && grid_x < MAX_GRID_SIZE && grid_y >= 0 &&
          grid_y < MAX_GRID_SIZE) {
        char tile_char = comp->ascii_tile[dy][dx];
        if (tile_char != ' ') { // Only remove non-space characters
          solver->grid[grid_y][grid_x] = ' ';
        }
      }
    }
  }

  // Update component state
  comp->is_placed = 0;
  comp->placed_x = -1;
  comp->placed_y = -1;

  printf("  🗑️  Removed %s from grid\n", comp->name);
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

void debug_log_ascii_grid_REMOVED(LayoutSolver *solver, const char *title) {
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
void debug_log_placement_grid(LayoutSolver *solver, const char *title,
                              Component *highlight_comp, int highlight_x,
                              int highlight_y) {
  if (!solver->debug_file)
    return;

  // Find actual bounds including the highlight component at its attempted
  // position
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

  // Include the highlight component bounds
  if (highlight_comp) {
    if (first) {
      min_x = highlight_x;
      max_x = highlight_x + highlight_comp->width - 1;
      min_y = highlight_y;
      max_y = highlight_y + highlight_comp->height - 1;
      first = 0;
    } else {
      if (highlight_x < min_x)
        min_x = highlight_x;
      if (highlight_x + highlight_comp->width - 1 > max_x)
        max_x = highlight_x + highlight_comp->width - 1;
      if (highlight_y < min_y)
        min_y = highlight_y;
      if (highlight_y + highlight_comp->height - 1 > max_y)
        max_y = highlight_y + highlight_comp->height - 1;
    }
  }

  if (first) {
    fprintf(solver->debug_file, "  %s: (no components to display)\n", title);
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

      // Check if this position is part of the highlighted component (draw it on
      // top)
      if (highlight_comp && x >= highlight_x &&
          x < highlight_x + highlight_comp->width && y >= highlight_y &&
          y < highlight_y + highlight_comp->height) {
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
  // Only tree-based constraint solver is used



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
    solver->failed_counts[i] = 0;       // Initialize failed position tracking
    for (int j = 0; j < 200; j++) {
      solver->failed_positions[i][j].valid =
          0; // Mark all failed positions as invalid initially
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
 * @brief Checks if two components would overlap at specified positions
 *
 * Tests if placing comp1 at (x1, y1) would overlap with comp2 at (x2, y2).
 * Only non-space characters count as occupied space.
 *
 * @param solver The layout solver instance
 * @param comp1  First component
 * @param x1     First component x position
 * @param y1     First component y position
 * @param comp2  Second component
 * @param x2     Second component x position
 * @param y2     Second component y position
 * @return       1 if overlap detected, 0 if no overlap
 */

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
 * Improved placement algorithm that prioritizes wall alignment and centered
 * placement:
 * 1. First tries wall alignment (left/right walls for N/S, top/bottom walls for
 * E/W)
 * 2. If wall alignment fails, slides from center outward in both directions
 * 3. This reduces diagonal connections by preferring aligned walls and centered
 * placement
 *
 * @param solver      The layout solver instance
 * @param new_comp    Component to be placed
 * @param placed_comp Already placed reference component
 * @param dir         Direction of adjacency constraint ('n', 's', 'e', 'w')
 * @return            1 if placement successful, 0 if failed
 */
int solve_constraints(LayoutSolver *solver) {
  // Directly use tree-based constraint solver
  printf("🌲 Using tree-based constraint resolution with conflict-depth "
         "backtracking\n");
  return solve_tree_constraint(solver);
}

// BACKTRACKING IMPLEMENTATION
// =============================================================================

/**
 * @brief Saves the current solver state to the backtracking stack
 *
 * Creates a snapshot of the current solver state including component
 * placements, grid state, and search parameters. This allows restoration if
 * backtracking is needed due to placement conflicts.
 *
 * @param solver            The layout solver instance
 * @param component_index   Current component being placed
 * @param constraint_index  Current constraint being satisfied
 * @param placement_option  Current placement attempt number
 */


/**
 * @brief Attempts alternative placements for a component using backtracking
 *
 * When a component cannot be placed, this function tries to find alternative
 * placements by backtracking to previous decisions and trying different
 * options.
 *
 * @param solver The layout solver instance
 * @param comp   Component that failed to place
 * @return       1 if alternative placement found, 0 if no more options
 */
// Legacy function removed - try_alternative_placement no longer used by tree
// solver

/**
 * @brief Gets next unplaced component using intelligent placement order
 *
 * @param solver The layout solver instance
 * @return       Next component to place, or NULL if all placed
 */
// Legacy function stub - no longer used
// Legacy function removed - get_next_component_intelligent no longer used by
// tree solver

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

/**
 * @brief Tries all placement options for a component (all branches from this
 * node)
 *
 * Each placement option represents a branch in the search tree. We try each
 * branch, and if it leads to a dead end, we backtrack and try the next branch.
 *
 * @param solver The layout solver instance
 * @param comp   Component to place at this node
 * @param depth  Current search depth
 * @return       1 if any branch leads to solution, 0 if all branches fail
 */

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

// =============================================================================
// INTELLIGENT CONFLICT RESOLUTION FUNCTIONS
// =============================================================================

/**
 * @brief Analyzes constraint dependencies between components
 *
 * Builds a dependency graph showing which components are connected to others
 * through constraints. This graph is used to determine mobility and placement
 * priority.
 *
 * @param solver The layout solver instance
 */
void analyze_constraint_dependencies(LayoutSolver *solver) {
  // Initialize dependency graph
  for (int i = 0; i < MAX_COMPONENTS; i++) {
    for (int j = 0; j < MAX_COMPONENTS; j++) {
      solver->dependency_graph[i][j] = 0;
    }
  }

  // Build adjacency matrix from constraints
  for (int i = 0; i < solver->constraint_count; i++) {
    DSLConstraint *constraint = &solver->constraints[i];
    Component *comp1 = find_component(solver, constraint->component_a);
    Component *comp2 = find_component(solver, constraint->component_b);

    if (comp1 && comp2) {
      int idx1 = comp1 - solver->components;
      int idx2 = comp2 - solver->components;

      // Mark bidirectional dependency
      solver->dependency_graph[idx1][idx2] = 1;
      solver->dependency_graph[idx2][idx1] = 1;
    }
  }

  if (solver->debug_file) {
    fprintf(solver->debug_file, "🔗 DEPENDENCY ANALYSIS:\n");
    for (int i = 0; i < solver->component_count; i++) {
      int connections = 0;
      for (int j = 0; j < solver->component_count; j++) {
        if (solver->dependency_graph[i][j])
          connections++;
      }
      fprintf(solver->debug_file, "  %s: %d connections\n",
              solver->components[i].name, connections);
    }
  }
}

/**
 * @brief Calculates mobility scores for all components
 *
 * Components with fewer constraints are more mobile (easier to move).
 * Mobility score = number of constraints involving this component.
 * Lower score = more mobile.
 *
 * @param solver The layout solver instance
 */
void calculate_mobility_scores(LayoutSolver *solver) {
  for (int i = 0; i < solver->component_count; i++) {
    Component *comp = &solver->components[i];
    comp->constraint_count = 0;
    comp->mobility_score = 0;

    // Count constraints involving this component
    for (int j = 0; j < solver->constraint_count; j++) {
      DSLConstraint *constraint = &solver->constraints[j];
      if (strcmp(constraint->component_a, comp->name) == 0 ||
          strcmp(constraint->component_b, comp->name) == 0) {
        comp->constraint_count++;
      }
    }

    // Calculate mobility score (lower = more mobile)
    comp->mobility_score = comp->constraint_count;

    // Add penalty for components that are "hubs" (connected to many others)
    int connection_count = 0;
    for (int j = 0; j < solver->component_count; j++) {
      if (solver->dependency_graph[i][j])
        connection_count++;
    }
    comp->mobility_score += connection_count;
  }

  if (solver->debug_file) {
    fprintf(solver->debug_file, "📊 MOBILITY ANALYSIS:\n");
    for (int i = 0; i < solver->component_count; i++) {
      Component *comp = &solver->components[i];
      fprintf(solver->debug_file, "  %s: mobility_score=%d, constraints=%d\n",
              comp->name, comp->mobility_score, comp->constraint_count);
    }
  }
}
/**
 * @brief Determines optimal placement order based on constraint complexity
 *
 * Places highly constrained components first (they have fewer valid positions),
 * and lightly constrained components last (they can adapt around existing
 * placements). This puts "easily movable" components near the leaves of the
 * search tree.
 *
 * @param solver The layout solver instance
 */
void determine_placement_order(LayoutSolver *solver) {
  // Create array of component indices sorted by mobility score (ascending)
  for (int i = 0; i < solver->component_count; i++) {
    solver->placement_order[i] = i;
  }

  // Sort by mobility score (higher scores placed first = most constrained
  // first)
  for (int i = 0; i < solver->component_count - 1; i++) {
    for (int j = i + 1; j < solver->component_count; j++) {
      Component *comp_i = &solver->components[solver->placement_order[i]];
      Component *comp_j = &solver->components[solver->placement_order[j]];

      if (comp_i->mobility_score < comp_j->mobility_score) {
        // Swap - put higher mobility score first
        int temp = solver->placement_order[i];
        solver->placement_order[i] = solver->placement_order[j];
        solver->placement_order[j] = temp;
      }
    }
  }

  if (solver->debug_file) {
    fprintf(solver->debug_file,
            "🎯 PLACEMENT ORDER (most constrained first):\n");
    for (int i = 0; i < solver->component_count; i++) {
      int comp_idx = solver->placement_order[i];
      Component *comp = &solver->components[comp_idx];
      fprintf(solver->debug_file, "  %d. %s (mobility_score=%d)\n", i + 1,
              comp->name, comp->mobility_score);
    }
  }
}

/**
 * @brief Detects which components are overlapping with a proposed placement
 */
int detect_placement_conflicts(LayoutSolver *solver, Component *target_comp,
                               int x, int y) {
  solver->conflict_state.overlap_count = 0;
  solver->conflict_state.target_component = target_comp - solver->components;
  solver->conflict_state.conflict_resolved = 0;

  // Check each placed component for overlap
  for (int i = 0; i < solver->component_count; i++) {
    Component *existing_comp = &solver->components[i];

    if (!existing_comp->is_placed || existing_comp == target_comp) {
      continue;
    }

    // Check for rectangular overlap
    int overlap_x = has_horizontal_overlap(
        x, target_comp->width, existing_comp->placed_x, existing_comp->width);
    int overlap_y = has_vertical_overlap(
        y, target_comp->height, existing_comp->placed_y, existing_comp->height);

    if (overlap_x && overlap_y) {
      // Check for actual character overlap
      if (has_character_overlap(solver, target_comp, x, y, existing_comp,
                                existing_comp->placed_x,
                                existing_comp->placed_y)) {
        solver->conflict_state
            .overlapping_components[solver->conflict_state.overlap_count] = i;
        solver->conflict_state.overlap_count++;

        if (solver->debug_file) {
          fprintf(solver->debug_file,
                  "⚠️  CONFLICT DETECTED: %s at (%d,%d) overlaps with %s at "
                  "(%d,%d)\n",
                  target_comp->name, x, y, existing_comp->name,
                  existing_comp->placed_x, existing_comp->placed_y);
        }
      }
    }
  }

  return solver->conflict_state.overlap_count;
}

// =============================================================================
// SLIDING PUZZLE CONFLICT RESOLUTION IMPLEMENTATION
// =============================================================================


// =============================================================================
// TREE-BASED CONSTRAINT SOLVER IMPLEMENTATION
// =============================================================================

/**
 * @brief Main entry point for tree-based constraint resolution
 *
 * Implements the tree-based approach where we:
 * 1. Place the most constrained component at root
 * 2. Generate all placement options for each constraint in order
 * 3. Order options by conflict status then preference score
 * 4. Use conflict-depth-based intelligent backtracking
 */
int solve_tree_constraint(LayoutSolver *solver) {
  printf("🌲 Starting tree-based constraint resolution\n");

  // Initialize debug logging
  init_tree_debug_file(solver);

  // Initialize the tree solver
  init_tree_solver(solver);

  // Step 1: Place the most constrained component (root)
  Component *root_comp = find_most_constrained_unplaced(solver);
  if (!root_comp) {
    printf("❌ No components to place\n");
    cleanup_tree_solver(solver);
    return 0;
  }

  printf("📍 Root component: %s\n", root_comp->name);

  // Place root component at origin (expand grid as needed)
  int root_x = 50, root_y = 50; // Center of grid
  place_component(solver, root_comp, root_x, root_y);

  // Create root node
  int root_comp_index = root_comp - solver->components;
  solver->tree_solver.root =
      create_tree_node(root_comp, NULL, root_x, root_y, 0, root_comp_index);
  solver->tree_solver.current_node = solver->tree_solver.root;

  // Log initial grid state
  debug_log_enhanced_grid_state(solver, "ROOT PLACEMENT");

  // Step 2: Process constraints in order
  int result = advance_to_next_constraint(solver);

  // Log final results
  if (result) {
    debug_log_tree_solution_path(solver);
    debug_log_enhanced_grid_state(solver, "FINAL SOLUTION");
  }

  cleanup_tree_solver(solver);
  close_tree_debug_file(solver);
  return result;
}

/**
 * @brief Initialize the tree solver state
 */
void init_tree_solver(LayoutSolver *solver) {
  TreeSolver *ts = &solver->tree_solver;
  memset(ts, 0, sizeof(TreeSolver));

  // Copy all constraints to remaining list
  for (int i = 0; i < solver->constraint_count; i++) {
    ts->remaining_constraints[i] = &solver->constraints[i];
  }
  ts->remaining_count = solver->constraint_count;
  ts->current_constraint = NULL;
}

/**
 * @brief Clean up tree solver resources
 */
void cleanup_tree_solver(LayoutSolver *solver) {
  TreeSolver *ts = &solver->tree_solver;

  if (ts->root) {
    free_tree_node(ts->root);
    ts->root = NULL;
  }

  printf(
      "📊 Tree solver stats: %d nodes, %d backtracks, %d conflict backtracks\n",
      ts->nodes_created, ts->backtracks_performed, ts->conflict_backtracks);
}

/**
 * @brief Create a new tree node
 */
TreeNode *create_tree_node(Component *comp, DSLConstraint *constraint, int x,
                           int y, int depth, int comp_index) {
  TreeNode *node = malloc(sizeof(TreeNode));
  if (!node)
    return NULL;

  memset(node, 0, sizeof(TreeNode));
  node->component = comp;
  node->constraint = constraint;
  node->x = x;
  node->y = y;
  node->depth = depth;
  node->component_index = comp_index;

  return node;
}

/**
 * @brief Free a tree node and all its children
 */
void free_tree_node(TreeNode *node) {
  if (!node)
    return;

  for (int i = 0; i < node->child_count; i++) {
    free_tree_node(node->children[i]);
  }
  free(node);
}

/**
 * @brief Advance to the next constraint and generate placement options
 */
int advance_to_next_constraint(LayoutSolver *solver) {
  TreeSolver *ts = &solver->tree_solver;

  // Find next constraint involving already placed components
  DSLConstraint *next_constraint = get_next_constraint_involving_placed(solver);
  if (!next_constraint) {
    printf("✅ All constraints resolved successfully\n");
    return 1; // Success - all constraints satisfied
  }

  ts->current_constraint = next_constraint;
  printf("🎯 Processing constraint: %s %s %s %c\n",
         next_constraint->component_a, "ADJACENT", next_constraint->component_b,
         next_constraint->direction);

  // Determine which component needs to be placed
  Component *comp_a = find_component(solver, next_constraint->component_a);
  Component *comp_b = find_component(solver, next_constraint->component_b);

  Component *unplaced_comp = NULL;
  if (comp_a && !comp_a->is_placed) {
    unplaced_comp = comp_a;
  } else if (comp_b && !comp_b->is_placed) {
    unplaced_comp = comp_b;
  }

  if (!unplaced_comp) {
    // Both components already placed, just validate constraint
    if (check_constraint_satisfied(solver, next_constraint, comp_a, comp_b,
                                   comp_a->placed_x, comp_a->placed_y)) {
      // Remove this constraint and continue
      for (int i = 0; i < ts->remaining_count; i++) {
        if (ts->remaining_constraints[i] == next_constraint) {
          // Shift remaining constraints
          for (int j = i; j < ts->remaining_count - 1; j++) {
            ts->remaining_constraints[j] = ts->remaining_constraints[j + 1];
          }
          ts->remaining_count--;
          break;
        }
      }
      return advance_to_next_constraint(solver);
    } else {
      printf("❌ Constraint already violated by existing placements\n");
      return 0;
    }
  }

  // Log constraint start
  debug_log_tree_constraint_start(solver, next_constraint, unplaced_comp);

  // Generate all placement options for this constraint
  TreePlacementOption options[200];
  int option_count = generate_placement_options_for_constraint(
      solver, next_constraint, unplaced_comp, options);

  if (option_count == 0) {
    printf("❌ No valid placement options for constraint\n");
    return 0;
  }

  printf("📋 Generated %d placement options\n", option_count);

  // Order options by conflict status then preference
  order_placement_options(options, option_count);

  // Log placement options
  debug_log_tree_placement_options(solver, options, option_count);

  // Try each option in order
  for (int i = 0; i < option_count; i++) {
    TreePlacementOption *option = &options[i];

    printf("🎯 Trying option %d: (%d,%d) conflict=%d score=%d\n", i + 1,
           option->x, option->y, option->has_conflict,
           option->preference_score);

    if (option->has_conflict) {
      printf("⚠️  Option has conflicts - trying anyway\n");
    }

    // Create child node for this placement
    int comp_index = unplaced_comp - solver->components;
    TreeNode *child =
        create_tree_node(unplaced_comp, next_constraint, option->x, option->y,
                         ts->current_node->depth + 1, comp_index);

    ts->current_node->children[ts->current_node->child_count++] = child;
    child->parent = ts->current_node;
    ts->nodes_created++;

    // Try placing component at this position
    int placement_success = tree_place_component(solver, child);
    debug_log_tree_placement_attempt(solver, unplaced_comp, option->x,
                                     option->y, i + 1, placement_success);

    if (placement_success) {
      // Success - move to this node and continue
      ts->current_node = child;
      debug_log_tree_node_creation(solver, child);

      // Remove this constraint from remaining
      for (int j = 0; j < ts->remaining_count; j++) {
        if (ts->remaining_constraints[j] == next_constraint) {
          for (int k = j; k < ts->remaining_count - 1; k++) {
            ts->remaining_constraints[k] = ts->remaining_constraints[k + 1];
          }
          ts->remaining_count--;
          break;
        }
      }

      // Recursively process next constraint
      int result = advance_to_next_constraint(solver);
      if (result) {
        return 1; // Success
      }

      // Failed - backtrack
      remove_component(solver, unplaced_comp);
      ts->current_node = child->parent;
    }

    // This option failed, try next
  }

  // All options failed - need intelligent backtracking
  printf("❌ All placement options failed for constraint\n");

  // Analyze conflicts for intelligent backtracking
  TreeNode *backtrack_target =
      find_conflict_backtrack_target(solver, options, option_count);
  if (backtrack_target) {
    printf("🔄 Intelligent backtrack to depth %d\n", backtrack_target->depth);
    ts->conflict_backtracks++;
    // TODO: Implement backtracking to specific node
  }

  return 0; // Failed
}

/**
 * @brief Generate all possible placement options for a constraint using modular system
 */
int generate_placement_options_for_constraint(LayoutSolver *solver,
                                              DSLConstraint *constraint,
                                              Component *unplaced_comp,
                                              TreePlacementOption *options) {
  // Find the already-placed component in this constraint
  Component *comp_a = find_component(solver, constraint->component_a);
  Component *comp_b = find_component(solver, constraint->component_b);

  Component *placed_comp = NULL;
  if (comp_a && comp_a->is_placed && comp_a != unplaced_comp) {
    placed_comp = comp_a;
  } else if (comp_b && comp_b->is_placed && comp_b != unplaced_comp) {
    placed_comp = comp_b;
  }

  if (!placed_comp) {
    printf("❌ No placed component found for constraint\n");
    return 0;
  }

  printf("📍 Placed component: %s at (%d,%d)\n", placed_comp->name,
         placed_comp->placed_x, placed_comp->placed_y);

  // Use the direct constraint system to generate placements
  int option_count = generate_constraint_placements(solver, constraint, unplaced_comp,
                                                    placed_comp, options, 200);

  return option_count;
}

/**
 * @brief Order placement options by conflict status then preference score
 */
void order_placement_options(TreePlacementOption *options, int option_count) {
  // Simple bubble sort - conflict-free options first, then by preference score
  for (int i = 0; i < option_count - 1; i++) {
    for (int j = 0; j < option_count - i - 1; j++) {
      TreePlacementOption *a = &options[j];
      TreePlacementOption *b = &options[j + 1];

      // Primary: conflict status (conflict-free first)
      if (a->has_conflict && !b->has_conflict) {
        TreePlacementOption temp = *a;
        *a = *b;
        *b = temp;
      }
      // Secondary: within same conflict status, sort by preference score
      // (higher first)
      else if (a->has_conflict == b->has_conflict &&
               a->preference_score < b->preference_score) {
        TreePlacementOption temp = *a;
        *a = *b;
        *b = temp;
      }
    }
  }
}


/**
 * @brief Detect conflicts and record detailed information about conflicting
 * components
 */
void detect_placement_conflicts_detailed(LayoutSolver *solver, Component *comp,
                                         int x, int y,
                                         ConflictInfo *conflicts) {
  conflicts->conflict_count = 0;

  for (int i = 0; i < solver->component_count; i++) {
    Component *other = &solver->components[i];
    if (other == comp || !other->is_placed)
      continue;

    if (has_character_overlap(solver, comp, x, y, other, other->placed_x,
                              other->placed_y)) {
      if (conflicts->conflict_count < MAX_COMPONENTS) {
        conflicts->conflicting_components[conflicts->conflict_count] = i;

        // Find depth of this component (for now, use a simple heuristic)
        // In a full implementation, we'd track placement order
        conflicts->conflict_depths[conflicts->conflict_count] =
            0; // TODO: Track actual depth

        conflicts->conflict_count++;
      }
    }
  }
}

/**
 * @brief Find the best backtrack target based on conflict analysis
 */
TreeNode *find_conflict_backtrack_target(LayoutSolver *solver,
                                         TreePlacementOption *failed_options,
                                         int option_count) {
  // Find the option with conflicts involving the shallowest components
  int min_max_depth = 999;
  TreePlacementOption *best_option = NULL;

  for (int i = 0; i < option_count; i++) {
    if (!failed_options[i].has_conflict)
      continue;

    // Find the maximum depth among conflicting components for this option
    int max_depth = 0;
    for (int j = 0; j < failed_options[i].conflicts.conflict_count; j++) {
      if (failed_options[i].conflicts.conflict_depths[j] > max_depth) {
        max_depth = failed_options[i].conflicts.conflict_depths[j];
      }
    }

    if (max_depth < min_max_depth) {
      min_max_depth = max_depth;
      best_option = &failed_options[i];
    }
  }

  // TODO: Find actual tree node at the target depth
  return NULL; // Placeholder
}

/**
 * @brief Place a component at a tree node's position
 */
int tree_place_component(LayoutSolver *solver, TreeNode *node) {
  if (!is_placement_valid(solver, node->component, node->x, node->y)) {
    return 0;
  }

  place_component(solver, node->component, node->x, node->y);
  return 1;
}

/**
 * @brief Get the next constraint that involves already placed components
 */
DSLConstraint *get_next_constraint_involving_placed(LayoutSolver *solver) {
  TreeSolver *ts = &solver->tree_solver;

  for (int i = 0; i < ts->remaining_count; i++) {
    DSLConstraint *constraint = ts->remaining_constraints[i];

    Component *comp_a = find_component(solver, constraint->component_a);
    Component *comp_b = find_component(solver, constraint->component_b);

    // Check if exactly one component is placed
    int a_placed = (comp_a && comp_a->is_placed);
    int b_placed = (comp_b && comp_b->is_placed);

    if ((a_placed && !b_placed) || (!a_placed && b_placed)) {
      return constraint;
    }
  }

  return NULL; // No more constraints with one placed component
}
