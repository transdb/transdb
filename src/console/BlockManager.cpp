//
//  BlockManager.cpp
//  TransDB
//
//  Created by Miroslav Kudrnac on 8/24/12.
//  Copyright (c) 2012 Miroslav Kudrnac. All rights reserved.
//

#include "StdAfx.h"

BlockManager::BlockManager(Storage &pStorage) : m_pStorage(pStorage)
{
    //create initial block
    ReallocBlocks();
}

BlockManager::BlockManager(Storage &pStorage, const uint64 &x, const Blocks &rBlocks) : m_pStorage(pStorage), m_rBlocks(rBlocks)
{
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
        pBlock = GetBlock(i);
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
}

BlockManager::~BlockManager()
{
    BlockSize_T *pBlock;
    std::lock_guard<tbb::mutex> rBGuard(m_pStorage.m_rBlockMemPoolLock);
    for(Blocks::iterator itr = m_rBlocks.begin();itr != m_rBlocks.end();++itr)
    {
        pBlock = (BlockSize_T*)(*itr);
        m_pStorage.m_pBlockMemPool->free(pBlock);
    }
}

void BlockManager::ReallocBlocks()
{
    uint8 *pNewBlock;
    
    //get new block
    {
        std::lock_guard<tbb::mutex> rBGuard(m_pStorage.m_rBlockMemPoolLock);        
        pNewBlock = (uint8*)m_pStorage.m_pBlockMemPool->alloc();
        memset(pNewBlock, 0, BLOCK_SIZE);
    }
    
    //init new block
    Block::InitBlock(pNewBlock);
    
    //add
    m_rBlocks.push_back(pNewBlock);
}

uint32 BlockManager::WriteRecord(const uint64 &recordkey, const uint8 *pRecord, const uint16 &recordSize, int32 *writtenInBlocks)
{
    //size check
    if(recordSize > MAX_RECORD_SIZE)
        return eBMS_FailRecordTooBig;
    
    E_BLS status;
    uint8 *pBlock;
    uint16 RDFPosition;
    uint16 recordPosition;
    uint32 retStatus = eBMS_Ok;
    BlocksIndex::iterator itr;
    uint64 recordKeyTmp;
    
    uint16 recordSizeLocal;
    ByteBuffer rOut;
    int cStatus;
    
    //PHASE 0 --> compression
    //save variables
    recordSizeLocal = recordSize;
    
    //try to compress
    if(recordSizeLocal > g_RecordSizeForCompression)
    {
        cStatus = Common::compressGzip(pRecord, recordSizeLocal, rOut);
        if(cStatus == Z_OK)
        {
            if(rOut.size() < recordSizeLocal)
            {
                pRecord = rOut.contents();
                recordSizeLocal = (uint16)rOut.size();
                g_NumOfRecordCompressions++;
            }
            else
            {
                Log.Warning(__FUNCTION__, "Compression generated bigger size. Source size: %u, new size: "I64FMTD, recordSizeLocal, rOut.size());
            }
        }
    }
    
    //PHASE 1 --> try to update
    //try to update record first 
    itr = m_blockIndex.find(recordkey);
    if(itr != m_blockIndex.end())
    {
        //get block
        pBlock = GetBlock(itr->second);
        //update
        status = Block::UpdateRecord(pBlock, recordkey, pRecord, recordSizeLocal, NULL, &RDFPosition, &recordPosition);
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
    
    //PHASE 2 --> check record limit
    //block limit - start to rewrite from begin (record key is timestamp)
    if(m_blockIndex.size() >= g_RecordLimit)
    {
        //delete 1st one (the oldest)
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
    
    //PHASE 3 --> try to write
    //find empty space in blocks
    for(uint16 i = 0;i < m_rBlocks.size();++i)
    {
        pBlock = GetBlock(i);
        status = Block::WriteRecord(pBlock, recordkey, pRecord, recordSizeLocal);
        if(status == eBLS_OK)
        {
            writtenInBlocks[0] = i;
            m_blockIndex.insert(BlocksIndex::value_type(recordkey, i));
            break;
        }
        else if(status == eBLS_NO_SPACE_FOR_NEW_DATA && (i == (m_rBlocks.size() - 1)))
        {
            //relloc block + 1
            ReallocBlocks();
            //set status
            retStatus |= eBMS_ReallocatedBlocks;
        }
    }

    return retStatus;
}

void BlockManager::ReadRecord(const uint64 &recordkey, ByteBuffer &rData)
{
    uint8 *pBlock;
    BlocksIndex::iterator itr = m_blockIndex.find(recordkey);
    if(itr != m_blockIndex.end())
    {
        pBlock = GetBlock(itr->second);
        Block::GetRecord(pBlock, recordkey, rData);
    }
}

void BlockManager::ReadRecords(ByteBuffer &rData)
{        
    //pre-realloc buffer
    //key|recordSize|record|....Nx
    rData.reserve(BLOCK_SIZE * m_rBlocks.size());
    
    uint8 *pBlock;
    for(uint16 i = 0;i < m_rBlocks.size();++i)
    {
        pBlock = GetBlock(i);
        Block::GetRecords(pBlock, rData);
    }
}

void BlockManager::DeleteRecord(const uint64 &recordkey)
{    
    BlocksIndex::iterator itr = m_blockIndex.find(recordkey);
    if(itr != m_blockIndex.end())
    {
        DeleteRecord(itr);
    }
}

void BlockManager::DeleteRecord(BlocksIndex::iterator &itr)
{    
    uint8 *pBlock;
    uint16 RDFPosition;
    uint16 recordPosition;
    E_BLS deleteStatus;
    
    //delete
    pBlock = GetBlock(itr->second);
    deleteStatus = Block::DeleteRecord(pBlock, itr->first, NULL, &RDFPosition, &recordPosition);
    if(deleteStatus != eBLS_OK)
    {
        Log.Warning(__FUNCTION__, "Trying to delete non-existant key: "I64FMTD" in block number: %u", itr->first, itr->second);
    }
    m_blockIndex.erase(itr);    
}

void BlockManager::GetAllRecordKeys(ByteBuffer &rY)
{
    if(m_blockIndex.size())
    {
        //prealloc
        rY.reserve(m_blockIndex.size() * sizeof(uint64));
        //add to buffer
        for(BlocksIndex::const_iterator itr = m_blockIndex.begin();itr != m_blockIndex.end();++itr)
        {
            rY << itr->first;
        }
    }
}

void BlockManager::ClearDirtyFlags()
{
    uint8 *pBlock;
    CIDF *pCIDF;
    
    for(uint16 i = 0;i < m_rBlocks.size();++i)
    {
        pBlock = GetBlock(i);
        pCIDF = GetCIDF(pBlock);
        pCIDF->m_flags &= ~eBLF_Dirty;
    }
}

size_t BlockManager::GetMemUsage()
{
    size_t size = 0;
    
    //blocks size
    size += (BLOCK_SIZE * m_rBlocks.size());
    //index size
    size += (m_blockIndex.size() * (sizeof(uint64) + sizeof(uint16)));
    //class size
    size += sizeof(BlockManager);
    
    return size;
}



