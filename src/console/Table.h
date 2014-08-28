//
//  Table.h
//  TransDB
//
//  Created by Miroslav Kudrnac on 28.08.14.
//  Copyright (c) 2014 Miroslav Kudrnac. All rights reserved.
//

#ifndef TransDB_Table_h
#define TransDB_Table_h

typedef enum E_SQL_DATA_TYPE
{
    esdtInt32       = 1,            //4 bytes
    esdtInt64       = 2,            //8 bytes
    esdtVarchar     = 3             //len coded string max 254 bytes - 1byte for len
} E_SDT;

typedef enum E_SQL_COLUMN_FLAGS
{
    escfNone            = 0,
    escfAutoIncrement   = 1,        //autoincrement column - value is equal to ROWID
    escfIndexed         = 2         //create index for this column
    
} E_SCF;

typedef enum E_SQL_TABLE_FLAGS
{
    
} E_STF;

struct Column
{
    uint8       m_dataType;         //1
    uint8       m_dataSize;         //1 - used for store len of varchar column
    uint8       m_flags;            //1
    char        m_name[8];          //8
};

struct Row
{
    uint64      m_rowId;            //8
    void        *m_pData;           //8
};

struct TableDefinition
{
    uint8       m_columns;          //1
    uint8       m_flags;            //1
    char        m_tableName[8];     //8
};

#endif
