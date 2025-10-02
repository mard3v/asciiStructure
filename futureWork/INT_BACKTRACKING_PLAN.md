# Intelligent Conflict-Driven Backtracking Implementation Plan

**Status:** In Progress
**Last Updated:** 2025-09-30
**Estimated Completion:** ~3 hours

---

## Overview

This document details the implementation of intelligent conflict-driven backtracking for the ASCII Structure System constraint solver. The goal is to make the solver smarter while maintaining completeness guarantees.

### Current Problem
When the solver cannot place a room due to conflicts, it gives up or uses inefficient chronological backtracking without understanding WHY placement failed.

### Solution Architecture
**Two-Tier Backtracking System:**
1. **Intelligent Tier:** Analyze conflicts → identify blocking rooms → strategically reposition them
2. **Exhaustive Tier:** Standard DFS backtracking with state tracking (completeness guarantee)

---

## Implementation Phases

### Phase 1: Data Structure Updates ⏳ NOT STARTED
**Files to modify:** `constraint_solver.h`
**Estimated time:** 20 minutes

#### Changes to TreeNode struct:
```c
typedef struct TreeNode {
    // ... existing fields ...

    // NEW: Track which placement options have been tried
    bool options_tried[200];              // Bitmap of attempted options

    // NEW: Constraint management
    Component* forbidden_component;        // Component tied to current constraint (can't reposition)

    // NEW: Forbidden zone for meta room (room we're trying to place)
    int forbidden_zone_x;                  // X coordinate of forbidden zone
    int forbidden_zone_y;                  // Y coordinate
    int forbidden_zone_width;              // Width of zone
    int forbidden_zone_height;             // Height of zone
    bool has_forbidden_zone;               // Whether forbidden zone is active
} TreeNode;
```

#### Changes to TreeSolver struct:
```c
typedef struct TreeSolver {
    // ... existing fields ...

    // NEW: Meta room tracking
    Component* meta_room;                  // Current room trying to place
    int meta_room_attempt_count;           // Number of positions tried for meta room
} TreeSolver;
```

#### New struct for backtrack targets:
```c
typedef struct BacktrackTarget {
    TreeNode* node;                        // Node to reposition
    TreePlacementOption alternatives[200]; // Non-conflicting placement alternatives
    int alternative_count;                 // Number of alternatives
    int depth;                             // Depth of this node (for sorting)
} BacktrackTarget;
```

**Validation:**
- Compile successfully
- No breaking changes to existing code

---

### Phase 2: Track Tried States ⏳ NOT STARTED
**Files to modify:** `constraint_solver.c` (function: `advance_to_next_constraint`)
**Estimated time:** 15 minutes

#### Modify placement loop (around line 1460):
```c
// Try each option in order
for (int i = 0; i < option_count; i++) {
    TreePlacementOption *option = &options[i];

    // NEW: Check if already tried this option
    if (ts->current_node->options_tried[i]) {
        printf("⏭️  Skipping option %d (already tried)\n", i + 1);
        continue;
    }

    // NEW: Mark this option as tried
    ts->current_node->options_tried[i] = true;

    // ... rest of existing placement logic ...
}
```

#### Initialize options_tried in create_tree_node:
```c
TreeNode* create_tree_node(...) {
    TreeNode* node = malloc(sizeof(TreeNode));
    // ... existing initialization ...

    // NEW: Initialize tried states
    memset(node->options_tried, 0, sizeof(node->options_tried));
    node->forbidden_component = NULL;
    node->has_forbidden_zone = false;

    return node;
}
```

**Validation:**
- Existing tests still pass
- No option is tried twice (check debug logs)

---

### Phase 3: Fix Component Depth Tracking ⏳ NOT STARTED
**Files to modify:** `constraint_solver.c`
**Estimated time:** 15 minutes

