typedef unsigned long anyios_size_t;
extern long write(int descriptor, const void *buffer, anyios_size_t size);
extern void exit(int code) __attribute__((noreturn));
extern void *malloc(anyios_size_t size);

static volatile int constructor_value = 0;

__attribute__((constructor))
static void anyios_initialize(void) {
    constructor_value = 17;
}

int main(int argc, char **argv, char **envp, char **apple) {
    if (constructor_value != 17 || argc != 2 ||
        argv == 0 || argv[0] == 0 || argv[1] == 0 || argv[2] != 0 ||
        argv[0][0] != 'a' || argv[1][0] != 'g' ||
        envp == 0 || envp[0] == 0 || envp[1] != 0 ||
        envp[0][0] != 'A' ||
        apple == 0 || apple[0] == 0 || apple[1] != 0 ||
        apple[0][0] != 'e') {
        return 19;
    }
    char *buffer = (char *)malloc(16);
    if (!buffer) return 20;
    buffer[0] = 'h';
    buffer[1] = 'e';
    buffer[2] = 'l';
    buffer[3] = 'l';
    buffer[4] = 'o';
    buffer[5] = '\n';
    if (write(1, buffer, 6) != 6) return 21;
    exit(23);
}
