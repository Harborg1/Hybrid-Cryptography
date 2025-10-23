#include <openssl/ssl.h>
#include <openssl/err.h>
#include <openssl/provider.h>
#include <netinet/in.h>
#include <unistd.h>
#include <string.h>
#include <stdio.h>
#include <time.h>

void print_tcp_bytes_ss(int port_no)
{
    char cmd[128];
    snprintf(
        cmd, 
        sizeof(cmd),
        "ss -tinp '( sport = :%d )' 2>/dev/null | grep bytes_ | head -n1", 
        port_no
    );

    FILE *fp = popen(cmd, "r");
    if (!fp) {
        perror("popen(ss)");
        return;
    }

    char line[512];
    if (!fgets(line, sizeof(line), fp)) {
        printf("No ss output found for port %d\n", port_no);
        pclose(fp);
        return;
    }

    int bytes_sent = 0;
    int bytes_acked = 0;
    int bytes_received = 0;
    // printf(line);

    /* Flexible parsing: scan for all three possible counters */
    char *p = line;
    while ((p = strstr(p, "bytes_")) != NULL) {
        if (sscanf(p, "bytes_sent:%d", &bytes_sent) == 1) {
            // found bytes_sent
        } else if (sscanf(p, "bytes_acked:%d", &bytes_acked) == 1) {
            // found bytes_acked
        } else if (sscanf(p, "bytes_received:%d", &bytes_received) == 1) {
            // found bytes_received
        }
        p += 6; // move past "bytes_"
    }
    printf("Bytes sent: %d\n", bytes_acked);
    printf("Bytes received: %d\n", bytes_received);
    printf("Total bytes: %d\n", bytes_acked + bytes_received);
    pclose(fp);
}


int ssl_send_file(SSL *ssl, FILE *fp) {
    char buffer[16384]; // 16KB is a good chunk size (fits SSL record size)
    size_t bytes_read;
    int bytes_written;
    int total_written;
    int ret;
    clock_t start_file_send = clock();
    while ((bytes_read = fread(buffer, 1, sizeof(buffer), fp)) > 0) {
        total_written = 0;
        while (total_written < bytes_read) {
            bytes_written = SSL_write(ssl, buffer + total_written, bytes_read - total_written);

            if (bytes_written <= 0) {
                int err = SSL_get_error(ssl, bytes_written);
                if (err == SSL_ERROR_WANT_WRITE || err == SSL_ERROR_WANT_READ) {
                    // Non-blocking retry
                    continue;
                } else {
                    fprintf(stderr, "SSL_write failed: %d\n", err);
                    ERR_print_errors_fp(stderr);
                    return -1;
                }
            }

            total_written += bytes_written;
        }
    }

    if (ferror(fp)) {
        perror("File read error");
        return -1;
    }
    clock_t end_file_send = clock();
    double elapsed = ((double)(end_file_send - start_file_send) / CLOCKS_PER_SEC) * 1000.0;
    printf("File transfer successful\n");
    printf("File transfer time: %.3f ms\n", elapsed);

    return 0; // success
}

