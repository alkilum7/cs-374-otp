#include <sys/socket.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <netdb.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <errno.h>

static int BUF_SIZE = 100000;

void send_chunks(int connected_socket, char *buf, int len) {
    int rem = len;
    char *src = buf;
    while(rem > 0) {
        int chunk_size = 1000;
        if(rem < 1000) chunk_size = rem;
        chunk_size = send(connected_socket, src, chunk_size, 0);
        rem -= chunk_size;
        src += chunk_size;
    }
}

void read_chunks(int connected_socket, char *buf, int len) {
    int rem = len;
    char *dest = buf;
    while(rem > 0) {
        int bytes_received = recv(connected_socket, dest, rem, 0);
        rem -= bytes_received;
        dest += bytes_received;
    }
}

void usage_error() {
    perror("Usage: enc_client [plaintext] [key] [port]");
    exit(1);
}

void get_socket(int *server_socket, int port) {
    // Connect to the server
    struct sockaddr_in server_addr;
    server_addr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(port);
    int connect_retval = connect(
        *server_socket,
        (struct sockaddr *) &server_addr,
        sizeof(server_addr)
    );
    if(connect_retval != 0) {
        printf("Connect retval: %d\n", connect_retval);
        printf("Errno: %d\n", errno);
        exit(1);
    }
}

int is_valid(char text[], int len) {
    for(int i = 0; i < len; i++) {
        if(text[i] != ' ' && (text[i] < 'A' || text[i] > 'Z')) {
            return 0;
        }
    }
    return 1;
}

int main(int argc, char *argv[]) {
    // Parse, check usage
    if(argc != 4) {
        usage_error();
    }
    int port = atoi(argv[3]);
    if(port <= 0) {
        usage_error();
    }

    // Attempt to open files
    FILE *text_file = fopen(argv[1], "r");
    FILE *key_file = fopen(argv[2], "r");
    if(text_file == NULL || key_file == NULL) {
        perror("File read error");
        exit(1);
    }
    char text[BUF_SIZE];
    char key[BUF_SIZE];
    fgets(text, BUF_SIZE, text_file);
    fgets(key, BUF_SIZE, key_file);
    int text_len = strlen(text) - 1;
    int key_len = strlen(key) - 1;

    if(key_len < text_len) {
        printf("Error: key '%s' is too short\n", argv[2]);
        exit(1);
    }

    if(!is_valid(text, text_len)) {
        perror("enc_client error: input contains bad characters");
        exit(1);
    }

    if(!is_valid(key, text_len)) {
        perror("enc_client error: key contains bad characters");
        exit(1);
    }

    printf("I am about to open the socket.\n");
    fflush(stdout);
    int server_socket = socket(AF_INET, SOCK_STREAM, 0);
    get_socket(&server_socket, port);
    printf("I got the socket.\n");
    fflush(stdout);
    // Send and receive data
    // Send greeting
    printf("I am about to send the greeting message.\n");
    fflush(stdout);
    char *greeting = "I AM ENC_CLIENT";
    send_chunks(server_socket, greeting, strlen(greeting));
    printf("I successfully sent the greeting message.\n");
    fflush(stdout);

    // Wait for server greeting
    char greeting_buf[256];
    read_chunks(server_socket, greeting_buf, 15);
    char *expected_greeting = "I AM ENC_SERVER";
    if(
        memcmp(
            greeting_buf,
            expected_greeting,
            strlen(expected_greeting)
        ) != 0
    ) {
        perror("Could not verify enc_server, connection refused");
        exit(1);
    }
    printf("I verified the server.\n");
    fflush(stdout);

    // Send text length as integer
    send_chunks(server_socket, (char *) &text_len, 4);
    printf("I sent the length of the text\n");
    fflush(stdout);

    // Send text and key
    send_chunks(server_socket, text, text_len);
    send_chunks(server_socket, key, text_len);

    // Receive result
    char result_buf[BUF_SIZE];
    read_chunks(server_socket, result_buf, text_len);

    // Print result
    write(STDOUT_FILENO, result_buf, text_len);
    printf("\n");
}