typedef unsigned long anyios_size_t;
extern void *malloc(anyios_size_t bytes);
extern long write(int fd, const void *data, anyios_size_t length);
extern void exit(int code) __attribute__((noreturn));

int main(void) {
    char *data = (char *)malloc(16);
    if (data == 0) return 8;
    data[0] = 'O';
    data[1] = 'K';
    if (write(1, data, 2) != 2) return 9;
    exit(0);
}
