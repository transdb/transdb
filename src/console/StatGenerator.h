//
//  WebService.h
//  TransDB
//
//  Created by Miroslav Kudrnac on 03.12.12.
//  Copyright (c) 2012 Miroslav Kudrnac. All rights reserved.
//

#ifndef __TransDB__StatGenerator__
#define __TransDB__StatGenerator__

class Storage;

class StatGenerator
{
public:
    explicit StatGenerator(Storage &rStorage);
    static void Init();
    
    void GenerateStats(ByteBuffer &rData, bool oAddDescription);
    
private:
    void GetFreeFraceStats(uint64 &freeSpaceChunkCount, uint64 &freeSpaceBlocksCount);
    void GetBlockMemPoolStats(uint64 &blockMemPoolSize);
    void GetRecordIndexMemPoolStats(uint64 &recordIndexMemPoolSize);
    void GetBlockManagerMemPoolStats(uint64 &blockManagerMemPoolSize);
    void GetLRUCacheStats(uint64 &size, uint64 &cacheSize);
    void GetDiskWriterStats(uint64 &queueSize, uint64 &lastNumOfItemsInProcess, uint64 &itemsToProcessSize);
    
    //declarations
    Storage &m_rStorage;
};

#endif /* defined(__TransDB__StatGenerator__) */
