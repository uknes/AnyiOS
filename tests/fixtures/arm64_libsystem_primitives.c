// Original AnyiOS owned SDK-free C executable exercising imported
// libSystem memory primitives under Dynarmic. No Apple runtime code.
typedef unsigned long anyios_size_t;
extern void *memcpy(void *, const void *, anyios_size_t);
extern void *memset(void *, int, anyios_size_t);
extern anyios_size_t strlen(const char *);
extern int strcmp(const char *, const char *);

int main(void) {
    char source[8] = {'A', 'B', 0, 0, 0, 0, 0, 0};
    char destination[8];
    if (memset(destination, 0x1234ff, sizeof(destination)) != destination)
        return 2;
    if ((unsigned char)destination[0] != 255 ||
        (unsigned char)destination[7] != 255)
        return 3;
    if (memcpy(destination, source, 3) != destination)
        return 4;
    if (strlen(destination) != 2 || strcmp(destination, source) != 0)
        return 5;
    if (strcmp("A", destination) >= 0)
        return 6;
    return 42;
}
