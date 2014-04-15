//
//  BlockManager.cpp
//  TransDB
//
//  Created by Miroslav Kudrnac on 8/24/12.
//  Copyright (c) 2012 Miroslav Kudrnac. All rights reserved.
//

#include "StdAfx.h"

BlockManager::BlockManager()
{

}

BlockManager::~BlockManager()
{
    BlockSize_T *pBlock;
    for(uint16 i = 0;i < m_rBlocks.size();++i)
    {
        pBlock = (BlockSize_T*)m_rBlocks[i];
        m_pStorage->m_pBlockMemoryPool->free(pBlock);
    }
    m_rBlocks.clear();
    m_blockIndex.clear();
    m_pStorage = NULL;
}

void BlockManager::Init(Storage *pStorage)
{
    m_pStorage  = pStorage;
}

void BlockManager::Load(uint64 x, Blocks &rBlocks)
{
    m_rBlocks   = rBlocks;
    
    //build index map
    uint8 *pBlock;
    uint16 position;
    uint16 recordPosition;    
    uint16 endOfRDFArea;
    CIDF *pCIDF;
    RDF *pRDF;
	//CRC stuff
	uint8 *pRecord;
	uint32 crc32check;
    
    for(uint16 i = 0;i < m_rBlocks.size();++i)
    {
        pBlock = m_rBlocks[i];
        pCIDF = GetCIDF(pBlock);
        recordPosition = 0;
        position = CIDFOffset - sizeof(RDF);
        endOfRDFArea = pCIDF->m_location + pCIDF->m_amoutOfFreeSpace;
        for(;;)
        {
            //end of RDF area
            if(position < endOfRDFArea)
                break;
                                    
            pRDF = (RDF*)(pBlock+position);
            m_blockIndex.insert(BlocksIndex::value_type(pRDF->m_key, i));
            position -= sizeof(RDF);

			//check crc32
			pRecord = (uint8*)(pBlock+recordPosition);
			crc32check = g_CRC32->ComputeCRC32(pRecord, pRDF->m_recordLength);
			if(crc32check != pRDF->m_crc32)
			{
				Log.Error(__FUNCTION__, "CRC32 DOES NOT MATCH - X: "I64FMTD", block number: %u, record id: "I64FMTD", computed crc32: %u, saved crc32: %u", x, i, pRDF->m_key, crc32check, pRDF->m_crc32);
			}
            
            //count position of data
            recordPosition += pRDF->m_recordLength;
        }
    }
    
#ifdef CUSTOM_CHECKS
    if(g_NumRecordsInBlock && m_blockIndex.size() != g_NumRecordsInBlock)
    {
        Log.Error(__FUNCTION__, I64FMTD, m_blockIndex.size());
        //dump and assert
        FILE *p = fopen("/Volumes/disk/dump.dmp", "wb");
        fwrite(pData, numOfBlocks*BLOCK_SIZE, 1, p);
        fclose(p);
		//assert
		assert(false);
    }
#endif
}

void BlockManager::ReallocBlocks()
{
    uint8 *pNewBlock;
    
    //get new block
    pNewBlock = (uint8*)m_pStorage->m_pBlockMemoryPool->alloc();
    //init new bclock
    Block::InitBlock(pNewBlock);
    //add
    m_rBlocks.push_back(pNewBlock);
}

