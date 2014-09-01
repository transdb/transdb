//
//  Block.cpp
//  TransDB
//
//  Created by Miroslav Kudrnac on 8/23/12.
//  Copyright (c) 2012 Miroslav Kudrnac. All rights reserved.
//

#include "StdAfx.h"

CIDF *Block::GetCIDF(uint8 *pBlock)
{
    return (CIDF*)(pBlock + CIDFOffset);
}

void Block::InitBlock(uint8 *pBlock)
{
    //write CIDF
    CIDF *pCIDF = Block::GetCIDF(pBlock);
    pCIDF->m_amoutOfFreeSpace = CIDFOffset;
    pCIDF->m_location = 0;
    pCIDF->m_flags = eBLF_Dirty;
}

RDF *Block::ContainsKey(uint8 *pBlock, uint64 recordKey, uint16 *RDFPosition, uint16 *recordPosition)
{
    //get CIDF
    CIDF *pCIDF = Block::GetCIDF(pBlock);
    RDF *pRDF;
    
    //find RDS by key - Loop must be from end of block to begin
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
        pRDF = (RDF*)(pBlock + position);
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

E_BLS Block::WriteRecord(uint8 *pBlock, uint64 recordKey, const uint8 *pRecord, uint16 recordSize)
{
    //!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!
    //key check must be done by blockmanager
    //!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!
    CIDF *pCIDF;
    RDF *pRDF;
    uint16 positionOfNewRDS;
    
    //get CIDF
    pCIDF = Block::GetCIDF(pBlock);
    
    //no space BlockManager should allocate new block
    if(pCIDF->m_amoutOfFreeSpace < (recordSize + sizeof(RDF)))
        return eBLS_NO_SPACE_FOR_NEW_DATA;
    
    //get position of new RDS
    positionOfNewRDS = (pCIDF->m_location + pCIDF->m_amoutOfFreeSpace) - sizeof(RDF);
    
    //write RDS
    pRDF = (RDF*)(pBlock + positionOfNewRDS);
    pRDF->m_key = recordKey;
    pRDF->m_recordLength = recordSize;
    
    //save data to block
    memcpy(pBlock + pCIDF->m_location, pRecord, recordSize);
    
    //update CIDF
    pCIDF->m_amoutOfFreeSpace -= (recordSize + sizeof(RDF));
    pCIDF->m_location += recordSize;
    pCIDF->m_flags |= eBLF_Dirty;
    
    return eBLS_OK;
}

E_BLS Block::DeleteRecord(uint8 *pBlock, uint64 recordKey, RDF *pRDF, uint16 *RDFPosition, uint16 *recordPosition)
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
    RDF rRDFCopy;
    
    //get CIDF
    pCIDF = Block::GetCIDF(pBlock);
    
    //create copy of RDF - current will be deleted
    memcpy(&rRDFCopy, pRDF, sizeof(RDF));
    
    
    /*
     
     Record size 700 bytes
     +---------+---------+---------+---------+------------+------+------+------+------+------+
     | record1 | record2 | record3 | record4 | free space | RDF4 | RDF3 | RDF2 | RDF1 | CIDF |
     +---------+---------+---------+---------+------------+------+------+------+------+------+
     0        700      1400      2100      2800         4051   4061   4071   4081   4091   4096
     
     */
    
    //Records
    /* move size is freespace start - start of next record */
    moveSIZE = pCIDF->m_location - (*recordPosition + rRDFCopy.m_recordLength);
    /* destination is record position */
    destinationPosition = *recordPosition;
    /* source is record position + record length = start of next record */
    sourcePosition = *recordPosition + rRDFCopy.m_recordLength;
    
    //move records - only if not last record then clear data only
    if(moveSIZE > 0)
    {
        memmove(pBlock + destinationPosition, pBlock + sourcePosition, moveSIZE);
    }
    
    //RDFs
    /* move size if RDF position - end of freespace */
    moveSIZE = *RDFPosition - (pCIDF->m_location + pCIDF->m_amoutOfFreeSpace);
    /* destination is end of freespace + size of RDF */
    destinationPosition = pCIDF->m_location + pCIDF->m_amoutOfFreeSpace + sizeof(RDF);
    /* source position is end of freespace */
    sourcePosition = pCIDF->m_location + pCIDF->m_amoutOfFreeSpace;
    
    //move RDFs - only if not last record then clear data only
    if(moveSIZE > 0)
    {
        memmove(pBlock + destinationPosition, pBlock + sourcePosition, moveSIZE);
    }
    
    //update free space
    pCIDF->m_amoutOfFreeSpace += (rRDFCopy.m_recordLength + sizeof(RDF));
    pCIDF->m_location -= rRDFCopy.m_recordLength;
    pCIDF->m_flags |= eBLF_Dirty;
    
    //clear empty space
    memset(pBlock + pCIDF->m_location, 0, pCIDF->m_amoutOfFreeSpace);
    
    return eBLS_OK;
}

