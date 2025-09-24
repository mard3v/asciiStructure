#include "tree_debug.h"
#include <stdio.h>

// =============================================================================
// TREE SOLVER DEBUG FUNCTIONS
// =============================================================================

/**
 * @brief Initialize tree solver debug logging
 */
void init_tree_debug_file(LayoutSolver* solver) {
    solver->tree_debug_file = fopen("tree_placement_debug.log", "w");
    if (solver->tree_debug_file) {
        fprintf(solver->tree_debug_file, "=============================================================================\n");
        fprintf(solver->tree_debug_file, "TREE-BASED CONSTRAINT SOLVER - DEBUG LOG\n");
        fprintf(solver->tree_debug_file, "=============================================================================\n\n");

        // Log components and constraints summary
        fprintf(solver->tree_debug_file, "COMPONENTS (%d total):\n", solver->component_count);
        for (int i = 0; i < solver->component_count; i++) {
            Component* comp = &solver->components[i];
            fprintf(solver->tree_debug_file, "  %d. %s (%dx%d)\n", i + 1, comp->name, comp->width, comp->height);
        }

        fprintf(solver->tree_debug_file, "\nCONSTRAINTS (%d total):\n", solver->constraint_count);
        for (int i = 0; i < solver->constraint_count; i++) {
            DSLConstraint* constraint = &solver->constraints[i];
            fprintf(solver->tree_debug_file, "  %d. %s ADJACENT %s %c\n", i + 1,
                   constraint->component_a, constraint->component_b, constraint->direction);
        }

        fprintf(solver->tree_debug_file, "\n=============================================================================\n");
        fprintf(solver->tree_debug_file, "TREE CONSTRAINT RESOLUTION PROCESS\n");
        fprintf(solver->tree_debug_file, "=============================================================================\n\n");

        fflush(solver->tree_debug_file);
    }
}

/**
 * @brief Close tree solver debug file
 */
void close_tree_debug_file(LayoutSolver* solver) {
    if (solver->tree_debug_file) {
        fprintf(solver->tree_debug_file, "\n=============================================================================\n");
        fprintf(solver->tree_debug_file, "TREE SOLVER COMPLETE\n");
        fprintf(solver->tree_debug_file, "=============================================================================\n");
        fclose(solver->tree_debug_file);
        solver->tree_debug_file = NULL;
    }
}

/**
 * @brief Log the start of processing a constraint
 */
void debug_log_tree_constraint_start(LayoutSolver* solver, DSLConstraint* constraint, Component* unplaced_comp) {
    if (!solver->tree_debug_file) return;

    fprintf(solver->tree_debug_file, "📋 PROCESSING CONSTRAINT: %s ADJACENT %s %c\n",
           constraint->component_a, constraint->component_b, constraint->direction);
    fprintf(solver->tree_debug_file, "   ├─ Component to place: %s (%dx%d)\n",
           unplaced_comp->name, unplaced_comp->width, unplaced_comp->height);

    // Show already placed components
    fprintf(solver->tree_debug_file, "   ├─ Already placed components:\n");
    for (int i = 0; i < solver->component_count; i++) {
        Component* comp = &solver->components[i];
        if (comp->is_placed) {
            fprintf(solver->tree_debug_file, "   │  └─ %s at (%d,%d)\n",
                   comp->name, comp->placed_x, comp->placed_y);
        }
    }
    fprintf(solver->tree_debug_file, "\n");
    fflush(solver->tree_debug_file);
}

/**
 * @brief Log all generated placement options
 */
void debug_log_tree_placement_options(LayoutSolver* solver, TreePlacementOption* options, int option_count) {
    if (!solver->tree_debug_file) return;

    fprintf(solver->tree_debug_file, "🎯 GENERATED %d PLACEMENT OPTIONS:\n", option_count);
    fprintf(solver->tree_debug_file, "   ┌─────┬──────────┬──────────┬─────────┬──────────┐\n");
    fprintf(solver->tree_debug_file, "   │ #   │ Position │ Conflict │ Score   │ Status   │\n");
    fprintf(solver->tree_debug_file, "   ├─────┼──────────┼──────────┼─────────┼──────────┤\n");

    for (int i = 0; i < option_count && i < 20; i++) { // Limit to first 20 for readability
        TreePlacementOption* opt = &options[i];
        fprintf(solver->tree_debug_file, "   │ %3d │ (%3d,%3d) │ %s      │ %7d │ %s   │\n",
               i + 1, opt->x, opt->y,
               opt->has_conflict ? "YES" : "NO ",
               opt->preference_score,
               opt->has_conflict ? "DEFERRED" : "PRIORITY");
    }

    if (option_count > 20) {
        fprintf(solver->tree_debug_file, "   │ ... │   ...    │   ...    │   ...   │   ...    │\n");
        fprintf(solver->tree_debug_file, "   │     │ (%d more options omitted)        │          │\n", option_count - 20);
    }

    fprintf(solver->tree_debug_file, "   └─────┴──────────┴──────────┴─────────┴──────────┘\n\n");

    // Show ordering logic
    fprintf(solver->tree_debug_file, "🔄 ORDERING LOGIC:\n");
    fprintf(solver->tree_debug_file, "   ├─ Primary: Conflict status (conflict-free first)\n");
    fprintf(solver->tree_debug_file, "   ├─ Secondary: Preference score (higher first)\n");
    fprintf(solver->tree_debug_file, "   │  ├─ Edge alignment: +10 points\n");
    fprintf(solver->tree_debug_file, "   │  ├─ Perfect alignment: +10 points\n");
    fprintf(solver->tree_debug_file, "   │  └─ Center alignment: +15 points\n");
    fprintf(solver->tree_debug_file, "   └─ Result: Options ordered from most to least preferred\n\n");

    fflush(solver->tree_debug_file);
}

