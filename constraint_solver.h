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
#define MAX_BACKTRACK_DEPTH 50       // Maximum backtracking depth

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

// Constraint solver strategy enumeration
typedef enum {
    SOLVER_RECURSIVE_TREE = 0,    // Recursive backtracking search tree with pruning
    SOLVER_COUNT                  // Number of available solvers
} ConstraintSolverType;

typedef struct Component {
    char name[64];
    char ascii_tile[MAX_TILE_SIZE][MAX_TILE_SIZE];
    int width, height;
    int placed_x, placed_y;
    int is_placed;
    int group_id;  // Components with same group_id move together
} Component;

// Backtracking state snapshot
typedef struct BacktrackState {
    Component components[MAX_COMPONENTS];    // Component states at this point
    char grid[MAX_GRID_SIZE][MAX_GRID_SIZE]; // Grid state at this point
    int grid_width, grid_height;             // Grid dimensions
    int grid_min_x, grid_min_y;              // Grid bounds
    int next_group_id;                       // Group ID counter
    int component_index;                     // Which component we're trying to place
    int constraint_index;                    // Which constraint we're trying to satisfy
    int placement_option;                    // Which placement option we tried
} BacktrackState;

typedef struct DSLConstraint {
    DSLConstraintType type;
    char component_a[64];
    char component_b[64];
    Direction direction;
} DSLConstraint;

// Placement option for tree search
typedef struct PlacementOption {
    DSLConstraint* constraint;
    Component* other_comp;
    Direction dir;
    int x, y; // Specific placement coordinates
} PlacementOption;

typedef struct LayoutSolver {
    Component components[MAX_COMPONENTS];
    int component_count;
    DSLConstraint constraints[MAX_CONSTRAINTS];
    int constraint_count;
    char grid[MAX_GRID_SIZE][MAX_GRID_SIZE];
    int grid_width, grid_height;
    int grid_min_x, grid_min_y;  // Track minimum coordinates for dynamic grid
    int placement_attempts[MAX_COMPONENTS]; // for backtracking
    int total_iterations; // Global iteration counter for safety
    int next_group_id;    // For assigning component group IDs
    FILE* debug_file;     // Debug output file

    // Solver configuration
    ConstraintSolverType solver_type;  // Which constraint solver strategy to use

    // Backtracking stack
    BacktrackState backtrack_stack[MAX_BACKTRACK_DEPTH];
    int backtrack_depth; // Current depth in backtracking stack

    // Failed position pruning - track positions that have failed for each component
    struct {
        int x, y;           // Failed position coordinates
        int valid;          // Whether this entry is active
    } failed_positions[MAX_COMPONENTS][200];  // Track up to 200 failed positions per component
    int failed_counts[MAX_COMPONENTS];        // Number of failed positions for each component
} LayoutSolver;

// =============================================================================
// FUNCTION PROTOTYPES  
// =============================================================================

// =============================
// CORE SOLVER INTERFACE
// =============================
void init_solver(LayoutSolver* solver, int width, int height);
void set_solver_type(LayoutSolver* solver, ConstraintSolverType type);
int solve_constraints(LayoutSolver* solver);

// =============================
// MODULAR SOLVER STRATEGIES
// =============================
int solve_recursive_tree(LayoutSolver* solver);  // Recursive backtracking with pruning

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

// =============================
// BACKTRACKING FUNCTIONS
// =============================
void save_solver_state(LayoutSolver* solver, int component_index, int constraint_index, int placement_option);
int restore_solver_state(LayoutSolver* solver);
void clear_backtrack_stack(LayoutSolver* solver);

// =============================
// RECURSIVE TREE SOLVER INTERNALS (PRIVATE)
// =============================
int place_component_recursive(LayoutSolver* solver, int depth);
int try_placement_options(LayoutSolver* solver, Component* comp, int depth);
void get_placement_options(LayoutSolver* solver, Component* comp, PlacementOption* options, int* option_count);
void record_failed_position(LayoutSolver* solver, int component_index, int x, int y);
int is_position_failed(LayoutSolver* solver, int component_index, int x, int y);

// =============================
// DYNAMIC GRID SYSTEM
// =============================
void expand_grid_for_component(LayoutSolver* solver, Component* comp, int x, int y);
void normalize_grid_coordinates(LayoutSolver* solver);
int count_constraint_degree(LayoutSolver* solver, Component* comp);
Component* find_most_constrained_unplaced(LayoutSolver* solver);

// =============================
// DEBUG AND VISUALIZATION
// =============================
void init_debug_file(LayoutSolver* solver);
void close_debug_file(LayoutSolver* solver);
void debug_log_placement_attempt(LayoutSolver* solver, Component* comp, Component* target_comp, Direction dir, int x, int y, int success);
void debug_log_ascii_grid(LayoutSolver* solver, const char* title);
void debug_log_placement_grid(LayoutSolver* solver, const char* title, Component* highlight_comp, int highlight_x, int highlight_y);

// =============================
// OUTPUT AND VERIFICATION
// =============================
void display_grid(LayoutSolver* solver);

#endif // CONSTRAINT_SOLVER_H