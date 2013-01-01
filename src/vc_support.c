/*
 * vc_support.c
 *
 *  Created on: 3 Dec 2012
 *      Author: Simon
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "xf86.h"
#include "xf86_OSproc.h"

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <sys/mman.h>
#include <sys/ioctl.h>

#include "arch/arm/mach-bcm2708/include/mach/vcio.h"

#include "generic_types.h"

static FILE *g_mboxFile = 0;
void *g_vpuCode = 0;
unsigned int g_vpuCodeSize = 0;

int g_numSymbols = 0;
static struct Symbol
{
	const char *m_pName;
	unsigned int m_offset;
	unsigned int m_export;
} *g_pSymbolTable = 0;

//send a command to the property mailbox
static int MboxProperty(FILE *fp, void *buf)
{
	MY_ASSERT(g_mboxFile);
	return ioctl(fileno(fp), IOCTL_MBOX_PROPERTY, buf);
}

//patch in a binary over a branch with link in the main elf
static void PatchElf(int newPosition, int oldPosition)
{
	unsigned short *opcode = (unsigned short *)(&((unsigned char *)g_vpuCode)[oldPosition]);
	MY_ASSERT(*opcode == 0x9080);

	unsigned short *ptr = (unsigned short *)(&((unsigned char *)g_vpuCode)[oldPosition + 2]);
	*ptr = (newPosition - oldPosition) / 2;
}

//clear the symbol table by setting the name to null
static void ClearSymbols(void)
{
	if (!g_pSymbolTable)
		return;

	free(g_pSymbolTable);
	g_pSymbolTable = 0;
	g_numSymbols = 0;
}

//copy a symbol into the table, linking the name into the image
static void AddSymbol(int index, const char *pName, unsigned int offset, unsigned int ex)
{
	MY_ASSERT(index == g_numSymbols);
	g_numSymbols++;

	g_pSymbolTable = realloc(g_pSymbolTable, g_numSymbols * sizeof(struct Symbol));
	MY_ASSERT(g_pSymbolTable);

	g_pSymbolTable[index].m_pName = pName;
	g_pSymbolTable[index].m_offset = offset;
	g_pSymbolTable[index].m_export = ex;
}

//search the symbol table for a matching name
int FindSymbolByName(const char *pName, unsigned int *pOffset)
{
	MY_ASSERT(pOffset);
	MY_ASSERT(pName);

	for (int count = 0; count < g_numSymbols; count++)
		if (g_pSymbolTable[count].m_pName)
			if (strcmp(g_pSymbolTable[count].m_pName, pName) == 0 && g_pSymbolTable[count].m_export == 1)			//make sure the name matches and it's a globally-visible symbol
			{
				*pOffset = g_pSymbolTable[count].m_offset;
				return 0;
			}

	return 1;
}

static void DumpSymbolTable(void)
{
	xf86DrvMsg(0, X_INFO, "Symbol table dump\n");

	for (int count = 0; count < g_numSymbols; count++)
		if (g_pSymbolTable[count].m_pName)
			xf86DrvMsg(0, X_INFO, "%3d: %5x %s %s\n", count, g_pSymbolTable[count].m_offset, g_pSymbolTable[count].m_export ? "TRUE " : "FALSE", g_pSymbolTable[count].m_pName);

	xf86DrvMsg(0, X_INFO, "End symbol table dump\n");
}

int OpenVcMbox(const char *filename)
{
	//no sense in opening it twice
	if (g_mboxFile)
		return 0;

	g_mboxFile = fopen(filename, "r+b");

	ClearSymbols();

	return g_mboxFile == 0 ? 1 : 0;
}

void CloseVcMbox(void)
{
	//or closing it
	if (!g_mboxFile)
		return;

	fclose(g_mboxFile);
	g_mboxFile = 0;

	ClearSymbols();
}

//returns the number of bytes required to be allocated in linear gpu-visible memory
unsigned int LoadVcCode(const char *filename, int isElf, int patchAddress)
{
	//basic tests
	if (!filename)
		return 0;
	//open the file
	FILE *fp = fopen(filename, "rb");
	if (!fp)
	{
		xf86DrvMsg(0, X_ERROR, "Failed to open binary file %s\n", filename);
		return 0;
	}

	if (!isElf)	//flat binary
	{
		//get the file size
		fseek(fp, 0, SEEK_END);

		unsigned int length = ftell(fp);
		rewind(fp);

		//load the file
		g_vpuCode = realloc(g_vpuCode, g_vpuCodeSize + length);

		if (fread((char *)g_vpuCode + g_vpuCodeSize, 1, length, fp) != length)
		{
			MY_ASSERT(0);		//failed to read the whole file?!?
		}

		fclose(fp);

		xf86DrvMsg(0, X_CONFIG, "Loaded VPU binary %s at offset %d\n", filename, g_vpuCodeSize);

		//patch if appropriate
		if (patchAddress > 0)
		{
			xf86DrvMsg(0, X_CONFIG, "Patching sub binary into address %x\n", patchAddress);
			PatchElf(g_vpuCodeSize, patchAddress);
		}

		g_vpuCodeSize += length;

		return length;
	}
	else
	{	//well, not an actual elf
		//so check the marker
		int marker;
		if (fread(&marker, 4, 1, fp) != 1)
		{
			xf86DrvMsg(0, X_ERROR, "failed to read file marker\n");
			fclose(fp);
			return 0;
		}

		if (marker != 0xc0dec0de)
		{
			xf86DrvMsg(0, X_ERROR, "binary does not appear to be loadable\n");
			fclose(fp);
			return 0;
		}

		//read the format version number
		int version;
		if (fread(&version, 4, 1, fp) != 1)
		{
			xf86DrvMsg(0, X_ERROR, "failed to read version number\n");
			fclose(fp);
			return 0;
		}

		if (version != 2)
		{
			xf86DrvMsg(0, X_ERROR, "incorrect format version number, %d\n", version);
			fclose(fp);
			return 0;
		}

		//get the file size
		fseek(fp, 0, SEEK_END);

		unsigned int length = ftell(fp);
		rewind(fp);

		//rewind to the beginning of the file and copy the lot into memory
		//load the file
		g_vpuCode = realloc(g_vpuCode, g_vpuCodeSize + length);

		if (fread((char *)g_vpuCode + g_vpuCodeSize, 1, length, fp) != length)
		{
			MY_ASSERT(0);		//failed to read the whole file?!?
		}

		fclose(fp);

		xf86DrvMsg(0, X_CONFIG, "Loaded VPU binary %s at offset %d\n", filename, g_vpuCodeSize);

		//now build the symbol table
		int headerOffset = ((int *)g_vpuCode)[2];		//offset in bytes
		int *pNumSymbols = (unsigned int *)&((char *)g_vpuCode)[headerOffset];

		//check the location of the header
		if (headerOffset <= 0)
		{
			xf86DrvMsg(0, X_ERROR, "header is at invalid offset, %d\n", headerOffset);
			fclose(fp);
			return 0;
		}

		if ((unsigned int)headerOffset >= length)
		{
			xf86DrvMsg(0, X_ERROR, "header points beyond the end of the file, %d versus\n", headerOffset, length);
			fclose(fp);
			return 0;
		}

		//check the number of symbols is legit
		if (*pNumSymbols <= 0 || *pNumSymbols > 1000)
		{
			xf86DrvMsg(0, X_ERROR, "symbol table has %d symbols\n", *pNumSymbols);
			fclose(fp);
			return 0;
		}

		//beginning of the table
		unsigned char *ptr = (unsigned char *)pNumSymbols;
		ptr += 4;

		for (int count = 0; count < *pNumSymbols; count++)
		{
			int symbolNo = *(int *)ptr;

			MY_ASSERT(symbolNo == count);
			ptr += 4;

			int offset = *(int *)ptr;
			MY_ASSERT(offset >= 0 && offset < length);
			ptr += 4;

			int ex = *(int *)ptr;
			MY_ASSERT(ex == 0 || ex == 1);
			ptr += 4;

			int numChars = *(int *)ptr;
			//check for something reasonable
			MY_ASSERT(numChars >= 0 && numChars < 200);
			ptr += 4;

			AddSymbol(count, (const char *)ptr, (unsigned int)(offset + g_vpuCodeSize), ex);

			ptr += numChars;
		}

		DumpSymbolTable();

		g_vpuCodeSize += length;

		return length;
	}
}

//free our local copy
void UnloadVcCode(void)
{
	if (!g_vpuCode)
		return;

	free(g_vpuCode);
	g_vpuCode = 0;
}

//copy into our mapped area
int UploadVcCode(void *pBase)
{
	//checks
	if (!pBase || !g_vpuCode || !g_vpuCodeSize)
		return 1;

	//upload the memory
	memcpy(pBase, g_vpuCode, g_vpuCodeSize);

	return 0;
}

unsigned int ExecuteVcCode(unsigned int code,
		unsigned int r0, unsigned int r1, unsigned int r2, unsigned int r3, unsigned int r4, unsigned int r5)
{
	struct vc_msg
	{
		unsigned int m_msgSize;
		unsigned int m_response;

		struct vc_tag
		{
			unsigned int m_tagId;
			unsigned int m_sendBufferSize;
			union {
				unsigned int m_sendDataSize;
				unsigned int m_recvDataSize;
			};

			struct args
			{
				union {
					unsigned int m_pCode;
					unsigned int m_return;
				};
				unsigned int m_r0;
				unsigned int m_r1;
				unsigned int m_r2;
				unsigned int m_r3;
				unsigned int m_r4;
				unsigned int m_r5;
			} m_args;
		} m_tag;

		unsigned int m_endTag;
	} msg;
	int s;

	msg.m_msgSize = sizeof(msg);
	msg.m_response = 0;
	msg.m_endTag = 0;

	//fill in the tag for the unlock command
	msg.m_tag.m_tagId = 0x30010;
	msg.m_tag.m_sendBufferSize = 28;
	msg.m_tag.m_sendDataSize = 28;

	//pass across the handle
	msg.m_tag.m_args.m_pCode = code;
	msg.m_tag.m_args.m_r0 = r0;
	msg.m_tag.m_args.m_r1 = r1;
	msg.m_tag.m_args.m_r2 = r2;
	msg.m_tag.m_args.m_r3 = r3;
	msg.m_tag.m_args.m_r4 = r4;
	msg.m_tag.m_args.m_r5 = r5;

	s = MboxProperty(g_mboxFile, &msg);

	//check the error code too
	if (s == 0 && msg.m_response == 0x80000000 && msg.m_tag.m_recvDataSize == 0x80000004)
		return msg.m_tag.m_args.m_return;
	else
	{
		fprintf(stderr, "failed to unlock vc memory: s=%d response=%08x recv data size=%08x\n",
				s, msg.m_response, msg.m_tag.m_recvDataSize);
		return 1;
	}
}
