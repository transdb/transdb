//
//  StringCompressor.h
//  TransDB
//
//  Created by Miroslav Kudrnac on 21.01.13.
//  Copyright (c) 2013 Miroslav Kudrnac. All rights reserved.
//

#ifndef __TransDB__StringCompressor__
#define __TransDB__StringCompressor__

struct Record
{
    explicit Record(const uint8 &positon, const uint8 &key) : m_position(positon), m_key(key)
    {
        
    }
    
    uint8   m_position;
    uint8   m_key;
};

struct Header
{
    uint16  m_count;
};

class StringCompressor
{   
public:
    int compress(const uint8 *pData, const size_t &dataLen, ByteBuffer &rOut);
    int decompress(const uint8 *pData, const size_t &dataLen, ByteBuffer &rOut);
};

#endif /* defined(__TransDB__StringCompressor__) */
