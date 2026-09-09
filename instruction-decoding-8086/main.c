// https://www.computerenhance.com/p/instruction-decoding-on-the-8086
#include <stdio.h>
#include <fcntl.h>
#include <string.h>
#include <unistd.h>
#include <stdint.h>
#include <assert.h>

#define SIZE(X) sizeof(X) / sizeof(*(X))

typedef struct {
	char *left;
	char *right;
} Pair;

typedef Pair (DecoderFunc)(uint8_t*, const size_t, int*);

typedef struct {
	uint8_t pattern;
	uint8_t mask;
	char *name;
	char *description;
	DecoderFunc *decoder;
} Instruction;
Pair decode_mov_rm_r(uint8_t *bytes, const size_t size, int *i);
Pair decode_mov_ir(uint8_t *buffer, const size_t size, int *i);

// https://codeberg.org/bolt/8086-Users-Manual
// Page 265
Instruction instructions[] = {
	{.pattern = 0b10001000, .mask = 0b11111100, .name = "mov", .description = "Register/memory to/from register", .decoder = decode_mov_rm_r},
	{.pattern = 0b11000110, .mask = 0b11111110, .name = "mov", .description = "Immediate to register/memory", .decoder = NULL},
	{.pattern = 0b10110000, .mask = 0b11110000, .name = "mov", .description = "Immediate to register", .decoder = decode_mov_ir},
	{.pattern = 0b10100000, .mask = 0b11111110, .name = "mov", .description = "Memory to accumulator", .decoder = NULL},
	{.pattern = 0b10100010, .mask = 0b11111110, .name = "mov", .description = "Accumulator to memory", .decoder = NULL},
	{.pattern = 0b10001110, .mask = 0b11111111, .name = "mov", .description = "Register/memory to segment register", .decoder = NULL},
	{.pattern = 0b10001100, .mask = 0b11111111, .name = "mov", .description = "Segment register to register/memory", .decoder = NULL},
};
size_t instructions_size = SIZE(instructions);

// https://codeberg.org/bolt/8086-Users-Manual
// Page 263
char *registers[][2] = {
	{"al", "ax"},
	{"cl", "cx"},
	{"dl", "dx"},
	{"bl", "bx"},
	{"ah", "sp"},
	{"ch", "bp"},
	{"dh", "si"},
	{"bh", "di"},
};

char *ea_registers[][2] = {
	{"bx", "si"},
	{"bx", "di"},
	{"bp", "si"},
	{"bp", "di"},
	{NULL, "si"},
	{NULL, "di"},
	{"bp", NULL},
	{"bx", NULL},
};

void	hex_to_string(char *buffer, int *j, const uint16_t og_value)
{
	uint8_t value = og_value;
	int length = 1;
	while (value / 16)
	{
		value /= 16;
		++length;
	}

	value = og_value;
	for (int i = 0; i < length; ++i)
	{
		buffer[*j + length - i - 1] = "0123456789abcdef"[value % 16];
		value /= 16;
	}

	*j += length;
}

void	print_bits(const uint8_t value)
{
	printf("%02x: ", value);
	for (int i = 7; i >= 0; --i)
	{
		printf("%d", (value >> i) & 1);
	}
	printf("\n");
}

