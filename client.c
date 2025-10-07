#include <openssl/ssl.h>
#include <openssl/err.h>
#include <openssl/provider.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <string.h>
#include <stdio.h>

int main(int argc, char **argv) {
    int use_hyb = 0;
    if (argc > 1 && strcmp(argv[1], "--hyb") == 0) {
        use_hyb = 1;
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
        if (!SSL_CTX_set1_groups_list(ctx, "X25519:P-256:P-384")) {
            fprintf(stderr, "Failed to restrict to classical groups\n");
            return 1;
        }
        printf("Classical mode: restricted to X25519, P-256, P-384\n");
    } else {
        // Hybrid mode: enforce post-quantum hybrid group
        if (!SSL_CTX_set1_groups_list(ctx, "p384_mlkem768")) {
            fprintf(stderr, "Failed to set hybrid group p384_mlkem768\n");
            return 1;
        }
        printf("Hybrid group set: p384_mlkem768\n");
    }

    // Create and connect socket
    int sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if (sockfd < 0) {
        perror("socket");
        return 1;
    }

    struct sockaddr_in addr;
    addr.sin_family = AF_INET;
    addr.sin_port = htons(4443);
    inet_pton(AF_INET, "127.0.0.1", &addr.sin_addr);

    printf("Connecting to 127.0.0.1:4443...\n");
    if (connect(sockfd, (struct sockaddr*)&addr, sizeof(addr)) < 0) {
        perror("connect");
        close(sockfd);
        return 1;
    }

    SSL *ssl = SSL_new(ctx);
    SSL_set_fd(ssl, sockfd);

    printf("Starting TLS handshake...\n");
    if (SSL_connect(ssl) <= 0) {
        ERR_print_errors_fp(stderr);
    } else {
        printf("TLS handshake successful\n");
        printf("Negotiated cipher: %s\n", SSL_get_cipher(ssl));
        // Exchange data
        char msg[] = "Hello from client";
        SSL_write(ssl, msg, strlen(msg));

        char buffer[1024] = {0};
        SSL_read(ssl, buffer, sizeof(buffer));
        printf("Received: %s\n", buffer);
    }

    SSL_free(ssl);
    close(sockfd);
    SSL_CTX_free(ctx);
    EVP_cleanup();

    printf("Connection closed.\n");
    return 0;
}