/**
 * @brief Log a placement attempt
 */
void debug_log_tree_placement_attempt(LayoutSolver* solver, Component* comp, int x, int y, int option_num, int success) {
    if (!solver->tree_debug_file) return;

    const char* result_symbol = success ? "✅" : "❌";
    const char* result_text = success ? "SUCCESS" : "FAILED";

    fprintf(solver->tree_debug_file, "%s PLACEMENT ATTEMPT #%d: %s at (%d,%d) - %s\n",
           result_symbol, option_num, comp->name, x, y, result_text);

    if (success) {
        fprintf(solver->tree_debug_file, "   ├─ Component successfully placed\n");
        fprintf(solver->tree_debug_file, "   └─ Creating child tree node at depth %d\n",
               solver->tree_solver.current_node->depth + 1);

        // Show current grid state after successful placement
        debug_log_placement_success_with_grid(solver, comp);
    } else {
        fprintf(solver->tree_debug_file, "   ├─ Placement validation failed\n");
        fprintf(solver->tree_debug_file, "   └─ Trying next option...\n");
    }
    fprintf(solver->tree_debug_file, "\n");
    fflush(solver->tree_debug_file);
}

/**
 * @brief Log tree node creation
 */
void debug_log_tree_node_creation(LayoutSolver* solver, TreeNode* node) {
    if (!solver->tree_debug_file) return;

    fprintf(solver->tree_debug_file, "🌳 TREE NODE CREATED:\n");
    fprintf(solver->tree_debug_file, "   ├─ Component: %s\n", node->component->name);
    fprintf(solver->tree_debug_file, "   ├─ Position: (%d,%d)\n", node->x, node->y);
    fprintf(solver->tree_debug_file, "   ├─ Tree depth: %d\n", node->depth);
    fprintf(solver->tree_debug_file, "   ├─ Parent: %s\n",
           node->parent ? node->parent->component->name : "ROOT");
    fprintf(solver->tree_debug_file, "   └─ Total nodes created: %d\n", solver->tree_solver.nodes_created);

    // Show current tree structure
    debug_log_current_tree_structure(solver, node);

    fprintf(solver->tree_debug_file, "\n");
    fflush(solver->tree_debug_file);
}

/**
 * @brief Log backtracking operation
 */
void debug_log_tree_backtrack(LayoutSolver* solver, int from_depth, int to_depth, const char* reason) {
    if (!solver->tree_debug_file) return;

    fprintf(solver->tree_debug_file, "🔄 BACKTRACKING:\n");
    fprintf(solver->tree_debug_file, "   ├─ Reason: %s\n", reason);
    fprintf(solver->tree_debug_file, "   ├─ From depth: %d\n", from_depth);
    fprintf(solver->tree_debug_file, "   ├─ To depth: %d\n", to_depth);
    fprintf(solver->tree_debug_file, "   └─ Backtrack type: %s\n",
           (to_depth < from_depth - 1) ? "INTELLIGENT (conflict-depth)" : "STANDARD");
    fprintf(solver->tree_debug_file, "\n");
    fflush(solver->tree_debug_file);
}

/**
 * @brief Log the final solution path
 */
void debug_log_tree_solution_path(LayoutSolver* solver) {
    if (!solver->tree_debug_file) return;

    fprintf(solver->tree_debug_file, "🎉 SOLUTION FOUND!\n");
    fprintf(solver->tree_debug_file, "===================\n\n");

    fprintf(solver->tree_debug_file, "📈 SOLUTION PATH:\n");
    int step = 1;
    for (int i = 0; i < solver->component_count; i++) {
        Component* comp = &solver->components[i];
        if (comp->is_placed) {
            fprintf(solver->tree_debug_file, "   %d. %s placed at (%d,%d)\n",
                   step++, comp->name, comp->placed_x, comp->placed_y);
        }
    }

    fprintf(solver->tree_debug_file, "\n📊 SOLUTION STATISTICS:\n");
    fprintf(solver->tree_debug_file, "   ├─ Total tree nodes: %d\n", solver->tree_solver.nodes_created);
    fprintf(solver->tree_debug_file, "   ├─ Standard backtracks: %d\n", solver->tree_solver.backtracks_performed);
    fprintf(solver->tree_debug_file, "   ├─ Conflict backtracks: %d\n", solver->tree_solver.conflict_backtracks);
    fprintf(solver->tree_debug_file, "   └─ Components placed: %d/%d\n", step - 1, solver->component_count);

    fflush(solver->tree_debug_file);
}

