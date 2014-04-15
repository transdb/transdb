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
    explicit MemoryWatcher(Storage &rStorage);
    
    //ThreadContext
    bool run();
    
private:
    Storage     &m_rStorage;
    uint32      m_memoryCheckerCount;
    uint32      m_memoryPoolCheckerCount;
};

#endif /* defined(__TransDB__MemoryWatcher__) */
