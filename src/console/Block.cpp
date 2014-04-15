//
//  Block.cpp
//  TransDB
//
//  Created by Miroslav Kudrnac on 8/23/12.
//  Copyright (c) 2012 Miroslav Kudrnac. All rights reserved.
//

#include "StdAfx.h"

void Block::InitBlock(uint8 *pBlock)
{
    //write CIDF
    CIDF *pCIDF = GetCIDF(pBlock);
    pCIDF->m_amoutOfFreeSpace = CIDFOffset;
    pCIDF->m_location = 0;
}

RDF *Block::ContainsKey(uint8 *pBlock, uint64 recordKey, uint16 *RDFPosition, uint16 *recordPosition)
{
    //get CIDF
    CIDF *pCIDF = GetCIDF(pBlock);
    RDF *pRDF;
    
    //find RDS by key - Loop must be from end of block to start
    uint16 RDFPositionLocal = 0;
    uint16 recordPositionLocal = 0;
    uint16 position = CIDFOffset - sizeof(RDF);
    uint16 endOfRDFArea = pCIDF->m_location + pCIDF->m_amoutOfFreeSpace;
    for(;;)
    {
        //end of RDF area
        if(position < endOfRDFArea)
        {
            pRDF = NULL;
            break;
        }
        
        //save RDF position
        RDFPositionLocal = position;
        //get RDF
        pRDF = (RDF*)(pBlock+position);
        //check key
        if(pRDF->m_key == recordKey)
            break;
        
        //count position of data
        recordPositionLocal += pRDF->m_recordLength;
        //
        position -= sizeof(RDF);
    }
    
    //set pointer`s value
    *recordPosition = recordPositionLocal;
    *RDFPosition = RDFPositionLocal;
    
    return pRDF;
}

E_BLS Block::DeleteRecord(uint8 *pBlock,
                          uint64 recordKey,
                          RDF *pRDF,
                          uint16 *RDFPosition,
                          uint16 *recordPosition)
{
    if(pRDF == NULL)
    {
        pRDF = Block::ContainsKey(pBlock, recordKey, RDFPosition, recordPosition);
        if(pRDF == NULL)
            return eBLS_KEY_NOT_FOUND;
    }
    
    size_t moveSIZE;
    size_t destinationPosition;
    size_t sourcePosition;
    CIDF *pCIDF;
    
    //get CIDF
    pCIDF = GetCIDF(pBlock);
    
    //create copy of RDF - current will be deleted
    RDF rRDFCopy = (*pRDF);
    
    
    /*
     
     Record size 700 bytes
     +---------+---------+------------+------+------+------+
     | record1 | record2 | free space | RDF2 | RDF1 | CIDF |
     +---------+---------+------------+------+------+------+
     0        700      1400          4064   4078   4092   4096
     
     */
    
    //move to deleting record start
    moveSIZE = pCIDF->m_location - (*recordPosition + pRDF->m_recordLength);
    destinationPosition = *recordPosition;
    sourcePosition = *recordPosition + pRDF->m_recordLength;
    
    //move records
    memmove(pBlock+destinationPosition, pBlock+sourcePosition, moveSIZE);
    
    //RDFs
    moveSIZE = *RDFPosition - (pCIDF->m_location + pCIDF->m_amoutOfFreeSpace);
    destinationPosition = pCIDF->m_location + pCIDF->m_amoutOfFreeSpace + sizeof(RDF);
    sourcePosition = pCIDF->m_location + pCIDF->m_amoutOfFreeSpace;
    
    //move RDFs
    memmove(pBlock+destinationPosition, pBlock+sourcePosition, moveSIZE);
    
    //update free space
    pCIDF->m_amoutOfFreeSpace += (rRDFCopy.m_recordLength + sizeof(RDF));
    pCIDF->m_location -= rRDFCopy.m_recordLength;
    
    //clear empty space
    memset(pBlock+pCIDF->m_location, 0, pCIDF->m_amoutOfFreeSpace);
    
    return eBLS_OK;
}

