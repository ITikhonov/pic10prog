#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <unistd.h>

#include "libcliavr.h"

#define MCUCR 0x55 

#define PINB 0x23 
#define DDRB 0x24 
#define PORTB 0x25

#define DAT 0
#define CLK 1
#define VDD 7

void input(int x) { teensy_clrbits(DDRB,1<<x); }
void output(int x) { teensy_setbits(DDRB,1<<x); }

void b(int pin,int x) {
	if(x) { teensy_setbits(PORTB,1<<pin); }
	else  { teensy_clrbits(PORTB,1<<pin); }
}

void disablepullups() {
	teensy_setbits(MCUCR,0b10000);
}

void poweron() {
	b(VDD,1);
	output(VDD);
}

void poweroff() {
	input(VDD);
	b(VDD,0);
}

void bit(int x) {
	b(CLK,1);
	b(DAT,x);
	b(CLK,0);
}

int inp() {
	b(CLK,1);
	usleep(1);
	int x=teensy_readmem(PINB)&(1<<DAT);
	b(CLK,0);
	return x;
}

void incaddr() {
	output(DAT);
        bit(0);
        bit(1);
        bit(1);
        bit(0);
        bit(0);
        bit(0);
	input(DAT);
	usleep(1);
}

int readmem() {
	output(DAT);
        bit(0);
        bit(0);
        bit(1);
        bit(0);
        bit(0);
        bit(0);
	input(DAT);
	usleep(100);

	uint16_t x=0;
	inp();
	       if(inp()) x|=0x8000;
	x>>=1; if(inp()) x|=0x8000;
	x>>=1; if(inp()) x|=0x8000;

	x>>=1; if(inp()) x|=0x8000;
	x>>=1; if(inp()) x|=0x8000;
	x>>=1; if(inp()) x|=0x8000;
	x>>=1; if(inp()) x|=0x8000;

	x>>=1; if(inp()) x|=0x8000;
	x>>=1; if(inp()) x|=0x8000;
	x>>=1; if(inp()) x|=0x8000;
	x>>=1; if(inp()) x|=0x8000;

	x>>=1; if(inp()) x|=0x8000;
	x>>=4; inp(); inp(); inp();
	usleep(1);

	return x;
}

void backup(char *name) {
	FILE *f=fopen(name,"w");

	printf("CONFIG: %x\n",readmem());
	incaddr();

	int i;
	for(i=0;i<0x13f;i++) {
		uint16_t x=readmem();
		fwrite(&x,2,1,f);
		printf("%03x/13f (%3u%%): %04x\r",i,(i*100)/0x13f,x);
		fflush(stdout);
		incaddr();
	}

	fclose(f);
}

void erase() {
	output(DAT);
	bit(1);
	bit(0);
	bit(0);
	bit(1);
	bit(0);
	bit(0);
	input(DAT);
	usleep(10000); // 10ms
}

void load(int x) {
	output(DAT);
        bit(0);
        bit(1);
        bit(0);
        bit(0);
        bit(0);
        bit(0);
	usleep(1); // 10ms

	bit(0);
        int i;
	for(i=0;i<14;i++) {
                bit(x&1);
                x>>=1;
	}
	bit(0);
	input(DAT);
	usleep(1); // 10ms
}

void begin() {
	output(DAT);
        bit(0);
        bit(0);
        bit(0);
        bit(1);
        bit(0);
        bit(0);
	input(DAT);
	usleep(2);
}

void end() {
        output(DAT);
        bit(0);
        bit(1);
        bit(1);
        bit(1);
        bit(0);
        bit(0);
        input(DAT);
        usleep(1);
}

int prog(x) {
	load(x);
	begin();
	end();
	return readmem();
}

void program(char *name) {
	FILE *f=fopen(name,"r");
	incaddr();
	int addr=0;
	for(;;) {
		uint16_t x;

		printf("%03x/100 (%3u%%): %04x\r",addr,(addr*100)/0x100,x);
		fflush(stdout);

		if(fread(&x,2,1,f)!=1) break;

		int c=prog(x);
		incaddr();
		addr++;

		if(x!=c) {
			printf("\nERROR: programmed %04x but read %04x\n",c,x);
			break;
		}
	}
	fclose(f);
}


void oscal(char *v) {
	unsigned int x=strtol(v,0,16);
	if(x>0xff) {
		printf("OSCAL should be one byte only\n");
		printf("ex: for 220c in hex dump use 22. 0c is opcode for movlw here\n");
		return;
	}
	x|=0x0c00;
	incaddr();
	int addr=0;
	for(;addr!=0xff;) {
		printf("%03x/0ff (%3u%%)\r",addr,(addr*100)/0xff);
		fflush(stdout);

		incaddr();
		addr++;
	}

	unsigned int c=prog(x);
	if(x!=c) {
		printf("\nERROR: programmed %04x but read %04x\n",c,x);
	}
}

void usage(char *argv[]) {
	printf("Usage:\n");
	printf("    %s backup filename.bin\n",argv[0]);
	printf("    %s erase\n",argv[0]);
	printf("    %s program filename.bin\n",argv[0]);
	printf("    %s oscal hexbyte (ex: oscal 2f) \n",argv[0]);
}

int main(int argc, char *argv[]) {
	switch(argc>1?argv[1][0]:0) {
	case 'b': 
	case 'p': 
	case 'o': 
	case 'e': break;
	default: usage(argv); return 1;
	}

        if(!teensy_open()) { printf("error open\n"); return 1; }
	disablepullups();

	poweron();
	printf("apply 12V to MCLR and press enter");
	getchar();
	output(CLK);

	switch(argv[1][0]) {
	case 'b': backup(argv[2]); break;
	case 'p': program(argv[2]); break;
	case 'o': oscal(argv[2]); break;
	case 'e': erase(); break;
	}

	printf("remove 12V from MCLR and press enter");
	getchar();
	poweroff();
	return 0;
}