int main(int argc, char **argv) {
    // we should proberly move these variables into a struct, which can then be used by both programs
    int test = 2; // 0 = onlt connect, 1 = short message, 2 = send file
    int use_hyb = 0;
    int port_no = 5003;
    if (argc == 2) {
        if (strcmp(argv[1], "--hyb") == 0) {
            use_hyb = 1;
        } else {
            port_no = atoi(argv[1]);;
        }
    } else if (argc == 3) {
        if (strcmp(argv[2], "--hyb") == 0) {
            use_hyb = 1;
        }
        port_no = atoi(argv[1]);
    }
    printf("Mode: %s\n", use_hyb ? "HYBRID" : "CLASSICAL");
    
    SSL_library_init();
    SSL_load_error_strings();
    OpenSSL_add_ssl_algorithms();
    // Load default provider always
    OSSL_PROVIDER_load(NULL, "default");

    // Load OQS provider only in hybrid mode
    if (use_hyb) {
        if (!OSSL_PROVIDER_load(NULL, "oqsprovider")) {
            fprintf(stderr, "Failed to load oqsprovider\n");
            return 1;
        }
        printf("oqsprovider loaded successfully\n");
    }

    const SSL_METHOD *method = TLS_server_method();
    SSL_CTX *ctx = SSL_CTX_new(method);
    if (!ctx) {
        fprintf(stderr, "Failed to create SSL context\n");
        return 1;
    }

    // Restrict groups based on mode
    if (!use_hyb) {
        // Classical mode: allow only standard elliptic curves
        if (!SSL_CTX_set1_groups_list(ctx, "P-384")) {
            fprintf(stderr, "Failed to restrict to classical groups\n");
            return 1;
        }

        printf("Classical mode: restricted P-384\n");
    } else {
        // Hybrid mode: enforce PQ-hybrid group
        if (!SSL_CTX_set1_groups_list(ctx, "p384_mlkem768")) {
            fprintf(stderr, "Failed to set hybrid group p384_mlkem768\n");
            return 1;
        }
        printf("Hybrid group set: p384_mlkem768\n");
    }
    // Load certificate and private key (same files for both modes)
    if (SSL_CTX_use_certificate_file(ctx, "cert.pem", SSL_FILETYPE_PEM) <= 0 ||
        SSL_CTX_use_PrivateKey_file(ctx, "key.pem", SSL_FILETYPE_PEM) <= 0) {
        ERR_print_errors_fp(stderr);
        return 1;
    }

    // Set up TCP socket
    int sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if (sockfd < 0) {
        perror("socket");
        return 1;
    }

    struct sockaddr_in addr;
    addr.sin_family = AF_INET;
    addr.sin_port = htons(port_no);
    addr.sin_addr.s_addr = INADDR_ANY;

    if (bind(sockfd, (struct sockaddr*)&addr, sizeof(addr)) < 0) {
        perror("bind");
        close(sockfd);
        return 1;
    }

    if (listen(sockfd, 1) < 0) {
        perror("listen");
        close(sockfd);
        return 1;
    }

    printf("Server listening on port %d...\n", port_no);

    int client = accept(sockfd, NULL, NULL);
    if (client < 0) {
        perror("accept");
        close(sockfd);
        return 1;
    }

    SSL *ssl = SSL_new(ctx);
    SSL_set_fd(ssl, client);
    printf("Waiting for TLS handshake...\n");

    clock_t start = clock();

    int handshake_result = SSL_accept(ssl);
    clock_t end = clock();
    
    if (handshake_result <= 0) {
        ERR_print_errors_fp(stderr);
    } else {
        double elapsed = ((double)(end - start) / CLOCKS_PER_SEC) * 1000.0;
        printf("TLS handshake successful\n");
        printf("Handshake time: %.3f ms\n", elapsed);
        printf("Negotiated cipher: %s\n", SSL_get_cipher(ssl));

        int group_id = SSL_get_shared_group(ssl, 0);
        if (group_id > 0) {
            const char *group_name = SSL_group_to_name(ssl, group_id);
            if (group_name)
                printf("Negotiated key exchange group: %s\n", group_name);
            else
                printf("Negotiated key exchange group ID: %d (name not found)\n", group_id);
        } else {
            printf("No shared group found.\n");
        }
    }
    if (test == 1) {
        char buffer[1024] = {0};
        printf("Waiting for message from client...\n");

        // measure time to receive message
        clock_t start_recv = clock();
        int bytes_read = SSL_read(ssl, buffer, sizeof(buffer));
        clock_t end_recv = clock();

        if (bytes_read > 0) {
            double recv_time = ((double)(end_recv - start_recv) / CLOCKS_PER_SEC) * 1000.0;
            printf("Received: %s\n", buffer);
            printf("Time to receive message: %.3f ms\n", recv_time);
        } else {
            printf("SSL_read failed.\n");
            ERR_print_errors_fp(stderr);
        }

        // measure time to send reply
        clock_t start_send = clock();
        SSL_write(ssl, "Hello from server", 17);
        clock_t end_send = clock();

        double send_time = ((double)(end_send - start_send) / CLOCKS_PER_SEC) * 1000.0;
        printf("Time to send reply: %.3f ms\n", send_time);

    } else if (test == 2) {
        FILE *file = fopen("data/enisa.pdf", "rb");
        ssl_send_file(ssl, file);
        sleep(1);
    }

    print_tcp_bytes_ss(port_no);

    SSL_shutdown(ssl);  
    SSL_free(ssl);
    close(client);
    close(sockfd);
    SSL_CTX_free(ctx);
    EVP_cleanup();

    printf("Connection closed.\n");
    return 0;
}