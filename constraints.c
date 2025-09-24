#include "constraints.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <math.h>

// =============================================================================
// CONSTRAINT SYSTEM - ALL CONSTRAINT IMPLEMENTATIONS
// =============================================================================
// This file contains all constraint type implementations in one place for
// easy maintenance and extension. The solver calls the dispatch functions
// directly based on constraint type.

// =============================================================================
// CONSTRAINT DISPATCH FUNCTIONS
// =============================================================================
// The solver calls these functions directly, which dispatch to specific implementations

/**
 * @brief Generate placement options for any constraint type
 */
int generate_constraint_placements(struct LayoutSolver* solver, struct DSLConstraint* constraint,
                                  struct Component* unplaced_comp, struct Component* placed_comp,
                                  struct TreePlacementOption* options, int max_options) {
    switch (constraint->type) {
        case DSL_ADJACENT:
            return adjacent_generate_placements(solver, constraint, unplaced_comp, placed_comp, options, max_options);
        // Add new constraint types here
        default:
            printf("❌ Unknown constraint type %d\n", constraint->type);
            return 0;
    }
}

/**
 * @brief Calculate preference score for any constraint type
 */
int calculate_constraint_score(struct LayoutSolver* solver, struct Component* comp,
                              int x, int y, struct DSLConstraint* constraint,
                              struct Component* placed_comp) {
    switch (constraint->type) {
        case DSL_ADJACENT:
            return adjacent_calculate_score(solver, comp, x, y, constraint, placed_comp);
        // Add new constraint types here
        default:
            return 0;
    }
}

/**
 * @brief Validate any constraint type
 */
int validate_constraint(struct LayoutSolver* solver, struct DSLConstraint* constraint) {
    switch (constraint->type) {
        case DSL_ADJACENT:
            return adjacent_validate_constraint(solver, constraint);
        // Add new constraint types here
        default:
            printf("❌ Unknown constraint type %d for validation\n", constraint->type);
            return 0;
    }
}

// =============================================================================
// ADJACENT CONSTRAINT IMPLEMENTATION
// =============================================================================
// Requires two components to be placed adjacent to each other in a specific
// direction: 'n' (north), 's' (south), 'e' (east), 'w' (west), 'a' (any)

/**
 * @brief Generate placement options for ADJACENT constraint
 *
 * Generates all possible adjacent placements for the unplaced component
 * relative to the placed component, based on the constraint direction.
 * Each option includes position coordinates and conflict detection.
 */
int adjacent_generate_placements(struct LayoutSolver* solver, struct DSLConstraint* constraint,
                                struct Component* unplaced_comp, struct Component* placed_comp,
                                struct TreePlacementOption* options, int max_options) {
    int option_count = 0;

    Direction dir = constraint->direction;
    int base_x = placed_comp->placed_x;
    int base_y = placed_comp->placed_y;
    int base_w = placed_comp->width;
    int base_h = placed_comp->height;

    // Generate placement options based on direction
    if (dir == 'n' || dir == 'a') {
        // North - place above the placed component
        int target_y = base_y - unplaced_comp->height;

        // Try different x positions for edge alignment
        for (int offset = -unplaced_comp->width + 1; offset < base_w; offset++) {
            if (option_count >= max_options) break;

            int target_x = base_x + offset;
            struct TreePlacementOption* opt = &options[option_count++];

            opt->x = target_x;
            opt->y = target_y;
            opt->preference_score = adjacent_calculate_score(solver, unplaced_comp,
                                                           target_x, target_y, constraint, placed_comp);

            // Check for conflicts (using existing function signature)
            detect_placement_conflicts_detailed(solver, unplaced_comp, target_x, target_y, &opt->conflicts);
            opt->has_conflict = (opt->conflicts.conflict_count > 0);
        }
    }

    if (dir == 's' || dir == 'a') {
        // South - place below the placed component
        int target_y = base_y + base_h;

        for (int offset = -unplaced_comp->width + 1; offset < base_w; offset++) {
            if (option_count >= max_options) break;

            int target_x = base_x + offset;
            struct TreePlacementOption* opt = &options[option_count++];

            opt->x = target_x;
            opt->y = target_y;
            opt->preference_score = adjacent_calculate_score(solver, unplaced_comp,
                                                           target_x, target_y, constraint, placed_comp);

            detect_placement_conflicts_detailed(solver, unplaced_comp, target_x, target_y, &opt->conflicts);
            opt->has_conflict = (opt->conflicts.conflict_count > 0);
        }
    }

    if (dir == 'e' || dir == 'a') {
        // East - place to the right of the placed component
        int target_x = base_x + base_w;

        for (int offset = -unplaced_comp->height + 1; offset < base_h; offset++) {
            if (option_count >= max_options) break;

            int target_y = base_y + offset;
            struct TreePlacementOption* opt = &options[option_count++];

            opt->x = target_x;
            opt->y = target_y;
            opt->preference_score = adjacent_calculate_score(solver, unplaced_comp,
                                                           target_x, target_y, constraint, placed_comp);

            detect_placement_conflicts_detailed(solver, unplaced_comp, target_x, target_y, &opt->conflicts);
            opt->has_conflict = (opt->conflicts.conflict_count > 0);
        }
    }

    if (dir == 'w' || dir == 'a') {
        // West - place to the left of the placed component
        int target_x = base_x - unplaced_comp->width;

        for (int offset = -unplaced_comp->height + 1; offset < base_h; offset++) {
            if (option_count >= max_options) break;

            int target_y = base_y + offset;
            struct TreePlacementOption* opt = &options[option_count++];

            opt->x = target_x;
            opt->y = target_y;
            opt->preference_score = adjacent_calculate_score(solver, unplaced_comp,
                                                           target_x, target_y, constraint, placed_comp);

            detect_placement_conflicts_detailed(solver, unplaced_comp, target_x, target_y, &opt->conflicts);
            opt->has_conflict = (opt->conflicts.conflict_count > 0);
        }
    }

    return option_count;
}

