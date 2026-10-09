extern int anyios_leaf_value(void);
static int initialized;
__attribute__((constructor))
static void initialize_middle(void) { initialized = anyios_leaf_value() * 10 + 3; }
int anyios_middle_value(void) { return initialized; }
