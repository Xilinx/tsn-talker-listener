/***************************************************************
* Copyright (c) 2022 Xilinx, Inc. All rights reserved.
* SPDX-License-Identifier: LGPL-2.1
***************************************************************/

/*
 * Acquire address for access the test pmod controller
 */

#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <fcntl.h>
#include <dirent.h>
#include <assert.h>

#include "tpmod_ctrl.h"

#define ERR_DEVICE_NOT_FOUND	(-2)
#define ERR_GENERIC_FAILURE	(-1)

#define UIO_DEVICE_NAME		"tpmod_ctrl"
#define FALLBACK_DEFAULT_BASE	0x80030000

#define MAX_UIO_PATH_SIZE       256
#define MAX_UIO_NAME_SIZE       64

static int line_from_file(char* filename, char* linebuf) {
	char* s;
	int i;
	FILE* fp = fopen(filename, "r");
	if (!fp) return -1;

	s = fgets(linebuf, MAX_UIO_NAME_SIZE, fp);

	fclose(fp);

	if (!s) return -2;

	for (i=0; (*s)&&(i<MAX_UIO_NAME_SIZE); i++) {
		if (*s == '\n') *s = 0;
		s++;
	}
	return 0;
}

static int uio_init(TPmod_ctrl *InstancePtr)
{
	struct dirent **namelist;
	int i, n;
	char* s = NULL;
	char file[ MAX_UIO_PATH_SIZE ];
	char path[ MAX_UIO_PATH_SIZE ];
	char name[ MAX_UIO_NAME_SIZE ];
	FILE *fp = NULL;
	int uio_num = -1;

	assert(InstancePtr != NULL);

	n = scandir("/sys/class/uio", &namelist, 0, alphasort);

	if (n < 0)  return ERR_DEVICE_NOT_FOUND;

	for (i = 0;  i < n; i++) {
		strcpy(file, "/sys/class/uio/");
		strcat(file, namelist[i]->d_name);
		strcat(file, "/name");
		if ((line_from_file(file, name) == 0) &&
				(strcmp(name, UIO_DEVICE_NAME) == 0)) {
			s = namelist[i]->d_name;
			s +=3; // strip uio
			uio_num = atoi(s);
			break;
		}
	}

	if (uio_num ==-1 || i == n) return ERR_DEVICE_NOT_FOUND;

	/* device node found*/
	strcpy(path, "/sys/class/uio/");
	strcat(path, namelist[i]->d_name);

	printf ("Using UIO device for Test PMOD controller:\n\t%s\n", path);

	/*
	 * only use the first map (only one mapping is expected for this device)
	 */
	strcpy(file, path);
	strcat(file,"/maps/map0/addr");

	fp = fopen(file, "r");
	assert(fp != NULL);
	fscanf(fp, "0x%lx", &(InstancePtr->PhysicalMap.BaseAddr));
	fclose(fp);

	printf("PysicalBase = 0x%lx\n", InstancePtr->PhysicalMap.BaseAddr);

	strcpy(file, path);
	strcat(file,"/maps/map0/size");

	fp = fopen(file, "r");
	assert(fp != NULL);
	fscanf(fp, "0x%x", &(InstancePtr->PhysicalMap.Size));
	fclose(fp);

	printf("Map Size = 0x%x\n", InstancePtr->PhysicalMap.Size);

	sprintf(file, "/dev/uio%d", uio_num);

	if ((InstancePtr->FileHandle = open(file, O_RDWR)) < 0) {
		return ERR_GENERIC_FAILURE;
	}

	InstancePtr->BaseAddr = (uint64_t) mmap(NULL, InstancePtr->PhysicalMap.Size,
					PROT_READ | PROT_WRITE, MAP_SHARED,
					InstancePtr->FileHandle,
					0 * getpagesize()); /* use map0 */

	if (InstancePtr->BaseAddr == (uint64_t) MAP_FAILED) {
		perror("Failed to map memory");
		return ERR_GENERIC_FAILURE;
	}

	printf("TPMod Cntrl Virt Addr: 0x%lx\n", InstancePtr->BaseAddr);

	return 0;
}

static int fallback_init(TPmod_ctrl *InstancePtr)
{
	InstancePtr->PhysicalMap.BaseAddr = FALLBACK_DEFAULT_BASE;
	InstancePtr->PhysicalMap.Size = 1 * (size_t)sysconf(_SC_PAGESIZE);

	InstancePtr->FileHandle = open("/dev/mem", (O_RDWR | O_SYNC));

	//assert(InstancePtr->FileHandle != -1);
	if (InstancePtr->FileHandle == -1) {
		perror("Failed to open memory device");
		return ERR_GENERIC_FAILURE;
	}

	InstancePtr->BaseAddr = (uint64_t) mmap(NULL, InstancePtr->PhysicalMap.Size,
				     PROT_READ | PROT_WRITE, MAP_SHARED,
				     InstancePtr->FileHandle,
				     InstancePtr->PhysicalMap.BaseAddr);

	if (InstancePtr->BaseAddr == (uint64_t) MAP_FAILED) {
		perror("Failed to map memory");
		return ERR_GENERIC_FAILURE;
	}
	return 0;
}

int TPmod_InitializeAddress(TPmod_ctrl *InstancePtr)
{
	int status = uio_init(InstancePtr);

	if (status == ERR_DEVICE_NOT_FOUND) {
		return fallback_init(InstancePtr);
	}

	return status;
}

void TPmod_ReleaseAddress(TPmod_ctrl *InstancePtr)
{
	munmap((void*)InstancePtr->BaseAddr, InstancePtr->PhysicalMap.Size);
	close(InstancePtr->FileHandle);
	InstancePtr->PhysicalMap.Size = 0;
	InstancePtr->PhysicalMap.BaseAddr = 0;
	InstancePtr->BaseAddr = 0;
	InstancePtr->FileHandle = -1;
}
