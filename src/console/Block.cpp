//
//  Block.cpp
//  TransDB
//
//  Created by Miroslav Kudrnac on 8/23/12.
//  Copyright (c) 2012 Miroslav Kudrnac. All rights reserved.
//

#include "StdAfx.h"

void Block::RecordTraverserInit(RecordTraverser *t, uint8 *pBlock)
{
    CIDF *pCIDF = Block::GetCIDF(pBlock);
    t->m_pBlock = pBlock;
    t->m_pBegin = (RDF*)(pBlock + CIDFOffset - sizeof(RDF));
    t->m_pEnd = (RDF*)(pBlock + pCIDF->m_location + pCIDF->m_amountOfFreeSpace - sizeof(RDF));
    t->m_recordOffset = 0;
}

bool Block::RecordTraverserNext(RecordTraverser *t, Record *pRecord)
{
    if(t->m_pBegin == t->m_pEnd)
        return false;
    
    //get RDF
    RDF *pRDF = t->m_pBegin;
    
    //save variables
    pRecord->m_pRDF = pRDF;
    pRecord->m_pRecord = (uint8*)(t->m_pBlock + t->m_recordOffset);
    
    //next RDF + next record offset
    t->m_recordOffset += pRDF->m_recordLength;
    --t->m_pBegin;
    return true;
}

CIDF *Block::GetCIDF(uint8 *pBlock)
{
    return (CIDF*)(pBlock + CIDFOffset);
}

void Block::InitBlock(uint8 *pBlock)
{
    //write CIDF
    CIDF *pCIDF = Block::GetCIDF(pBlock);
    pCIDF->m_amountOfFreeSpace = CIDFOffset;
    pCIDF->m_location = 0;
    pCIDF->m_flags = eBLF_Dirty;
}

E_BLS Block::FindRecord(uint8 *pBlock, uint64 recordKey, Record *pRecord)
{
    //init traverser
    RecordTraverser rT;
    Block::RecordTraverserInit(&rT, pBlock);
    
    while(Block::RecordTraverserNext(&rT, pRecord))
    {
        if(pRecord->m_pRDF->m_key == recordKey)
            return eBLS_OK;
    }
    return eBLS_KEY_NOT_FOUND;
}

E_BLS Block::WriteRecord(uint8 *pBlock, uint64 recordKey, const uint8 *pRecord, uint16 recordSize)
{
    CIDF *pCIDF;
    RDF *pRDF;
    uint16 positionOfNewRDS;
    
    //get CIDF
    pCIDF = Block::GetCIDF(pBlock);
    
    //no space BlockManager should allocate new block
    if(pCIDF->m_amountOfFreeSpace < (recordSize + sizeof(RDF)))
        return eBLS_NO_SPACE_FOR_NEW_DATA;
    
    //get position of new RDS
    positionOfNewRDS = (pCIDF->m_location + pCIDF->m_amountOfFreeSpace) - sizeof(RDF);
    
    //write RDS
    pRDF = (RDF*)(pBlock + positionOfNewRDS);
    pRDF->m_key	= recordKey;
    pRDF->m_recordLength = recordSize;
    
    //save data to block
    memcpy(pBlock + pCIDF->m_location, pRecord, recordSize);
    
    //update CIDF
    pCIDF->m_amountOfFreeSpace -= (recordSize + sizeof(RDF));
    pCIDF->m_location += recordSize;
    pCIDF->m_flags |= eBLF_Dirty;
    
    return eBLS_OK;
}

E_BLS Block::UpdateRecord(uint8 *pBlock, uint64 recordKey, const uint8 *pRecord, uint16 recordSize)
{
    CIDF *pCIDF;
    E_BLS status;
    Record rRecord;
    
    //get record from block
    status = Block::FindRecord(pBlock, recordKey, &rRecord);
    if(status == eBLS_KEY_NOT_FOUND)
        return status;
    
    //RDF found we can continue
    if(recordSize == rRecord.m_pRDF->m_recordLength)
    {
        //data has same size replace
        memcpy(rRecord.m_pRecord, pRecord, recordSize);
        //get CIDF - update dirty flag
        pCIDF = Block::GetCIDF(pBlock);
        pCIDF->m_flags |= eBLF_Dirty;
    }
    else
    {
        //delete old data
        Block::DeleteRecordByRecord(pBlock, &rRecord);
        //try to write new record
        status = Block::WriteRecord(pBlock, recordKey, pRecord, recordSize);
    }
    return status;
}

E_BLS Block::DeleteRecordByKey(uint8 *pBlock, uint64 recordKey)
{
    E_BLS status;
    Record rRecord;
    
    //get record from block
    status = Block::FindRecord(pBlock, recordKey, &rRecord);
    if(status == eBLS_KEY_NOT_FOUND)
        return status;
    
    //delete from block
    Block::DeleteRecordByRecord(pBlock, &rRecord);
    return eBLS_OK;
}

