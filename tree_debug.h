#ifndef TREE_DEBUG_H
#define TREE_DEBUG_H

#include "constraint_solver.h"

// =============================================================================
// TREE SOLVER DEBUG FUNCTION PROTOTYPES
// =============================================================================

void init_tree_debug_file(LayoutSolver* solver);
void close_tree_debug_file(LayoutSolver* solver);
void debug_log_tree_constraint_start(LayoutSolver* solver, DSLConstraint* constraint, Component* unplaced_comp);
void debug_log_tree_placement_options(LayoutSolver* solver, TreePlacementOption* options, int option_count);
void debug_log_tree_placement_attempt(LayoutSolver* solver, Component* comp, int x, int y, int option_num, int success);
void debug_log_tree_node_creation(LayoutSolver* solver, TreeNode* node);
void debug_log_tree_backtrack(LayoutSolver* solver, int from_depth, int to_depth, const char* reason);
void debug_log_tree_solution_path(LayoutSolver* solver);
void debug_log_enhanced_grid_state(LayoutSolver* solver, const char* stage);
void debug_log_current_tree_structure(LayoutSolver* solver, TreeNode* current_node);
void debug_log_placement_success_with_grid(LayoutSolver* solver, Component* comp);
void debug_log_backtrack_event(LayoutSolver* solver, TreeNode* repositioned_node, int old_x, int old_y, int new_x, int new_y);

#endif // TREE_DEBUG_H