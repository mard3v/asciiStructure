#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "constraint_solver.h"
#include "llm_integration.h"

// =============================================================================
// MAIN APPLICATION - PARSING AND MENU SYSTEM
// =============================================================================

// Parsing state enumeration
typedef enum {
    SECTION_NONE,
    SECTION_COMPONENTS,
    SECTION_CONSTRAINTS,
    SECTION_TILES
} ParsingSection;

// =============================
// FUNCTION PROTOTYPES
// =============================
int parse_specification_file(const char* filename, LayoutSolver* solver);
int parse_specification_string(const char* specification, LayoutSolver* solver);
void parse_and_solve_specification(const char* specification);
void show_menu(void);

/**
 * @brief Displays the interactive menu with available structure types and options
 */
void show_menu(void) {
    printf("\n🏗️  ASCII Structure System (DSL-Based)\n");
    printf("========================================\n");
    printf("1. Generate Castle\n");
    printf("2. Generate Village\n");
    printf("3. Generate Dungeon\n");
    printf("4. Generate Cathedral\n");
    printf("5. Generate Tower\n");
    printf("6. Load test_castle.txt and solve\n");
    printf("7. Load custom file and solve\n");
    printf("8. Test string parsing (no API needed)\n");
    printf("0. Exit\n");
    printf("========================================\n");
    printf("Select option: ");
}

/**
 * @brief Parses DSL specification from a text file
 * 
 * Loads the entire file content into memory and delegates to string parser.
 * Handles file I/O operations and memory management.
 * 
 * @param filename Path to DSL specification file
 * @param solver   Layout solver instance to populate
 * @return         1 on success, 0 on failure
 */
int parse_specification_file(const char* filename, LayoutSolver* solver) {
    printf("📋 Parsing specification file: %s\n", filename);
    
    FILE* file = fopen(filename, "r");
    if (!file) {
        printf("❌ Cannot open file: %s\n", filename);
        return 0;
    }
    
    // Read entire file into memory
    fseek(file, 0, SEEK_END);
    long file_size = ftell(file);
    fseek(file, 0, SEEK_SET);
    
    char* file_content = malloc(file_size + 1);
    if (!file_content) {
        printf("❌ Memory allocation failed\n");
        fclose(file);
        return 0;
    }
    
    fread(file_content, 1, file_size, file);
    file_content[file_size] = '\0';
    fclose(file);
    
    // Parse the content
    int result = parse_specification_string(file_content, solver);
    free(file_content);
    
    return result;
}

/**
 * @brief Parses DSL specification from string content
 * 
 * Main parsing engine that processes markdown-style DSL format:
 * - ## Components section with component descriptions
 * - ## Constraints section with ADJACENT() statements
 * - ## Component Tiles section with ASCII art in code blocks
 * 
 * @param specification DSL specification string to parse
 * @param solver        Layout solver instance to populate
 * @return              1 on success, 0 on failure
 */
