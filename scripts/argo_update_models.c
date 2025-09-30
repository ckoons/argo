/* © 2025 Casey Koons All rights reserved */

/* Model update utility - query APIs and update model defaults */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "../include/argo_http.h"
#include "../include/argo_error.h"

static void print_usage(const char* prog) {
    printf("Usage: %s [options]\n", prog);
    printf("\n");
    printf("Query API providers and generate argo_local_models.h\n");
    printf("\n");
    printf("Options:\n");
    printf("  --output FILE, -o FILE   Output file (default: include/argo_local_models.h)\n");
    printf("  --help, -h               Show this help\n");
    printf("\n");
}

int main(int argc, char* argv[]) {
    const char* output_file = "include/argo_local_models.h";

    /* Parse arguments */
    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "--output") == 0 || strcmp(argv[i], "-o") == 0) {
            if (i + 1 < argc) {
                output_file = argv[++i];
            }
        } else if (strcmp(argv[i], "--help") == 0 || strcmp(argv[i], "-h") == 0) {
            print_usage(argv[0]);
            return 0;
        }
    }

    printf("Argo Model Update Utility\n");
    printf("=========================\n");
    printf("\n");
    printf("NOTE: Full implementation pending\n");
    printf("Currently using bash script: scripts/update_models.sh\n");
    printf("\n");
    printf("This C version will:\n");
    printf("  - Use argo_http.c for API queries\n");
    printf("  - Parse JSON responses directly\n");
    printf("  - Generate %s\n", output_file);
    printf("\n");
    printf("For now, please use: make update-models\n");
    printf("\n");

    return 0;
}