#ifndef LLM_INTEGRATION_H
#define LLM_INTEGRATION_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <curl/curl.h>
#include <cjson/cJSON.h>

// =============================================================================
// LLM API INTEGRATION
// =============================================================================

/**
 * @brief Container for HTTP response data from API calls
 * 
 * Used by libcurl to accumulate response data during HTTP requests.
 * The data pointer is dynamically allocated and must be freed after use.
 */
struct APIResponse {
    char *data;
    size_t size;
};

// =============================================================================
// FUNCTION PROTOTYPES
// =============================================================================

// =============================
// HTTP RESPONSE HANDLING
// =============================
/**
 * @brief Callback function for libcurl to handle HTTP response data
 * @param contents Response data chunk
 * @param size     Size of each data element
 * @param nmemb    Number of data elements
 * @param response APIResponse structure to accumulate data
 * @return         Number of bytes processed
 */
size_t WriteCallback(void *contents, size_t size, size_t nmemb, struct APIResponse *response);

// =============================
// PROMPT GENERATION
// =============================

/**
 * @brief Generates system and user prompts for DSL specification generation
 * @param structure_type Type of structure to generate (castle, village, etc.)
 * @param system_prompt  Buffer to store system prompt
 * @param user_prompt    Buffer to store user prompt
 */
void generate_dsl_prompt(const char* structure_type, char* system_prompt, char* user_prompt);

// =============================
// LLM API INTERFACE
// =============================

/**
 * @brief Main interface for generating structure specifications via OpenAI API
 * 
 * Coordinates the complete LLM workflow:
 * 1. Generate prompts for the specified structure type
 * 2. Make API request to OpenAI with proper JSON formatting
 * 3. Parse response and extract DSL specification
 * 4. Handle errors and API failures gracefully
 * 
 * Requires OPENAI_API_KEY environment variable to be set.
 * 
 * @param structure_type Type of structure (castle, village, dungeon, etc.)
 * @param output_buffer  Buffer to store generated DSL specification
 * @param buffer_size    Size of output buffer
 * @return               0 on success, non-zero on failure
 */
int generate_structure_specification(const char* structure_type, char* output_buffer, size_t buffer_size);

#endif // LLM_INTEGRATION_H