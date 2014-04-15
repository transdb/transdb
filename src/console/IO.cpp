//
//  IO.cpp
//  TransDB
//
//  Created by Miroslav Kudrnac on 19.04.13.
//  Copyright (c) 2013 Miroslav Kudrnac. All rights reserved.
//

#include "StdAfx.h"

HANDLE IO::ftrans()
{
#ifdef WIN32
    HANDLE hTransaction = CreateTransaction(NULL, 0, 0, 0, 0, 0, NULL);
    return hTransaction;
#else
    return 0;
#endif
}

HANDLE IO::fopentrans(const char *pPath, const ACCESS &eAccess, const HANDLE &hTransaction)
{
    HANDLE hHandle;
#ifdef WIN32
    USHORT view = 0xFFFE; // TXFS_MINIVERSION_DEFAULT_VIEW
    DWORD dwDesiredAccess;
    DWORD dwFlagsAndAttributes;
    switch(eAccess)
    {
        case IO_READ_ONLY:
            dwDesiredAccess = GENERIC_READ;
            dwFlagsAndAttributes = FILE_ATTRIBUTE_READONLY;
            break;
        case IO_WRITE_ONLY:
            dwDesiredAccess = GENERIC_WRITE;
            dwFlagsAndAttributes = FILE_ATTRIBUTE_NORMAL;
            break;
        case IO_RDWR:
            dwDesiredAccess = GENERIC_READ | GENERIC_WRITE;
            dwFlagsAndAttributes = FILE_ATTRIBUTE_NORMAL;
            break;
    }
    hHandle = CreateFileTransacted(pPath, dwDesiredAccess, FILE_SHARE_READ | FILE_SHARE_WRITE, NULL, OPEN_EXISTING, dwFlagsAndAttributes, NULL, hTransaction, &view, NULL);
#else
    hHandle = IO::fopen(pPath, eAccess);
#endif
    return hHandle;
}

void IO::fcommittrans(const HANDLE &hTransaction)
{
#ifdef WIN32
    CommitTransaction(hTransaction);
#endif
}

void IO::frollbacktrans(const HANDLE &hTransaction)
{
#ifdef WIN32
    RollbackTransaction(hTransaction);
#endif
}

void IO::fclosetrans(const HANDLE &hTransaction)
{
#ifdef WIN32
    CloseHandle(hTransaction);
#endif
}

HANDLE IO::fopen(const char *pPath, const ACCESS &eAccess)
{
    HANDLE hHandle;
#ifdef WIN32
    DWORD dwDesiredAccess;
    DWORD dwFlagsAndAttributes;
    switch(eAccess)
    {
        case IO_READ_ONLY:
            dwDesiredAccess = GENERIC_READ;
            dwFlagsAndAttributes = FILE_ATTRIBUTE_READONLY;
            break;
        case IO_WRITE_ONLY:
            dwDesiredAccess = GENERIC_WRITE;
            dwFlagsAndAttributes = FILE_ATTRIBUTE_NORMAL;
            break;
        case IO_RDWR:
            dwDesiredAccess = GENERIC_READ | GENERIC_WRITE;
            dwFlagsAndAttributes = FILE_ATTRIBUTE_NORMAL;
            break;
    }
    hHandle = CreateFile(pPath, dwDesiredAccess, FILE_SHARE_READ | FILE_SHARE_WRITE, NULL, OPEN_EXISTING, dwFlagsAndAttributes, NULL);
#else
    switch(eAccess)
    {
        case IO_READ_ONLY:
            hHandle = ::open(pPath, O_RDONLY);
            break;
        case IO_WRITE_ONLY:
            hHandle = ::open(pPath, O_WRONLY);
            break;
        case IO_RDWR:
            hHandle = ::open(pPath, O_RDWR);
            break;
    }
#endif
    return hHandle;
}

int64 IO::ftell(const HANDLE &hFile)
{
#ifdef WIN32
    LARGE_INTEGER liDistanceToMove = { 0 };
    LARGE_INTEGER lpNewFilePointer;
    
    SetFilePointerEx(hFile, liDistanceToMove, &lpNewFilePointer, FILE_CURRENT);
    return lpNewFilePointer.QuadPart;
#else
    return lseek(hFile, 0, SEEK_CUR);
#endif
}

size_t IO::fseek(const HANDLE &hFile, const int64 &offset, const int &origin)
{
#ifdef WIN32
    LARGE_INTEGER liDistanceToMove;
    LARGE_INTEGER lpNewFilePointer;
    
    liDistanceToMove.QuadPart = offset;
    SetFilePointerEx(hFile, liDistanceToMove, &lpNewFilePointer, origin);
    return lpNewFilePointer.QuadPart;
#else
    return lseek(hFile, offset, origin);
#endif
}

size_t IO::fwrite(const void *pBuffer, const size_t &nNumberOfBytesToWrite, const HANDLE &hFile)
{
#ifdef WIN32
    DWORD lpNumberOfBytesWritten;
    WriteFile(hFile, pBuffer, (DWORD)nNumberOfBytesToWrite, &lpNumberOfBytesWritten, NULL);
    assert(lpNumberOfBytesWritten == nNumberOfBytesToWrite);
#else
    ssize_t lpNumberOfBytesWritten;
    lpNumberOfBytesWritten = ::write(hFile, pBuffer, nNumberOfBytesToWrite);
    assert(lpNumberOfBytesWritten == nNumberOfBytesToWrite);
#endif
    return lpNumberOfBytesWritten;
}

size_t IO::fread(void *pBuffer, const size_t &nNumberOfBytesToRead, const HANDLE &hFile)
{
#ifdef WIN32
    DWORD lpNumberOfBytesRead;
    ReadFile(hFile, pBuffer, (DWORD)nNumberOfBytesToRead, &lpNumberOfBytesRead, NULL);
    assert(lpNumberOfBytesRead == nNumberOfBytesToRead);
#else
    ssize_t lpNumberOfBytesRead;
    lpNumberOfBytesRead = ::read(hFile, pBuffer, nNumberOfBytesToRead);
    assert(lpNumberOfBytesRead == nNumberOfBytesToRead);
#endif
    return lpNumberOfBytesRead;
}

void IO::fresize(const HANDLE &hFile, const int64 &newSize)
{
#ifdef WIN32
	LARGE_INTEGER liDistanceToMove;
    LARGE_INTEGER lpNewFilePointer;
    
    liDistanceToMove.QuadPart = newSize;
    SetFilePointerEx(hFile, liDistanceToMove, &lpNewFilePointer, FILE_BEGIN);
	SetEndOfFile(hFile);
#else
    ftruncate(hFile, newSize);
#endif
}

void IO::fclose(const HANDLE &hFile)
{
#ifdef WIN32
    CloseHandle(hFile);
#else
    ::close(hFile);
#endif
}