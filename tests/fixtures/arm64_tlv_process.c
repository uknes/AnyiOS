// Project-owned, SDK-free Apple ARM64 TLV fixture.
// Clang emits __thread_vars descriptors calling __tlv_bootstrap.
__thread int anyios_counter = 7;
__thread int anyios_zero_value;

int main(void) {
    anyios_counter += 5;
    anyios_zero_value = anyios_counter;
    return anyios_zero_value;
}
