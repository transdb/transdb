//
//  Block.cpp
//  TransDB
//
//  Created by Miroslav Kudrnac on 8/23/12.
//  Copyright (c) 2012 Miroslav Kudrnac. All rights reserved.
//

#include <string.h>
#include "../shared/clib/CCommon.h"
#include "../shared/zlib/zlib.h"
#include "Block.h"

CIDF *Block_GetCIDF(uint8 *pBlock)
{
    return (CIDF*)(pBlock + CIDFOffset);
}

void Block_InitBlock(uint8 *pBlock)
{
    //write CIDF
    CIDF *pCIDF = Block_GetCIDF(pBlock);
    pCIDF->m_amoutOfFreeSpace = CIDFOffset;
    pCIDF->m_location = 0;
    pCIDF->m_flags = eBLF_Dirty;
}

RDF *Block_ContainsKey(uint8 *pBlock, uint64 recordKey, uint16 *recordPosition_Out)
{
    //get CIDF
    CIDF *pCIDF = Block_GetCIDF(pBlock);
    RDF *pRDF = NULL;
    
    //find RDS by key - Loop must be from end of block to begin
    *recordPosition_Out = 0;
    uint16 RDFposition = CIDFOffset - sizeof(RDF);
    uint16 endOfRDFArea = pCIDF->m_location + pCIDF->m_amoutOfFreeSpace;
    for(;;)
    {
        //end of RDF area
        if(RDFposition < endOfRDFArea)
            break;
        
        //get RDF
        pRDF = (RDF*)(pBlock + RDFposition);
        //check key
        if(pRDF->m_key == recordKey)
            break;
        
        //update position of data
        *recordPosition_Out += pRDF->m_recordLength;
        //update position of RDF
        RDFposition -= sizeof(RDF);
    }
    return pRDF;
}

