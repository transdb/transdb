//
//  IndexBlock.cpp
//  TransDB
//
//  Created by Miroslav Kudrnac on 9/3/12.
//  Copyright (c) 2012 Miroslav Kudrnac. All rights reserved.
//

#include "StdAfx.h"

IndexBlock::IndexBlock()
{
    m_pIndexFile            = NULL;
    m_blockCount            = 0;
    m_cachedDiskBlockNum    = 0;
    m_pCachedDiskBlock      = (uint8*)aligned_malloc(INDEX_BLOCK_SIZE);
}

IndexBlock::~IndexBlock()
{
    IO::fflush(m_pIndexFile);
    IO::fclose(m_pIndexFile);
    
    //clear cache
    aligned_free(m_pCachedDiskBlock);
}

void IndexBlock::Sync()
{
    IO::fflush(m_pIndexFile);
}

struct IndexDef
{
    uint64              m_key;
    std::streamoff      m_dataPosition;
    std::streamsize     m_dataLen;
};

static bool SortFreeSpace(IndexDef pFreeSpace1, IndexDef pFreeSpace2)
{
	return (pFreeSpace1.m_dataPosition < pFreeSpace2.m_dataPosition);
}

void IndexBlock::Init(Storage *pStorage,
					  const char *pIndexFilePath, 
				      RecordIndexMap *pRecordIndexMap, 
					  std::streamoff dataFileSize)
{
	uint8 *pData;
    uint8 *pBlock;
    ICIDF *pICIDF;
    uint16 position;
    DREC *pDREC;
    RecordIndex *pIREC;
    std::streamoff dataFileSizeTmp;
    std::streamsize freeSpaceLen;
    uint64 counter;
    bool oDataCurrupted;
    
    typedef std::vector<IndexDef> IndexDefVec;
    IndexDefVec rIndexDefVec;
    IndexDefVec::iterator itr, prevItr;

    oDataCurrupted = false;
    
	//save dataFileSize for later use
	dataFileSizeTmp = dataFileSize;

    //check file exists
    Common::CheckFileExists(pIndexFilePath, true);

    //open index file
    m_pIndexFile = fopen(pIndexFilePath, "rb+");
    assert(m_pIndexFile);
    
	//get index file size
    IO::fseek(m_pIndexFile, 0, SEEK_END);
    size_t fileSize = IO::ftell(m_pIndexFile);
    IO::fseek(m_pIndexFile, 0, SEEK_SET);
    
    if(fileSize != 0)
    {
        pData = (uint8*)aligned_malloc(fileSize);
		m_blockCount = static_cast<uint32>(fileSize / INDEX_BLOCK_SIZE);
        
        //read
        IO::fread(pData, fileSize, 1, m_pIndexFile);

        //add 1st record to cache
        memcpy(m_pCachedDiskBlock, pData, INDEX_BLOCK_SIZE);
        m_cachedDiskBlockNum = 0;

        //build index map
        for(uint32 i = 0;i < m_blockCount;++i)
        {
            position = 0;
            pBlock = pData+(INDEX_BLOCK_SIZE*i);
            pICIDF = GetICIDF(pBlock);

            //read record
            for(;;)
            {
                if(position >= pICIDF->m_location)
                    break;
                
                //get record
                pDREC = (DREC*)(pBlock+position);
                
                //check recordstart
                if(pDREC->m_recordStart == -1)
                {
                    Log.Error(__FUNCTION__, "INDEX CORRUPTION X: "I64FMTD" recordStart == -1 skipping", pDREC->m_key);
                    oDataCurrupted = true;
                }
                else
                {
                    //create storage index
                    pIREC                       = pStorage->m_pRecordIndexMemoryPool->alloc();
                    pIREC->m_recordStart        = pDREC->m_recordStart;
                    pIREC->m_blockCount         = pDREC->m_blockCount;
                    pIREC->m_pBlockManager      = NULL;
                    //set internal index
                    pIREC->m_IB_blockNumber     = i;
                    pIREC->m_IB_recordOffset    = position;
                    pRecordIndexMap->insert(RecordIndexMap::value_type(pDREC->m_key, pIREC));
                    
                    //create tmp freespace for calculating freespace list
                    IndexDef rIndexDef;
                    rIndexDef.m_key = pDREC->m_key;
                    rIndexDef.m_dataPosition = pDREC->m_recordStart;
                    rIndexDef.m_dataLen = pIREC->m_blockCount * BLOCK_SIZE;
                    rIndexDefVec.push_back(rIndexDef);
                }
                //update position
                position += sizeof(DREC);
            }
            //add block with free space
            if(pICIDF->m_amoutOfFreeSpace >= sizeof(DREC))
            {
                m_freeBlocks.insert(i);
            }
        }
        
        //release memory
        aligned_free(pData);
	}

    //sort
    std::sort(rIndexDefVec.begin(), rIndexDefVec.end(), SortFreeSpace);
        
	//load free space to memory
    if(rIndexDefVec.size())
    {
        counter = 0;
        itr = rIndexDefVec.begin();
        prevItr = itr;
        dataFileSizeTmp -= prevItr->m_dataLen;
        ++itr;
        
        std::streamoff freeSpacePos;
        std::streamsize freeSpaceLenght;
        
        for(;itr != rIndexDefVec.end();)
        {
            freeSpaceLen = itr->m_dataPosition - (prevItr->m_dataPosition + prevItr->m_dataLen);
            if(freeSpaceLen > 0)
            {
                freeSpacePos = (prevItr->m_dataPosition + prevItr->m_dataLen);
                freeSpaceLenght = freeSpaceLen;
                pStorage->AddFreeSpace(freeSpacePos, freeSpaceLenght);
            }
            else if(freeSpaceLen < 0)
            {
                Log.Error(__FUNCTION__, "Data curruption - "I64FMTD" - X: "I64FMTD" freeSpaceLen is negative", counter, itr->m_key);
                oDataCurrupted = true;
            }
                
            dataFileSizeTmp -= itr->m_dataLen;
            prevItr = itr;
            ++itr;
            ++counter;
        }
    }
	
    //exit app
    //if(oDataCurrupted)
    //{
    //    Log.Error(__FUNCTION__, "Found corrupted data stopping server start.");
    //    exit(-1);
    //}
    
	//add last piece of freespace
    if(dataFileSizeTmp != 0)
    {
        pStorage->AddFreeSpace(dataFileSize - dataFileSizeTmp, dataFileSizeTmp);
    }
}