E_BLS Block::UpdateRecord(uint8 *pBlock, uint64 recordKey, const uint8 *pNewRecord, uint16 recordSize, RDF *pRDF, uint16 *RDFPosition, uint16 *recordPosition)
{
    CIDF *pCIDF;
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
        memcpy(pBlock + (*recordPosition), pNewRecord, recordSize);
        //get CIDF - update dirty flag
        pCIDF = Block::GetCIDF(pBlock);
        pCIDF->m_flags |= eBLF_Dirty;
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

void Block::GetRecord(uint8 *pBlock, uint64 recordKey, ByteBuffer &rData)
{
    RDF *pRDF;
    uint16 RDFPosition;
    uint16 recordPosition;
    ByteBuffer rOut;
    uint8 *pRecordLocal;
    uint16 recordSizeLocal;
    
    pRDF = Block::ContainsKey(pBlock, recordKey, &RDFPosition, &recordPosition);
    if(pRDF == NULL)
        return;
    
    //set support variables
    pRecordLocal = (uint8*)(pBlock + recordPosition);
    recordSizeLocal = pRDF->m_recordLength;
    
    //check if gzipped
    if(CommonFunctions::isGziped(pRecordLocal))
    {
        int status = CommonFunctions::decompressGzip(pRecordLocal, recordSizeLocal, rOut);
        if(status == Z_OK)
        {
            pRecordLocal = (uint8*)rOut.contents();
            recordSizeLocal = (uint16)rOut.size();
            ++g_NumOfRecordDecompressions;
        }
    }
    
    //append record
    rData.append(pRecordLocal, recordSizeLocal);
}

void Block::GetRecords(uint8 *pBlock, ByteBuffer &rData)
{
    //get CIDF
    CIDF *pCIDF = Block::GetCIDF(pBlock);
    
    //interate RDS
    uint16 recordSize = 0;
    uint16 position = CIDFOffset - sizeof(RDF);
    uint16 endOfRDFArea = pCIDF->m_location + pCIDF->m_amoutOfFreeSpace;
    
    ByteBuffer rOut;    
    for(;;)
    {
        //end of RDF area
        if(position < endOfRDFArea)
            break;
        
        //get RDF
        RDF *pRDF = (RDF*)(pBlock + position);
        position -= sizeof(RDF);
        
        //save variables
        uint8 *pRecordLocal = (uint8*)(pBlock + recordSize);
        uint16 recordSizeLocal = pRDF->m_recordLength;
        
        //unzip + rewrite variables
        if(CommonFunctions::isGziped(pRecordLocal))
        {
            int status = CommonFunctions::decompressGzip(pRecordLocal, recordSizeLocal, rOut);
            if(status == Z_OK)
            {
                pRecordLocal = (uint8*)rOut.contents();
                recordSizeLocal = (uint16)rOut.size();
                ++g_NumOfRecordDecompressions;
            }
        }
        
        //copy key + records size + record
        rData << uint64(pRDF->m_key);
        rData << uint16(recordSizeLocal);
        rData.append(pRecordLocal, recordSizeLocal);
        
        //count position of data
        recordSize += pRDF->m_recordLength;
    }
}