#### Add helper function to find component's tree node:
```c
/**
 * @brief Find the tree node where a component was placed
 * @return TreeNode* or NULL if not found
 */
TreeNode* find_component_tree_node(TreeNode* root, Component* comp) {
    if (!root) return NULL;
    if (root->component == comp) return root;

    // Search children recursively
    for (int i = 0; i < root->child_count; i++) {
        TreeNode* result = find_component_tree_node(root->children[i], comp);
        if (result) return result;
    }

    return NULL;
}
```

#### Update `detect_placement_conflicts_detailed` (around line 1614):
```c
// OLD: conflicts->conflict_depths[conflicts->conflict_count] = 0; // TODO: Track actual depth

// NEW:
TreeNode* conflicting_node = find_component_tree_node(solver->tree_solver.root, other);
conflicts->conflict_depths[conflicts->conflict_count] =
    conflicting_node ? conflicting_node->depth : 0;
```

**Validation:**
- Debug logs show correct depths for conflicting components
- Depths increase as we go deeper in tree

---

### Phase 4: Intelligent Conflict Analysis ⏳ NOT STARTED
**Files to modify:** `constraint_solver.c`
**Estimated time:** 30 minutes

#### Implement conflict analysis function:
```c
/**
 * @brief Analyze failed placements and build list of strategic backtrack targets
 *
 * @param solver Layout solver
 * @param failed_options All placement options that failed
 * @param option_count Number of failed options
 * @param meta_room The room we're trying to place
 * @param forbidden_component Component tied to current constraint (can't reposition)
 * @param targets Output array of backtrack targets
 * @return Number of targets found
 */
int analyze_conflicts_and_build_targets(
    LayoutSolver* solver,
    TreePlacementOption* failed_options,
    int option_count,
    Component* meta_room,
    Component* forbidden_component,
    BacktrackTarget* targets
) {
    // Step 1: Collect all blocking components across all failed options
    int blocking_components[MAX_COMPONENTS];
    int blocking_count = 0;

    for (int i = 0; i < option_count; i++) {
        if (!failed_options[i].has_conflict) continue;

        ConflictInfo* conflicts = &failed_options[i].conflicts;
        for (int j = 0; j < conflicts->conflict_count; j++) {
            int comp_index = conflicts->conflicting_components[j];

            // Check if already in list
            bool already_added = false;
            for (int k = 0; k < blocking_count; k++) {
                if (blocking_components[k] == comp_index) {
                    already_added = true;
                    break;
                }
            }

            if (!already_added) {
                blocking_components[blocking_count++] = comp_index;
            }
        }
    }

    printf("🔍 Found %d unique blocking components\n", blocking_count);

    // Step 2: For each blocking component, find its tree node and build target
    int target_count = 0;
    for (int i = 0; i < blocking_count; i++) {
        Component* blocking_comp = &solver->components[blocking_components[i]];

        // Skip if this is the forbidden component
        if (blocking_comp == forbidden_component) {
            printf("  ⛔ Skipping %s (constraint-tied component)\n", blocking_comp->name);
            continue;
        }

        // Find tree node for this component
        TreeNode* node = find_component_tree_node(solver->tree_solver.root, blocking_comp);
        if (!node) continue;

        // Find alternative placements that don't conflict with meta room's preferred positions
        BacktrackTarget* target = &targets[target_count];
        target->node = node;
        target->depth = node->depth;
        target->alternative_count = 0;

        // Calculate forbidden zone from meta room's attempted positions
        // (Use bounding box of all attempted positions for meta room)
        int min_x = 9999, min_y = 9999, max_x = -9999, max_y = -9999;
        for (int j = 0; j < option_count; j++) {
            int x = failed_options[j].x;
            int y = failed_options[j].y;
            if (x < min_x) min_x = x;
            if (y < min_y) min_y = y;
            if (x + meta_room->width > max_x) max_x = x + meta_room->width;
            if (y + meta_room->height > max_y) max_y = y + meta_room->height;
        }

        // Generate alternatives for this blocking component
        // Use same constraint logic as original placement
        DSLConstraint* original_constraint = node->constraint;
        TreePlacementOption temp_options[200];
        int temp_count = generate_placement_options_for_constraint(
            solver, original_constraint, blocking_comp, temp_options
        );

        // Filter alternatives: only keep those that don't overlap forbidden zone
        for (int j = 0; j < temp_count; j++) {
            int alt_x = temp_options[j].x;
            int alt_y = temp_options[j].y;

            // Check if this alternative overlaps forbidden zone
            bool overlaps = !(alt_x + blocking_comp->width <= min_x ||
                            alt_x >= max_x ||
                            alt_y + blocking_comp->height <= min_y ||
                            alt_y >= max_y);

            if (!overlaps && !node->options_tried[j]) {
                target->alternatives[target->alternative_count++] = temp_options[j];
            }
        }

        if (target->alternative_count > 0) {
            printf("  ✅ %s: %d alternative placements found\n",
                   blocking_comp->name, target->alternative_count);
            target_count++;
        } else {
            printf("  ❌ %s: no valid alternatives\n", blocking_comp->name);
        }
    }

    // Step 3: Sort targets by depth (deepest first)
    for (int i = 0; i < target_count - 1; i++) {
        for (int j = i + 1; j < target_count; j++) {
            if (targets[j].depth > targets[i].depth) {
                BacktrackTarget temp = targets[i];
                targets[i] = targets[j];
                targets[j] = temp;
            }
        }
    }

    printf("🎯 Built %d backtrack targets (sorted by depth)\n", target_count);
    return target_count;
}
```

