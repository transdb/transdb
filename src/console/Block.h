//
//  Block.h
//  TransDB
//
//  Created by Miroslav Kudrnac on 8/23/12.
//  Copyright (c) 2012 Miroslav Kudrnac. All rights reserved.
//

#ifndef TransDB_Block_h
#define TransDB_Block_h

#ifdef __cplusplus
extern "C" {
#endif
    
#include "../shared/clib/Buffers/CByteBuffer.h"

//disable alignment
#pragma pack(push, 1)
//Control interval
typedef struct ControlIntervalDefinitionField
{
    uint16              m_amoutOfFreeSpace;         //2
    uint16              m_location;                 //2
    uint8               m_flags;                    //1
} CIDF;

//Record definition field
typedef struct RecordDefinitionField
{
    uint64              m_key;                      //8
    uint16              m_recordLength;             //2
} RDF;
#pragma pack(pop)

typedef enum E_BLOCK_STATUS
{
    eBLS_OK                         = 1,
    eBLS_KEY_NOT_FOUND              = 2,
    eBLS_NO_SPACE_FOR_NEW_DATA      = 3
} E_BLS;

typedef enum E_BLOCK_FLAGS
{
    eBLF_None                       = 0,
    eBLF_Dirty                      = 1,
    eBLF_Metadata                   = 2
} E_BLF;

//block size - 4kB 
#define BLOCK_SIZE          4096
//CIDF offset in block
#define CIDFOffset          (BLOCK_SIZE - sizeof(CIDF))
//max record size
#define MAX_RECORD_SIZE     (CIDFOffset - sizeof(RDF))

/** Get ControlIntervalDefinitionField from block
 */
CIDF *Block_GetCIDF(uint8 *pBlock);
    
/** Init block
 */
void Block_InitBlock(uint8 *pBlock);

/** Search for record with key in block
 */
RDF *Block_ContainsKey(uint8 *pBlock, uint64 recordKey, uint16 *recordPosition_Out);

/** Write record to block - return NULL if record cannot be wrtitten
 */
E_BLS Block_WriteRecord(uint8 *pBlock, uint64 recordKey, const uint8 *pRecord, uint16 recordSize);

/** Delete record from block by record key
 */
E_BLS Block_DeleteRecordByKey(uint8 *pBlock, uint64 recordKey);
    
/** Delete record from block by RDF
 */
void Block_DeleteRecordByRDF(uint8 *pBlock, RDF *pRDF, uint16 recordPosition);

/** Update record in block
 */
E_BLS Block_UpdateRecord(uint8 *pBlock, uint64 recordKey, const uint8 *pNewRecord, uint16 recordSize);

/** Get record by key
 */
void Block_GetRecord(uint8 *pBlock, uint64 recordKey, bbuff *pData);

/** Get all records from block
 */
void Block_GetRecords(uint8 *pBlock, bbuff *pData);


#ifdef __cplusplus
}
#endif
    
#endif
