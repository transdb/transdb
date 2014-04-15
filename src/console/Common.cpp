//
//  Common.cpp
//  TransDB
//
//  Created by Miroslav Kudrnac on 9/14/12.
//  Copyright (c) 2012 Miroslav Kudrnac. All rights reserved.
//

#include "StdAfx.h"

int Common::decompressGzip(uint8 *pData, size_t dataLen, ByteBuffer &rBuffOut)
{
    int ret;
    size_t processed;
    uInt buffSize = g_ZlibBufferSize;
    uint8 *outBuff = new uint8[buffSize];

    //set up stream
    z_stream stream;
    stream.zalloc = Z_NULL;
    stream.zfree  = Z_NULL;
    stream.opaque = Z_NULL;
    stream.avail_in = 0;
    stream.next_in = Z_NULL;

    //init for gzip
    ret = inflateInit2(&stream, GZIP_ENCODING+MAX_WBITS);
    if(ret != Z_OK)
    {
        delete [] outBuff;
        return ret;
    }

    /* decompress until deflate stream ends or end of file */
    stream.avail_in = static_cast<uInt>(dataLen);
    stream.next_in = pData;

    /* run inflate() on input until output buffer not full */
    do
    {   
        stream.avail_out = buffSize;
        stream.next_out = outBuff;
        ret = inflate(&stream, Z_FINISH);
        switch (ret) 
        {
            case Z_NEED_DICT:
                ret = Z_DATA_ERROR;     /* and fall through */
            case Z_DATA_ERROR:
            case Z_MEM_ERROR:
            {
                delete [] outBuff;               
                inflateEnd(&stream);
                return ret;
            }
        }

        processed = buffSize - stream.avail_out;
        rBuffOut.append(outBuff, processed);

    }while(stream.avail_out == 0);

    /* clean up and return */
    delete [] outBuff;
    inflateEnd(&stream);
    return ret == Z_STREAM_END ? Z_OK : Z_DATA_ERROR;
}

int Common::compressGzip(uint8 *pData, size_t dataLen, ByteBuffer &rBuffOut)
{
    int ret;
    uint32 processed;
    uInt buffSize = g_ZlibBufferSize;
    uint8 *outBuff = new uint8[buffSize];
    
    //set up stream
    z_stream stream;
    stream.zalloc = Z_NULL;
    stream.zfree  = Z_NULL;
    stream.opaque = Z_NULL;
    stream.avail_in = 0;
    stream.next_in = Z_NULL;
    
    //init for gzip
    ret = deflateInit2(&stream, g_GzipCompressionLevel, Z_DEFLATED, GZIP_ENCODING+MAX_WBITS, MAX_MEM_LEVEL, Z_DEFAULT_STRATEGY);
    if(ret != Z_OK)
    {
        delete [] outBuff;
        return ret;
    }

    //setup input informations
    stream.next_in = (Bytef*)pData;
    stream.avail_in = static_cast<uInt>(dataLen);
    
    do
    {
        stream.avail_out = g_ZlibBufferSize;
        stream.next_out = outBuff;
        ret = deflate(&stream, Z_FINISH);
        switch (ret) 
        {
            case Z_NEED_DICT:
                ret = Z_DATA_ERROR;     /* and fall through */
            case Z_DATA_ERROR:
            case Z_MEM_ERROR:
            {
                delete [] outBuff;
                deflateEnd(&stream);
                return ret;
            }
        }

        processed = g_ZlibBufferSize - stream.avail_out;
        rBuffOut.append(outBuff, processed);

    }while(stream.avail_out == 0);

    /* clean up and return */
    delete [] outBuff;
    deflateEnd(&stream);
    return ret == Z_STREAM_END ? Z_OK : Z_DATA_ERROR;    
}

bool Common::CheckFileExists(const char *pFileName, bool oCreate)
{
	bool oReturnVal = true;
	FILE * rFile;
	rFile = fopen(pFileName, "rb");
	if(rFile == NULL && oCreate)
	{
		//create file
		rFile = fopen(pFileName, "wb+");
		oReturnVal = false;
	}
    
	if(rFile)
	{
		fclose(rFile);
	}
	
	return oReturnVal;
}