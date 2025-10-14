#include <openssl/ssl.h>
#include <openssl/err.h>
#include <openssl/provider.h>
#include <netinet/in.h>
#include <unistd.h>
#include <string.h>
#include <stdio.h>
#include <time.h>

int main(int argc, char **argv) {
    int use_hyb = 0;
    if (argc > 1 && strcmp(argv[1], "--hyb") == 0) {
        use_hyb = 1;
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
        if (!SSL_CTX_set1_groups_list(ctx, "X25519:P-256:P-384")) {
            fprintf(stderr, "Failed to restrict to classical groups\n");
            return 1;
        }

        printf("Classical mode: restricted to X25519, P-256, P-384\n");
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
    addr.sin_port = htons(4443);
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

    printf("Server listening on port 4443...\n");

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

    char buffer[1024] = {0};
    SSL_read(ssl, buffer, sizeof(buffer));
    printf("Received: %s\n", buffer);
    SSL_write(ssl, "Hello from server", 17);

    SSL_free(ssl);
    close(client);
    close(sockfd);
    SSL_CTX_free(ctx);
    EVP_cleanup();

    printf("Connection closed.\n");
    return 0;
}