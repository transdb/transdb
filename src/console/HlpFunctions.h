//
//  HlpFunctions.h
//  TransDB
//
//  Created by Miroslav Kudrnac on 15.09.14.
//  Copyright (c) 2014 Miroslav Kudrnac. All rights reserved.
//

#ifndef TransDB_HlpFunctions_h
#define TransDB_HlpFunctions_h

//Allocator
template <class K, class V>
class ScalableHashMapNodeAllocator
{
public:
    HashNode<K, V> *allocate(const K &key, const V &value)
    {
        void *pMem = scalable_malloc(sizeof(HashNode<K, V>));
        return new(pMem) HashNode<K, V>(key, value);
    }
    
    void deallocate(HashNode<K, V> *p)
    {
        p->~HashNode<K, V>();
        scalable_free(p);
    }
};

#ifdef WIN32
static void trans_func(unsigned int u, EXCEPTION_POINTERS* pExp)
{
    PrintCrashInformation(pExp);
    HandleCrash(pExp);
}
#endif

#endif
