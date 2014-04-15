//
//  MemoryWatcher.h
//  TransDB
//
//  Created by Miroslav Kudrnac on 14.01.13.
//  Copyright (c) 2013 Miroslav Kudrnac. All rights reserved.
//

#ifndef __TransDB__MemoryWatcher__
#define __TransDB__MemoryWatcher__

class MemoryWatcher : public ThreadContext
{
public:
    MemoryWatcher(Storage &rStorage);
    bool run();
    
private:
    Storage     &m_rStorage;
    uint32      m_memoryCheckerCount;
};

#endif /* defined(__TransDB__MemoryWatcher__) */