/**
 * @brief Calculate preference score for ADJACENT constraint placement
 *
 * Assigns scores based on alignment preferences:
 * - Edge alignment: +10 points for perfect edge alignment
 * - Center alignment: +15 points for centered placement
 * Higher scores indicate more preferred placements.
 */
int adjacent_calculate_score(struct LayoutSolver* solver, struct Component* comp,
                            int x, int y, struct DSLConstraint* constraint,
                            struct Component* placed_comp) {
    int score = 0;

    if (constraint->direction == 'n' || constraint->direction == 's') {
        // North/South placement - horizontal alignment matters
        int comp_left = x;
        int comp_right = x + comp->width;
        int ref_left = placed_comp->placed_x;
        int ref_right = placed_comp->placed_x + placed_comp->width;

        // 1. Edge alignment (highest priority - 100 points)
        if (comp_left == ref_left) {
            score = 100; // Left edge aligned
        } else if (comp_right == ref_right) {
            score = 100; // Right edge aligned
        }
        // 2. Centered (if both have same parity and possible - 90 points)
        else if ((comp->width % 2) == (placed_comp->width % 2)) {
            int comp_center = comp_left + comp->width / 2;
            int ref_center = ref_left + placed_comp->width / 2;
            if (comp_center == ref_center) {
                score = 90; // Perfect center
            }
        }

        // 3. If not edge-aligned or centered, score by overlap amount
        if (score == 0) {
            int overlap_start = (comp_left > ref_left) ? comp_left : ref_left;
            int overlap_end = (comp_right < ref_right) ? comp_right : ref_right;
            int overlap = (overlap_end > overlap_start) ? (overlap_end - overlap_start) : 0;

            if (overlap > 0) {
                // Overlapping: prioritize "hugging the wall" (least centered first)
                // Calculate distance from either edge (closer to edge = higher score)
                int dist_from_left = abs(comp_left - ref_left);
                int dist_from_right = abs(comp_right - ref_right);
                int min_edge_distance = (dist_from_left < dist_from_right) ? dist_from_left : dist_from_right;

                // Base score for overlap, then bonus for hugging edges
                score = 50 + overlap * 2 + (10 - min_edge_distance);
                if (score > 89) score = 89; // Cap to keep below centered
            } else {
                // Non-overlapping: closer = higher score (1-49 points)
                int gap = (comp_left > ref_right) ? (comp_left - ref_right) : (ref_left - comp_right);
                score = 49 - gap;
                if (score < 1) score = 1;
            }
        }
    }

    if (constraint->direction == 'e' || constraint->direction == 'w') {
        // East/West placement - vertical alignment matters
        int comp_top = y;
        int comp_bottom = y + comp->height;
        int ref_top = placed_comp->placed_y;
        int ref_bottom = placed_comp->placed_y + placed_comp->height;

        // 1. Edge alignment (highest priority - 100 points)
        if (comp_top == ref_top) {
            score = 100; // Top edge aligned
        } else if (comp_bottom == ref_bottom) {
            score = 100; // Bottom edge aligned
        }
        // 2. Centered (if both have same parity and possible - 90 points)
        else if ((comp->height % 2) == (placed_comp->height % 2)) {
            int comp_center = comp_top + comp->height / 2;
            int ref_center = ref_top + placed_comp->height / 2;
            if (comp_center == ref_center) {
                score = 90; // Perfect center
            }
        }

        // 3. If not edge-aligned or centered, score by overlap amount
        if (score == 0) {
            int overlap_start = (comp_top > ref_top) ? comp_top : ref_top;
            int overlap_end = (comp_bottom < ref_bottom) ? comp_bottom : ref_bottom;
            int overlap = (overlap_end > overlap_start) ? (overlap_end - overlap_start) : 0;

            if (overlap > 0) {
                // Overlapping: prioritize "hugging the wall" (least centered first)
                // Calculate distance from either edge (closer to edge = higher score)
                int dist_from_top = abs(comp_top - ref_top);
                int dist_from_bottom = abs(comp_bottom - ref_bottom);
                int min_edge_distance = (dist_from_top < dist_from_bottom) ? dist_from_top : dist_from_bottom;

                // Base score for overlap, then bonus for hugging edges
                score = 50 + overlap * 2 + (10 - min_edge_distance);
                if (score > 89) score = 89; // Cap to keep below centered
            } else {
                // Non-overlapping: closer = higher score (1-49 points)
                int gap = (comp_top > ref_bottom) ? (comp_top - ref_bottom) : (ref_top - comp_bottom);
                score = 49 - gap;
                if (score < 1) score = 1;
            }
        }
    }

    return score;
}

