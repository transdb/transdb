//
//  IO.h
//  TransDB
//
//  Created by Miroslav Kudrnac on 19.04.13.
//  Copyright (c) 2013 Miroslav Kudrnac. All rights reserved.
//

#ifndef __TransDB__IO__
#define __TransDB__IO__

class IO
{
public:
    enum ACCESS
    {
        IO_READ_ONLY    = 1,
        IO_WRITE_ONLY   = 2,
        IO_RDWR         = 3
    };
    
    static HANDLE ftrans();
    static HANDLE fopentrans(const char *pPath, const ACCESS &eAccess, const HANDLE &hTransaction);
    static void fcommittrans(const HANDLE &hTransaction);
    static void frollbacktrans(const HANDLE &hTransaction);
    static void fclosetrans(const HANDLE &hTransaction);
    static HANDLE fopen(const char *pPath, const ACCESS &eAccess);
    static int64 ftell(const HANDLE &hFile);
    static size_t fseek(const HANDLE &hFile, const int64 &offset, const int &origin);
    static size_t fwrite(const void *pBuffer, const size_t &nNumberOfBytesToWrite, const HANDLE &hFile);
    static size_t fread(void *pBuffer, const size_t &nNumberOfBytesToRead, const HANDLE &hFile);
    static void fresize(const HANDLE &hFile, const int64 &newSize);
    static void fclose(const HANDLE &hFile);
};

class IOHandleGuard
{
public:
    explicit IOHandleGuard(HANDLE *pHandle) : m_pHandle(pHandle)
    {
        
    }
    
    ~IOHandleGuard()
    {
        IO::fclose(*m_pHandle);
        *m_pHandle = INVALID_HANDLE_VALUE;
    }
    
private:
    HANDLE *m_pHandle;
};

#endif /* defined(__TransDB__IO__) */
