import socket
import struct
import zlib
import Queue
import threading
import json
import select
import multiprocessing
import time
from collections import OrderedDict

import cfunctions
import packets


GZIP = 1
BLOCK_SIZE = 4096
DEBUG = True
LOG = True
g_token = 0

# Server OPCODES
C_MSG_WRITE_DATA            = 1
S_MSG_WRITE_DATA            = 2
C_MSG_READ_DATA             = 3
S_MSG_READ_DATA             = 4
C_MSG_DELETE_DATA           = 5
S_MSG_DELETE_DATA           = 6
C_MSG_PONG                  = 9
S_MSG_PING                  = 10
C_MSG_STATUS                = 13
S_MSG_STATUS                = 14
C_MSG_GET_FREESPACE         = 21
S_MSG_GET_FREESPACE         = 22
C_MSG_WRITE_DATA_NUM        = 23
S_MSG_WRITE_DATA_NUM        = 24
C_MSG_READ_LOG              = 25
S_MSG_READ_LOG              = 26
C_MSG_READ_CONFIG           = 27
S_MSG_READ_CONFIG           = 28
C_MSG_EXEC_PYTHON_SCRIPT    = 31
S_MSG_EXEC_PYTHON_SCRIPT    = 32

SELECT_WAIT_TIME = 10
recvQueue = multiprocessing.Queue()
sendQueue = multiprocessing.Queue()

def getToken():
    """ Must be thread safe """
    try:
        token = 0
        lock = threading.Lock()
        lock.acquire()
        global g_token
        g_token += 1
        token = g_token
        lock.release()
        return token
    except Exception as e:
        cfunctions.Log_Error("transDB.getToken: " + str(e))

def inspectFreeSpaceFragment(data):
    """
        Parse file fragmentation data.
        """
    try:
        fmt_s = "<QQ"
        fmt_sz = struct.calcsize(fmt_s)
        data_sz, items = struct.unpack_from(fmt_s, data)
        offset = fmt_sz
        blocks = OrderedDict()
        for i in range(items) :
            blocksize, blockcount = struct.unpack_from("<QQ", data, offset)
            offset = offset + fmt_sz
            blocks[blocksize] = blockcount
        return data_sz, items, blocks
    except Exception as e:
        cfunctions.Log_Error("transDB.inspectFreeSpaceFragment: " + str(e))

def getData(token):
    """ get data by token """
    try:
        while True:
            try:
                #wait for data in queue
                tokenTmp, data = recvQueue.get(True, 10)
                if tokenTmp != token:
                    recvQueue.put((tokenTmp, data))
                else:
                    return data
            except Queue.Empty as e:
                pass
    
    except Exception as e:
        cfunctions.Log_Error("transDB.getData: " + str(e))

def stats():
    """ Get basic statistics. """
    try:
        token = getToken()
        packet = packets.TransDBPacket(C_MSG_STATUS)
        packet.data = struct.pack('<I', token)
        sendQueue.put(packet)
        data = getData(token)
        return json.loads(data)
    except Exception as e:
        cfunctions.Log_Error("transDB.stats: " + str(e))

def fragment():
    """ Get file fragmentation info. """
    try:
        token = getToken()
        packet = packets.TransDBPacket(C_MSG_GET_FREESPACE)
        packet.data = struct.pack('<III', token, 0, 1)
        sendQueue.put(packet)
        data = getData(token)
        return inspectFreeSpaceFragment(data)
    except Exception as e:
        cfunctions.Log_Error("transDB.fragment: " + str(e))

def readData(x, y):
    """ Get data from DB """
    try:
        token = getToken()
        packet = packets.TransDBPacket(C_MSG_READ_DATA)
        packet.data = struct.pack('<IIQQ', token, 0, long(x), long(y))
        sendQueue.put(packet)
        data = getData(token)
        return data
    except Exception as e:
        cfunctions.Log_Error("transDB.readData: " + str(e))

def writeData(x, y, data):
    """ Write data to DB """
    try:
        token = getToken()
        packet = packets.TransDBPacket(C_MSG_WRITE_DATA)
        packet.data = struct.pack('<IIQQ', token, 0, long(x), long(y)) + data
        sendQueue.put(packet)
        data = getData(token)
        return data
    except Exception as e:
        cfunctions.Log_Error("transDB.writeData: " + str(e))

def deleteData(x, y):
    """ Delete data in DB if y=0 delete all data for x """
    try:
        token = getToken()
        packet = packets.TransDBPacket(C_MSG_DELETE_DATA)
        packet.data = struct.pack('<IIQQ', token, 0, long(x), long(y))
        sendQueue.put(packet)
        data = getData(token)
        return data
    except Exception as e:
        cfunctions.Log_Error("transDB.deleteData: " + str(e))

