//
//  Block.cpp
//  TransDB
//
//  Created by Miroslav Kudrnac on 8/23/12.
//  Copyright (c) 2012 Miroslav Kudrnac. All rights reserved.
//

#include "StdAfx.h"

void Block::InitBlock(const uint8 *pBlock)
{
    //write CIDF
    CIDF *pCIDF = GetCIDF(pBlock);
    pCIDF->m_amoutOfFreeSpace   = CIDFOffset;
    pCIDF->m_location           = 0;
    pCIDF->m_flags              = eBLF_Dirty;
}

RDF *Block::ContainsKey(const uint8 *pBlock, const uint64 &recordKey, uint16 *RDFPosition, uint16 *recordPosition)
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
                          const uint64 &recordKey,
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
    pCIDF->m_flags |= eBLF_Dirty;
    
    //clear empty space
    memset(pBlock+pCIDF->m_location, 0, pCIDF->m_amoutOfFreeSpace);
    
    return eBLS_OK;
}

E_BLS Block::UpdateRecord(uint8 *pBlock,
                          const uint64 &recordKey,
                          const uint8 *pNewRecord,
                          const uint16 &recordSize,
                          RDF *pRDF,
                          uint16 *RDFPosition,
                          uint16 *recordPosition)
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
		//update crc
		pRDF->m_crc32 = g_CRC32->ComputeCRC32(pNewRecord, recordSize);
        //get CIDF - update dirty flag
        pCIDF = GetCIDF(pBlock);
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

E_BLS Block::WriteRecord(uint8 *pBlock, const uint64 &recordKey, const uint8 *pRecord, const uint16 &recordSize)
{
    //!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!
    //key check must be done by blockmanager
    //!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!    
    CIDF *pCIDF;
    RDF *pRDF;
    uint16 positionOfNewRDS;
    
    //get CIDF
    pCIDF = GetCIDF(pBlock);
    
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
    memcpy(pBlock + pCIDF->m_location, pRecord, recordSize);
    
    //update CIDF
    pCIDF->m_amoutOfFreeSpace -= (recordSize + sizeof(RDF));
    pCIDF->m_location += recordSize;
    pCIDF->m_flags |= eBLF_Dirty;
    
    return eBLS_OK;
}

void Block::GetRecord(const uint8 *pBlock, const uint64 &recordKey, ByteBuffer &rData)
{
    RDF *pRDF;
    uint16 RDFPosition;
    uint16 recordPosition;
    ByteBuffer rOut;
    int status;
    uint8 *pRecordLocal;
    uint16 recordSizeLocal;
    
    pRDF = Block::ContainsKey(pBlock, recordKey, &RDFPosition, &recordPosition);
    if(pRDF == NULL)
        return;
    
    //set support variables
    pRecordLocal = (uint8*)(pBlock + recordPosition);
    recordSizeLocal = pRDF->m_recordLength;
    
    //check if gzipped
    if(Common::isGziped(pRecordLocal))
    {
        status = Common::decompressGzip(pRecordLocal, recordSizeLocal, rOut);
        if(status == Z_OK)
        {
            pRecordLocal = (uint8*)rOut.contents();
            recordSizeLocal = (uint16)rOut.size();
            g_NumOfRecordDecompressions++;
        }
    }
    
    //append record
    rData.append(pRecordLocal, recordSizeLocal);
}

void Block::GetRecords(const uint8 *pBlock, ByteBuffer &rData)
{
    //get CIDF
    CIDF *pCIDF = GetCIDF(pBlock);
    RDF *pRDF;
    
    //interate RDS
    uint16 recordSize = 0;
    uint16 position = CIDFOffset - sizeof(RDF);
    uint16 endOfRDFArea = pCIDF->m_location + pCIDF->m_amoutOfFreeSpace;
    
    ByteBuffer rOut;
    int status;
    uint8 *pRecordLocal;
    uint16 recordSizeLocal;
    
    for(;;)
    {
        //end of RDF area
        if(position < endOfRDFArea)
            break;
        
        //get RDF
        pRDF = (RDF*)(pBlock+position);
        position -= sizeof(RDF);
        
        //save variables
        pRecordLocal = (uint8*)(pBlock + recordSize);
        recordSizeLocal = pRDF->m_recordLength;
        
        //unzip + rewrite variables
        if(Common::isGziped(pRecordLocal))
        {
            status = Common::decompressGzip(pRecordLocal, recordSizeLocal, rOut);
            if(status == Z_OK)
            {
                pRecordLocal = (uint8*)rOut.contents();
                recordSizeLocal = (uint16)rOut.size();
                g_NumOfRecordDecompressions++;
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

































