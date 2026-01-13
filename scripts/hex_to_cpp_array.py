import sys
input_hex_str = sys.argv[1]
print("{" + ", ".join(f"0x{input_hex_str[i:i+2]}" for i in range(0, len(input_hex_str), 2)) + "};")
