#include <openssl/ssl.h>
#include <openssl/err.h>
#include <openssl/provider.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <string.h>
#include <stdio.h>

int main(int argc, char **argv) {
    int use_hyb = 0;
    int port_no;
    if (argc == 2) {
        if (strcmp(argv[1], "--hyb") == 0) {
            use_hyb = 1;
            port_no = 5003;
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

    // Always load the default provider
    OSSL_PROVIDER_load(NULL, "default");

    // Load OQS provider only in hybrid mode
    if (use_hyb) {
        if (!OSSL_PROVIDER_load(NULL, "oqsprovider")) {
            fprintf(stderr, "Failed to load oqsprovider\n");
            return 1;
        }
        printf("oqsprovider loaded successfully\n");
    }

    const SSL_METHOD *method = TLS_client_method();
    SSL_CTX *ctx = SSL_CTX_new(method);
    if (!ctx) {
        fprintf(stderr, "Failed to create SSL context\n");
        return 1;
    }

    // Restrict groups depending on mode
    if (!use_hyb) {
        // Classical mode: allow only standard elliptic curves
        if (!SSL_CTX_set1_groups_list(ctx, "P-384")) {
            fprintf(stderr, "Failed to restrict to classical groups\n");
            return 1;
        }
        printf("Classical mode: restricted to P-384\n");
    } else {
        // Hybrid mode: enforce post-quantum hybrid group
        if (!SSL_CTX_set1_groups_list(ctx, "p384_mldsa65")) {
            fprintf(stderr, "Failed to set hybrid group p384_mldsa65\n");
            return 1;
        }
        printf("Hybrid group set: p384_mldsa65\n");
    }
    int sockfd = socket(AF_INET, SOCK_STREAM, 0);

    if (sockfd < 0) {
        perror("socket");
        return 1;
    }

    struct sockaddr_in addr;
    addr.sin_family = AF_INET;
    addr.sin_port = htons(port_no);
    inet_pton(AF_INET, "127.0.0.1", &addr.sin_addr);

    printf("Connecting to 127.0.0.1:%d...\n", port_no);
    if (connect(sockfd, (struct sockaddr*)&addr, sizeof(addr)) < 0) {
        perror("connect");
        close(sockfd);
        return 1;
    }
    uint16_t tests = 10; // make into argument later (and maybe also a .sh file later)
    uint16_t tests_net = htons(tests); 
    send(sockfd, &tests_net, sizeof(tests_net), 0);

    for (int i = 0; i < tests; i++) {
        // Create and connect socket
        int sockfd = socket(AF_INET, SOCK_STREAM, 0);
        if (sockfd < 0) {
            perror("socket");
            return 1;
        }

        struct sockaddr_in addr;
        addr.sin_family = AF_INET;
        addr.sin_port = htons(port_no);
        inet_pton(AF_INET, "127.0.0.1", &addr.sin_addr);

        if (connect(sockfd, (struct sockaddr*)&addr, sizeof(addr)) < 0) {
            perror("connect");
            close(sockfd);
            return 1;
        }

        SSL *ssl = SSL_new(ctx);
        SSL_set_fd(ssl, sockfd);

        if (SSL_connect(ssl) <= 0) {
            ERR_print_errors_fp(stderr);
        } else {
            // Exchange data
            char msg[] = "Hello from client";
            SSL_write(ssl, msg, strlen(msg));

            char buffer[1024] = {0};
            SSL_read(ssl, buffer, sizeof(buffer));
            printf("Received: %s\n", buffer);
        }
        SSL_free(ssl);
        close(sockfd);
    }
    SSL_CTX_free(ctx);
    EVP_cleanup();


    printf("Connection closed.\n");
    return 0;
}