/*
 * Copyright (C) 2009-2012 Chris McClelland
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU Lesser General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */
#include <stdio.h>
#include <stdlib.h>
#include <libfpgalink.h>
#include "args.h"

#define CHECK(x) if ( status != FL_SUCCESS ) { FAIL(x); }

int main(int argc, const char *argv[]) {
	int returnCode;
	struct FLContext *handle = NULL;
	FLStatus status;
	const char *error = NULL;
	uint8 byte = 0x10;
	uint8 buf[256];
	bool flag;
	bool isNeroCapable, isCommCapable;
	uint32 fileLen;
	uint8 *buffer = NULL;
	uint32 numDevices, scanChain[16], i;
	const char *vp = NULL, *ivp = NULL, *queryPort = NULL, *portConfig = NULL, *progConfig = NULL, *dataFile = NULL;
	const char *const prog = argv[0];

	printf("FPGALink \"C\" Example Copyright (C) 2011-2013 Chris McClelland\n\n");
	argv++;
	argc--;
	while ( argc ) {
		if ( argv[0][0] != '-' ) {
			unexpected(prog, *argv);
			FAIL(1);
		}
		switch ( argv[0][1] ) {
		case 'h':
			usage(prog);
			FAIL(0);
			break;
		case 'q':
			GET_ARG("q", queryPort, 2);
			break;
		case 'd':
			GET_ARG("d", portConfig, 3);
			break;
		case 'v':
			GET_ARG("v", vp, 4);
			break;
		case 'i':
			GET_ARG("i", ivp, 5);
			break;
		case 'p':
			GET_ARG("p", progConfig, 6);
			break;
		case 'f':
			GET_ARG("f", dataFile, 7);
			break;
		default:
			invalid(prog, argv[0][1]);
			FAIL(8);
		}
		argv++;
		argc--;
	}
	if ( !vp ) {
		missing(prog, "v <VID:PID>");
		FAIL(9);
	}

	status = flInitialise(0, &error);
	CHECK(10);
	
	printf("Attempting to open connection to FPGALink device %s...\n", vp);
	status = flOpen(vp, &handle, NULL);
	if ( status ) {
		if ( ivp ) {
			int count = 60;
			printf("Loading firmware into %s...\n", ivp);
			status = flLoadStandardFirmware(ivp, vp, &error);
			CHECK(11);
			
			printf("Awaiting renumeration");
			flSleep(1000);
			do {
				printf(".");
				fflush(stdout);
				flSleep(100);
				status = flIsDeviceAvailable(vp, &flag, &error);
				CHECK(12);
				count--;
			} while ( !flag && count );
			printf("\n");
			if ( !flag ) {
				fprintf(stderr, "FPGALink device did not renumerate properly as %s\n", vp);
				FAIL(13);
			}
			
			printf("Attempting to open connection to FPGLink device %s again...\n", vp);
			status = flOpen(vp, &handle, &error);
			CHECK(14);
		} else {
			fprintf(stderr, "Could not open FPGALink device at %s and no initial VID:PID was supplied\n", vp);
			FAIL(15);
		}
	}
	
	if ( portConfig ) {
		printf("Configuring ports...\n");
		status = flPortConfig(handle, portConfig, &error);
		CHECK(16);
		flSleep(100);
	}

	isNeroCapable = flIsNeroCapable(handle);
	isCommCapable = flIsCommCapable(handle);
	if ( queryPort ) {
		if ( isNeroCapable ) {
			status = jtagScanChain(handle, queryPort, &numDevices, scanChain, 16, &error);
			CHECK(17);
			if ( numDevices ) {
				printf("The FPGALink device at %s scanned its JTAG chain, yielding:\n", vp);
				for ( i = 0; i < numDevices; i++ ) {
					printf("  0x%08X\n", scanChain[i]);
				}
			} else {
				printf("The FPGALink device at %s scanned its JTAG chain but did not find any attached devices\n", vp);
			}
		} else {
			fprintf(stderr, "JTAG chain scan requested but FPGALink device at %s does not support NeroJTAG\n", vp);
			FAIL(18);
		}
	}

	if ( progConfig ) {
		printf("Executing programming configuration \"%s\" on FPGALink device %s...\n", progConfig, vp);
		if ( isNeroCapable ) {
			status = flProgram(handle, progConfig, NULL, &error);
			CHECK(19);
		} else {
			fprintf(stderr, "Program operation requested but device at %s does not support NeroProg\n", vp);
			FAIL(20);
		}
	}
	
	if ( dataFile ) {
		if ( isCommCapable ) {
			printf("Enabling FIFO mode...\n");
			status = flFifoMode(handle, true, &error);
			CHECK(21);
			printf("Zeroing registers 1 & 2...\n");
			byte = 0x00;
			status = flWriteChannel(handle, 1000, 0x01, 1, &byte, &error);
			CHECK(22);
			status = flWriteChannel(handle, 1000, 0x02, 1, &byte, &error);
			CHECK(23);
			
			buffer = flLoadFile(dataFile, &fileLen);
			if ( buffer ) {
				uint16 checksum = 0x0000;
				uint32 i;
				for ( i = 0; i < fileLen; i++ ) {
					checksum += buffer[i];
				}
				printf(
					"Writing %0.2f MiB (checksum 0x%04X) from %s to FPGALink device %s...\n",
					(double)fileLen/(1024*1024), checksum, dataFile, vp);
				status = flWriteChannel(handle, 30000, 0x00, fileLen, buffer, &error);
				CHECK(24);
			} else {
				fprintf(stderr, "Unable to load file %s!\n", dataFile);
				FAIL(25);
			}
			printf("Reading channel 0...");
			status = flReadChannel(handle, 1000, 0x00, 1, buf, &error);
			CHECK(26);
			printf("got 0x%02X\n", buf[0]);
			printf("Reading channel 1...");
			status = flReadChannel(handle, 1000, 0x01, 1, buf, &error);
			CHECK(27);
			printf("got 0x%02X\n", buf[0]);
			printf("Reading channel 2...");
			status = flReadChannel(handle, 1000, 0x02, 1, buf, &error);
			CHECK(28);
			printf("got 0x%02X\n", buf[0]);
		} else {
			fprintf(stderr, "Data file load requested but device at %s does not support CommFPGA\n", vp);
			FAIL(29);
		}
	}
	returnCode = 0;

cleanup:
	if ( error ) {
		fprintf(stderr, "%s\n", error);
		flFreeError(error);
	}
	flFreeFile(buffer);
	flClose(handle);
	return returnCode;
}

void usage(const char *prog) {
	printf("Usage: %s [-h] [-i <VID:PID>] -v <VID:PID> [-d <portConfig>]\n", prog);
	printf("          [-q <jtagPort>] [-p <progConfig>] [-f <dataFile>]\n\n");
	printf("Load FX2LP firmware, load the FPGA, interact with the FPGA.\n\n");
	printf("  -i <VID:PID>    initial vendor and product ID of the FPGALink device\n");
	printf("  -v <VID:PID>    renumerated vendor and product ID of the FPGALink device\n");
	printf("  -d <portConfig> configure the ports\n");
	printf("  -q <jtagPort>   scan the JTAG chain\n");
	printf("  -p <progConfig> configuration and programming file\n");
	printf("  -f <dataFile>   binary data to write to channel 0\n");
	printf("  -h              print this help and exit\n");
}
