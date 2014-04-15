//
//  StringCompressor.cpp
//  TransDB
//
//  Created by Miroslav Kudrnac on 21.01.13.
//  Copyright (c) 2013 Miroslav Kudrnac. All rights reserved.
//

#include "StdAfx.h"

static const char *pDictionary[] = {
    "\"status\"",
    "\"createTime\"",
    "\"oppUser\"",
    "\"userID\"",
    "\"standing\"",
    "\"state\"",
    "\"moesPub/invite\"",
    "\"nick\"",
    "\"lost\"",
    "\"matchID\"",
    "trophyID",
    "levelID",
    "productID",
    "enabled",
    '\0'
};

static bool SortRecords(const Record &pRecord1, const Record &pRecord2)
{
    return pRecord1.m_position < pRecord2.m_position;
}

int StringCompressor::compress(const uint8 *pData, const size_t &dataLen, ByteBuffer &rOut)
{
    string sInputData = string((char*)pData);
    size_t counter = 0;
    size_t pos = 0;
    uint16 recordCount = 0;
    typedef std::vector<Record> RecordVec;
    RecordVec rRecords;
    
    //record count
    rOut << uint16(0);
    
    while(pDictionary[counter] != '\0')
    {
        pos = 0;
        
    next:
        pos = sInputData.find(pDictionary[counter], pos);
        if(pos != string::npos)
        {
            rOut << uint8(pos);         //position
            rOut << uint8(counter);     //key
            rRecords.push_back(Record(pos, counter));
            
            //move to next
            ++pos;
            ++recordCount;
            goto next;
        }
        //
        ++counter;
    }
    
    //remove from input data
    for(RecordVec::iterator itr = rRecords.begin();itr != rRecords.end();++itr)
    {
        replace(sInputData, pDictionary[itr->m_key], "");
    }
    
    //add record count
    rOut.put(0, recordCount);
    //add rest of string
    rOut.append(sInputData.data(), sInputData.length());
    
    return 0;
}

int StringCompressor::decompress(const uint8 *pData, const size_t &dataLen, ByteBuffer &rOut)
{
    uint16 recordCount;
    typedef std::vector<Record> RecordVec;
    RecordVec rRecords;
    uint8 recordPosition;
    uint8 key;
    ByteBuffer rData;
    string outString;
    size_t outStringSize;
    
    //init data
    rData.append(pData, dataLen);
    
    //get record count
    rData >> recordCount;
    
    //read records descriptors
    for(uint16 i = 0;i < recordCount;++i)
    {
        rData >> recordPosition;
        rData >> key;
        rRecords.push_back(Record(recordPosition, key));
    }
    
    //prepare outString
    outStringSize = rData.size() - rData.rpos();
    outString.resize(outStringSize);
    rData.read((uint8*)outString.data(), outStringSize);
    
    //sort
    std::sort(rRecords.begin(), rRecords.end(), SortRecords);
    
    //
    for(RecordVec::iterator itr = rRecords.begin();itr != rRecords.end();++itr)
    {
        outString.insert(itr->m_position, pDictionary[itr->m_key]);
    }
    
    //
    rOut.append(outString.data(), outString.length());
    
    return 0;
}





















