int parse_specification_string(const char* specification, LayoutSolver* solver) {
    printf("📏 Specification string length: %zu bytes\n", strlen(specification));
    printf("📋 Parsing DSL specification from string...\n");
    
    // Create a copy of the specification to work with
    char* spec_copy = malloc(strlen(specification) + 1);
    if (!spec_copy) {
        printf("❌ Memory allocation failed\n");
        return 0;
    }
    strcpy(spec_copy, specification);
    
    ParsingSection current_section = SECTION_NONE;
    char current_component[256] = "";
    char tile_buffer[2048] = "";
    int in_code_block = 0;
    
    // Parse line by line
    char* line = strtok(spec_copy, "\n");
    while (line != NULL) {
        // Trim leading whitespace
        while (*line == ' ' || *line == '\t') line++;
        
        // Skip empty lines
        if (strlen(line) == 0) {
            line = strtok(NULL, "\n");
            continue;
        }
        
        // Check for section headers
        if (strstr(line, "## Components") || strstr(line, "Components")) {
            current_section = SECTION_COMPONENTS;
            printf("📋 Found Components section\n");
        } else if (strstr(line, "## Constraints") || strstr(line, "Constraints")) {
            current_section = SECTION_CONSTRAINTS;
            printf("📋 Found Constraints section\n");
        } else if (strstr(line, "## Component Tiles") || strstr(line, "Component Tiles")) {
            current_section = SECTION_TILES;
            printf("📋 Found Component Tiles section\n");
        }

        // Parse components in the Components section
        if (current_section == SECTION_COMPONENTS) {
            // Look for **ComponentName** - description format
            char* first_star = strstr(line, "**");
            if (first_star) {
                char* second_star = strstr(first_star + 2, "**");
                if (second_star) {
                    char* start = first_star + 2;
                    char* end = second_star;
                    if (end && end > start) {
                        int len = end - start;
                        if (len < (int)sizeof(current_component)) {
                            strncpy(current_component, start, len);
                            current_component[len] = '\0';
                            printf("  🏷️  Found component: '%s'\n", current_component);
                        }
                    }
                }
            }
            // Handle numbered list format: "1. Component Name"
            else if ((line[0] >= '1' && line[0] <= '9') && strstr(line, ". ")) {
                char* start = strstr(line, ". ") + 2;
                // Find end of component name (before newline or dash)
                char* end = strchr(start, '\n');
                if (!end) end = strchr(start, '-');
                if (!end) end = start + strlen(start);

                // Trim whitespace from end
                while (end > start && (*(end-1) == ' ' || *(end-1) == '\t')) end--;

                int len = end - start;
                if (len > 0 && len < (int)sizeof(current_component)) {
                    strncpy(current_component, start, len);
                    current_component[len] = '\0';
                    printf("  🏷️  Found numbered component: '%s'\n", current_component);
                }
            }
        }
        
        // Parse component tiles
        if (current_section == SECTION_TILES) {
            if (strstr(line, "```")) {
                if (!in_code_block) {
                    in_code_block = 1;
                    tile_buffer[0] = '\0'; // Clear buffer
                } else {
                    // End of code block - add component
                    in_code_block = 0;
                    if (strlen(current_component) > 0 && strlen(tile_buffer) > 0) {
                        add_component(solver, current_component, tile_buffer);
                    }
                }
            } else if (in_code_block) {
                // Accumulate tile data
                if (strlen(tile_buffer) + strlen(line) + 1 < sizeof(tile_buffer)) {
                    if (strlen(tile_buffer) > 0) strcat(tile_buffer, "\n");
                    strcat(tile_buffer, line);
                }
            } else if (strstr(line, "**") && !in_code_block) {
                // Component name in Component Tiles section: **ComponentName**
                char* first_star = strstr(line, "**");
                if (first_star) {
                    char* second_star = strstr(first_star + 2, "**");
                    if (second_star) {
                        char* start = first_star + 2;
                        char* end = second_star;
                        if (end && end > start) {
                            int len = end - start;
                            if (len < (int)sizeof(current_component)) {
                                strncpy(current_component, start, len);
                                current_component[len] = '\0';

                                // Remove trailing colon if present
                                char* colon = strchr(current_component, ':');
                                if (colon) *colon = '\0';

                                printf("  🏷️  Found tile component name: '%s'\n", current_component);
                            }
                        }
                    }
                }
            } else if (strstr(line, ":") && !in_code_block && !strstr(line, "**")) {
                // Component name in "Name:" format (fallback)
                char* colon = strchr(line, ':');
                if (colon) {
                    int len = colon - line;
                    if (len > 0 && len < (int)sizeof(current_component)) {
                        strncpy(current_component, line, len);
                        current_component[len] = '\0';

                        // Trim whitespace
                        while (len > 0 && current_component[len-1] == ' ') {
                            current_component[--len] = '\0';
                        }
                        printf("  🏷️  Found fallback component name: '%s'\n", current_component);
                    }
                }
            }
        }
        
        // Parse constraints
        if (current_section == SECTION_CONSTRAINTS && strstr(line, "(")) {
            // Handle bullet point format (- ADJACENT(...))
            char* constraint_start = line;
            if (*constraint_start == '-' || *constraint_start == '*') {
                constraint_start++;
                while (*constraint_start == ' ' || *constraint_start == '\t') constraint_start++; // Skip whitespace
            }
            printf("  🔗 Found constraint: '%s'\n", constraint_start);
            add_constraint(solver, constraint_start);
        }
        
        line = strtok(NULL, "\n");
    }
    
    free(spec_copy);
    printf("📊 Loaded %d components and %d constraints\n", solver->component_count, solver->constraint_count);
    return 1;
}

/**
 * @brief High-level interface for parsing and solving DSL specifications
 * 
 * Determines input type (file or string), parses the DSL specification,
 * runs the constraint solver, and displays the result. Main workflow
 * coordination function.
 * 
 * @param specification Either a filename or DSL specification string
 */
