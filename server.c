#include <openssl/ssl.h>
#include <openssl/err.h>
#include <openssl/provider.h>
#include <netinet/in.h>
#include <unistd.h>
#include <string.h>
#include <stdio.h>
#include <stddef.h>
#include <netinet/tcp.h>


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

        printf("Classical mode: restricted to P-384\n");
    } else {
        // Hybrid mode: enforce PQ-hybrid group
        if (!SSL_CTX_set1_groups_list(ctx, "p384_mldsa65")) {
            fprintf(stderr, "Failed to set hybrid group p384_mldsa65\n");
            return 1;
        }
        printf("Hybrid group set: p384_mldsa65\n");
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

    // bind socket to address, fail it it does not succeed
    if (bind(sockfd, (struct sockaddr*)&addr, sizeof(addr)) < 0) {
        perror("bind failuer");
        close(sockfd);
        return 1;
    }

    // put server in passive mode and limit incoming requests queue to 1
    if (listen(sockfd, 1) < 0) {
        perror("listen");
        close(sockfd);
        return 1;
    }
    printf("Server listening on port %d...\n", port_no);

    // getting parameters for tests
    int client = accept(sockfd, NULL, NULL);
    if (client < 0) {
        perror("accept");
        close(sockfd);
        return 1;
    }
    uint16_t tests;
    recv(client, &tests, sizeof(tests), MSG_WAITALL);
    tests = ntohs(tests);
    printf("Performing %u tests\n", tests);
    close(client);

    size_t total_sent = 0, total_recv = 0;

    // recieve n requests
    for (int i = 0; i < tests; i++) {
        int client = accept(sockfd, NULL, NULL);
        if (client < 0) {
            perror("accept");
            close(sockfd);
            return 1;
        }

        SSL *ssl = SSL_new(ctx);
        SSL_set_fd(ssl, client);
        printf("Waiting for TLS handshake...\n");

        if (SSL_accept(ssl) <= 0) {
            ERR_print_errors_fp(stderr);
        } else {
            printf("TLS handshake successful\n");
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
        size_t test_sent = 0, test_recv = 0;
        
        char buffer[1024] = {0};
        test_recv += SSL_read(ssl, buffer, sizeof(buffer));
        total_recv += test_recv;
        printf("Received: %s\n", buffer);
        test_sent += SSL_write(ssl, "Hello from server", 17);
        total_sent += test_sent;
        printf("Bytes sent in test: %zu\n", test_sent);
        printf("Bytes received in test: %zu\n", test_recv);


        SSL_free(ssl);
        close(client);
    }
    printf("Total Bytes sent: %zu\n", total_sent);
    printf("Total Bytes received: %zu\n", total_recv);

    SSL_CTX_free(ctx);
    EVP_cleanup();
    close(sockfd);

    printf("Connection closed.\n");
    return 0;
}