// --- src/client.c ---
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <ctype.h>
#define BUF_SIZE 512
#define SERVER_IP "127.0.0.1"
#define SERVER_PORT 8055

// Displays the menu and returns the user's choice (1-6)
int display_menu() {
    int choice;
    char input[32];
    printf("\n=== Flight Reservation System ===\n");
    printf("1. Make a reservation\n");
    printf("2. Cancel a reservation\n");
    printf("3. View all flights\n");
    printf("4. View transaction history\n");
    printf("5. View agency invoice\n");
    printf("6. Exit\n");
    printf("Enter your choice (1-6): ");
    if (fgets(input, sizeof(input), stdin) == NULL) return -1;
    if (sscanf(input, "%d", &choice) != 1 || choice < 1 || choice > 6) {
        printf("Error: Please enter a number between 1 and 6.\n");
        return -1;
    }
    return choice;
}

// Validates and gets a positive integer input
int get_positive_int(const char *prompt) {
    int value;
    char input[32];
    printf("%s", prompt);
    if (fgets(input, sizeof(input), stdin) == NULL) return -1;
    if (sscanf(input, "%d", &value) != 1 || value <= 0) {
        printf("Error: Please enter a positive integer.\n");
        return -1;
    }
    return value;
}

int main() {
    int sock;
    struct sockaddr_in serv;
    char buf[BUF_SIZE];
    char response[BUF_SIZE * 4]; // Larger buffer for multiline responses
    int agency_id;

    // Get agency ID
    agency_id = get_positive_int("Enter Agency ID: ");
    if (agency_id == -1) {
        printf("Invalid agency ID. Exiting.\n");
        return 1;
    }

    // Create socket
    sock = socket(AF_INET, SOCK_STREAM, 0);
    if (sock < 0) {
        perror("socket");
        return 1;
    }

    // Configure server address
    serv.sin_family = AF_INET;
    serv.sin_port = htons(SERVER_PORT);
    if (inet_pton(AF_INET, SERVER_IP, &serv.sin_addr) <= 0) {
        fprintf(stderr, "Invalid address: %s\n", SERVER_IP);
        close(sock);
        return 1;
    }

    // Connect to server
    if (connect(sock, (struct sockaddr *)&serv, sizeof(serv)) < 0) {
        perror("connect");
        printf("Failed to connect to server at %s:%d. Ensure the server is running.\n", 
               SERVER_IP, SERVER_PORT);
        close(sock);
        return 1;
    }
    printf("Connected to server at %s:%d\n", SERVER_IP, SERVER_PORT);

    // Main loop
    while (1) {
        int choice = display_menu();
        if (choice == -1) continue; // Invalid menu choice
        if (choice == 6) break; // Exit

        // Prepare command based on choice
        buf[0] = '\0';
        if (choice == 1 || choice == 2) { // Reservation or Cancellation
            int ref = get_positive_int("Enter Flight Reference: ");
            if (ref == -1) continue;
            int seats = get_positive_int("Enter Number of Seats: ");
            if (seats == -1) continue;
            snprintf(buf, BUF_SIZE, "TRANSACTION %d %d %s %d\n", 
                     ref, agency_id, choice == 1 ? "Demande" : "Annulation", seats);
        } else if (choice == 3) { // View flights
            snprintf(buf, BUF_SIZE, "AFF_VOL\n");
        } else if (choice == 4) { // View history
            snprintf(buf, BUF_SIZE, "AFF_HISTO\n");
        } else if (choice == 5) { // View invoice
            snprintf(buf, BUF_SIZE, "AFF_FACTURE %d\n", agency_id);
        }

        // Send command
        if (send(sock, buf, strlen(buf), 0) < 0) {
            perror("send");
            printf("Failed to send command. Server may have disconnected.\n");
            break;
        }

        // Receive and print response
        size_t total = 0;
        response[0] = '\0';
        while (1) {
            ssize_t n = recv(sock, response + total, sizeof(response) - total - 1, 0);
            if (n < 0) {
                perror("recv");
                printf("Error receiving response from server.\n");
                break;
            }
            if (n == 0) {
                printf("Server disconnected.\n");
                break;
            }
            total += n;
            response[total] = '\0';
            // Check for complete response
            if (strstr(response, "\n")) {
                printf("%s", response);
                break;
            }
        }
        if (total == 0) break; // Server disconnected
    }

    // Cleanup
    close(sock);
    printf("Disconnected from server.\n");
    return 0;
}
