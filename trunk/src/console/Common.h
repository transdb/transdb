//
//  Common.h
//  TransDB
//
//  Created by Miroslav Kudrnac on 9/14/12.
//  Copyright (c) 2012 Miroslav Kudrnac. All rights reserved.
//

#ifndef TransDB_Common_h
#define TransDB_Common_h

class Common
{
public:
	static int decompressGzip(const uint8 *pData, const size_t &dataLen, ByteBuffer &rBuffOut);
	static int compressGzip(const uint8 *pData, const size_t &dataLen, ByteBuffer &rBuffOut);
    static INLINE bool isGziped(const uint8 *pData)
    {
        return (pData[0] == 0x1f) && (pData[1] == 0x8b);
    }
          
    static bool CheckFileExists(const char *pFileName, bool oCreate);
    static time_t GetLastFileModificationTime(const char *pFilePath);
};

#endif