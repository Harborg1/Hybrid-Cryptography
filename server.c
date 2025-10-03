#include <openssl/ssl.h>
#include <openssl/err.h>
#include <openssl/provider.h>   // for OSSL_PROVIDER_load
#include <netinet/in.h>
#include <unistd.h>
#include <string.h>
#include <stdio.h>

int main(int argc, char **argv) {
    int use_pq = 0;
    if (argc > 1 && strcmp(argv[1], "--hyb") == 0) {
        use_pq = 1;
    }

    SSL_library_init();
    SSL_load_error_strings();
    OpenSSL_add_ssl_algorithms();

    // Load providers explicitly
    OSSL_PROVIDER_load(NULL, "default");
    if (use_pq) {
        if (!OSSL_PROVIDER_load(NULL, "oqsprovider")) {
            fprintf(stderr, "Failed to load oqsprovider\n");
            return 1;
        }
    }

    const SSL_METHOD *method = TLS_server_method();
    SSL_CTX *ctx = SSL_CTX_new(method);

    // Use classical certs by default, PQ if --pq was given
    if (use_pq) {
        SSL_CTX_use_certificate_file(ctx, "mldsa.crt", SSL_FILETYPE_PEM);
        SSL_CTX_use_PrivateKey_file(ctx, "mldsa.key", SSL_FILETYPE_PEM);

        // Optional: force a PQ/hybrid group like p384_mlkem768
        SSL_CTX_set1_groups_list(ctx, "p384_mlkem768");
    } else {
        SSL_CTX_use_certificate_file(ctx, "cert.pem", SSL_FILETYPE_PEM);
        SSL_CTX_use_PrivateKey_file(ctx, "key.pem", SSL_FILETYPE_PEM);
    }

    int sockfd = socket(AF_INET, SOCK_STREAM, 0);
    struct sockaddr_in addr;
    addr.sin_family = AF_INET;
    addr.sin_port = htons(4443);
    addr.sin_addr.s_addr = INADDR_ANY;

    bind(sockfd, (struct sockaddr*)&addr, sizeof(addr));
    listen(sockfd, 1);

    int client = accept(sockfd, NULL, NULL);
    SSL *ssl = SSL_new(ctx);
    SSL_set_fd(ssl, client);
    SSL_accept(ssl);

    char buffer[1024] = {0};
    SSL_read(ssl, buffer, sizeof(buffer));
    printf("Received: %s\n", buffer);
    SSL_write(ssl, "Hello from server", 17);

    SSL_free(ssl);
    close(client);
    close(sockfd);
    SSL_CTX_free(ctx);
    EVP_cleanup();
    return 0;
}