E_BLS Block::UpdateRecord(uint8 *pBlock,
                          uint64 recordKey,
                          uint8 *pNewRecord,
                          uint16 recordSize,
                          RDF *pRDF,
                          uint16 *RDFPosition,
                          uint16 *recordPosition)
{
    E_BLS addStatus = eBLS_OK;
    
    //find RDS by key
    if(pRDF == NULL)
    {
        pRDF = Block::ContainsKey(pBlock, recordKey, RDFPosition, recordPosition);
        if(pRDF == NULL)
            return eBLS_KEY_NOT_FOUND;
    }
    
    //found key
    if(recordSize == pRDF->m_recordLength)
    {
        //data has same size replace
        memcpy(pBlock+(*recordPosition), pNewRecord, recordSize);
		//update crc
		pRDF->m_crc32 = g_CRC32->ComputeCRC32(pNewRecord, recordSize);
    }
    else
    {
        //delete old data and add new
        Block::DeleteRecord(pBlock, recordKey, pRDF, RDFPosition, recordPosition);
        //add new data
        addStatus = Block::WriteRecord(pBlock, recordKey, pNewRecord, recordSize);
    }
    
    return addStatus;
}

E_BLS Block::WriteRecord(uint8 *pBlock, uint64 recordKey, uint8 *pRecord, uint16 recordSize)
{
    //!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!
    //key check must be done by blockmanager
    //!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!    
    
    //get CIDF
    CIDF *pCIDF = GetCIDF(pBlock);
    RDF *pRDF;
    uint16 positionOfNewRDS;
    
    //no space BlockManager should allocate new block
    if(pCIDF->m_amoutOfFreeSpace < (recordSize + sizeof(RDF)))
        return eBLS_NO_SPACE_FOR_NEW_DATA;
    
    //get position of new RDS
    positionOfNewRDS = (pCIDF->m_location + pCIDF->m_amoutOfFreeSpace) - sizeof(RDF);
    
    //write RDS
    pRDF = (RDF*)(pBlock+positionOfNewRDS);
    pRDF->m_key				= recordKey;
    pRDF->m_recordLength	= recordSize;
	pRDF->m_crc32			= g_CRC32->ComputeCRC32(pRecord, recordSize);
    
    //save data to block
    memcpy(pBlock+pCIDF->m_location, pRecord, recordSize);
    
    //update CIDF
    pCIDF->m_amoutOfFreeSpace -= (recordSize + sizeof(RDF));
    pCIDF->m_location += recordSize;
    
    return eBLS_OK;
}

void Block::GetRecord(uint8 *pBlock, uint64 recordKey, uint8 **pRecord, uint16 *recordSize)
{
    RDF *pRDF;
    uint16 RDFPosition;
    uint16 recordPosition;
    
    pRDF = Block::ContainsKey(pBlock, recordKey, &RDFPosition, &recordPosition);
    if(pRDF == NULL)
    {
        *pRecord = NULL;
        *recordSize = 0;
        return;
    }
    
    //allocate record
    *recordSize = pRDF->m_recordLength;
    *pRecord = (uint8*)aligned_malloc(pRDF->m_recordLength);
    memcpy(*pRecord, pBlock+recordPosition, pRDF->m_recordLength);
}

void Block::GetRecords(uint8 *pBlock, uint8 **pRecords, uint32 *recordsSize)
{
    //get CIDF
    CIDF *pCIDF = GetCIDF(pBlock);
    RDF *pRDF;
    
    //find RDS by key
    uint16 recordSize = 0;
    uint16 position = CIDFOffset - sizeof(RDF);
    uint16 endOfRDFArea = pCIDF->m_location + pCIDF->m_amoutOfFreeSpace;
    uint32 recordsSizeLocal = *recordsSize;
    for(;;)
    {
        //end of RDF area
        if(position < endOfRDFArea)
            break;
        
        //get RDF
        pRDF = (RDF*)(pBlock+position);
        position -= sizeof(RDF);
        
        //copy key + records size + record
        memcpy(*pRecords+recordsSizeLocal, &pRDF->m_key, sizeof(pRDF->m_key));
        memcpy(*pRecords+recordsSizeLocal+sizeof(pRDF->m_key), &pRDF->m_recordLength, sizeof(pRDF->m_recordLength));
        memcpy(*pRecords+recordsSizeLocal+sizeof(pRDF->m_key)+sizeof(pRDF->m_recordLength), pBlock+recordSize, pRDF->m_recordLength);
        
        //count position of data
        recordSize += pRDF->m_recordLength;
        recordsSizeLocal += (pRDF->m_recordLength + sizeof(pRDF->m_key) + sizeof(pRDF->m_recordLength));
    }
    
    //set pointer`s value
    *recordsSize = recordsSizeLocal;
}

