void Block::DeleteRecordByRecord(uint8 *pBlock, Record *pRecord)
{
    CIDF *pCIDF;
    RDF rRDFCopy;
    uint16 moveSIZE;
    uint16 destinationPosition;
    uint16 sourcePosition;
    ptrdiff_t recordPosition;
    ptrdiff_t RDFPosition;
    
    //get CIDF
    pCIDF = Block::GetCIDF(pBlock);
    
    //create copy of RDF - current will be deleted
    memcpy(&rRDFCopy, pRecord->m_pRDF, sizeof(RDF));
    
    //calc record and RDF position from Record struct
    recordPosition = pRecord->m_pRecord - pBlock;
    RDFPosition = (uint8*)pRecord->m_pRDF - pBlock;
    
    /*
     
     Record size 700 bytes
     +---------+---------+---------+---------+------------+------+------+------+------+------+
     | record1 | record2 | record3 | record4 | free space | RDF4 | RDF3 | RDF2 | RDF1 | CIDF |
     +---------+---------+---------+---------+------------+------+------+------+------+------+
     0        700      1400      2100      2800         4051   4061   4071   4081   4091   4096
     
     */
    
    //Records
    /* move size is freespace start - start of next record */
    moveSIZE = pCIDF->m_location - (recordPosition + rRDFCopy.m_recordLength);
    /* destination is record position */
    destinationPosition = recordPosition;
    /* source is record position + record length = start of next record */
    sourcePosition = recordPosition + rRDFCopy.m_recordLength;
    
    //move records - only if not last record then clear data only
    if(moveSIZE > 0)
    {
        memmove(pBlock + destinationPosition, pBlock + sourcePosition, moveSIZE);
    }
    
    //RDFs
    /* move size if RDF position - end of freespace */
    moveSIZE = RDFPosition - (pCIDF->m_location + pCIDF->m_amountOfFreeSpace);
    /* destination is end of freespace + size of RDF */
    destinationPosition = pCIDF->m_location + pCIDF->m_amountOfFreeSpace + sizeof(RDF);
    /* source position is end of freespace */
    sourcePosition = pCIDF->m_location + pCIDF->m_amountOfFreeSpace;
    
    //move RDFs - only if not last record then clear data only
    if(moveSIZE > 0)
    {
        memmove(pBlock + destinationPosition, pBlock + sourcePosition, moveSIZE);
    }
    
    //update CIDF, freespace + set dirty flag
    pCIDF->m_amountOfFreeSpace += (rRDFCopy.m_recordLength + sizeof(RDF));
    pCIDF->m_location -= rRDFCopy.m_recordLength;
    pCIDF->m_flags |= eBLF_Dirty;
    
    //clear freespace
    memset(pBlock + pCIDF->m_location, 0, pCIDF->m_amountOfFreeSpace);
}

E_BLS Block::GetRecord(uint8 *pBlock, uint64 recordKey, ByteBuffer &rData)
{
    E_BLS status;
    Record rRecord;
    ByteBuffer rOut;
    
    //get record from block
    status = Block::FindRecord(pBlock, recordKey, &rRecord);
    if(status == eBLS_KEY_NOT_FOUND)
        return status;

    //check if gzipped
    if(CommonFunctions::isGziped(rRecord.m_pRecord))
    {
        int decompressStatus = CommonFunctions::decompressGzip(rRecord.m_pRecord, rRecord.m_pRDF->m_recordLength, rOut);
        if(decompressStatus == Z_OK)
        {
            rRecord.m_pRecord = (uint8*)rOut.contents();
            rRecord.m_pRDF->m_recordLength = (uint16)rOut.size();
            ++g_NumOfRecordDecompressions;
        }
    }

    //append record
    rData.append(rRecord.m_pRecord, rRecord.m_pRDF->m_recordLength);
    
    return eBLS_OK;
}

void Block::GetRecords(uint8 *pBlock, ByteBuffer &rData)
{
    Record rRecord;
    ByteBuffer rOut;
    //init traverser
    RecordTraverser rT;
    Block::RecordTraverserInit(&rT, pBlock);
    
    while(Block::RecordTraverserNext(&rT, &rRecord))
    {
        //unzip + rewrite variables
        if(CommonFunctions::isGziped(rRecord.m_pRecord))
        {
            int decompressStatus = CommonFunctions::decompressGzip(rRecord.m_pRecord, rRecord.m_pRDF->m_recordLength, rOut);
            if(decompressStatus == Z_OK)
            {
                rRecord.m_pRecord = (uint8*)rOut.contents();
                rRecord.m_pRDF->m_recordLength = (uint16)rOut.size();
                ++g_NumOfRecordDecompressions;
            }
        }
        
        //copy key + records size + record
        rData << uint64(rRecord.m_pRDF->m_key);
        rData << uint16(rRecord.m_pRDF->m_recordLength);
        rData.append(rRecord.m_pRecord, rRecord.m_pRDF->m_recordLength);
    }
}











