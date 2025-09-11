#include "llm_integration.h"

// =============================================================================
// LLM API INTEGRATION IMPLEMENTATION
// =============================================================================

// Callback function to write HTTP response data
size_t WriteCallback(void *contents, size_t size, size_t nmemb, struct APIResponse *response) {
    size_t total_size = size * nmemb;
    response->data = realloc(response->data, response->size + total_size + 1);
    if (response->data == NULL) {
        printf("Failed to reallocate memory\n");
        return 0;
    }
    memcpy(&(response->data[response->size]), contents, total_size);
    response->size += total_size;
    response->data[response->size] = 0;
    return total_size;
}

// =============================================================================
// DSL PROMPT GENERATION
// =============================================================================

void generate_dsl_prompt(const char* structure_type, char* system_prompt, char* user_prompt) {
    snprintf(system_prompt, 4096,
        "🏗️ ASCII Structure Prompt (Enhanced with Expanded Symbol Library)\n\n"
        "Your task is to design a top-down ASCII map of a %s using a recursive, constraint-driven process.\n"
        "You will NOT generate the final structure layout.\n"
        "Instead, you will define:\n\n"
        "1. Core components\n"
        "2. Spatial and functional constraints\n"
        "3. Self-contained ASCII components (tiles) to be assembled later by an external constraint solver\n\n"
        "🧱 Step 1: Identify Core Components\n\n"
        "List and briefly describe the individual components that make up the structure (e.g., rooms, chambers, towers, vaults, courtyards). For each component, include:\n"
        "- Name\n"
        "- Function/purpose/narrative purpose\n"
        "- Approximate scale (small/medium/large), scaled to a player represented by one tile (@)\n"
        "- Notable features\n\n"
        "🧭 Step 2: Define Spatial Constraints (DSL)\n\n"
        "Generate a list of spatial constraints using the following custom DSL format:\n"
        "🗺️ DSL Constraint Types:\n"
        "- ADJACENT(a, b, dir) – b must share the dir edge of a (n, e, s, w, or a for any)\n\n"
        "🧩 Step 3: Generate Individual ASCII Components\n\n"
        "For each component identified in Step 1:\n"
        "- Output a standalone ASCII block representing that space\n"
        "- Use only characters from the symbol library below\n"
        "- Each component must be self-contained and enclosed in a code block\n"
        "- Contain no words\n"
        "- Reflect the function and any notable features\n\n"
        "🔠 Symbol Library (Expanded and Refined)\n\n"
        "(space): Empty\n"
        ". - Ground / Walkable Floor\n"
        "X - Wall\n"
        "_ - Horizontal Structure\n"
        "| - Vertical Structure\n"
        "/ or \\ - Diagonal Structure\n"
        "C - Chest / Container / Crate\n"
        "$ - Coins / Currency / Treasure\n"
        "G - Glass / Window / Pane\n"
        "M - Metal Object / Machinery\n"
        "S - Stone\n"
        "w - Wood\n"
        "t - Tree (Natural)\n"
        "v - Vegetation / Vines / Moss\n"
        "* - Ice / Snow / Frost\n"
        "~ - Liquid / Water / Pool\n"
        "^ - Spike / Hazard\n"
        "%% - Food / Provisions / Rations\n"
        "s - Fire / Furnace / Heat Source\n"
        "b - Book / Scroll / Written Object\n"
        "B - Bed\n"
        "T - Table / Work Surface\n"
        "r - Rug / Carpet / Decorative Floor\n"
        "a - Altar / Shrine\n"
        "h - Chair / Stool / Seating\n"
        "p - Pillar / Column\n"
        "d - Debris / Rubble / Broken Object\n"
        "f - Flag / Banner / Hanging Cloth\n"
        ": - Lamp / Light Source / Torch\n\n"
        "✅ Output Format\n\n"
        "Organize your output into:\n"
        "## Components – Component list with descriptions\n"
        "## Constraints – DSL format only\n"
        "## Component Tiles – One ASCII tile per component (code block)\n\n"
        "Do not generate or describe the final assembled layout.",
        structure_type);

    snprintf(user_prompt, 1024,
        "Generate a detailed specification for a %s structure using the DSL format described above. "
        "Focus on creating modular, well-defined components with clear spatial relationships. "
        "Ensure all constraints use proper DSL syntax and that ASCII tiles are detailed and distinctive. "
        "Return only the structured output with Components, Constraints, and Component Tiles sections.",
        structure_type);
}

