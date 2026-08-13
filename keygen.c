#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <time.h>

int char_to_code(char c) {
    if(c == ' ') return 0;
    return c - 64;
}

char code_to_char(int i) {
    if(i == 0) return ' ';
    return (char) (i + 64);
}

void usage_error() {
    perror("Usage: keygen [length]");
    exit(1);
}

int main(int argc, char *argv[]) {
    // Parse input, get length
    if(argc != 2) usage_error();
    int length = atoi(argv[1]);
    if(length <= 0) usage_error();

    // Generate the key
    char *key = malloc(length + 2);
    srand(clock());
    for(int i = 0; i < length; i++) {
        int code = rand() % 26;
        key[i] = code_to_char(code);
    }
    key[length] = '\n';
    key[length + 1] = '\0';
    printf(key);
}