#include <stdio.h>
#include "heap.h"

int main(int argc, char* argv[]) {
    heap_setup();
    char* input_text = argv[1];
    size_t input_text_len = strlen(input_text);
    char* ptr_m = (char*) heap_malloc(input_text_len);
    char* ptr_c = (char*) heap_calloc(input_text_len, sizeof(char*));
    char* ptr_r = (char*) heap_malloc(1);
    heap_realloc(ptr_r, input_text_len);
    sprintf(ptr_m, "%s", input_text);
    sprintf(ptr_r, "%s", input_text);
    sprintf(ptr_c, "%s", input_text);
    printf("malloc: %s\n", ptr_m);
    printf("calloc: %s\n", ptr_c);
    printf("realloc: %s\n", ptr_r);
    heap_free(ptr_m);
    heap_free(ptr_c);
    heap_free(ptr_r);
    heap_clean();
    return 0;
}