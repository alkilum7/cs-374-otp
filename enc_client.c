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

void usage_error() {
    perror("Usage: enc_client [plaintext] [key] [port]");
    exit(1);
}

void get_socket(int *server_socket, int port) {
    // Connect to the server
    struct sockaddr_in server_addr;
    struct hostent *server_hostent = gethostbyname("localhost");
    if(server_hostent == NULL) {
        perror("ERROR: could not find localhost");
        exit(1);
    }
    // Copy the IP Address
    server_addr.sin_addr.s_addr = *(server_hostent->h_addr_list[0]);
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
    if(text_file < 0 || key_file < 0) {
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

    if(!is_valid(text, text_len) || !is_valid(key, key_len)) {
        perror("enc_client error: input contains bad characters");
    }

    int server_socket = socket(AF_INET, SOCK_STREAM, 0);
    get_socket(&server_socket, port);
    // Send and receive data
    // Send greeting
    char *greeting = "I AM ENC_CLIENT";
    send(server_socket, greeting, strlen(greeting), 0);

    // Wait for "OK"
    char ok_buff[2];
    if(recv(server_socket, ok_buff, 2, 0) != 2) {
        perror("Did not receive OK");
        exit(1);
    }

    // Send text
    send(server_socket, text, strlen(text), 0);

    // Wait for "GIVE KEY"
    char give_key_buf[8];
    if(recv(server_socket, give_key_buf, 8, 0) != 8) {
        perror("Did not receive GIVE KEY");
        exit(1);
    }

    // Send key
    send(server_socket, key, strlen(text), 0);
    send(server_socket, "\n", 1, 0);

    // Receive result
    char result_buf[BUF_SIZE];
    int result_len = recv(server_socket, result_buf, BUF_SIZE, 0);

    // Print result
    write(STDOUT_FILENO, result_buf, text_len);
    printf("\n");
}