//
//  StdAfx.cpp
//  TransDB
//
//  Created by Miroslav Kudrnac on 8/20/12.
//  Copyright (c) 2012 Miroslav Kudrnac. All rights reserved.
//

#include "StdAfx.h"

//#ifdef USE_INTEL_SCALABLE_ALLOCATORS
//
//#ifndef WIN32
//    #define _THROW1(x)  throw(x)
//    #define _THROW0(x)  throw()
//#endif
//
////operator override
//void *operator new(size_t size) _THROW1(std::bad_alloc)
//{
//    void *ptr;
//    ptr = scalable_malloc(size);
//    if(ptr)
//        return ptr;
//    else
//        throw std::bad_alloc();
//}
//
//void *operator new[](size_t size) _THROW1(std::bad_alloc)
//{
//    return operator new(size);
//}
//
//void *operator new(size_t size, const std::nothrow_t&) _THROW0()
//{
//    void *ptr;
//    ptr = scalable_malloc(size);
//    return ptr;
//}
//
//void *operator new[](size_t size, const std::nothrow_t&) _THROW0()
//{
//    return operator new(size, std::nothrow);
//}
//
//void operator delete(void *ptr) _THROW0()
//{
//    scalable_free(ptr);
//}
//
//void operator delete[](void *ptr) _THROW0()
//{
//    operator delete(ptr);
//}
//
//void operator delete(void *ptr, const std::nothrow_t&) _THROW0()
//{
//    scalable_free(ptr);
//}
//
//void operator delete[](void *ptr, const std::nothrow_t&) _THROW0()
//{
//    operator delete(ptr, std::nothrow);
//}
//
//#endif










