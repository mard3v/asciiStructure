#ifndef CONSTRAINT_SOLVER_H
#define CONSTRAINT_SOLVER_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

// =============================================================================
// CONSTRAINT SOLVER DATA STRUCTURES AND CONSTANTS
// =============================================================================

#define MAX_COMPONENTS 20
#define MAX_CONSTRAINTS 50
#define MAX_TILE_SIZE 20
#define MAX_GRID_SIZE 200
#define MAX_SOLVER_ITERATIONS 10000  // Prevent infinite loops
#define MAX_PLACEMENT_ATTEMPTS 100   // Per component
#define MAX_OUTPUT_LINES 40          // Limit grid output
#define MAX_OUTPUT_WIDTH 120         // Limit grid width output
#define MAX_COMPONENT_GROUP_SIZE 20  // Maximum components in a group

typedef enum {
    DSL_ADJACENT
} DSLConstraintType;

// Direction characters for constraints:
// 'n' = north (up)
// 'e' = east (right)  
// 's' = south (down)
// 'w' = west (left)
// 'a' = any direction
typedef char Direction;

typedef struct Component {
    char name[64];
    char ascii_tile[MAX_TILE_SIZE][MAX_TILE_SIZE];
    int width, height;
    int placed_x, placed_y;
    int is_placed;
    int group_id;  // Components with same group_id move together
} Component;

typedef struct DSLConstraint {
    DSLConstraintType type;
    char component_a[64];
    char component_b[64];
    Direction direction;
} DSLConstraint;

typedef struct LayoutSolver {
    Component components[MAX_COMPONENTS];
    int component_count;
    DSLConstraint constraints[MAX_CONSTRAINTS];
    int constraint_count;
    char grid[MAX_GRID_SIZE][MAX_GRID_SIZE];
    int grid_width, grid_height;
    int grid_min_x, grid_min_y;  // Track minimum coordinates for dynamic grid
    int constraint_graph[MAX_COMPONENTS][MAX_COMPONENTS]; // dependency graph
    int placement_attempts[MAX_COMPONENTS]; // for backtracking
    int total_iterations; // Global iteration counter for safety
    int next_group_id;    // For assigning component group IDs
    FILE* debug_file;     // Debug output file
} LayoutSolver;

// =============================================================================
// FUNCTION PROTOTYPES  
// =============================================================================

// =============================
// CORE SOLVER INTERFACE
// =============================
void init_solver(LayoutSolver* solver, int width, int height);
int solve_constraints(LayoutSolver* solver);

// =============================
// COMPONENT MANAGEMENT
// =============================
Component* find_component(LayoutSolver* solver, const char* name);
void add_component(LayoutSolver* solver, const char* name, const char* tile_data);
void remove_component(LayoutSolver* solver, Component* comp);
int is_placement_valid(LayoutSolver* solver, Component* comp, int x, int y);
void place_component(LayoutSolver* solver, Component* comp, int x, int y);

// =============================
// CONSTRAINT MANAGEMENT
// =============================
void add_constraint(LayoutSolver* solver, const char* constraint_line);
int satisfies_constraints(LayoutSolver* solver, Component* comp, int x, int y);
int check_constraint_satisfied(LayoutSolver* solver, DSLConstraint* constraint, 
                             Component* comp1, Component* comp2, int test_x, int test_y);

// =============================
// SPATIAL RELATIONSHIP FUNCTIONS
// =============================
int check_adjacent(int x1, int y1, int w1, int h1, int x2, int y2, int w2, int h2, Direction dir);
int has_overlap(LayoutSolver* solver, Component* comp, int x, int y);
int has_horizontal_overlap(int x1, int w1, int x2, int w2);
int has_vertical_overlap(int y1, int h1, int y2, int h2);

// =============================
// CONSTRAINT VALIDATION
// =============================
int check_adjacent_constraint(LayoutSolver* solver, DSLConstraint* constraint, 
                             Component* comp1, Component* comp2, int test_x, int test_y);
int solve_adjacent_constraint(LayoutSolver* solver, DSLConstraint* constraint, 
                             Component* comp_to_place, Component* comp_placed);

// =============================
// SOLVER INTERNALS
// =============================
void build_constraint_graph(LayoutSolver* solver);
int solve_constraints_recursive(LayoutSolver* solver, int component_index);
int solve_constraints_backtracking(LayoutSolver* solver, int component_index);

// =============================
// DYNAMIC GRID SYSTEM
// =============================
void expand_grid_for_component(LayoutSolver* solver, Component* comp, int x, int y);
void move_component_group(LayoutSolver* solver, int group_id, int dx, int dy);
int try_place_with_sliding(LayoutSolver* solver, Component* new_comp, Component* placed_comp, Direction dir);
void normalize_grid_coordinates(LayoutSolver* solver);
int count_constraint_degree(LayoutSolver* solver, Component* comp);
Component* find_most_constrained_unplaced(LayoutSolver* solver);
int solve_dynamic_constraints(LayoutSolver* solver);

// =============================
// DEBUG AND VISUALIZATION
// =============================
void init_debug_file(LayoutSolver* solver);
void close_debug_file(LayoutSolver* solver);
void debug_log_placement_attempt(LayoutSolver* solver, Component* comp, Component* target_comp, Direction dir, int x, int y, int success);
void debug_log_grid_state(LayoutSolver* solver, const char* stage);
void debug_log_constraint_analysis(LayoutSolver* solver, Component* comp);
void debug_log_ascii_grid(LayoutSolver* solver, const char* title);

// =============================
// OUTPUT AND VERIFICATION
// =============================
void display_components(LayoutSolver* solver);
void display_constraints(LayoutSolver* solver);
void display_grid(LayoutSolver* solver);
void verify_solution(LayoutSolver* solver);

#endif // CONSTRAINT_SOLVER_H