void IndexBlock::WriteRecordIndexToDisk(RecordIndexMap::iterator &itr)
{    
    ICIDF *pICIDF;
    uint32 freeBlock;
    size_t diskPosition;
    
    //fill disk record    
    DREC rDiskRecord;    
    rDiskRecord.m_key = itr->first;
    rDiskRecord.m_recordStart = itr->second->m_recordStart;
    rDiskRecord.m_blockCount = itr->second->m_blockCount;
    
    //x is existing rewrite
    if(itr->second->m_IB_recordOffset != -1)
    {
        //calc position on disk
        diskPosition = (itr->second->m_IB_blockNumber * INDEX_BLOCK_SIZE) + itr->second->m_IB_recordOffset;
        //update cache
        if(m_cachedDiskBlockNum == itr->second->m_IB_blockNumber)
        {
            memcpy(m_pCachedDiskBlock+itr->second->m_IB_recordOffset, &rDiskRecord, sizeof(rDiskRecord));
        }
        
        //write to disk
        IO::fseek(m_pIndexFile, diskPosition, SEEK_SET);
        IO::fwrite(&rDiskRecord, sizeof(rDiskRecord), 1, m_pIndexFile);
    }
    else
    {
        //we have no empty block
        if(m_freeBlocks.empty())
        {
            //clean up
            memset(m_pCachedDiskBlock, 0, INDEX_BLOCK_SIZE);
            m_cachedDiskBlockNum = m_blockCount;
            
            //create ICIDF
            pICIDF = GetICIDF(m_pCachedDiskBlock);
            pICIDF->m_location = sizeof(rDiskRecord);
            pICIDF->m_amoutOfFreeSpace = INDEX_BLOCK_SIZE - sizeof(rDiskRecord) - sizeof(ICIDF);
            
            //add record
            memcpy(m_pCachedDiskBlock, &rDiskRecord, sizeof(rDiskRecord));
            
            //set index
            itr->second->m_IB_blockNumber = m_blockCount;
            itr->second->m_IB_recordOffset = 0;
            
            //add freeblock
            m_freeBlocks.insert(m_blockCount);
            
            //update block count
            ++m_blockCount;
            
            //write to disk - to the end
            IO::fseek(m_pIndexFile, 0, SEEK_END);
            IO::fwrite(m_pCachedDiskBlock, INDEX_BLOCK_SIZE, 1, m_pIndexFile);
        }
        else
        {
            //write to block with free space
            freeBlock = (*m_freeBlocks.begin());
            diskPosition = (INDEX_BLOCK_SIZE*freeBlock);            
            
            //try to use cache
            if(freeBlock != m_cachedDiskBlockNum)
            {                
                //read new block
                IO::fseek(m_pIndexFile, diskPosition, SEEK_SET);
                IO::fread(m_pCachedDiskBlock, INDEX_BLOCK_SIZE, 1, m_pIndexFile);
                m_cachedDiskBlockNum = freeBlock;
            }
            
            //get ICIDF
            pICIDF = GetICIDF(m_pCachedDiskBlock);
            
            //add record
            memcpy(m_pCachedDiskBlock+pICIDF->m_location, &rDiskRecord, sizeof(rDiskRecord));
            
            //set index
            itr->second->m_IB_blockNumber = (m_blockCount - 1);
            itr->second->m_IB_recordOffset = pICIDF->m_location;
            
            //update ICIDF
            pICIDF->m_location += sizeof(rDiskRecord);
            pICIDF->m_amoutOfFreeSpace -= sizeof(rDiskRecord);
            
            //check free space
            if(pICIDF->m_amoutOfFreeSpace < sizeof(rDiskRecord))
            {
                m_freeBlocks.erase(freeBlock);
            }
            
            //write to disk
            IO::fseek(m_pIndexFile, diskPosition, SEEK_SET);
            IO::fwrite(m_pCachedDiskBlock, INDEX_BLOCK_SIZE, 1, m_pIndexFile);        
        }
    }
}

