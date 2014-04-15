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
	static int decompressGzip(uint8 *pData, size_t dataLen, ByteBuffer &rBuffOut);
	static int compressGzip(uint8 *pData, size_t dataLen, ByteBuffer &rBuffOut);
    
    static bool CheckFileExists(const char *pFileName, bool oCreate);
};

#endif