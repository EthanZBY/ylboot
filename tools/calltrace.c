#include <stdio.h>
#include <stdbool.h>
#include <string.h>
//using namespace std;

bool isHexChar(char c){
	if (c >= '0' && c <= '9')
		return true;
	if (c >= 'A' && c <= 'F')
		return true;
	if (c >= 'a' && c <= 'f')
		return true;
	return false;
}

bool HexToTen(char *target, long long *pret)
{
	long long ret = 0, d = 0;
	
	while((*target != 0) && !isHexChar(*target))
		target++;
	while(*target != 0){
		char ch = *target;
		d = 0;
		if(ch >= '0' && ch <= '9')
			d = ch - '0';
		else if(ch >= 'a' && ch <= 'f')
			d = 10 + ch - 'a';
		else if(ch > 'f' && ch <= 'z')
			return false;
		else
			break;
		ret = 16*ret + d;
		target++;
	}
	*pret = ret;
	return true;
}


void TenToHex(long long add, char *offset)
{
	char res[40];
	int l=0, d=0, cnt=0;
	
	while(add){
		d = add%16;
		if(d >= 0 && d <= 9)
			res[l++] = d + '0';
		else if(d >= 10 && d <= 15)
			res[l++] = d - 10 + 'a';
		add = add/16;
	}
	while (l<6)
		res[l++] = '0';
	for(l=l-1; l>=0; l--)
		offset[cnt++] = res[l];
	offset[cnt++] = 0;
}

bool Parse(char *buffer, long long *pend) {
	while (*buffer == ' ')
		buffer++;
	char *sep = strstr(buffer, " ");
	if (sep != NULL) {
		return HexToTen(buffer, pend);
	}else
		return false;
}

bool Find(char *filename, long long address, char* output){
	FILE *pfile = fopen(filename, "r");
	if (pfile == NULL) {
		return false;			
	}
	long long start = 0, end = 0;
	char buffer1[256];
	char buffer2[256];
	buffer1[0] = 0;
	char *buf1 = buffer1;
	char *buf2 = buffer2;
	while (fgets(buf2, sizeof(buffer2), pfile) != NULL){
		if (Parse(buf2, &end)) {
			if (address >= start && address < end) {
				buf1[strlen(buf1)-1] = 0;		//去掉换行符 
				strcpy(output, buf1);
				char offset[40];
				TenToHex(address-start, offset);
				strcat(output, " +");
				strcat(output, offset);
				TenToHex(end-start, offset);
				strcat(output, ", ");
				strcat(output, offset);
				fclose(pfile);
				return true;
			}
			start = end;
			char *buf = buf1;
			buf1 = buf2;
			buf2 = buf;
		}
	}
	
	fclose(pfile);
	return false;
}

void TraceDumpFile(char *dump_file, char *map_file){
	char line[256], output[300]; 
	char *begin;
	long long address = 0;
	
	FILE *pfile = fopen(dump_file, "r");
	if (pfile == NULL) {
		printf("Cannot open %s.\n", dump_file);
		return;			
	}
	while (fgets(line, sizeof(line), pfile) != NULL){
		begin = line;
		while (*begin == ' ')  begin++;				//去掉行首的空格
		if(begin[0] == '\n') {
			printf("\n");
			continue;
		}			
		char *bracket = strstr(begin, "[<");
		if (bracket == NULL)
			bracket = begin;
		begin[strlen(begin)-1] = 0;					//去掉行末的换行符 
		if (Parse(bracket, &address) && address != 0) {
			if (Find(map_file, address, output)){
				printf("%s: %s\n", begin, output);
			} else {
				printf("%s: Cannot find any matched symbol.\n", begin);
			}
		} else {
			printf("%s: invalid address\n", begin);
		}
	}
	
	fclose(pfile);
	return;
}


void InteractiveTrace(char *map_file) {
	char output[300]; 
	char target[256]; 
	long long address = 0;
	printf("You have now entered interactive mode. To exit it, enter '0' or 'exit'.\n");
	while (1) {
		printf(">: ");
		fgets(target, sizeof(target), stdin);
		if(strcmp(target, "0\n") == 0 || strcmp(target, "exit\n") == 0){
			break;
		}
		char *begin = strstr(target, "[<");
		if(begin == NULL)
			begin = target;
		if(!HexToTen(begin, &address) || address == 0){
			printf("Invalid address.\n");
			continue;
		}
		if (!Find(map_file, address, output))
			printf("Cannot find any matched symbol.\n");
		else{
			printf("%s\n", output);
		}
	}
}

void showHowToUse(){
	printf("Usage: calltrace [error_file] [map_file]\n"
			"Example: calltrace dump.txt system.map\n"
			"Example: calltrace dump.txt (using system.map by default)\n"
			"Example: calltrace abc.map (interactive mode)\n"
			"Example: calltrace (interactive mode, using system.map by default)\n");
}

int main(int argc, char *argv[])
{
	if (argc > 1 &&
		(strcmp(argv[1], "?") == 0
		|| strcmp(argv[1], "-help") == 0
		|| strcmp(argv[1], "--help") == 0)) {
		showHowToUse();
		return 0;
	}else if (argc > 1 && strncmp(argv[1], "-a", 2) == 0) {
		printf("#Copyright (c) 2025 Gedu Technology Co., Ltd\n");
		return 0;
	}else{
		char *dump_file = NULL;
		char *map_file = "System.map";
		if (argc == 2) {
			if (strstr(argv[1],".map") != NULL) 
				map_file = argv[1];
			else 
				dump_file = argv[1];
		} else if (argc == 3) {
			if (strstr(argv[2],".map") != NULL) {		
				map_file = argv[2];
				dump_file = argv[1];
			} else if (strstr(argv[1],".map") != NULL){
				map_file = argv[1];
				dump_file = argv[2];
			} else {
				printf("One must be map file.\n");
				showHowToUse();
				return 0;
			}
		} else if (argc > 3) {
			printf("Too many arguments.\n");
			showHowToUse();
			return 0;
		}
		
		FILE *pfile = fopen(map_file, "r");
		if (pfile == NULL) {
			printf("Cannot open %s.\n", map_file);
			return 0;			
		}else
			fclose(pfile);
		
		if(dump_file != NULL) 
			TraceDumpFile(dump_file, map_file);
		else
			InteractiveTrace(map_file);
	}
	return 0;
}

