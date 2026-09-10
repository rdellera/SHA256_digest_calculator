
#include "digest.h"
#include <openssl/sha.h>
#include <fcntl.h>
#include <unistd.h>
#include <stdio.h>

void digest_file(const char *filename, char output[65]) {
    SHA256_CTX ctx;
    SHA256_Init(&ctx);

    char buffer[32];
    int fd = open(filename, O_RDONLY);
    if (fd < 0) {
        perror("open");
        output[0] = '\0';
        return;
    }

    ssize_t r;
    while ((r = read(fd, buffer, 32)) > 0)
        SHA256_Update(&ctx, buffer, r);

    unsigned char hash[32];
    SHA256_Final(hash, &ctx);

    for (int i = 0; i < 32; i++)
        sprintf(output + i*2, "%02x", hash[i]);

    output[64] = '\0';
    close(fd);
}
