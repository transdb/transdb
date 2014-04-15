//
//  WebService.h
//  TransDB
//
//  Created by Miroslav Kudrnac on 03.12.12.
//  Copyright (c) 2012 Miroslav Kudrnac. All rights reserved.
//

#ifndef __TransDB__WebService__
#define __TransDB__WebService__

using namespace SocketEnums;

class WebService : public Socket
{
    typedef void (WebService::*GETHandler)(ByteBuffer&);
    typedef std::map<string, GETHandler> GETHandlerMap;
    
public:
    WebService(SOCKET fd);
	~WebService();
    
	void OnRead();
	void OnConnect();
	void OnDisconnect();
    
    void HandleRoot(ByteBuffer&);
    void HandleLog(ByteBuffer&);
    void HandleOther(ByteBuffer&);
    
private:
    OUTPACKET_RESULT OutPacket(const size_t &len, const void* data);
    
    //
    GETHandlerMap   m_rGETHandlerMap;
};

#endif /* defined(__TransDB__WebService__) */
