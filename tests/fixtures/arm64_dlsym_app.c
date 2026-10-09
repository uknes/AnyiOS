extern void *dlsym(void *handle, const char *name);
extern const char *dlerror(void);
#define DEFAULT ((void *)-2L)
#define MAIN_ONLY ((void *)-5L)
int guest_counter = 9;
int owned_value(void) { return 7; }
int main(void) {
    int (*widget)(void) = (int (*)(void))dlsym(DEFAULT, "anyios_widget");
    if (!widget || dlerror()) return 1;
    int answer = widget();
    if (dlsym(MAIN_ONLY, "anyios_widget") || !dlerror() || dlerror()) return 2;
    if (dlsym(DEFAULT, "missing_owned_symbol")) return 3;
    int (*own)(void) = (int (*)(void))dlsym(MAIN_ONLY, "owned_value");
    if (!own || dlerror()) return 4;
    int *counter = (int *)dlsym(MAIN_ONLY, "guest_counter");
    if (!counter || dlerror()) return 5;
    return answer + own() + *counter;
}