/**
 * @brief Enhanced grid state logging with bordered display
 */
void debug_log_enhanced_grid_state(LayoutSolver* solver, const char* stage) {
    if (!solver->tree_debug_file) return;

    fprintf(solver->tree_debug_file, "🏗️  GRID STATE: %s\n", stage);
    fprintf(solver->tree_debug_file, "   ╔════════════════════════════════════════════╗\n");

    // Find grid bounds
    int min_x = 999, max_x = -999, min_y = 999, max_y = -999;
    int found_any = 0;

    for (int i = 0; i < solver->component_count; i++) {
        Component* component = &solver->components[i];
        if (component->is_placed) {
            found_any = 1;
            if (component->placed_x < min_x) min_x = component->placed_x;
            if (component->placed_x + component->width > max_x) max_x = component->placed_x + component->width;
            if (component->placed_y < min_y) min_y = component->placed_y;
            if (component->placed_y + component->height > max_y) max_y = component->placed_y + component->height;
        }
    }

    if (!found_any) {
        fprintf(solver->tree_debug_file, "   ║ No components placed yet                   ║\n");
        fprintf(solver->tree_debug_file, "   ╚════════════════════════════════════════════╝\n\n");
        return;
    }

    // Add minimal padding
    min_x -= 1; max_x += 1; min_y -= 1; max_y += 1;

    // Draw grid with border
    for (int y = min_y; y < max_y; y++) {
        fprintf(solver->tree_debug_file, "   ║ ");
        for (int x = min_x; x < max_x; x++) {
            if (x >= 0 && x < MAX_GRID_SIZE && y >= 0 && y < MAX_GRID_SIZE) {
                char c = solver->grid[y][x];
                fprintf(solver->tree_debug_file, "%c", c == 0 ? '.' : c);
            } else {
                fprintf(solver->tree_debug_file, " ");
            }
        }
        fprintf(solver->tree_debug_file, " ║\n");
    }

    fprintf(solver->tree_debug_file, "   ╚════════════════════════════════════════════╝\n\n");
    fflush(solver->tree_debug_file);
}

/**
 * @brief Display current grid state after successful placement
 */
void debug_log_placement_success_with_grid(LayoutSolver* solver, Component* comp) {
    if (!solver->tree_debug_file) return;

    // Add a newline before the grid display
    fprintf(solver->tree_debug_file, "\n");

    // Create a dynamic message for the grid state
    char stage_message[100];
    snprintf(stage_message, sizeof(stage_message), "CURRENT STATE (after placing %s)", comp->name);

    // Use the enhanced grid display function
    debug_log_enhanced_grid_state(solver, stage_message);
}

/**
 * @brief Display current tree structure showing the path to current node
 */
void debug_log_current_tree_structure(LayoutSolver* solver, TreeNode* current_node) {
    if (!solver->tree_debug_file || !current_node) return;

    fprintf(solver->tree_debug_file, "\n🌲 CURRENT TREE STRUCTURE (path to current node):\n");
    fprintf(solver->tree_debug_file, "   ┌─────────────────────────────────────────────┐\n");

    // Build path from root to current node
    TreeNode* path[20];  // Max depth
    int path_length = 0;
    TreeNode* node = current_node;

    // Build path backwards
    while (node && path_length < 20) {
        path[path_length++] = node;
        node = node->parent;
    }

    // Display path from root to current (reverse order)
    for (int i = path_length - 1; i >= 0; i--) {
        TreeNode* n = path[i];

        // Create indent based on depth
        fprintf(solver->tree_debug_file, "   │ ");
        for (int d = 0; d < n->depth; d++) {
            fprintf(solver->tree_debug_file, "  ");
        }

        // Show tree symbols
        if (n->depth == 0) {
            fprintf(solver->tree_debug_file, "🌱 ROOT: %s at (%d,%d)", n->component->name, n->x, n->y);
        } else {
            fprintf(solver->tree_debug_file, "├─ %s at (%d,%d)", n->component->name, n->x, n->y);
        }

        // Mark current node
        if (n == current_node) {
            fprintf(solver->tree_debug_file, " ← CURRENT");
        }

        fprintf(solver->tree_debug_file, "\n");
    }

    // Show tree stats
    fprintf(solver->tree_debug_file, "   │\n");
    fprintf(solver->tree_debug_file, "   │ Tree Stats: Depth %d, Total Nodes %d\n",
            current_node->depth, solver->tree_solver.nodes_created);
    fprintf(solver->tree_debug_file, "   └─────────────────────────────────────────────┘\n");

    fflush(solver->tree_debug_file);
}