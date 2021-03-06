//
//  main.c
//  PDFCrack
//
//  Created by Alex Nichol on 8/15/11.
//  Copyright 2011 __MyCompanyName__. All rights reserved.
//

#include <stdio.h>
#include <string.h>
#include <openssl/md5.h>
#include <openssl/rc4.h>
#include "PDFReader.h"

#define kPasswordMax 8

// cracking state
FILE * dictionaryFp = NULL;
const char * BruteForceChars[] = {"", "a", "b", "c", "d", "e", "f", "g", "h", "i", "j", "k", "l", "m", "n", "o", "p", "q", "r", "s", "t", "u", "v", "w", "x", "y", "z", "A", "B", "C", "D", "E", "F", "G", "H", "I", "J", "K", "L", "M", "N", "O", "P", "Q", "R", "S", "T", "U", "V", "W", "X", "Y", "Z", "0", "1", "2", "3", "4", "5", "6", "7", "8", "9"};
int BFCharsVector[] = {0, 0, 0, 0, 0, 0, 0, 0}; // empty string

#define kNumCharacters 63

// encryption variables/constants
const unsigned char AdobeEncString[32] = {0x28, 0xBF, 0x4E, 0x5E, 0x4E, 0x75, 0x8A, 0x41,
	0x64, 0x00, 0x4E, 0x56, 0xFF, 0xFA, 0x01, 0x08,
	0x2E, 0x2E, 0x00, 0xB6, 0xD0, 0x68, 0x3E, 0x80,
	0x2F, 0x0C, 0xA9, 0xFE, 0x64, 0x53, 0x69, 0x7A};


void strTrim (char * buffer) {
	if (strlen(buffer) > 0) {
		if (buffer[strlen(buffer) - 1] == '\n') {
			buffer[strlen(buffer) - 1] = 0;
		}
	}
	if (strlen(buffer) > 0) {
		if (buffer[strlen(buffer) - 1] == '\r') {
			buffer[strlen(buffer) - 1] = 0;
		}
	}
}

// call to get the next password from either the wordlist
// or the brute force pool...
bool getNextPassword (char * destionation, int maxLength) {
	if (dictionaryFp) {
		int len = 0;
		while (len < maxLength - 1) {
			int aChar = fgetc(dictionaryFp);
			if (aChar == EOF) {
				if (len == 0) return false;
				else break;
			}
			if (aChar == '\n') break;
			if (aChar != '\r') {
				destionation[len++] = (char)aChar;
				destionation[len] = 0; // NULL terminate
			}
		}
		return true;
	} else {
		// get a string
		int index = 0;
		for (int i = 0; i < kPasswordMax; i++) {
			sprintf(&destionation[index], "%s", BruteForceChars[BFCharsVector[i]]);
			index += strlen(BruteForceChars[BFCharsVector[i]]);
		}
		// increment all of the values
		for (int i = kPasswordMax - 1; i >= 0; i--) {
			BFCharsVector[i] += 1;
			if (BFCharsVector[i] < kNumCharacters) break;
			else {
				BFCharsVector[i] = 0; // have it roll over to the next one
				if (i == 0) return false;
			}
		}
		return true;
	}
}

// Generate a new work area for cracking
char * workareaAlloc (const char * docID, int docIDLen, const unsigned char * ownerHash, int perms, int * workLen) {
	char * intBuffer = (char *)&perms;
	char * keyUnhash = (char *)malloc(32 + 32 + 4 + docIDLen);
	int i;
	for (i = 0; i < 32; i++) { keyUnhash[i] = AdobeEncString[i]; }
	for (i = 32; i < 64; i++) {
		keyUnhash[i] = ownerHash[i - 32];
	}
	for (i = 64; i < 68; i++) {
		keyUnhash[i] = intBuffer[i - 64];
	}
	for (i = 68; i < 68 + docIDLen; i++) {
		keyUnhash[i] = docID[i - 68];
	}
	*workLen = 32 + 32 + 4 + docIDLen;
	return keyUnhash;
}