**Validation:**
- Function compiles and links
- Returns sensible target list for palace.txt conflict scenario
- Forbidden component correctly filtered out

---

### Phase 5: Forward Trace Implementation ⏳ NOT STARTED
**Files to modify:** `constraint_solver.c`
**Estimated time:** 30 minutes

#### Implement forward trace from repositioned node:
```c
/**
 * @brief Trace forward from a node, replaying placements with forbidden zone
 *
 * @param solver Layout solver
 * @param start_node Node to trace forward from
 * @param forbidden_zone_x Forbidden zone X coordinate
 * @param forbidden_zone_y Forbidden zone Y coordinate
 * @param forbidden_zone_w Forbidden zone width
 * @param forbidden_zone_h Forbidden zone height
 * @param forbidden_component Component we cannot reposition
 * @return 1 if successful, 0 if failed
 */
int forward_trace_with_forbidden_zone(
    LayoutSolver* solver,
    TreeNode* start_node,
    int forbidden_zone_x,
    int forbidden_zone_y,
    int forbidden_zone_w,
    int forbidden_zone_h,
    Component* forbidden_component
) {
    printf("🔄 Forward tracing from %s with forbidden zone (%d,%d,%d,%d)\n",
           start_node->component->name, forbidden_zone_x, forbidden_zone_y,
           forbidden_zone_w, forbidden_zone_h);

    // Recursively process each child
    for (int i = 0; i < start_node->child_count; i++) {
        TreeNode* child = start_node->children[i];
        Component* child_comp = child->component;

        // Check if this is the forbidden component
        if (child_comp == forbidden_component) {
            printf("  ⛔ Encountered forbidden component %s - terminating branch\n",
                   child_comp->name);
            return 0;
        }

        // Check if current placement conflicts with forbidden zone
        bool conflicts_with_zone = !(
            child->x + child_comp->width <= forbidden_zone_x ||
            child->x >= forbidden_zone_x + forbidden_zone_w ||
            child->y + child_comp->height <= forbidden_zone_y ||
            child->y >= forbidden_zone_y + forbidden_zone_h
        );

        if (conflicts_with_zone) {
            printf("  🔀 %s conflicts with forbidden zone - repositioning\n",
                   child_comp->name);

            // Generate alternative placements for this child
            TreePlacementOption options[200];
            int option_count = generate_placement_options_for_constraint(
                solver, child->constraint, child_comp, options
            );

            // Try to find placement that avoids forbidden zone and hasn't been tried
            bool found_placement = false;
            for (int j = 0; j < option_count; j++) {
                if (child->options_tried[j]) continue; // Already tried this

                int new_x = options[j].x;
                int new_y = options[j].y;

                // Check if avoids forbidden zone
                bool avoids_zone = (
                    new_x + child_comp->width <= forbidden_zone_x ||
                    new_x >= forbidden_zone_x + forbidden_zone_w ||
                    new_y + child_comp->height <= forbidden_zone_y ||
                    new_y >= forbidden_zone_y + forbidden_zone_h
                );

                if (avoids_zone && !options[j].has_conflict) {
                    // Try this placement
                    remove_component(solver, child_comp);
                    child->x = new_x;
                    child->y = new_y;
                    child->options_tried[j] = true;

                    if (tree_place_component(solver, child)) {
                        printf("    ✅ Repositioned %s to (%d,%d)\n",
                               child_comp->name, new_x, new_y);
                        found_placement = true;
                        break;
                    }
                }
            }

            if (!found_placement) {
                printf("    ❌ Could not reposition %s - branch failed\n",
                       child_comp->name);
                return 0;
            }
        }

        // Recursively trace this child's subtree
        if (!forward_trace_with_forbidden_zone(
                solver, child, forbidden_zone_x, forbidden_zone_y,
                forbidden_zone_w, forbidden_zone_h, forbidden_component)) {
            return 0;
        }
    }

    return 1; // Success
}
```