// =============================================================================
// MAIN LLM API FUNCTION
// =============================================================================

int generate_structure_specification(const char* structure_type, char* output_buffer, size_t buffer_size) {
    CURL *curl;
    CURLcode res;
    struct APIResponse response = {0};

    curl = curl_easy_init();
    if (!curl) {
        fprintf(stderr, "Failed to initialize curl\n");
        return 1;
    }

    // Prepare API request
    cJSON *root = cJSON_CreateObject();
    cJSON *model = cJSON_CreateString("gpt-4");
    cJSON *messages = cJSON_CreateArray();
    cJSON *temperature = cJSON_CreateNumber(0.7);
    cJSON *max_tokens = cJSON_CreateNumber(2000);

    cJSON_AddItemToObject(root, "model", model);
    cJSON_AddItemToObject(root, "messages", messages);
    cJSON_AddItemToObject(root, "temperature", temperature);
    cJSON_AddItemToObject(root, "max_tokens", max_tokens);

    // Generate prompts
    char system_prompt[4096];
    char user_prompt[1024];
    generate_dsl_prompt(structure_type, system_prompt, user_prompt);

    // System message
    cJSON *sys = cJSON_CreateObject();
    cJSON_AddStringToObject(sys, "role", "system");
    cJSON_AddStringToObject(sys, "content", system_prompt);
    cJSON_AddItemToArray(messages, sys);

    // User message
    cJSON *usr = cJSON_CreateObject();
    cJSON_AddStringToObject(usr, "role", "user");
    cJSON_AddStringToObject(usr, "content", user_prompt);
    cJSON_AddItemToArray(messages, usr);

    char *json_string = cJSON_Print(root);
    cJSON_Delete(root);

    // Set up HTTP headers
    struct curl_slist *headers = NULL;
    char auth_header[256];
    snprintf(auth_header, sizeof(auth_header), "Authorization: Bearer %s", getenv("OPENAI_API_KEY"));
    headers = curl_slist_append(headers, "Content-Type: application/json");
    headers = curl_slist_append(headers, auth_header);

    // Configure curl
    curl_easy_setopt(curl, CURLOPT_URL, "https://api.openai.com/v1/chat/completions");
    curl_easy_setopt(curl, CURLOPT_POSTFIELDS, json_string);
    curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, WriteCallback);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &response);

    // Make the API call
    printf("Generating %s structure specification...\n", structure_type);
    res = curl_easy_perform(curl);

    // Cleanup
    curl_easy_cleanup(curl);
    curl_slist_free_all(headers);
    free(json_string);

    if (res != CURLE_OK) {
        fprintf(stderr, "curl_easy_perform() failed: %s\n", curl_easy_strerror(res));
        if (response.data) free(response.data);
        return 1;
    }

    // Parse response
    if (response.data) {
        cJSON *json = cJSON_Parse(response.data);
        if (json) {
            cJSON *choices = cJSON_GetObjectItem(json, "choices");
            if (choices && cJSON_GetArraySize(choices) > 0) {
                cJSON *first_choice = cJSON_GetArrayItem(choices, 0);
                cJSON *message = cJSON_GetObjectItem(first_choice, "message");
                cJSON *content = cJSON_GetObjectItem(message, "content");
                
                if (content && cJSON_IsString(content)) {
                    strncpy(output_buffer, content->valuestring, buffer_size - 1);
                    output_buffer[buffer_size - 1] = '\0';
                }
            }
            cJSON_Delete(json);
        }
        free(response.data);
    }

    return 0;
}