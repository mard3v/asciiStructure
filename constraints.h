#ifndef CONSTRAINTS_H
#define CONSTRAINTS_H

#include "constraint_solver.h"

// =============================================================================
// DIRECT CONSTRAINT INTERFACE
// =============================================================================
// The solver calls these functions directly based on constraint type

/**
 * @brief Generate placement options for a constraint
 * @param solver The layout solver instance
 * @param constraint The constraint being processed
 * @param unplaced_comp The component to be placed
 * @param placed_comp The already placed component this constraint references
 * @param options Array to store generated placement options
 * @param max_options Maximum number of options to generate
 * @return Number of options generated
 */
int generate_constraint_placements(struct LayoutSolver* solver, struct DSLConstraint* constraint,
                                  struct Component* unplaced_comp, struct Component* placed_comp,
                                  struct TreePlacementOption* options, int max_options);

/**
 * @brief Calculate placement preference score for a constraint
 * @param solver The layout solver instance
 * @param comp The component being placed
 * @param x X coordinate of placement
 * @param y Y coordinate of placement
 * @param constraint The constraint context
 * @param placed_comp The reference component (already placed)
 * @return Preference score (higher = more preferred)
 */
int calculate_constraint_score(struct LayoutSolver* solver, struct Component* comp,
                              int x, int y, struct DSLConstraint* constraint,
                              struct Component* placed_comp);

/**
 * @brief Validate that a constraint is satisfied
 * @param solver The layout solver instance
 * @param constraint The constraint to validate
 * @return 1 if satisfied, 0 if not
 */
int validate_constraint(struct LayoutSolver* solver, struct DSLConstraint* constraint);

// =============================================================================
// CONSTRAINT IMPLEMENTATION FUNCTIONS
// =============================================================================

// ADJACENT constraint functions
int adjacent_generate_placements(struct LayoutSolver* solver, struct DSLConstraint* constraint,
                                struct Component* unplaced_comp, struct Component* placed_comp,
                                struct TreePlacementOption* options, int max_options);

int adjacent_calculate_score(struct LayoutSolver* solver, struct Component* comp,
                            int x, int y, struct DSLConstraint* constraint,
                            struct Component* placed_comp);

int adjacent_validate_constraint(struct LayoutSolver* solver, struct DSLConstraint* constraint);

// =============================================================================
// SHARED CONSTRAINT HELPER FUNCTIONS
// =============================================================================

int check_adjacent(int x1, int y1, int w1, int h1, int x2, int y2, int w2, int h2, char dir);
int check_constraint_satisfied(struct LayoutSolver* solver, struct DSLConstraint* constraint,
                              struct Component* comp1, struct Component* comp2, int test_x, int test_y);
int has_character_overlap(struct LayoutSolver* solver, struct Component* comp1, int x1, int y1,
                         struct Component* comp2, int x2, int y2);

#endif // CONSTRAINTS_H