**Validation:**
- Can successfully reposition child components
- Correctly terminates when hitting forbidden component
- Respects options_tried to avoid retrying failed states

---

### Phase 6: Intelligent Backtracking Main Logic ⏳ NOT STARTED
**Files to modify:** `constraint_solver.c`
**Estimated time:** 40 minutes

#### Implement main intelligent backtrack function:
```c
/**
 * @brief Try intelligent conflict-driven backtracking
 *
 * @param solver Layout solver
 * @param failed_options All placement options that failed for meta room
 * @param option_count Number of failed options
 * @param meta_room Room we're trying to place
 * @param forbidden_component Component tied to current constraint
 * @return 1 if successful, 0 if all intelligent attempts failed
 */
int try_intelligent_backtrack(
    LayoutSolver* solver,
    TreePlacementOption* failed_options,
    int option_count,
    Component* meta_room,
    Component* forbidden_component
) {
    printf("\n🧠 INTELLIGENT BACKTRACKING INITIATED\n");
    printf("   Meta room: %s\n", meta_room->name);
    printf("   Forbidden component: %s\n",
           forbidden_component ? forbidden_component->name : "none");

    // Build list of strategic backtrack targets
    BacktrackTarget targets[MAX_COMPONENTS];
    int target_count = analyze_conflicts_and_build_targets(
        solver, failed_options, option_count, meta_room,
        forbidden_component, targets
    );

    if (target_count == 0) {
        printf("   ❌ No valid backtrack targets found\n");
        return 0;
    }

    // Calculate forbidden zone for meta room
    int zone_min_x = 9999, zone_min_y = 9999;
    int zone_max_x = -9999, zone_max_y = -9999;
    for (int i = 0; i < option_count; i++) {
        int x = failed_options[i].x;
        int y = failed_options[i].y;
        if (x < zone_min_x) zone_min_x = x;
        if (y < zone_min_y) zone_min_y = y;
        if (x + meta_room->width > zone_max_x) zone_max_x = x + meta_room->width;
        if (y + meta_room->height > zone_max_y) zone_max_y = y + meta_room->height;
    }
    int zone_w = zone_max_x - zone_min_x;
    int zone_h = zone_max_y - zone_min_y;

    // Try each target in priority order
    for (int t = 0; t < target_count; t++) {
        BacktrackTarget* target = &targets[t];
        printf("\n🎯 Target %d/%d: %s (depth %d) - %d alternatives\n",
               t + 1, target_count, target->node->component->name,
               target->depth, target->alternative_count);

        // Save current grid state (rebuild approach - simpler)
        // We'll rebuild from root after each attempt

        // Try each alternative placement for this target
        for (int a = 0; a < target->alternative_count; a++) {
            TreePlacementOption* alt = &target->alternatives[a];
            printf("   🔄 Alternative %d/%d: (%d,%d) score=%d\n",
                   a + 1, target->alternative_count, alt->x, alt->y,
                   alt->preference_score);

            // Rebuild grid from root to target's parent
            rebuild_grid_to_node(solver, target->node->parent);

            // Apply alternative placement to target node
            target->node->x = alt->x;
            target->node->y = alt->y;

            if (!tree_place_component(solver, target->node)) {
                printf("      ❌ Could not place at alternative position\n");
                continue;
            }

            printf("      ✅ Placed at alternative position\n");

            // Forward trace from this node with forbidden zone
            if (!forward_trace_with_forbidden_zone(
                    solver, target->node, zone_min_x, zone_min_y,
                    zone_w, zone_h, forbidden_component)) {
                printf("      ❌ Forward trace failed\n");
                continue;
            }

            printf("      ✅ Forward trace successful\n");

            // Try to place meta room now
            for (int m = 0; m < option_count; m++) {
                if (failed_options[m].has_conflict) continue; // Was conflicting before

                int mx = failed_options[m].x;
                int my = failed_options[m].y;

                if (is_placement_valid(solver, meta_room, mx, my)) {
                    place_component(solver, meta_room, mx, my);
                    printf("   🎉 SUCCESS! Meta room placed at (%d,%d)\n", mx, my);
                    solver->tree_solver.conflict_backtracks++;
                    return 1;
                }
            }

            printf("      ❌ Still cannot place meta room\n");
        }
    }

    printf("\n❌ All intelligent backtrack attempts failed\n");
    return 0;
}
```

