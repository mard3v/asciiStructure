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
#define MAX_SLIDE_CHAINS 10          // Maximum simultaneous sliding chains
#define MAX_CHAIN_LENGTH 8           // Maximum components in a single sliding chain
#define MAX_SLIDE_DISTANCE 20        // Maximum distance a component can slide

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

// Sliding puzzle structures
typedef struct SlideMove {
    int component_index;                        // Which component to slide
    Direction direction;                        // Direction to slide (n/s/e/w)
    int distance;                              // How far to slide
    int new_x, new_y;                          // Final position after slide
} SlideMove;

typedef struct SlideChain {
    SlideMove moves[MAX_CHAIN_LENGTH];         // Sequence of moves to execute
    int chain_length;                          // Number of moves in chain
    int feasible;                              // Whether chain is geometrically valid
    int constraint_preserving;                 // Whether chain preserves all constraints
    int total_displacement;                    // Total grid area displacement
    int target_x, target_y;                    // Target position we're trying to clear
} SlideChain;

// Constraint solver strategy enumeration
typedef enum {
    SOLVER_RECURSIVE_TREE = 0,    // Recursive backtracking search tree with pruning
    SOLVER_TREE_CONSTRAINT = 1,   // Tree-based constraint resolution with conflict-depth backtracking
    SOLVER_COUNT                  // Number of available solvers
} ConstraintSolverType;

