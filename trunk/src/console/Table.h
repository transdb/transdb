//
//  Table.h
//  TransDB
//
//  Created by Miroslav Kudrnac on 28.08.14.
//  Copyright (c) 2014 Miroslav Kudrnac. All rights reserved.
//

#ifndef TransDB_Table_h
#define TransDB_Table_h

struct Collumn
{
    uint8       m_dataType;         //1
    uint8       m_position;         //1
    uint8       m_flags;            //1
    char        m_name[5];          //5
};

struct TableDefinition
{
    uint8       m_collumns;         //1
    uint8       m_flags;            //1
    char        m_tableName[5];     //5
};

#endif
