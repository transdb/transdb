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
    uint16              m_amoutOfFreeSpace;         //2
    uint16              m_location;                 //2
} CIDF;

//Record definition field
typedef struct RecordDefinitionField
{
    uint64              m_key;                      //8
	uint32				m_crc32;					//4
    uint16              m_recordLength;             //2
} RDF;
#pragma pack(pop)

typedef enum E_BLOCK_STATUS
{
    eBLS_OK                         = 1,
    eBLS_KEY_NOT_FOUND              = 2,
    eBLS_NO_SPACE_FOR_NEW_DATA      = 3
} E_BLS;

//block size
static const uint16 BLOCK_SIZE = 4096;              //4kB
//CIDF offset in block
static const uint16 CIDFOffset = BLOCK_SIZE - sizeof(CIDF);

//block store
typedef std::vector<uint8*> Blocks;

//for memory pool
struct BlockSize_T
{
	BlockSize_T()
	{
//		memset(&m_val, 0, BLOCK_SIZE);
	}

    uint8 m_val[BLOCK_SIZE];
};

//macro for CIDF
#define GetCIDF(block)      (CIDF*)(block+CIDFOffset)

class Block
{
public:
    static void InitBlock(uint8 *pBlock);
    static RDF *ContainsKey(uint8 *pBlock, uint64 recordKey, uint16 *RDFPosition, uint16 *recordPosition);
    static E_BLS WriteRecord(uint8 *pBlock, uint64 recordKey, uint8 *pRecord, uint16 recordSize);
    static E_BLS DeleteRecord(uint8 *pBlock, uint64 recordKey, RDF *pRDF, uint16 *RDFPosition, uint16 *recordPosition);
    static E_BLS UpdateRecord(uint8 *pBlock, uint64 recordKey, uint8 *pNewRecord, uint16 recordSize, RDF *pRDF, uint16 *RDFPosition, uint16 *recordPosition);
    static void GetRecord(uint8 *pBlock, uint64 recordKey, uint8 **pRecord, uint16 *recordSize);
    static void GetRecords(uint8 *pBlock, uint8 **pRecords, uint32 *recordsSize);
};
    
#endif
