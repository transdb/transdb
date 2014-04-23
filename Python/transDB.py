import time
import socket
import struct
import zlib
import Queue
import threading
import json
import select
import common
import cfunctions
import multiprocessing
import packets
from collections import OrderedDict

GZIP = 1
BLOCK_SIZE = 4096
DEBUG = True
LOG = True
g_token = 0

# Server OPCODES
C_MSG_WRITE_DATA        = 1
S_MSG_WRITE_DATA        = 2
C_MSG_READ_DATA         = 3
S_MSG_READ_DATA         = 4
C_MSG_DELETE_DATA       = 5
S_MSG_DELETE_DATA       = 6
C_MSG_PONG              = 9
S_MSG_PING              = 10
C_MSG_STATUS            = 13
S_MSG_STATUS            = 14
C_MSG_GET_FREESPACE     = 21
S_MSG_GET_FREESPACE     = 22
C_MSG_WRITE_DATA_NUM    = 23
S_MSG_WRITE_DATA_NUM    = 24

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
        cfunctions.Log_Error("trandDB.getToken: " + str(e))

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
        cfunctions.Log_Error("trandDB.inspectFreeSpaceFragment: " + str(e))

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
        cfunctions.Log_Error("trandDB.getData: " + str(e))

def stats():
    """ Get basic statistics. """
    try:
        token = getToken()
        packet = packets.TransDBPacket(C_MSG_STATUS)
        packet.data = struct.pack('<I', token)
        sendQueue.put(packet)
        data = getData(token)
        return json.loads(data[:-1])
    except Exception as e:
        cfunctions.Log_Error("trandDB.stats: " + str(e))

def fragment():
    try:
        """ Get file fragmentation info. """
        token = getToken()
        packet = packets.TransDBPacket(C_MSG_GET_FREESPACE)
        packet.data = struct.pack('<III', token, 0, 1)
        sendQueue.put(packet)
        data = getData(token)
        return inspectFreeSpaceFragment(data)
    except Exception as e:
        cfunctions.Log_Error("trandDB.fragment: " + str(e))

def readData(x, y):
    try:
        """ Get data from DB """
        token = getToken()
        packet = packets.TransDBPacket(C_MSG_READ_DATA)
        packet.data = struct.pack('<IIQQ', token, 0, long(x), long(y))
        sendQueue.put(packet)
        data = getData(token)
        return data
    except Exception as e:
        cfunctions.Log_Error("trandDB.readData: " + str(e))

def writeData(x, y, data):
    try:
        """ Write data to DB """
        token = getToken()
        packet = packets.TransDBPacket(C_MSG_WRITE_DATA)
        packet.data = struct.pack('<IIQQ', token, 0, long(x), long(y)) + data
        sendQueue.put(packet)
        data = getData(token)
        return data
    except Exception as e:
        cfunctions.Log_Error("trandDB.writeData: " + str(e))

def writeDataNoWait(x, y, data):
    try:
        """ Write data to DB witout getting result """
        packet = packets.TransDBPacket(C_MSG_WRITE_DATA)
        packet.data = struct.pack('<IIQQ', 0, 0, long(x), long(y)) + data
        sendQueue.put(packet)
    except Exception as e:
        cfunctions.Log_Error("trandDB.writeDataNoWait: " + str(e))

def deleteData(x, y):
    try:
        """ Delete data in DB if y=0 delete all data for x """
        token = getToken()
        packet = packets.TransDBPacket(C_MSG_DELETE_DATA)
        packet.data = struct.pack('<IIQQ', token, 0, long(x), long(y))
        sendQueue.put(packet)
        data = getData(token)
        return data
    except Exception as e:
        cfunctions.Log_Error("trandDB.deleteData: " + str(e))

def writeDataNum(x, data):
    try:
        """ Write key|recordSize|record|....Nx array to DB """
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
        cfunctions.Log_Error("trandDB.writeDataNum: " + str(e))

def socket_run(rcv_queue, send_queue, stop_event, addr, port):
    """ 
    TransDB connection thread function, handles all communication utilizing Queues.
    """
    try:
        #create socket + connect
        s = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
        s.setsockopt(socket.SOL_SOCKET, socket.SO_KEEPALIVE, 1)
        s.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
        s.connect((addr, port))
        s.setblocking(0)
        
        #loop
        while not stop_event.is_set():
            #wait for socket read event or queue read event
            readSet, writeSet, errorSet = select.select([s, sendQueue._reader], [], [], SELECT_WAIT_TIME)
            #process send queue
            if sendQueue._reader in readSet:
                packet = sendQueue.get()
                packetData = packet.createPacket()
                s.sendall(packetData)
        
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
                    packetData = packet.createPacket()
                    s.sendall(packetData)
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
                    
                    token, flag = struct.unpack_from("<II", data)
                    if flag == GZIP:
                        try:
                            data = zlib.decompress(data[offset:], 16+zlib.MAX_WBITS)
                        except zlib.error as e:
                            print(str(e))
                    else:
                        data = data[offset:]
                    
                    #if token == 0 then we dont want to handle response
                    if token != 0:
                        rcv_queue.put((token, data))

        #close socket
        s.shutdown(1)
        s.close()
        
    except Exception as e:
        cfunctions.Log_Error("socket_run: " + str(e))