#### Implement grid rebuild helper:
```c
/**
 * @brief Rebuild grid from root to specified node
 */
void rebuild_grid_to_node(LayoutSolver* solver, TreeNode* target) {
    // Clear all components from grid
    for (int i = 0; i < solver->component_count; i++) {
        if (solver->components[i].is_placed) {
            remove_component(solver, &solver->components[i]);
        }
    }

    // Build path from root to target
    TreeNode* path[MAX_BACKTRACK_DEPTH];
    int path_length = 0;
    TreeNode* current = target;

    while (current) {
        path[path_length++] = current;
        current = current->parent;
    }

    // Place components in order (root to target)
    for (int i = path_length - 1; i >= 0; i--) {
        TreeNode* node = path[i];
        place_component(solver, node->component, node->x, node->y);
    }
}
```

**Validation:**
- Function attempts intelligent backtracking on palace.txt
- Debug logs show target selection and alternative attempts
- Grid rebuild works correctly

---

### Phase 7: Integration with Main Solver ⏳ NOT STARTED
**Files to modify:** `constraint_solver.c` (function: `advance_to_next_constraint`)
**Estimated time:** 20 minutes

#### Update the "all options failed" section (around line 1516):
```c
// All options failed - need intelligent backtracking
printf("❌ All placement options failed for constraint\n");

// Determine forbidden component (the one tied to current constraint)
Component *comp_a = find_component(solver, next_constraint->component_a);
Component *comp_b = find_component(solver, next_constraint->component_b);
Component *forbidden_comp = (comp_a && comp_a->is_placed) ? comp_a :
                           (comp_b && comp_b->is_placed) ? comp_b : NULL;

// Try intelligent backtracking
if (try_intelligent_backtrack(solver, options, option_count,
                              unplaced_comp, forbidden_comp)) {
    return 1; // Success!
}

printf("⚠️  Falling back to exhaustive backtracking\n");

// Exhaustive backtracking: return failure, parent will try next option
return 0;
```

