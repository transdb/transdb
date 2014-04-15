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
    static bool isGziped(const uint8 *pData);
    
    static bool CheckFileExists(const char *pFileName, bool oCreate);

    static time_t GetLastFileModificationTime(const char *pFilePath);
    
private:
	static INLINE voidpf my_alloc_func(voidpf opaque, uInt items, uInt size)
	{
		return scalable_malloc(items * size);
	}

	static INLINE void my_free_func(voidpf opaque, voidpf address)
	{
		scalable_free(address);
	}
};

#endif