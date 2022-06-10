#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>

uint32_t crc_table[256];

int main(int argc, const char * argv[]) {

    FILE *fp;

	if(argc!=2){
		printf("File is not specified.\n");
		return 0;
	}

	fp = fopen(argv[1], "rb");
	if(fp == NULL) {
		printf("File open fail\n");
		return 0;
	}

    for (uint32_t i = 0; i < 256; i++) {
        uint32_t c = i;
        for (int j = 0; j < 8; j++) {
            c = (c & 1) ? (0xEDB88320 ^ (c >> 1)) : (c >> 1);
        }
        crc_table[i] = c;
    }

	int ch;
    uint32_t crc = 0xFFFFFFFF;
    uint32_t sum = 0;
	while (( ch = fgetc(fp)) != EOF ) {
		crc = crc_table[(crc ^ (uint32_t)ch) & 0xFF] ^ (crc >> 8);
		sum += (uint32_t)ch;
	}
	crc ^= 0xFFFFFFFF;

	sum += ((crc >> 24) & 0x000000ff) + ((crc >> 16) & 0x000000ff) + ((crc >> 8) & 0x000000ff) + (crc & 0x000000ff);

	printf("CRC:%08x, SUM:%08x\n", crc, sum);

	fclose(fp);

	fp = fopen(argv[1], "ab");
	fputc(crc & 0x000000ff, fp);
	fputc((crc >> 8) & 0x000000ff & 0x000000ff, fp);
	fputc((crc >> 16) & 0x000000ff & 0x000000ff, fp);
	fputc((crc >> 24) & 0x000000ff & 0x000000ff, fp);
	fputc(sum & 0x000000ff, fp);
	fputc((sum >> 8) & 0x000000ff & 0x000000ff, fp);
	fputc((sum >> 16) & 0x000000ff & 0x000000ff, fp);
	fputc((sum >> 24) & 0x000000ff & 0x000000ff, fp);
	fclose(fp);

    return 0;

}


