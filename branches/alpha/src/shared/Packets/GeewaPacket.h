//
//  GeewaPacket.h
//  comLib
//
//  Created by Miroslav Kudrnac on 6/21/12.
//  Copyright (c) 2012 __MyCompanyName__. All rights reserved.
//

#ifndef comLib_GeewaPacket_h
#define comLib_GeewaPacket_h

#include "../Defines.h"
#include "ByteBuffer.h"

typedef std::map<string, string> HeaderMap;

class GeewaPacket
{
public:
    GeewaPacket();
    ~GeewaPacket();
    
    //Add HTTP data to packet (auto parse)
    void AddHTTPData(char *pData, size_t dataSize);
    
    //clear packet for reuse
    void ClearPacket();
    
    INLINE bool hasAllData()        { return m_hasAllData; }
    INLINE size_t allDataCounter()  { return m_allDataCounter; }
    INLINE HeaderMap &headers()     { return m_headers; }
    INLINE ByteBuffer &data()       { return m_data; }
    
private:
    //Parses HTTP header from input string
    void ParseHTTPHeader(char *pHeader);
    
    //declarations
    string      m_header;
    size_t      m_allDataCounter;
    ByteBuffer  m_data;
    uint32      m_contentLenght;
    uint32      m_headerLenght;
    HeaderMap   m_headers;
    bool        m_hasAllData;
};

#endif