// Check a password in an allocated work area
bool workareaCheckPass (const char * userPass, const unsigned char * userHash, char * workArea, int workLen) {
	unsigned char md5Buff[16];
	unsigned char theKey[5];
	unsigned char destination[32];
	int i;
	for (i = (int)strlen(userPass); i < 32; i++) { workArea[i] = AdobeEncString[i - (int)strlen(userPass)]; }
	for (i = 0; i < (int)strlen(userPass); i++) {
		workArea[i] = userPass[i];
	}
	MD5((const unsigned char *)workArea, workLen, md5Buff);
	theKey[0] = md5Buff[0];
	theKey[1] = md5Buff[1];
	theKey[2] = md5Buff[2];
	theKey[3] = md5Buff[3];
	theKey[4] = md5Buff[4];
	RC4_KEY key;
	RC4_set_key(&key, 5, theKey);
	RC4(&key, 32, userHash, destination);
	for (i = 0; i < 32; i++) {
		if (destination[i] != AdobeEncString[i]) return false;
	}
	return true;
}

int main (int argc, const char * argv[]) {
	const char * pdfPath = NULL;
	const char * dictionaryPath = NULL;
	PDFReader reader;
	unsigned char docID[512];
	unsigned char userPassword[32];
	unsigned char ownerPassword[32];
	char testPassword[512];
	int docIDLength = 0, permissions = 0;
	
	if (argc >= 2) {
		int i;
		for (i = 1; i < argc - 1; i++) {
			if (strcmp(argv[i], "-d") == 0) {
				dictionaryPath = argv[++i];
			} else if (strcmp(argv[i], "--stdin") == 0) {
				dictionaryFp = stdin;
			} else {
				fprintf(stderr, "Invalid option: %s\n", argv[i]);
				fflush(stderr);
				return -1;
			}
		}
		pdfPath = argv[argc - 1];
	} else {
		fprintf(stderr, "Usage: %s [-d dictionary | --stdin] <pdf file>\n", argv[0]);
		fflush(stderr);
		return -1;
	}
	
	if (!pdfPath) {
		fprintf(stderr, "Please specify a PDF file.\n");
		fflush(stderr);
		return 1;
	}
	if (!PDFReaderNew(&reader, pdfPath)) {
		fprintf(stderr, "Failed to open: %s\n", pdfPath);
		fflush(stderr);
		return 1;
	}
	
	docID[0] = 0;
	if ((docIDLength = PDFReaderGetID(reader, docID, 512)) == 0) {
		fprintf(stderr, "Failed to get document ID\n");
		fflush(stderr);
		return 1;
	}
	
	printf("Got ID of %d bytes: ", docIDLength);
	for (int i = 0; i < docIDLength; i++) {
		printf("%02x", docID[i]);
	}
	printf("\n");
	
	if (!PDFReaderGetFlags(reader, &permissions)) {
		fprintf(stderr, "Failed to get flags\n");
		fflush(stderr);
		return 1;
	}
	printf("Permissions: %d\n", permissions);
	
	if (!PDFReaderGetUserPass(reader, userPassword)) {
		fprintf(stderr, "Failed to get user password hash\n");
		fflush(stderr);
		return 1;
	}
	
	printf("Got user hash: ");
	for (int i = 0; i < 32; i++) {
		printf("%02x", userPassword[i]);
	}
	printf("\n");
	
	if (!PDFReaderGetOwnerPass(reader, ownerPassword)) {
		fprintf(stderr, "Failed to get owner password hash\n");
		fflush(stderr);
		return 1;
	}
	
	printf("Got owner hash: ");
	for (int i = 0; i < 32; i++) {
		printf("%02x", ownerPassword[i]);
	}
	printf("\n");
	
	PDFReaderClose(reader);
	
	if (dictionaryPath) {
		printf("Using dictionary file: %s\n", dictionaryPath);
		dictionaryFp = fopen(dictionaryPath, "r");
	} else {
		printf("Using raw alpha-numeric brute force.\n");
	}
	
	int workLen = 0;
	char * workArea = workareaAlloc((const char *)docID, docIDLength, ownerPassword, permissions, &workLen);
	
	unsigned long long missed = 0;
	while (getNextPassword(testPassword, 512)) {
		if (workareaCheckPass(testPassword, userPassword, workArea, workLen)) {
			printf("Found password: %s\n", testPassword);
			free(workArea);
			return 0;
		} else {
			missed ++;
			if (missed % 30000 == 0) {
				printf("Tried %lld passwords.  Current password: %s\n", missed, testPassword);
			}
		}
	}
	
	free(workArea);
	
	if (dictionaryFp != stdin) fclose(dictionaryFp);
	printf("Password not found.\n");
	
    return 0;
}

