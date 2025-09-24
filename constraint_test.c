#include "constraint_solver.h"
#include "constraints.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// =============================================================================
// CONSTRAINT TESTING SYSTEM
// =============================================================================
// This system allows testing individual constraints with simple room setups.
// It provides visual feedback showing all placement options ordered by
// preference.

typedef struct {
  int x, y;
  int score;
  int has_conflict;
} TestPlacementResult;

/**
 * @brief Setup two simple test rooms for constraint testing
 */
void setup_test_rooms(LayoutSolver *solver) {
  // Room A - Large rectangular room (7x5)
  const char *room_a = "+------+\n"
                       "|      |\n"
                       "|      |\n"
                       "|      |\n"
                       "+------+";

  // Room B - Small rectangular room (4x3)
  const char *room_b = "+--+\n"
                       "|  |\n"
                       "+--+";

  add_component(solver, "RoomA", room_a);
  add_component(solver, "RoomB", room_b);
}

/**
 * @brief Parse constraint type from string input
 */
int parse_constraint_type(const char *type_str) {
  if (strcmp(type_str, "ADJACENT") == 0) {
    return DSL_ADJACENT;
  }
  printf("❌ Unknown constraint type: %s\n", type_str);
  return -1;
}

/**
 * @brief Parse direction parameter for constraints
 */
char parse_direction(const char *dir_str) {
  if (strcmp(dir_str, "N") == 0 || strcmp(dir_str, "north") == 0)
    return 'n';
  if (strcmp(dir_str, "S") == 0 || strcmp(dir_str, "south") == 0)
    return 's';
  if (strcmp(dir_str, "E") == 0 || strcmp(dir_str, "east") == 0)
    return 'e';
  if (strcmp(dir_str, "W") == 0 || strcmp(dir_str, "west") == 0)
    return 'w';
  if (strcmp(dir_str, "*") == 0 || strcmp(dir_str, "any") == 0)
    return 'a';

  printf("❌ Unknown direction: %s (use N/S/E/W/*)\n", dir_str);
  return '?';
}

/**
 * @brief Display a component at a specific position in a grid
 */
void display_component_at_position(LayoutSolver *solver, Component *comp,
                                   int pos_x, int pos_y, int grid_width,
                                   int grid_height, FILE *log_file) {
  char display_grid[50][100];

  // Initialize grid with dots to show grid cells
  for (int y = 0; y < grid_height && y < 50; y++) {
    for (int x = 0; x < grid_width && x < 100; x++) {
      display_grid[y][x] = '.';
    }
    display_grid[y][grid_width] = '\0';
  }

  // Place already placed components (in this case, just RoomA at 0,0)
  for (int i = 0; i < solver->component_count; i++) {
    Component *placed_comp = &solver->components[i];
    if (placed_comp->is_placed) {
      for (int y = 0; y < placed_comp->height; y++) {
        for (int x = 0; x < placed_comp->width; x++) {
          int grid_x = placed_comp->placed_x + x;
          int grid_y = placed_comp->placed_y + y;
          if (grid_x >= 0 && grid_x < grid_width && grid_y >= 0 &&
              grid_y < grid_height) {
            display_grid[grid_y][grid_x] = placed_comp->ascii_tile[y][x];
          }
        }
      }
    }
  }

  // Place the test component at the specified position
  for (int y = 0; y < comp->height; y++) {
    for (int x = 0; x < comp->width; x++) {
      int grid_x = pos_x + x;
      int grid_y = pos_y + y;
      if (grid_x >= 0 && grid_x < grid_width && grid_y >= 0 &&
          grid_y < grid_height) {
        // Use different character to distinguish test placement
        char tile_char = comp->ascii_tile[y][x];
        if (tile_char == '+' || tile_char == '-' || tile_char == '|') {
          display_grid[grid_y][grid_x] =
              tile_char == '+'
                  ? '#'
                  : (tile_char == '-' ? '='
                                      : (tile_char == '|' ? ':' : tile_char));
        } else {
          display_grid[grid_y][grid_x] = tile_char;
        }
      }
    }
  }

  // Print the grid
  for (int y = 0; y < grid_height && y < 50; y++) {
    fprintf(log_file, "%s\n", display_grid[y]);
  }
}

/**
 * @brief Compare function for sorting placement results by score
 */
int compare_placement_results(const void *a, const void *b) {
  TestPlacementResult *result_a = (TestPlacementResult *)a;
  TestPlacementResult *result_b = (TestPlacementResult *)b;

  // Conflict-free placements come first
  if (result_a->has_conflict != result_b->has_conflict) {
    return result_a->has_conflict - result_b->has_conflict;
  }

  // Then sort by score (higher is better)
  return result_b->score - result_a->score;
}

/**
 * @brief Test a constraint and display all placement options
 */