E_BLS Block_WriteRecord(uint8 *pBlock, uint64 recordKey, const uint8 *pRecord, uint16 recordSize)
{
    //!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!
    //key check must be done by blockmanager
    //!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!
    
    //get CIDF
    CIDF *pCIDF = Block_GetCIDF(pBlock);
    //no space BlockManager should allocate new block
    if(pCIDF->m_amoutOfFreeSpace < (recordSize + sizeof(RDF)))
    {
        return eBLS_NO_SPACE_FOR_NEW_DATA;
    }
    
    //get position of new RDS
    uint16 positionOfNewRDS = (pCIDF->m_location + pCIDF->m_amoutOfFreeSpace) - sizeof(RDF);
    
    //write RDF
    RDF *pRDF = (RDF*)(pBlock + positionOfNewRDS);
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

E_BLS Block_DeleteRecordByKey(uint8 *pBlock, uint64 recordKey)
{
    uint16 recordPosition;
    RDF *pRDF = Block_ContainsKey(pBlock, recordKey, &recordPosition);
    if(pRDF == NULL)
        return eBLS_KEY_NOT_FOUND;
    
    Block_DeleteRecordByRDF(pBlock, pRDF, recordPosition);
    return eBLS_OK;
}

void Block_DeleteRecordByRDF(uint8 *pBlock, RDF *pRDF, uint16 recordPosition)
{
    //helpers variables
    size_t moveSIZE;
    size_t destinationPosition;
    size_t sourcePosition;
    
    //get CIDF
    CIDF *pCIDF = Block_GetCIDF(pBlock);
    
    //create copy of RDF - current will be deleted
    RDF rRDFCopy;
    memcpy(&rRDFCopy, pRDF, sizeof(RDF));

    //calc RDF position
    uint16 RDFPosition = (uint16)((uint8*)pRDF - pBlock);
    
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
    moveSIZE = RDFPosition - (pCIDF->m_location + pCIDF->m_amoutOfFreeSpace);
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
}

E_BLS Block_UpdateRecord(uint8 *pBlock, uint64 recordKey, const uint8 *pNewRecord, uint16 recordSize)
{
    E_BLS addStatus = eBLS_OK;
    
    //find RDF by key
    uint16 recordPosition;
    RDF *pRDF = Block_ContainsKey(pBlock, recordKey, &recordPosition);
    if(pRDF == NULL)
        return eBLS_KEY_NOT_FOUND;
    
    //found key
    if(recordSize == pRDF->m_recordLength)
    {
        //data has same size replace
        memcpy(pBlock + recordPosition, pNewRecord, recordSize);
        //get CIDF - update dirty flag
        CIDF *pCIDF = Block_GetCIDF(pBlock);
        pCIDF->m_flags |= eBLF_Dirty;
    }
    else
    {
        //delete old data and add new
        Block_DeleteRecordByRDF(pBlock, pRDF, recordPosition);
        //add new data
        addStatus = Block_WriteRecord(pBlock, recordKey, pNewRecord, recordSize);
    }
    
    return addStatus;
}

void Block_GetRecord(uint8 *pBlock, uint64 recordKey, bbuff *pData)
{
    uint16 recordPosition;
    RDF *pRDF = Block_ContainsKey(pBlock, recordKey, &recordPosition);
    if(pRDF == NULL)
        return;
    
    //set support variables
    uint8 *pRecordLocal = (uint8*)(pBlock + recordPosition);
    uint16 recordSizeLocal = pRDF->m_recordLength;
    
    //check if gzipped
    if(CCommon_isGziped(pRecordLocal))
    {
        int status = CCommon_decompressGzip(pRecordLocal, recordSizeLocal, pData, 512*1024);
        if(status != Z_OK)
        {
            //decompress failed append original record
            bbuff_resize(pData, 0);
            bbuff_append(pData, pRecordLocal, recordSizeLocal);
        }
    }
    else
    {
        //append record
        bbuff_append(pData, pRecordLocal, recordSizeLocal);
    }
}

void Block_GetRecords(uint8 *pBlock, bbuff *pData)
{
    //get CIDF
    CIDF *pCIDF = Block_GetCIDF(pBlock);
    
    //interate RDS
    uint16 recordPosition = 0;
    uint16 RDFPosition = CIDFOffset - sizeof(RDF);
    uint16 endOfRDFArea = pCIDF->m_location + pCIDF->m_amoutOfFreeSpace;
    
    for(;;)
    {
        //end of RDF area
        if(RDFPosition < endOfRDFArea)
            break;
        
        //get RDF
        RDF *pRDF = (RDF*)(pBlock + RDFPosition);
        RDFPosition -= sizeof(RDF);
        
        //save variables
        uint8 *pRecord = (uint8*)(pBlock + recordPosition);
        uint16 recordSize = pRDF->m_recordLength;
        
        //append key
        bbuff_append(pData, &pRDF->m_key, sizeof(pRDF->m_key));
        //append size, will be rewrited if record is compressed
        bbuff_append(pData, &recordSize, sizeof(recordSize));
        
        //unzip + rewrite variables
        if(CCommon_isGziped(pRecord))
        {
            //save wpos if decompress fails or we need to change record size after decompress
            size_t wpos = pData->wpos;
            
            //decompress directly to bytebuffer
            int status = CCommon_decompressGzip(pRecord, recordSize, pData, 512*1024);
            if(status != Z_OK)
            {
                //decompress failed append original record
                bbuff_resize(pData, wpos);
                bbuff_append(pData, pRecord, recordSize);
            }
            else
            {
                //calc decompressed record size
                recordSize = pData->wpos - wpos;
                //change record size
                bbuff_put(pData, wpos - sizeof(recordSize), &recordSize, sizeof(recordSize));
            }
        }
        else
        {
            //append record non compressed record
            bbuff_append(pData, pRecord, recordSize);
        }
        
        //count position of data
        recordPosition += pRDF->m_recordLength;
    }
}
