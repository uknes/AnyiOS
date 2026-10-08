import pathlib
import struct
import sys


def main():
    destination = pathlib.Path(sys.argv[1])
    destination.mkdir(parents=True, exist_ok=True)
    thin = bytearray(32)
    struct.pack_into("<IIIIIIII", thin, 0, 0xFEEDFACF, 0x0100000C, 0, 1, 0, 0, 0, 0)
    (destination / "thin").write_bytes(thin)
    fat = bytearray(64)
    struct.pack_into(">II", fat, 0, 0xCAFEBABE, 1)
    struct.pack_into(">IIIII", fat, 8, 0x0100000C, 0, 32, len(thin), 3)
    fat[32:] = thin
    (destination / "fat").write_bytes(fat)
    chain = bytearray(88)
    chain[:len(thin)] = thin
    struct.pack_into("<II", chain, 16, 1, 16)
    struct.pack_into("<IIII", chain, 32, 0x80000034, 16, 48, 40)
    struct.pack_into("<IIIIIII", chain, 48, 0, 28, 32, 36, 1, 1, 0)
    chain[84:87] = b"foo"
    (destination / "chained").write_bytes(chain)
    export = bytearray(57)
    export[:len(thin)] = thin
    struct.pack_into("<II", export, 16, 1, 16)
    struct.pack_into("<IIII", export, 32, 0x80000033, 16, 48, 9)
    export[48:] = bytes([0, 1, 97, 0, 5, 2, 0, 42, 0])
    (destination / "exports").write_bytes(export)
    print(f"Generated {len(list(destination.iterdir()))} owned Mach-O corpus seeds")


if __name__ == "__main__":
    main()