/**
 * @brief Validate that an ADJACENT constraint is satisfied
 *
 * Checks whether two components are actually adjacent to each other
 * in the specified direction with proper edge contact.
 */
int adjacent_validate_constraint(struct LayoutSolver* solver, struct DSLConstraint* constraint) {
    // Find both components
    struct Component* comp_a = NULL;
    struct Component* comp_b = NULL;

    for (int i = 0; i < solver->component_count; i++) {
        if (strcmp(solver->components[i].name, constraint->component_a) == 0) {
            comp_a = &solver->components[i];
        }
        if (strcmp(solver->components[i].name, constraint->component_b) == 0) {
            comp_b = &solver->components[i];
        }
    }

    if (!comp_a || !comp_b || !comp_a->is_placed || !comp_b->is_placed) {
        return 0; // Components not found or not placed
    }

    // Check adjacency based on constraint direction
    Direction dir = constraint->direction;

    // Component A bounds
    int a_x1 = comp_a->placed_x;
    int a_y1 = comp_a->placed_y;
    int a_x2 = comp_a->placed_x + comp_a->width;
    int a_y2 = comp_a->placed_y + comp_a->height;

    // Component B bounds
    int b_x1 = comp_b->placed_x;
    int b_y1 = comp_b->placed_y;
    int b_x2 = comp_b->placed_x + comp_b->width;
    int b_y2 = comp_b->placed_y + comp_b->height;

    if (dir == 'n' || dir == 'a') {
        // Check if A is north of B (A above B)
        if (a_y2 == b_y1 && a_x1 < b_x2 && a_x2 > b_x1) {
            return 1; // A touches B from above
        }
        // Check if B is north of A (B above A)
        if (b_y2 == a_y1 && b_x1 < a_x2 && b_x2 > a_x1) {
            return 1; // B touches A from above
        }
    }

    if (dir == 's' || dir == 'a') {
        // Check if A is south of B (A below B)
        if (a_y1 == b_y2 && a_x1 < b_x2 && a_x2 > b_x1) {
            return 1; // A touches B from below
        }
        // Check if B is south of A (B below A)
        if (b_y1 == a_y2 && b_x1 < a_x2 && b_x2 > a_x1) {
            return 1; // B touches A from below
        }
    }

    if (dir == 'e' || dir == 'a') {
        // Check if A is east of B (A right of B)
        if (a_x1 == b_x2 && a_y1 < b_y2 && a_y2 > b_y1) {
            return 1; // A touches B from right
        }
        // Check if B is east of A (B right of A)
        if (b_x1 == a_x2 && b_y1 < a_y2 && b_y2 > a_y1) {
            return 1; // B touches A from right
        }
    }

    if (dir == 'w' || dir == 'a') {
        // Check if A is west of B (A left of B)
        if (a_x2 == b_x1 && a_y1 < b_y2 && a_y2 > b_y1) {
            return 1; // A touches B from left
        }
        // Check if B is west of A (B left of A)
        if (b_x2 == a_x1 && b_y1 < a_y2 && b_y2 > a_y1) {
            return 1; // B touches A from left
        }
    }

    return 0; // Not adjacent in the specified direction
}

// =============================================================================
// CONSTRAINT HELPER FUNCTIONS
// =============================================================================
// These functions are shared across constraint types for common operations
// like adjacency checking and overlap detection.

/**
 * @brief Check if two components are adjacent in a specific direction
 *
 * Validates that two rectangular components are adjacent according to
 * the specified direction constraint.
 */
