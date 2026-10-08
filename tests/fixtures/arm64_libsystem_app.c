typedef unsigned long anyios_size_t;
extern void *_malloc(anyios_size_t bytes) __asm__("_malloc");
extern long _write(int descriptor, const void *buffer, anyios_size_t bytes)
    __asm__("_write");
extern void _exit(int code) __asm__("_exit") __attribute__((noreturn));

int main(void) {
    char *memory = (char *)_malloc(16);
    if (memory == 0) return 8;
    memory[0] = 'O';
    memory[1] = 'K';
    if (_write(1, memory, 2) != 2) return 9;
    _exit(0);
}