void IndexBlock::EraseRecord(RecordIndexMap::iterator &itr, LockingPtr<RecordIndexMap> &pRecordIndexMap)
{
    //not written yet
    if(itr->second->m_IB_recordOffset == -1)
        return;
    
    ICIDF *pICIDF;  
	DREC *pDREC;
    size_t diskPosition;
    size_t moveSize;
	uint16 position;
    RecordIndexMap::iterator itrReindex;
    
    //calc position on disk
    diskPosition = (INDEX_BLOCK_SIZE*itr->second->m_IB_blockNumber);
    
    //load block from disk
    if(itr->second->m_IB_blockNumber != m_cachedDiskBlockNum)
    {
        //read new block
        IO::fseek(m_pIndexFile, diskPosition, SEEK_SET);
        IO::fread(m_pCachedDiskBlock, INDEX_BLOCK_SIZE, 1, m_pIndexFile);
        m_cachedDiskBlockNum = itr->second->m_IB_blockNumber;
    }
    
    //get ICIDF
    pICIDF = GetICIDF(m_pCachedDiskBlock);
    
    //move
    moveSize = INDEX_BLOCK_SIZE - itr->second->m_IB_recordOffset - pICIDF->m_amoutOfFreeSpace - sizeof(ICIDF);
    memmove(m_pCachedDiskBlock + itr->second->m_IB_recordOffset, m_pCachedDiskBlock+itr->second->m_IB_recordOffset + sizeof(DREC), moveSize);
    
    //update free space
    pICIDF->m_amoutOfFreeSpace += sizeof(DREC);
    pICIDF->m_location -= sizeof(DREC);
    
    //clear free space
    memset(m_pCachedDiskBlock+pICIDF->m_location, 0, pICIDF->m_amoutOfFreeSpace);

	//reindex 
	position = itr->second->m_IB_recordOffset;
    for(;;)
    {
        if(position >= pICIDF->m_location)
            break;
                
        pDREC = (DREC*)(m_pCachedDiskBlock+position);

        //change internal index
		itrReindex = pRecordIndexMap->find(pDREC->m_key);
		assert(itrReindex != pRecordIndexMap->end());
		itrReindex->second->m_IB_recordOffset -= sizeof(DREC);
                
        //update position
        position += sizeof(DREC);
    }

    //block has freespace
    m_freeBlocks.insert(itr->second->m_IB_blockNumber);
    
    //write to disk
    IO::fseek(m_pIndexFile, diskPosition, SEEK_SET);
    IO::fwrite(m_pCachedDiskBlock, INDEX_BLOCK_SIZE, 1, m_pIndexFile);
    IO::fflush(m_pIndexFile);
}



































