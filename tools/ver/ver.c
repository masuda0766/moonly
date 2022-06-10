#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>

int main(int argc, const char * argv[]) {

	FILE *fp;
	char *st;
	char *end;
	char ver[256] = {0};
	char buf[1024 + 1];

	if(argc!=3){
		printf("File is not specified.\n");
		return 0;
	}

	fp = fopen(argv[1], "r");
	if(fp == NULL) {
		printf("File open fail\n");
		return 0;
	}

	while (fgets(buf, 1024, fp)) {
		if(strstr(buf, "AUDREY_VERSION") == NULL) {
			continue;
		}
		if((st = strstr(buf, "\"")) == NULL) {
			printf("Incorrect version information\n");
		}
		st++;
		if((end = strstr(st, "\"")) == NULL) {
			printf("Incorrect No version information 2\n");
		}
		strncpy(ver, st, end - st);
		strcat(ver, ".bin");
		remove(ver);
		if(rename(argv[2], ver) == 0) {
			printf("OTA file : %s\n", ver);
		}
		fclose(fp);
		return 0;
	}
	printf("No version information\n");
	fclose(fp);
	return 0;

}


