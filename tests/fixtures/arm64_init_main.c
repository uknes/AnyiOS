extern int anyios_middle_value(void);
static int initialized;
__attribute__((constructor))
static void initialize_main(void) { initialized = anyios_middle_value() * 10 + 5; }
int main(void) { return initialized == 735 && anyios_middle_value() == 73 ? 735 : -1; }