typedef struct Component {
    char name[64];
    char ascii_tile[MAX_TILE_SIZE][MAX_TILE_SIZE];
    int width, height;
    int placed_x, placed_y;
    int is_placed;
    int group_id;  // Components with same group_id move together

    // Intelligent backtracking fields
    int mobility_score;        // Lower = more constrained, harder to move
    int constraint_count;      // Number of constraints involving this component
    int dependency_level;      // Placement priority (0 = place first, higher = place later)
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

// Tree-based constraint solver structures
typedef struct ConflictInfo {
    int conflicting_components[MAX_COMPONENTS];  // Indices of conflicting components
    int conflict_depths[MAX_COMPONENTS];         // Tree depths where conflicts were placed
    int conflict_count;                          // Number of conflicting components
} ConflictInfo;

typedef struct TreePlacementOption {
    int x, y;                                    // Placement coordinates
    int has_conflict;                            // Whether this placement has conflicts
    ConflictInfo conflicts;                      // Details about conflicts if any
    int preference_score;                        // Constraint preference score (edge alignment, etc.)
} TreePlacementOption;

typedef struct TreeNode {
    Component* component;                        // Component being placed at this node
    DSLConstraint* constraint;                   // Constraint being satisfied
    int x, y;                                    // Placement position
    int depth;                                   // Tree depth (room depth)
    int component_index;                         // Index in solver->components array

    // Placement options at this node
    TreePlacementOption placement_options[200];  // All possible placements for next constraint
    int option_count;                           // Number of placement options
    int current_option;                         // Currently trying this option index

    // Tree structure
    struct TreeNode* parent;                    // Parent node
    struct TreeNode* children[50];              // Child nodes
    int child_count;                            // Number of children

    // Backtracking info
    int failed_completely;                      // Whether this subtree failed completely
} TreeNode;

typedef struct TreeSolver {
    TreeNode* root;                             // Root of the search tree
    TreeNode* current_node;                     // Currently active node
    TreeNode* node_stack[MAX_BACKTRACK_DEPTH];  // Stack for backtracking
    int stack_depth;                            // Current stack depth

    // Constraint processing
    DSLConstraint* remaining_constraints[MAX_CONSTRAINTS];  // Unprocessed constraints
    int remaining_count;                        // Number of remaining constraints
    DSLConstraint* current_constraint;          // Currently processing constraint

    // Statistics
    int nodes_created;                          // Total nodes created
    int backtracks_performed;                   // Number of backtrack operations
    int conflict_backtracks;                    // Number of conflict-based backtracks
} TreeSolver;

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
    FILE* debug_file;     // Debug output file for main solver
    FILE* tree_debug_file; // Debug output file for tree solver

    // Solver configuration
    ConstraintSolverType solver_type;  // Which constraint solver strategy to use

    // Tree-based constraint solver
    TreeSolver tree_solver;            // Tree-based constraint resolution state

    // Backtracking stack
    BacktrackState backtrack_stack[MAX_BACKTRACK_DEPTH];
    int backtrack_depth; // Current depth in backtracking stack

    // Failed position pruning - track positions that have failed for each component
    struct {
        int x, y;           // Failed position coordinates
        int valid;          // Whether this entry is active
    } failed_positions[MAX_COMPONENTS][200];  // Track up to 200 failed positions per component
    int failed_counts[MAX_COMPONENTS];        // Number of failed positions for each component

    // Intelligent conflict resolution fields
    int placement_order[MAX_COMPONENTS];      // Order to place components (most constrained first)
    int dependency_graph[MAX_COMPONENTS][MAX_COMPONENTS]; // Component dependency adjacency matrix
    struct {
        int overlapping_components[MAX_COMPONENTS];  // Which components are overlapping
        int overlap_count;                          // Number of overlapping components
        int target_component;                       // Component being placed
        int conflict_resolved;                      // Whether conflict was resolved
    } conflict_state;

    // Sliding puzzle conflict resolution
    struct {
        int active_chains;                          // Number of active sliding chains
        int chain_attempts;                         // Number of chain attempts made
        int successful_slides;                      // Number of successful slide operations
        int max_chain_length;                       // Longest successful chain length
    } slide_stats;
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
int solve_recursive_tree(LayoutSolver* solver);        // Recursive backtracking with pruning
int solve_tree_constraint(LayoutSolver* solver);       // Tree-based constraint resolution

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
int has_character_overlap(LayoutSolver *solver, Component *comp1, int x1, int y1,
                         Component *comp2, int x2, int y2);

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
// INTELLIGENT CONFLICT RESOLUTION
// =============================
void analyze_constraint_dependencies(LayoutSolver* solver);
void calculate_mobility_scores(LayoutSolver* solver);
void determine_placement_order(LayoutSolver* solver);
int detect_placement_conflicts(LayoutSolver* solver, Component* target_comp, int x, int y);
int attempt_conflict_resolution(LayoutSolver* solver, Component* target_comp, int x, int y);
int try_relocate_component(LayoutSolver* solver, int comp_index, Component* target_comp);

// =============================
// SLIDING PUZZLE CONFLICT RESOLUTION
// =============================
int attempt_sliding_resolution(LayoutSolver* solver, Component* target_comp, int x, int y);
int find_sliding_chains(LayoutSolver* solver, Component* target_comp, int x, int y, SlideChain* chains, int* chain_count);
int validate_slide_chain(LayoutSolver* solver, SlideChain* chain);
int execute_slide_chain(LayoutSolver* solver, SlideChain* chain);
int can_component_slide(LayoutSolver* solver, int comp_index, Direction dir, int* max_distance);
int calculate_slide_move(LayoutSolver* solver, int comp_index, Direction dir, int distance, SlideMove* move);
int check_constraint_preservation(LayoutSolver* solver, SlideChain* chain);
void rollback_slide_chain(LayoutSolver* solver, SlideChain* chain);
int find_directional_slide_chain(LayoutSolver* solver, int comp_index, Direction push_dir, SlideChain* chain);
int component_mobility_score(LayoutSolver* solver, int comp_index);

// =============================
// RECURSIVE TREE SOLVER INTERNALS (PRIVATE)
// =============================
int place_component_recursive(LayoutSolver* solver, int depth);
int try_placement_options(LayoutSolver* solver, Component* comp, int depth);
void get_placement_options(LayoutSolver* solver, Component* comp, PlacementOption* options, int* option_count);
Component* get_next_component_intelligent(LayoutSolver* solver);
void record_failed_position(LayoutSolver* solver, int component_index, int x, int y);
int is_position_failed(LayoutSolver* solver, int component_index, int x, int y);

// =============================
// TREE CONSTRAINT SOLVER INTERNALS (PRIVATE)
// =============================
void init_tree_solver(LayoutSolver* solver);
void cleanup_tree_solver(LayoutSolver* solver);
TreeNode* create_tree_node(Component* comp, DSLConstraint* constraint, int x, int y, int depth, int comp_index);
void free_tree_node(TreeNode* node);
int generate_placement_options_for_constraint(LayoutSolver* solver, DSLConstraint* constraint, Component* unplaced_comp, TreePlacementOption* options);
void order_placement_options(TreePlacementOption* options, int option_count);
int calculate_preference_score(LayoutSolver* solver, Component* comp, DSLConstraint* constraint, int x, int y);
void detect_placement_conflicts_detailed(LayoutSolver* solver, Component* comp, int x, int y, ConflictInfo* conflicts);
TreeNode* find_conflict_backtrack_target(LayoutSolver* solver, TreePlacementOption* failed_options, int option_count);
int tree_place_component(LayoutSolver* solver, TreeNode* node);
int advance_to_next_constraint(LayoutSolver* solver);
DSLConstraint* get_next_constraint_involving_placed(LayoutSolver* solver);

// =============================
// TREE SOLVER DEBUG FUNCTIONS
// =============================
void init_tree_debug_file(LayoutSolver* solver);
void close_tree_debug_file(LayoutSolver* solver);
void debug_log_tree_constraint_start(LayoutSolver* solver, DSLConstraint* constraint, Component* unplaced_comp);
void debug_log_tree_placement_options(LayoutSolver* solver, TreePlacementOption* options, int option_count);
void debug_log_tree_placement_attempt(LayoutSolver* solver, Component* comp, int x, int y, int option_num, int success);
void debug_log_tree_node_creation(LayoutSolver* solver, TreeNode* node);
void debug_log_tree_backtrack(LayoutSolver* solver, int from_depth, int to_depth, const char* reason);
void debug_log_tree_solution_path(LayoutSolver* solver);
void debug_log_tree_grid_state(LayoutSolver* solver, const char* stage);

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