void parse_and_solve_specification(const char* specification) {
    LayoutSolver solver;
    init_solver(&solver, 60, 40);
    
    // If specification starts with a filename, load from file
    if (strstr(specification, ".txt") && strlen(specification) < 100) {
        if (!parse_specification_file(specification, &solver)) {
            return;
        }
    } else {
        if (!parse_specification_string(specification, &solver)) {
            printf("❌ Failed to parse DSL specification from string\n");
            return;
        }
    }
    
    if (solve_constraints(&solver)) {
        display_grid(&solver);
    }
}

/**
 * @brief Main application entry point with interactive menu system
 * 
 * Provides menu-driven interface for:
 * - LLM-generated structure specifications
 * - File-based DSL parsing
 * - Built-in test cases
 * - Direct string parsing
 * 
 * Coordinates the two-phase process:
 * Phase 1: Generate or load DSL specification
 * Phase 2: Parse and solve constraints
 * 
 * @return Program exit code
 */
int main() {
    char output_buffer[32768]; // Increased to 32KB for larger LLM responses
    char structure_type[256];
    int choice;

    // Initialize output buffer
    memset(output_buffer, 0, sizeof(output_buffer));

    printf("ASCII Structure System - Phase 1 (DSL) + Phase 2 (Solver)\n");
    printf("This system generates and solves structure layouts using DSL constraints.\n");

    while (1) {
        show_menu();
        scanf("%d", &choice);

        switch (choice) {
            case 1: strcpy(structure_type, "castle"); break;
            case 2: strcpy(structure_type, "village"); break;
            case 3: strcpy(structure_type, "dungeon"); break;
            case 4: strcpy(structure_type, "cathedral"); break;
            case 5: strcpy(structure_type, "tower"); break;
            case 6:
                parse_and_solve_specification("test_castle.txt");
                return 0; // Exit after processing
            case 7:
                printf("Enter filename: ");
                scanf("%255s", structure_type);
                parse_and_solve_specification(structure_type);
                return 0; // Exit after processing
            case 8:
                // Test string parsing with test_castle.txt content
                {
                    const char* test_spec = 
                        "## Components\n\n"
                        "**Gatehouse** - The main entrance with defensive features. Medium scale fortified entry point.\n\n"
                        "**Courtyard** - Large open central area for gatherings. Large scale open space.\n\n"
                        "**Keep** - The main defensive tower. Large scale central fortification.\n\n"
                        "## Constraints\n\n"
                        "ADJACENT(Gatehouse, Courtyard, n)\n"
                        "CONNECTED(Courtyard, Keep, door, n)\n"
                        "ACCESSIBLE_FROM(Gatehouse, ALL)\n\n"
                        "## Component Tiles\n\n"
                        "**Gatehouse:**\n"
                        "```\n"
                        "XXXXXXX\n"
                        "X.....X\n"
                        "X..D..X\n"
                        "X.....X\n"
                        "XXXXXXX\n"
                        "```\n\n"
                        "**Courtyard:**\n"
                        "```\n"
                        "...........\n"
                        "...........\n"
                        "...........\n"
                        ".....:.....\n"
                        "...........\n"
                        "...........\n"
                        "```\n\n"
                        "**Keep:**\n"
                        "```\n"
                        "XXXXXXXXX\n"
                        "X.......X\n"
                        "X..$....X\n"
                        "X...a...X\n"
                        "X.......X\n"
                        "XXXXXXXXX\n"
                        "```\n";
                    
                    printf("🧪 Testing string parsing with sample castle specification...\n");
                    parse_and_solve_specification(test_spec);
                    return 0; // Exit after test
                }
                continue;
            case 0:
                printf("Goodbye!\n");
                return 0;
            default:
                printf("Invalid choice. Please try again.\n");
                continue;
        }

        // Phase 1: Generate DSL specification
        if (generate_structure_specification(structure_type, output_buffer, sizeof(output_buffer)) == 0) {
            printf("\n📝 Generated DSL Specification:\n");
            printf("==================================================\n");
            printf("%s\n", output_buffer);
            printf("==================================================\n");

            // Phase 2: Parse and solve constraints
            parse_and_solve_specification(output_buffer);
        } else {
            printf("❌ Failed to generate structure specification.\n");
        }
        return 0; // Exit after processing LLM request
    }

    return 0;
}