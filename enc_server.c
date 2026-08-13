#include <sys/socket.h>
#include <sys/wait.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <sys/types.h>
#include <errno.h>

int char_to_code(char c) {
    if(c == ' ') return 0;
    return c - 64;
}

char code_to_char(int i) {
    if(i == 0) return ' ';
    return (char) (i + 64);
}

void usage_error() {
    perror("Usage: enc_server [port]");
    exit(1);
}

void pad(char *text, int text_len, char *key, char *result) {
    for(int i = 0; i < text_len; i++) {
        int text_code = char_to_code(text[i]);
        int key_code = char_to_code(key[i]);
        int result_code = (text_code + key_code) % 27;
        result[i] = code_to_char(result_code);
    }
}

void serve_client(int connected_socket) {
    // Receive greeting message
    char greeting_buf[256];
    int greeting_len = read(connected_socket, greeting_buf, 256);
    char *expected_greeting = "I AM ENC_CLIENT";
    if(
        memcmp(
            greeting_buf,
            expected_greeting,
            strlen(expected_greeting)
        ) != 0
    ) {
        perror("Could not verify enc_client, connection refused");
        exit(1);
    }

    // Send OK
    send(connected_socket, "OK", 2, 0);

    // Get plaintext and key
    char text[1024];
    char key[1024];

    // Get text
    int text_len = recv(connected_socket, text, 1024, 0);

    // Send "GIVE KEY"
    send(connected_socket, "GIVE KEY", 8, 0);

    // Get key
    int key_len = recv(connected_socket, key, 1024, 0);

    if(key_len < text_len) {
        perror("Error: Key length less than text length");
        exit(1);
    }

    // Return result
    char result[1024];
    pad(text, text_len, key, result);
    send(connected_socket, result, text_len, 0);

    // Exit successfully
    exit(0);
}

void check_children(int child_pids[5]) {
    for(int i = 0; i < 5; i++) {
        if(child_pids[i] > -1) {
            int state = waitpid(child_pids[i], NULL, WNOHANG);
            if(state != 0) child_pids[i] = -1;
        }
    }
}

int main(int argc, char *argv[]) {
    // Parse input
    if(argc != 2) usage_error();
    int port = atoi(argv[1]);
    if(port <= 0) usage_error();

    // Set up the array for process tracking
    int child_pids[5] = {-1};

    // Set up the socket
    int in_socket = socket(AF_INET, SOCK_STREAM, 0);
    struct sockaddr_in server_address;
    server_address.sin_family = AF_INET;
    server_address.sin_port = htons(port);
    server_address.sin_addr.s_addr = INADDR_ANY;
    int bind_error = bind(
        in_socket,
        (struct sockaddr *) &server_address,
        sizeof(server_address)
    );
    if(bind_error) {
        printf("Return value: %d\n", bind_error);
        printf("Errno: %d\n", errno);
        fflush(stdout);
    }
    listen(in_socket, 5);

    // Check and accept loop
    while(1) {
        check_children(child_pids);
        struct sockaddr client;
        int client_len;
        int connected_socket = accept(in_socket, &client, &client_len);
        int pid = fork();
        if(pid == 0) {
            serve_client(connected_socket);
        } else {
            // Find a slot for the pid, hang until one is open
            int slot_not_found = 1;
            while(slot_not_found) {
                for(int i = 0; i < 5; i++) {
                    check_children(child_pids);
                    if(child_pids[i] == -1) {
                        child_pids[i] = pid;
                        slot_not_found = 0;
                    }
                    if(slot_not_found) {
                        sleep(1);
                    }
                }
            }
        }
    }
}