void test_constraint(LayoutSolver *solver, int constraint_type,
                     const char *direction_param, FILE *log_file) {
  Component *room_a = NULL;
  Component *room_b = NULL;

  // Find the test components
  for (int i = 0; i < solver->component_count; i++) {
    if (strcmp(solver->components[i].name, "RoomA") == 0) {
      room_a = &solver->components[i];
    } else if (strcmp(solver->components[i].name, "RoomB") == 0) {
      room_b = &solver->components[i];
    }
  }

  if (!room_a || !room_b) {
    fprintf(log_file, "❌ Could not find test rooms\n");
    return;
  }

  // Place RoomA at origin
  room_a->is_placed = 1;
  room_a->placed_x = 5;
  room_a->placed_y = 3;

  // Create constraint
  DSLConstraint test_constraint;
  test_constraint.type = constraint_type;
  strcpy(test_constraint.component_a, "RoomB");
  strcpy(test_constraint.component_b, "RoomA");
  test_constraint.direction = parse_direction(direction_param);

  fprintf(log_file, "\n🧪 CONSTRAINT TEST RESULTS\n");
  fprintf(log_file, "=========================================\n");
  fprintf(log_file, "Constraint: %s\n",
          constraint_type == DSL_ADJACENT ? "ADJACENT" : "UNKNOWN");
  fprintf(log_file, "Direction: %s (%c)\n", direction_param,
          test_constraint.direction);
  fprintf(log_file, "Placing: RoomB relative to RoomA\n");
  fprintf(log_file, "RoomA position: (%d, %d)\n\n", room_a->placed_x,
          room_a->placed_y);

  // Generate placement options
  TreePlacementOption options[200];
  int option_count = generate_constraint_placements(
      solver, &test_constraint, room_b, room_a, options, 200);

  fprintf(log_file, "Generated %d placement options:\n\n", option_count);

  if (option_count == 0) {
    fprintf(log_file, "❌ No placement options generated!\n");
    return;
  }

  // Calculate scores and check for conflicts
  TestPlacementResult results[200];
  for (int i = 0; i < option_count; i++) {
    results[i].x = options[i].x;
    results[i].y = options[i].y;
    results[i].score = calculate_constraint_score(
        solver, room_b, options[i].x, options[i].y, &test_constraint, room_a);

    // Simple conflict check - just check bounds for now
    results[i].has_conflict = 0;
    if (options[i].x < 0 || options[i].y < 0 ||
        options[i].x + room_b->width > 30 ||
        options[i].y + room_b->height > 20) {
      results[i].has_conflict = 1;
    }
  }

  // Sort by preference
  qsort(results, option_count, sizeof(TestPlacementResult),
        compare_placement_results);

  // Display results
  int display_width = 30;
  int display_height = 20;

  for (int i = 0; i < option_count; i++) {
    fprintf(log_file, "--- Option %d: Position (%d, %d) Score: %d %s ---\n",
            i + 1, results[i].x, results[i].y, results[i].score,
            results[i].has_conflict ? "[CONFLICT]" : "[OK]");

    display_component_at_position(solver, room_b, results[i].x, results[i].y,
                                  display_width, display_height, log_file);
    fprintf(log_file, "\n");
  }
}

/**
 * @brief Main constraint testing interface
 */
int main() {
  printf("🧪 Constraint Testing System\n");
  printf("=============================\n");
  printf("This system tests individual constraints with two simple rooms.\n");
  printf("Results are logged to 'constraint_test.log'\n\n");

  // Open log file
  FILE *log_file = fopen("constraint_test.log", "w");
  if (!log_file) {
    printf("❌ Could not create log file\n");
    return 1;
  }

  // Initialize solver
  LayoutSolver solver;
  init_solver(&solver, 30, 20);

  // Setup test rooms
  setup_test_rooms(&solver);

  fprintf(log_file, "🏠 TEST ROOM DEFINITIONS\n");
  fprintf(log_file, "========================\n\n");

  fprintf(log_file, "RoomA (will be placed at origin):\n");
  for (int y = 0; y < solver.components[0].height; y++) {
    for (int x = 0; x < solver.components[0].width; x++) {
      fprintf(log_file, "%c", solver.components[0].ascii_tile[y][x]);
    }
    fprintf(log_file, "\n");
  }

  fprintf(log_file, "\nRoomB (will be placed relative to RoomA):\n");
  for (int y = 0; y < solver.components[1].height; y++) {
    for (int x = 0; x < solver.components[1].width; x++) {
      fprintf(log_file, "%c", solver.components[1].ascii_tile[y][x]);
    }
    fprintf(log_file, "\n");
  }

  // Interactive testing loop
  char constraint_type[50];
  char direction[10];

  while (1) {
    printf("\nEnter constraint type (ADJACENT) or 'quit': ");
    if (scanf("%49s", constraint_type) != 1)
      break;

    if (strcmp(constraint_type, "quit") == 0)
      break;

    int type = parse_constraint_type(constraint_type);
    if (type == -1)
      continue;

    printf("Enter direction parameter (N/S/E/W/*): ");
    if (scanf("%9s", direction) != 1)
      break;

    printf("Testing %s constraint with direction %s...\n", constraint_type,
           direction);
    printf("Check constraint_test.log for visual results.\n");

    test_constraint(&solver, type, direction, log_file);
    fflush(log_file);
  }

  fclose(log_file);
  printf("🎯 Testing complete! Results saved to constraint_test.log\n");
  return 0;
}
