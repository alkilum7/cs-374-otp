#include <sys/socket.h>
#include <sys/wait.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <sys/types.h>
#include <errno.h>
#include <signal.h>

static int BUF_SIZE = 100000;

void send_chunks(int connected_socket, char *buf, int len) {
    int rem = len;
    char *src = buf;
    while(rem > 0) {
        int chunk_size = 1000;
        if(rem < 1000) chunk_size = rem;
        rem -= chunk_size;
        send(connected_socket, src, chunk_size, 0);
        src += chunk_size;
    }
}

void read_chunks(int connected_socket, char *buf, int len) {
    int rem = len;
    char *dest = buf;
    while(rem > 0) {
        int chunk_size = 1000;
        if(rem < 1000) chunk_size = rem;
        rem -= chunk_size;
        recv(connected_socket, dest, chunk_size, 0);
        dest += chunk_size;
    }
}

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
        send(connected_socket, "!", 1, 0);
        exit(1);
    }

    // Send OK
    send(connected_socket, "OK", 2, 0);

    // Set up text and key buffers
    char text[BUF_SIZE];
    char key[BUF_SIZE];

    // Receive text_len
    int text_len;
    recv(connected_socket, &text_len, 4, 0);

    // Receive text and key
    read_chunks(connected_socket, text, text_len);
    read_chunks(connected_socket, key, text_len);

    // Return result
    char result[BUF_SIZE];
    pad(text, text_len, key, result);
    send_chunks(connected_socket, result, text_len);

    // Exit successfully
    exit(0);
}

void check_children(int child_pids[5]) {
    for(int i = 0; i < 5; i++) {
        if(child_pids[i] > -1) {
            int state = waitpid(child_pids[i], NULL, WNOHANG);
            if(state != 0) {
                child_pids[i] = -1;
            }
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