def writeDataNum(x, data):
    """ Write key|recordSize|record|....Nx array to DB """
    try:
        flags = 0
        #GZIP data
        if len(data) > 4096:
            encoder = zlib.compressobj(8, zlib.DEFLATED, 16+zlib.MAX_WBITS, zlib.DEF_MEM_LEVEL, 0)
            dataGZIP = encoder.compress(data)
            dataGZIP += encoder.flush()
            flags = GZIP
            data = dataGZIP
        
        token = getToken()
        packet = packets.TransDBPacket(C_MSG_WRITE_DATA_NUM)
        packet.data = struct.pack('<IIQ', token, flags, long(x)) + data
        sendQueue.put(packet)
        data = getData(token)
        return data
    except Exception as e:
        cfunctions.Log_Error("transDB.writeDataNum: " + str(e))

def readLog():
    """ Read log """
    try:
        token = getToken()
        packet = packets.TransDBPacket(C_MSG_READ_LOG)
        packet.data = struct.pack('<I', token)
        sendQueue.put(packet)
        data = getData(token)
        return data
    except Exception as e:
        cfunctions.Log_Error("transDB.readLog: " + str(e))

def readConfig():
    """ Read config """
    try:
        token = getToken()
        packet = packets.TransDBPacket(C_MSG_READ_CONFIG)
        packet.data = struct.pack('<I', token)
        sendQueue.put(packet)
        data = getData(token)
        return data
    except Exception as e:
        cfunctions.Log_Error("transDB.readLog: " + str(e))

def executePythonScript(script):
    """ Read config """
    try:
        token = getToken()
        packet = packets.TransDBPacket(C_MSG_EXEC_PYTHON_SCRIPT)
        packet.data = struct.pack('<II', token, 0) + script
        sendQueue.put(packet)
        data = getData(token)
        return data
    except Exception as e:
        cfunctions.Log_Error("transDB.readLog: " + str(e))


def socket_run(rcv_queue, send_queue, stop_event, addr, port):
    """ TransDB connection thread function, handles all communication utilizing Queues. """
    try:
        #if socket is connected
        socketConnected = False
        
        #loop
        while not stop_event.is_set():
            try:
                #create socket + connect
                if socketConnected == False:
                    s = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
                    s.connect((addr, port))
                    socketConnected = True
            except socket.error as e:
                socketConnected = False
                cfunctions.Log_Error("socket_run: " + str(e))
                time.sleep(1)
                continue
            
            try:
                #wait for socket read event or queue read event
                readSet, writeSet, errorSet = select.select([s, sendQueue._reader], [], [], SELECT_WAIT_TIME)
                #process client socket
                if s in readSet:
                    packetHeader = ''
                    #wait for header
                    headerLen = struct.calcsize("<IH")
                    while len(packetHeader) < headerLen:
                        chunk = s.recv(headerLen - len(packetHeader))
                        if chunk == '':
                            raise RuntimeError("Socket connection broken")
                        packetHeader = packetHeader + chunk
                    
                    #get rest of the packet data
                    size, opcode  = struct.unpack("<IH", packetHeader)
                    data = ''
                    while len(data) < size:
                        chunk = s.recv(size - len(data))
                        if chunk == '':
                            raise RuntimeError("Socket connection broken")
                        data = data + chunk
                    
                    #opcode handler
                    if opcode == S_MSG_PING:
                        packet = packets.TransDBPacket(C_MSG_PONG)
                        packet.data = data
                        sendQueue.put(packet)
                    else:
                        #uint32 token, flag
                        offset = 8
                        if opcode == S_MSG_READ_DATA:
                            #uint64 x, y
                            offset += 16
                        elif opcode == S_MSG_DELETE_DATA:
                            #uint64 x, y
                            offset += 16
                        elif opcode == S_MSG_WRITE_DATA:
                            #uint64 x, y
                            #uint32 writeStatus
                            offset += 20
                        elif opcode == C_MSG_WRITE_DATA_NUM:
                            #uint64 x
                            #uint32 writeStatus
                            offset += 12
                        
                        #get token + flags -> if GZIP decompress
                        token, flag = struct.unpack_from("<II", data)
                        if flag == GZIP:
                            try:
                                data = zlib.decompress(data[offset:], 16+zlib.MAX_WBITS)
                            except zlib.error as e:
                                cfunctions.Log_Error("socket_run: " + str(e))
                        else:
                            data = data[offset:]
                        
                        #put token and data to rcv_queue another thread will pickup data
                        rcv_queue.put((token, data))
                
                #process send queue
                if sendQueue._reader in readSet:
                    try:
                        while True:
                            packet = sendQueue.get_nowait()
                            packetData = packet.createPacket()
                            s.sendall(packetData)
                    except Queue.Empty as e:
                        pass
            
            #handle socket error --> disconnect socket and connect again
            except (socket.error, select.error, RuntimeError) as e:
                socketConnected = False
                cfunctions.Log_Error("socket_run: " + str(e))
                #try to close socket will be created new
                try:
                    #close socket
                    s.shutdown(1)
                    s.close()
                except Exception as e:
                    pass
        
        #close socket
        s.shutdown(1)
        s.close()
    except Exception as e:
        cfunctions.Log_Error("socket_run: " + str(e))


