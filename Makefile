default: server.c client.c
	gcc server.c -o server -I/opt/openssl-3.5/include -L/opt/openssl-3.5/lib64 -lssl -lcrypto
	gcc client.c -o client -I/opt/openssl-3.5/include -L/opt/openssl-3.5/lib64 -lssl -lcrypto
clean:
	rm -f server
	rm -f client