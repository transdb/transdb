//
//  HlpFunctions.h
//  TransDB
//
//  Created by Miroslav Kudrnac on 15.09.14.
//  Copyright (c) 2014 Miroslav Kudrnac. All rights reserved.
//

#ifndef TransDB_HlpFunctions_h
#define TransDB_HlpFunctions_h

#ifdef WIN32
static void trans_func(unsigned int u, EXCEPTION_POINTERS* pExp)
{
    PrintCrashInformation(pExp);
    HandleCrash(pExp);
}
#endif

#endif
