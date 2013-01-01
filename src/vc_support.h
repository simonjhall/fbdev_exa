/*
 * vc_support.h
 *
 *  Created on: 3 Dec 2012
 *      Author: Simon
 */

#ifndef VC_SUPPORT_H_
#define VC_SUPPORT_H_

int OpenVcMbox(const char *filename);
void CloseVcMbox(void);

unsigned int LoadVcCode(const char *filename, int isElf, int patchAddress);
void UnloadVcCode(void);
int UploadVcCode(void *pBase);

unsigned int ExecuteVcCode(unsigned int code,
		unsigned int r0, unsigned int r1, unsigned int r2, unsigned int r3, unsigned int r4, unsigned int r5);

#endif /* VC_SUPPORT_H_ */
