static int initialized;
__attribute__((constructor))
static void initialize_leaf(int argc, const char **argv, const char **envp, const char **apple) {
    initialized = argc == 2 && argv && argv[0] && argv[0][0] == 'i' &&
                  envp && envp[0] && envp[0][0] == 'A' &&
                  apple && apple[0] && apple[0][0] == 'e' ? 7 : -1;
}
int anyios_leaf_value(void) { return initialized; }