uint32 BlockManager::WriteRecord(uint64 recordkey, uint8 *pRecord, uint16 recordSize, int32 *writtenInBlocks)
{
    //size check
    if(recordSize > (BLOCK_SIZE - sizeof(RDF) - sizeof(CIDF)))
        return eBMS_FailRecordTooBig;
    
    E_BLS status;
    uint8 *pBlock;
    uint16 RDFPosition;
    uint16 recordPosition;
    uint32 retStatus = eBMS_Ok;
    BlocksIndex::iterator itr;
    uint64 recordKeyTmp;
    
    //block limit - start to rewrite from begin (record key is timestamp)
    if(m_blockIndex.size() >= g_RecordLimit)
    {
        //delete 1st one
        itr = m_blockIndex.begin();
        
        //DO NOT ADD OLDER DATA
        //check if new data are not the older one
        recordKeyTmp = itr->first;
        if(recordkey < recordKeyTmp)
        {
            retStatus |= (eBMS_RecordCountLimit | eBMS_OldRecord);
            return retStatus;
        }
        
        //from this block is record deleted, we must sync it on disk
        writtenInBlocks[1] = itr->second;
        //this invalidate iterator
        DeleteRecord(itr);
        //set return status
        retStatus |= eBMS_RecordCountLimit;
    }
    else
    {
        //block limit deleted this record so there is no reason to try find it
        itr = m_blockIndex.find(recordkey);
        if(itr != m_blockIndex.end())
        {
            //get block
            pBlock = m_rBlocks[itr->second];
            //update
            status = Block::UpdateRecord(pBlock, recordkey, pRecord, recordSize, NULL, &RDFPosition, &recordPosition);
            if(status == eBLS_OK)
            {
                writtenInBlocks[0] = itr->second;
                return retStatus;
            }
            
            //from this block is record deleted, we must sync it on disk
            writtenInBlocks[1] = itr->second;
            //delete key will be on another block
            m_blockIndex.erase(itr);
        }
    }
    
    //find empty space in blocks
    for(uint16 i = 0;i < m_rBlocks.size();++i)
    {
        pBlock = m_rBlocks[i];
        status = Block::WriteRecord(pBlock, recordkey, pRecord, recordSize);
        if(status == eBLS_OK)
        {
            writtenInBlocks[0] = i;
            m_blockIndex.insert(BlocksIndex::value_type(recordkey, i));
            break;
        }
        else if(status == eBLS_NO_SPACE_FOR_NEW_DATA && (i == (m_rBlocks.size()-1)))
        {
            //relloc block + 1
            ReallocBlocks();
            //set status
            retStatus |= eBMS_ReallocatedBlocks;
        }
    }
    
    return retStatus;
}

void BlockManager::ReadRecord(uint64 recordkey, uint8 **pRecord, uint16 *recordSize)
{
    //clean vars
    *pRecord = NULL;
    *recordSize = 0;
    
    uint8 *pBlock;
    BlocksIndex::iterator itr = m_blockIndex.find(recordkey);
    if(itr != m_blockIndex.end())
    {
        pBlock = m_rBlocks[itr->second];
        Block::GetRecord(pBlock, recordkey, pRecord, recordSize);
    }
}

void BlockManager::ReadRecords(uint8 **pRecords, uint32 *recordsSize)
{        
    //pre-realloc buffer
    //key|recordSize|record|....Nx
    *pRecords = (uint8*)aligned_malloc(BLOCK_SIZE*m_rBlocks.size());
    *recordsSize = 0;
    
    uint8 *pBlock;
    for(uint16 i = 0;i < m_rBlocks.size();++i)
    {
        pBlock = m_rBlocks[i];
        Block::GetRecords(pBlock, pRecords, recordsSize);
    }
}

void BlockManager::DeleteRecord(uint64 recordkey)
{    
    BlocksIndex::iterator itr = m_blockIndex.find(recordkey);
    if(itr != m_blockIndex.end())
    {
        DeleteRecord(itr);
    }
}

void BlockManager::DeleteRecord(BlocksIndex::iterator itr)
{    
    uint8 *pBlock;
    uint16 RDFPosition;
    uint16 recordPosition;
    
    //delete
    pBlock = m_rBlocks[itr->second];
    Block::DeleteRecord(pBlock, itr->first, NULL, &RDFPosition, &recordPosition);
    m_blockIndex.erase(itr);
}

void BlockManager::GetAllRecordKeys(uint8 **pRecordKeys, uint64 *pRecordKeysSize)
{
    if(m_blockIndex.size())
    {
        *pRecordKeysSize = m_blockIndex.size() * sizeof(uint64);
        *pRecordKeys = (uint8*)aligned_malloc(*pRecordKeysSize);
        
        uint64 position = 0;
        for(BlocksIndex::iterator itr = m_blockIndex.begin();itr != m_blockIndex.end();++itr)
        {
            memcpy(*pRecordKeys+position, &itr->first, sizeof(itr->first));
            position += sizeof(itr->first);
        }
    }
}
