//
//  Block.h
//  TransDB
//
//  Created by Miroslav Kudrnac on 8/23/12.
//  Copyright (c) 2012 Miroslav Kudrnac. All rights reserved.
//

#ifndef TransDB_Block_h
#define TransDB_Block_h

//disable alignment
#pragma pack(push, 1)
//Control interval
typedef struct ControlIntervalDefinitionField
{
    uint16              m_amountOfFreeSpace;        //2
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

//Record
typedef struct _Record
{
    uint8   *m_pRecord;
    RDF     *m_pRDF;
} Record;

//Record traverser
typedef struct _RecordTraverser
{
    uint8   *m_pBlock;
    RDF     *m_pBegin;
    RDF     *m_pEnd;
    uint16  m_recordOffset;
} RecordTraverser;

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

//block size
static const uint16 BLOCK_SIZE      = 4096;              //4kB
//CIDF offset in block
static const uint16 CIDFOffset      = (BLOCK_SIZE - sizeof(CIDF));
//max record size
static const uint16 MAX_RECORD_SIZE = (CIDFOffset - sizeof(RDF));

class Block
{
public:
    /** Init record traverser
     */
    static void RecordTraverserInit(RecordTraverser *t, uint8 *pBlock);
    
    /** Return next Record struct
     */
    static bool RecordTraverserNext(RecordTraverser *t, Record *pRecord);
    
    /** Get ControlIntervalDefinitionField from block
     */
    static CIDF *GetCIDF(uint8 *pBlock);
    
    /** Init block
     */
    static void InitBlock(uint8 *pBlock);
    
    /** Search for record with key in block and fill Record struct
     */
    static E_BLS FindRecord(uint8 *pBlock, uint64 recordKey, Record *pRecord);
    
    /** Delete record from block by record key
     */
    static E_BLS WriteRecord(uint8 *pBlock, uint64 recordKey, const uint8 *pRecord, uint16 recordSize);
    
    /** Delete record from block by record key
     */
    static E_BLS DeleteRecordByKey(uint8 *pBlock, uint64 recordKey);
    
    /** Delete record from block by Record
     */
    static void DeleteRecordByRecord(uint8 *pBlock, Record *pRecord);
    
    /** Update record in block
     */
    static E_BLS UpdateRecord(uint8 *pBlock, uint64 recordKey, const uint8 *pRecord, uint16 recordSize);
    
    /** Get record by key
     */
    static E_BLS GetRecord(uint8 *pBlock, uint64 recordKey, ByteBuffer &rData);
    
    /** Get all records from block
     */
    static void GetRecords(uint8 *pBlock, ByteBuffer &rData);
};

#endif
