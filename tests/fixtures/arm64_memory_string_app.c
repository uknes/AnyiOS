typedef unsigned long size_t;
extern void *memcpy(void *restrict, const void *restrict, size_t);
extern void *memset(void *, int, size_t);
extern size_t strlen(const char *);
extern int strcmp(const char *, const char *);

int main(void) {
    char source[8] = {'c', 'a', 't', 0};
    char buffer[16];
    if (memset(buffer, 0x12345661, sizeof(buffer)) != buffer)
        return 11;
    if (buffer[12] != 'a')
        return 12;
    if (memcpy(buffer, source, 4) != buffer)
        return 13;
    if (strlen(buffer) != 3)
        return 14;
    if (strcmp(buffer, source) != 0)
        return 15;
    if (strcmp(buffer, "dog") >= 0)
        return 16;
    if (strcmp("dog", buffer) <= 0)
        return 17;
    if (memset(buffer + 4, -1, 1) != buffer + 4)
        return 18;
    if ((unsigned char)buffer[4] != 255)
        return 19;
    return 27;
}