**Validation:**
- Solver attempts intelligent backtracking before giving up
- Falls back to exhaustive if intelligent fails
- Existing tests still pass

---

### Phase 8: Enhanced Debug Logging ⏳ NOT STARTED
**Files to modify:** `tree_debug.c`, `tree_debug.h`
**Estimated time:** 10 minutes

#### Add new debug logging functions:
```c
void debug_log_intelligent_backtrack_start(LayoutSolver* solver, int target_count);
void debug_log_backtrack_target(LayoutSolver* solver, BacktrackTarget* target);
void debug_log_forward_trace_start(LayoutSolver* solver, TreeNode* node);
void debug_log_forward_trace_result(LayoutSolver* solver, int success);
void debug_log_meta_room_placement_attempt(LayoutSolver* solver, Component* meta_room, int x, int y, int success);
```

#### Update tree_placement_debug.log format to include:
- Intelligent backtracking section
- Target analysis with depths
- Forward trace progress
- Success/failure of each strategy

**Validation:**
- Debug log is readable and informative
- Can trace through intelligent backtracking logic
- Shows why each attempt succeeded or failed

---

### Phase 9: Testing & Validation ⏳ NOT STARTED
**Estimated time:** 20 minutes

#### Test Case 1: palace.txt (existing test)
**Expected:** Should solve successfully, possibly with fewer nodes than before

#### Test Case 2: Create adversarial test
**File:** `tests/adversarial_castle.txt`
**Content:** Structure designed to require exhaustive backtracking
**Expected:** Still solves (proves completeness)

#### Test Case 3: Constraint-tied component test
**Scenario:** Ensure we don't try to reposition the component tied to current constraint
**Expected:** Forbidden component is never moved during intelligent backtracking

#### Test Case 4: Infinite loop prevention
**Scenario:** Ensure options_tried prevents retrying same states
**Expected:** No state is visited twice (check debug logs)

---

## Progress Tracking

### Completed ✅
- None yet

### In Progress 🚧
- None yet

### Blocked ⛔
- None

---

## Notes & Decisions

### Design Decisions:
1. **Grid rebuild vs. state save/restore:** Chose rebuild approach for simplicity
2. **Forbidden zone calculation:** Use bounding box of all attempted meta room positions
3. **Target sorting:** Prioritize deepest (closest to leaves) blocking components first
4. **Completeness guarantee:** Exhaustive backtracking via parent retry ensures all states explored

### Potential Optimizations (Future):
- Cache grid states instead of rebuilding
- Use more sophisticated forbidden zone shapes (not just bounding box)
- Parallelize alternative attempts
- Add heuristics to prune obviously bad alternatives

---

## File Modification Summary

| File | Lines Changed (Est.) | Status |
|------|---------------------|--------|
| constraint_solver.h | ~50 | ⏳ Not Started |
| constraint_solver.c | ~400 | ⏳ Not Started |
| tree_debug.h | ~10 | ⏳ Not Started |
| tree_debug.c | ~50 | ⏳ Not Started |
| tests/adversarial_castle.txt | New file | ⏳ Not Started |

**Total estimated lines changed:** ~510

---

## Testing Checklist

- [ ] Phase 1: Structs compile successfully
- [ ] Phase 2: Options not retried (check logs)
- [ ] Phase 3: Depths show correctly in logs
- [ ] Phase 4: Conflict analysis returns sensible targets
- [ ] Phase 5: Forward trace can reposition children
- [ ] Phase 6: Intelligent backtrack attempts execute
- [ ] Phase 7: Integration doesn't break existing tests
- [ ] Phase 8: Debug logs are informative
- [ ] Phase 9: All test cases pass
- [ ] palace.txt still solves
- [ ] Adversarial test proves completeness
- [ ] No infinite loops observed

---

## Questions & Issues

### Open Questions:
- None currently

### Known Issues:
- None yet

---

**End of Implementation Plan**