int check_adjacent(int x1, int y1, int w1, int h1, int x2, int y2, int w2, int h2, char dir) {
    switch (dir) {
    case 'n': // Component 1 is north of component 2
        // Component 1's bottom edge should touch component 2's top edge
        if (y1 + h1 == y2) {
            // Check horizontal overlap
            return (x1 < x2 + w2) && (x1 + w1 > x2);
        }
        return 0;

    case 's': // Component 1 is south of component 2
        // Component 1's top edge should touch component 2's bottom edge
        if (y1 == y2 + h2) {
            // Check horizontal overlap
            return (x1 < x2 + w2) && (x1 + w1 > x2);
        }
        return 0;

    case 'e': // Component 1 is east of component 2
        // Component 1's left edge should touch component 2's right edge
        if (x1 == x2 + w2) {
            // Check vertical overlap
            return (y1 < y2 + h2) && (y1 + h1 > y2);
        }
        return 0;

    case 'w': // Component 1 is west of component 2
        // Component 1's right edge should touch component 2's left edge
        if (x1 + w1 == x2) {
            // Check vertical overlap
            return (y1 < y2 + h2) && (y1 + h1 > y2);
        }
        return 0;

    case 'a': // Any direction - check all four directions
        return check_adjacent(x1, y1, w1, h1, x2, y2, w2, h2, 'n') ||
               check_adjacent(x1, y1, w1, h1, x2, y2, w2, h2, 's') ||
               check_adjacent(x1, y1, w1, h1, x2, y2, w2, h2, 'e') ||
               check_adjacent(x1, y1, w1, h1, x2, y2, w2, h2, 'w');

    default:
        return 0;
    }
}

/**
 * @brief Check if a constraint is satisfied between two components
 *
 * Tests whether two components satisfy the specified constraint at the given
 * test coordinates for the first component.
 */
int check_constraint_satisfied(struct LayoutSolver* solver, struct DSLConstraint* constraint,
                              struct Component* comp1, struct Component* comp2, int test_x, int test_y) {
    if (!constraint || !comp1 || !comp2)
        return 0;
    if (!comp1->is_placed || !comp2->is_placed)
        return 0;

    // Determine which component is which in the constraint
    int comp1_is_a = (strcmp(constraint->component_a, comp1->name) == 0);

    if (constraint->type == DSL_ADJACENT) {
        if (comp1_is_a) {
            return check_adjacent(test_x, test_y, comp1->width, comp1->height,
                                comp2->placed_x, comp2->placed_y, comp2->width,
                                comp2->height, constraint->direction);
        } else {
            return check_adjacent(comp2->placed_x, comp2->placed_y, comp2->width,
                                comp2->height, test_x, test_y, comp1->width,
                                comp1->height, constraint->direction);
        }
    }

    return 0; // Unknown constraint type
}

/**
 * @brief Check for character-level overlap between two components
 *
 * Examines the ASCII tile data of both components to detect if any non-space
 * characters would occupy the same grid position when placed at the specified
 * coordinates.
 */
int has_character_overlap(struct LayoutSolver* solver, struct Component* comp1, int x1, int y1,
                         struct Component* comp2, int x2, int y2) {
    // Check each non-space character in comp1
    for (int row1 = 0; row1 < comp1->height; row1++) {
        for (int col1 = 0; col1 < comp1->width; col1++) {
            if (comp1->ascii_tile[row1][col1] != ' ') {
                int world_x1 = x1 + col1;
                int world_y1 = y1 + row1;

                // Check if this position overlaps with any non-space character in comp2
                for (int row2 = 0; row2 < comp2->height; row2++) {
                    for (int col2 = 0; col2 < comp2->width; col2++) {
                        if (comp2->ascii_tile[row2][col2] != ' ') {
                            int world_x2 = x2 + col2;
                            int world_y2 = y2 + row2;

                            if (world_x1 == world_x2 && world_y1 == world_y2) {
                                return 1; // Overlap detected
                            }
                        }
                    }
                }
            }
        }
    }
    return 0; // No overlap
}

// =============================================================================
// FUTURE CONSTRAINT IMPLEMENTATIONS
// =============================================================================
// Add new constraint types here. Each constraint needs three functions:
// 1. [type]_generate_placements() - Generate placement options
// 2. [type]_calculate_score() - Calculate preference scores
// 3. [type]_validate_constraint() - Validate constraint satisfaction
//
// Example for ALIGNED constraint:
// int aligned_generate_placements(...) { /* implementation */ }
// int aligned_calculate_score(...) { /* implementation */ }
// int aligned_validate_constraint(...) { /* implementation */ }
//
// Then add to constraint_handlers array in constraint_types.c