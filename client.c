#include <openssl/ssl.h>
#include <openssl/err.h>
#include <openssl/provider.h>   // for OSSL_PROVIDER_load
#include <arpa/inet.h>
#include <unistd.h>
#include <string.h>
#include <stdio.h>


int ssl_receive_file(SSL *ssl, FILE *fp) {
    char buffer[16384];  // same chunk size as sender
    int bytes_read;
    int total_received = 0;

    for (;;) {
        bytes_read = SSL_read(ssl, buffer, sizeof(buffer));

        if (bytes_read > 0) {
            size_t written = fwrite(buffer, 1, bytes_read, fp);
            if (written < (size_t)bytes_read) {
                perror("File write error");
                return -1;
            }
            total_received += bytes_read;
        } else {
            int err = SSL_get_error(ssl, bytes_read);

            if (err == SSL_ERROR_WANT_READ || err == SSL_ERROR_WANT_WRITE) {
                // Non-blocking mode: retry later
                continue;
            } else if (err == SSL_ERROR_ZERO_RETURN) {
                // Clean shutdown (EOF)
                printf("SSL connection closed by peer\n");
                break;
            } else {
                fprintf(stderr, "SSL_read failed: %d\n", err);
                ERR_print_errors_fp(stderr);
                return -1;
            }
        }
    }
    printf("Total bytes received: %d\n", total_received);
    return 0;
}

int main(int argc, char **argv) {
    int test = 2;
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

    SSL_library_init();
    SSL_load_error_strings();
    OpenSSL_add_ssl_algorithms();

    // Load providers
    OSSL_PROVIDER_load(NULL, "default");
    if (use_hyb) {
        if (!OSSL_PROVIDER_load(NULL, "oqsprovider")) {
            fprintf(stderr, "Failed to load oqsprovider\n");
            return 1;
        }
    }

    const SSL_METHOD *method = TLS_client_method();
    SSL_CTX *ctx = SSL_CTX_new(method);

    // Optional: select hyb group when in hyb mode
    if (use_hyb) {
        SSL_CTX_set1_groups_list(ctx, "p384_mlkem768");
    }

    int sockfd = socket(AF_INET, SOCK_STREAM, 0);
    struct sockaddr_in addr;
    addr.sin_family = AF_INET;
    addr.sin_port = htons(port_no);
    inet_pton(AF_INET, "127.0.0.1", &addr.sin_addr);

    if (connect(sockfd, (struct sockaddr*)&addr, sizeof(addr)) < 0) {
        perror("connect");
        return 1;
    }
    SSL *ssl = SSL_new(ctx);
    SSL_set_fd(ssl, sockfd);
    if (SSL_connect(ssl) <= 0) {
        ERR_print_errors_fp(stderr);
    } 
    else {
        if (test == 1) {
            char msg[] = "Hello from client";
            char buffer[1024] = {0};
            // measure time to send message
            clock_t start_send = clock();
            SSL_write(ssl, msg, strlen(msg));
            clock_t end_send = clock();
            double send_time = ((double)(end_send - start_send) / CLOCKS_PER_SEC) * 1000.0;
            printf("Time to send message: %.3f ms\n", send_time);
            
            // measure time to receive reply
            clock_t start_recv = clock();
            SSL_read(ssl, buffer, sizeof(buffer));
            clock_t end_recv = clock();
            double recv_time = ((double)(end_recv - start_recv) / CLOCKS_PER_SEC) * 1000.0;

            printf("Received: %s\n", buffer);
            printf("Time to receive reply: %.3f ms\n", recv_time);
        } else if (test == 2) {
            FILE *file = fopen("enisa2.pdf", "wb");
            if (!file) {
                perror("fopen");
                return 1;
            }
            if (ssl_receive_file(ssl, file) == 0) {
                printf("File received successfully.\n");
            } else {
                printf("File receive failed.\n");
            }
        }
    }
    // give time for server to collect socket data before closing connection
    sleep(1); 

    SSL_free(ssl);
    close(sockfd);
    SSL_CTX_free(ctx);
    EVP_cleanup();
    return 0;
}