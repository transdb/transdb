//
//  Common.cpp
//  TransDB
//
//  Created by Miroslav Kudrnac on 9/14/12.
//  Copyright (c) 2012 Miroslav Kudrnac. All rights reserved.
//

#include "StdAfx.h"

int Common::decompressGzip(const uint8 *pData, const size_t &dataLen, ByteBuffer &rBuffOut)
{
    int ret;
    size_t processed;
    uInt buffSize = ZLIB_CHUNK;
    uint8 outBuff[ZLIB_CHUNK];    
    
    //we need clean buffer
    rBuffOut.clear();
    
    //set up stream
    z_stream stream;
    memset(&stream, 0, sizeof(z_stream));

    //init for gzip
    ret = inflateInit2(&stream, GZIP_ENCODING+MAX_WBITS);
    if(ret != Z_OK)
        return ret;

    /* decompress until deflate stream ends or end of file */
    stream.avail_in = static_cast<uInt>(dataLen);
    stream.next_in = (Bytef*)pData;

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
                inflateEnd(&stream);
                return ret;
            }
        }

        processed = buffSize - stream.avail_out;
        rBuffOut.append(outBuff, processed);

    }while(stream.avail_out == 0);

    /* clean up and return */
    inflateEnd(&stream);
    return ret == Z_STREAM_END ? Z_OK : Z_DATA_ERROR;
}

int Common::compressGzip(const uint8 *pData, const size_t &dataLen, ByteBuffer &rBuffOut)
{
    int ret;
    uint32 processed;
    uInt buffSize = ZLIB_CHUNK;
    uint8 outBuff[ZLIB_CHUNK];
    
    //we need clean buffer
    rBuffOut.clear();
    
    //set up stream
    z_stream stream;
    memset(&stream, 0, sizeof(z_stream));
    
    //init for gzip
    ret = deflateInit2(&stream, g_GzipCompressionLevel, Z_DEFLATED, GZIP_ENCODING+MAX_WBITS, MAX_MEM_LEVEL, Z_DEFAULT_STRATEGY);
    if(ret != Z_OK)
        return ret;

    //setup input informations
    stream.next_in = (Bytef*)pData;
    stream.avail_in = static_cast<uInt>(dataLen);
    
    do
    {
        stream.avail_out = buffSize;
        stream.next_out = outBuff;
        ret = deflate(&stream, Z_FINISH);
        switch (ret) 
        {
            case Z_NEED_DICT:
                ret = Z_DATA_ERROR;     /* and fall through */
            case Z_DATA_ERROR:
            case Z_MEM_ERROR:
            {
                deflateEnd(&stream);
                return ret;
            }
        }

        processed = buffSize - stream.avail_out;
        rBuffOut.append(outBuff, processed);

    }while(stream.avail_out == 0);

    /* clean up and return */
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

time_t Common::GetLastFileModificationTime(const char *pFilePath)
{
#ifdef WIN32
	struct _stat rStat;
	if(_stat(pFilePath, &rStat) != -1)
		return rStat.st_mtime;
	else
		return 0;
#else
	struct stat rStat;
    if(stat(pFilePath, &rStat) != -1)
        return rStat.st_mtime;
    else
		return 0;
#endif
}