// Register/memory to/from register: 100010 d w
Pair	decode_mov_rm_r(uint8_t *buffer, const size_t size, int *i)
{
	assert(*i + 1 < size);
	uint8_t d = (buffer[*i] & 0b00000010) >> 1;
	uint8_t w = buffer[*i] & 0b00000001;

	uint8_t mod = (buffer[*i + 1] & 0b11000000) >> 6;
	uint8_t rm = buffer[*i + 1] & 0b00000111;
	uint8_t src_reg;
	uint8_t dst_reg;
	Pair pair;
	memset(&pair, 0, sizeof(pair));
	switch (mod)
	{
		case 0b00:
		{
			dst_reg = (buffer[*i + 1] >> 3) & 0b0000111;
			static char ea_calc[16];
			int j = 0;
			memset(ea_calc, 0, SIZE(ea_calc));
			memset(ea_calc, '[', 1);
			++j;
			if (rm == 0b110)
			{
				assert(*i + 3 < size);
				memcpy(ea_calc + j, "0x", 2);
				j += 2;
				hex_to_string(ea_calc, &j, buffer[*i + 3]);
				hex_to_string(ea_calc, &j, buffer[*i + 2]);
			}
			else
			{
				if (ea_registers[rm][0])
				{
					memcpy(ea_calc + j, ea_registers[rm][0], strlen(ea_registers[rm][0]));
					j += strlen(ea_registers[rm][0]);
				}
				if (ea_registers[rm][1])
				{
					if (ea_registers[rm][0])
					{
						memset(ea_calc + j, '+', 1);
						++j;
					}
					memcpy(ea_calc + j, ea_registers[rm][1], strlen(ea_registers[rm][1]));
					j += strlen(ea_registers[rm][1]);
				}
			}

			memset(ea_calc + j, ']', 1);
			++j;

			*i += rm == 0b110 ? 3 : 1;
			pair = (Pair){.left = registers[dst_reg][w], .right = ea_calc};
			break;
		}
		case 0b01:
		{
			assert(*i + 2 < size);

			dst_reg = (buffer[*i + 1] >> 3) & 0b0000111;
			static char ea_calc[32];
			int j = 0;
			memset(ea_calc, 0, SIZE(ea_calc));
			memset(ea_calc, '[', 1);
			++j;

			if (ea_registers[rm][0])
			{
				memcpy(ea_calc + j, ea_registers[rm][0], strlen(ea_registers[rm][0]));
				j += strlen(ea_registers[rm][0]);
			}
			if (ea_registers[rm][1])
			{
				if (ea_registers[rm][0])
				{
					memset(ea_calc + j, '+', 1);
					++j;
				}
				memcpy(ea_calc + j, ea_registers[rm][1], strlen(ea_registers[rm][1]));
				j += strlen(ea_registers[rm][1]);
			}

			if (buffer[*i + 2])
			{
				memcpy(ea_calc + j, "+0x", 3);
				j += 3;
				hex_to_string(ea_calc, &j, buffer[*i + 2]);
			}

			memset(ea_calc + j, ']', 1);
			++j;

			*i += 2;
			pair = (Pair){.left = registers[dst_reg][w], .right = ea_calc};
			break;
		}
		case 0b10:
		{
			assert(*i + 3 < size);

			dst_reg = (buffer[*i + 1] >> 3) & 0b0000111;
			static char ea_calc[32];
			int j = 0;
			memset(ea_calc, 0, SIZE(ea_calc));
			memset(ea_calc, '[', 1);
			++j;

			if (ea_registers[rm][0])
			{
				memcpy(ea_calc + j, ea_registers[rm][0], strlen(ea_registers[rm][0]));
				j += strlen(ea_registers[rm][0]);
			}
			if (ea_registers[rm][1])
			{
				if (ea_registers[rm][0])
				{
					memset(ea_calc + j, '+', 1);
					++j;
				}
				memcpy(ea_calc + j, ea_registers[rm][1], strlen(ea_registers[rm][1]));
				j += strlen(ea_registers[rm][1]);
			}

			memcpy(ea_calc + j, "+0x", 3);
			j += 3;
			hex_to_string(ea_calc, &j, buffer[*i + 3]);
			hex_to_string(ea_calc, &j, buffer[*i + 2]);

			memset(ea_calc + j, ']', 1);
			++j;

			*i += 3;
			pair = (Pair){.left = registers[dst_reg][w], .right = ea_calc};
			break;
		}
		case 0b11:
		{
			src_reg = (buffer[*i + 1] >> 3) & 0b0000111;
			dst_reg = buffer[*i + 1] & 0b00000111;

			*i += 1;
			pair = (Pair){.left = registers[src_reg][w], .right = registers[dst_reg][w]};
			break;
		}
		default:
			printf("UNREACHABLE\n");
			break;
	}

	return d ? pair : (Pair){.left = pair.right, .right = pair.left};
}

// Immediate to register: 1011 w reg
Pair	decode_mov_ir(uint8_t *buffer, const size_t size, int *i)
{
	assert(*i + 2 < size);
	uint8_t w = buffer[*i] & 0b00000001;
	uint8_t dst_reg = buffer[*i] & 0b00000111;

	static char value_buffer[16];
	memcpy(value_buffer, "0x", 2);
	int j = 2;
	if (w)
	{
		hex_to_string(value_buffer, &j, buffer[*i + 2]);
	}
	hex_to_string(value_buffer, &j, buffer[*i + 1]);

	*i += 2;
	return (Pair){.left = registers[dst_reg][w], .right = value_buffer};
}

void	decode_file(const char *filename)
{
	int fd = open(filename, O_RDONLY);
	if (fd < 0)
	{
		printf("[ERROR] Couldn't open the file\n");
		return;
	}

	uint8_t buffer[1024];
	size_t buffer_size = SIZE(buffer);
	memset(buffer, 0, buffer_size);
	ssize_t bytes_read = read(fd, buffer, buffer_size);
	close(fd);
	if (bytes_read < 0)
	{
		printf("[ERROR] Couldn't read the file");
		return;
	}

	printf("bits 16\n");
	for (int i = 0; i < bytes_read; ++i)
	{
		for (int j = 0; j < instructions_size; ++j)
		{
			if ((buffer[i] & instructions[j].mask) == instructions[j].pattern)
			{
				if (instructions[j].decoder)
				{
					Pair decoded_output = instructions[j].decoder(buffer, bytes_read, &i);
					printf("%s %s", instructions[j].name, decoded_output.left);
					if (decoded_output.right)
					{
						printf(", %s", decoded_output.right);
					}
					printf("\n");
				}
				else
				{
					//printf("Decoder for %s, %s not done yet\n", instructions[j].name, instructions[j].description);
				}
				break;
			}
		}
	}
}

int	main(int argc, char **argv)
{
	if (argc < 2)
	{
		printf("Usage: %s <file>...\n", argv[0]);
		return 1;
	}
	for (int i = 1; i < argc; ++i)
	{
		decode_file(argv[i]);
	}

	return 0;
}
