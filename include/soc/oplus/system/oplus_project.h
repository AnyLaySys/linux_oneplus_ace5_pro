#ifndef _OPLUS_PROJECT_H
#define _OPLUS_PROJECT_H

#include <linux/types.h>

#define AGING 1
#define FACTORY 2
#define PTCRB 3
#define HIGH_TEMP_AGING 4
#define PREVERSION 5
#define OEM_RELEASE 6
#define EVT1 1
#define MP1 32

unsigned int get_project(void);
unsigned int get_prj(void);
unsigned int get_eng_version(void);
unsigned int get_PCB_Version(void);
int32_t get_Operator_Version(void);

#endif
