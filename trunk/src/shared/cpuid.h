//
//  cpuid.h
//  TransDB
//
//  Created by Miroslav Kudrnac on 02.05.13.
//  Copyright (c) 2013 Miroslav Kudrnac. All rights reserved.
//

#ifndef TransDB_cpuid_h
#define TransDB_cpuid_h

/* SSE42 flag */
static const INT SSE4_2_SUPPORTED = (1 << 20);
/* AVX flag */
static const INT AVX_SUPPORTED = (1 << 28);

/* CPUID function */
#ifdef WIN32
static void cpuid(int *CPUInfo, int InfoType)
{
    __cpuid(CPUInfo, InfoType);
}
#else
static void cpuid(int *CPUInfo, unsigned int number)
{
    /* Store number in %eax, call CPUID, save %eax, %ebx, %ecx, %edx in variables.
     As output constraint "m=" has been used as this keeps gcc's optimizer
     from overwriting %eax, %ebx, %ecx, %edx by accident */
    asm("movl %4, %%eax; cpuid; movl %%eax, %0; movl %%ebx, %1; movl %%ecx, %2; movl %%edx, %3;"
        : "=m" (CPUInfo[0]),
        "=m" (CPUInfo[1]),
        "=m" (CPUInfo[2]),
        "=m" (CPUInfo[3])				  /* output */
        : "r"  (number)                   /* input */
        : "eax", "ebx", "ecx", "edx"      /* no changed registers except output registers */
        );
}
#endif

#endif
