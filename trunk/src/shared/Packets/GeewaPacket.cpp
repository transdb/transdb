//
//  GeewaPacket.cpp
//  comLib
//
//  Created by Miroslav Kudrnac on 6/21/12.
//  Copyright (c) 2012 __MyCompanyName__. All rights reserved.
//

#include "GeewaPacket.h"
#include "../Logs/Log.h"

GeewaPacket::GeewaPacket()
{
    ClearPacket();
}

GeewaPacket::~GeewaPacket()
{
    
}

//Add data to HTTP packet (auto parse)
void GeewaPacket::AddHTTPData(char *pData, size_t dataSize)
{   
    //data counter
    m_allDataCounter += dataSize;
    
    //chunked data parser - http://www.ietf.org/rfc/rfc2616.txt - section 3.6.1
    HeaderMap::iterator itr = m_headers.find("Transfer-Encoding");
    if(itr != m_headers.end() && itr->second == "chunked")
    {
        Log.Debug(__FUNCTION__, "Chunked data size: %u", dataSize);
        
        //append data to buffer
        m_data.append(pData, dataSize);
        
        //find end of data
        char *pEnd = strstr(pData, "\r\n0\r\n");
        if(pEnd != NULL)
        {
            //make a copy of data
            size_t buffSize = m_data.size();
            char *pTmpBuff = new char[buffSize+1];         //!!!HEAP MEMORY ALLOC
            m_data.read((uint8*)pTmpBuff, buffSize);
            pTmpBuff[buffSize] = '\0';
            m_data.resize(0);
            
            //parse
            uint32 chunkSize;
            ptrdiff_t diff;
            ptrdiff_t possition = 0;
            char *pChunk;
            char *pTmp;
            for(;;)
            {
                //get data by offset
                pTmp = pTmpBuff + possition;
                //get chunk size
                pChunk = strstr(pTmp, "\r\n");
                if(pChunk == NULL)
                    break;
                
                diff = pChunk - pTmp;
                pChunk = (char*)alloca(diff+1);
                memcpy(pChunk, pTmp, diff);
                pChunk[diff] = '\0';
                //convert to number
                chunkSize = hex2number(pChunk);
                if(chunkSize == 0)
                    break;
                
                //copy to buffer
                pChunk = (char*)alloca(chunkSize);
                memcpy(pChunk, pTmp+diff+2, chunkSize);         //+diff(hex chunk size)+2(\r\n)
                m_data.append(pChunk, chunkSize);
                
                //move to next chunk number
                //diff(hex chunk size)+2(\r\n)+chunkSize+2(\r\n)
                possition += (diff+2+chunkSize+2);
            }
            
            delete [] pTmpBuff;                                     //!!!HEAP MEMORY FREE
            m_hasAllData = true;
        }
    }
    else
    {
        //dont have parsed header
        if(m_contentLenght == 0)
        {
            //we have saved part of header, add new one and change pointer
            if(m_header.length() != 0)
            {
                m_header += pData;
				pData = (char*)m_header.c_str();
            }
            
            //get end of the header
            char *pHeaderEnd = strstr(pData, "\r\n\r\n");   //4bytes
            if(pHeaderEnd == NULL)
            {
                //save this part and wait for another part, if we dont have any
                if(m_header.length() == 0)
                {
                    m_header += pData;
                }
                return;
            }
            
            //get header lenght
            m_headerLenght = static_cast<uint32>(pHeaderEnd - pData);        //possible overflow on x64
            
            //create tmp header for parsing
            char *pHeader = new char[m_headerLenght+1+2];
            memcpy(pHeader, pData, m_headerLenght+2);
            pHeader[m_headerLenght+2] = '\0';
    
            //parse HTTP header
            ParseHTTPHeader(pHeader);
            
            //release memory
            delete [] pHeader;
            
            //load content lenght
            itr = m_headers.find("Content-Length");
            if(itr == m_headers.end())
            {
                //TO-DO log err
                return;
            }
            m_contentLenght = atoi(itr->second.c_str());
            
            //add rest of packet to data
            //append data without 4bytes (\r\n\r\n)
            m_data.append(pData+m_headerLenght+4, dataSize-m_headerLenght-4);
        }
        else
        {
            //we have parsed header, so copy data to m_data
            m_data.append(pData, dataSize);
        }
        
        //check if we have all data
        size_t httpBufferSize = m_data.size();
        if(httpBufferSize < m_contentLenght)
            return;
        
        m_hasAllData = true;
    }
}

void GeewaPacket::ParseHTTPHeader(char *pHeader)
{
    char pKey[128];
    char *pTmp;
    char *pValue;
    ptrdiff_t diff;
    uint32 counter = 0;
    char *pRow = strtok(pHeader, "\r\n");
    while(pRow != NULL)
    {
        if(counter == 0)
        {
            m_headers.insert(HeaderMap::value_type("0", pRow));
        }
        else
        {
            //find :
            pTmp = strstr(pRow, ":");
            //string len before :
            diff = pTmp - pRow;
            //value (+2 because there is : and space)
            pValue = pTmp + 2;
            //get key
            memcpy(&pKey, pRow, diff);
            pKey[diff] = '\0';
            //insert to map
            m_headers.insert(HeaderMap::value_type(pKey, pValue));      
        }
        
        pRow = strtok(NULL, "\r\n");
        ++counter;
    }
}

void GeewaPacket::ClearPacket()
{
    m_data.clear();
    m_contentLenght = 0;
    m_headerLenght = 0;
    m_headers.clear();
    m_header.clear();
    m_hasAllData = false;
    m_allDataCounter = 0;
} 
