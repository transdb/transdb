import cfunctions
import socket
import time
import transDB
import zlib
import packets
import struct
import consts
from datetime import datetime

def set_keepalive_linux(sock, after_idle_sec=1, interval_sec=3, max_fails=5):
    """Set TCP keepalive on an open socket.

        It activates after 1 second (after_idle_sec) of idleness,
        then sends a keepalive ping once every 3 seconds (interval_sec),
        and closes the connection after 5 failed ping (max_fails), or 15 seconds
        """
    try:
        sock.setsockopt(socket.SOL_SOCKET, socket.SO_KEEPALIVE, 1)
        sock.setsockopt(socket.IPPROTO_TCP, socket.TCP_KEEPIDLE, after_idle_sec)
        sock.setsockopt(socket.IPPROTO_TCP, socket.TCP_KEEPINTVL, interval_sec)
        sock.setsockopt(socket.IPPROTO_TCP, socket.TCP_KEEPCNT, max_fails)
    except Exception as e:
        cfunctions.Log_Error("common.set_keepalive_linux: " + str(e))

def set_keepalive_osx(sock, after_idle_sec=1, interval_sec=3, max_fails=5):
    """Set TCP keepalive on an open socket.

        sends a keepalive ping once every 3 seconds (interval_sec)
        """
    try:
        # scraped from /usr/include, not exported by python's socket module
        TCP_KEEPALIVE = 0x10
        sock.setsockopt(socket.SOL_SOCKET, socket.SO_KEEPALIVE, 1)
        sock.setsockopt(socket.IPPROTO_TCP, TCP_KEEPALIVE, interval_sec)
    except Exception as e:
        cfunctions.Log_Error("common.set_keepalive_osx: " + str(e))

def calcRemainingTimeFromDayHour(dayHour):
    """ return remaning time which remaing to dayHour """
    try:
        #get time
        timeStamp = int(time.time())
        timeStruct = time.gmtime(timeStamp)
        #get remaining active time
        mk1 = datetime(timeStruct.tm_year, timeStruct.tm_mon, timeStruct.tm_mday, dayHour, 0, 0)
        mk2 = datetime(timeStruct.tm_year, timeStruct.tm_mon, timeStruct.tm_mday, timeStruct.tm_hour, timeStruct.tm_min, timeStruct.tm_sec)
        #calc remaining time in sec
        remainingTime = int(24*60*60 - (time.mktime(mk2.timetuple()) - time.mktime(mk1.timetuple())))
        return remainingTime
    except Exception as e:
        cfunctions.Log_Error("common.getRemainingTime: " + str(e))

#emergency sceen packet handler
def HandleEmergencyScreenData(data):
    """ Handle C_MSG_GET_EMERGENCY_SCREEN_DATA opcode """
    try:
        #get version
        emergenyScreenData = ''
        version = 0
        s_fmt = "<I"
        if len(data) == struct.calcsize(s_fmt):
            version = struct.unpack_from(s_fmt, data)[0]
        
        #version 0 is JSON, version 1 is LUA
        if version == 0:
            emergenyScreenData = transDB.readData(consts.X_KEY_FOR_SETTINGS, consts.SETTINGS_Y_KEY_EMERGENCY_SCREEN)
        elif version == 1:
            emergenyScreenData = transDB.readData(consts.X_KEY_FOR_SETTINGS, consts.SETTINGS_Y_KEY_EMERGENCY_SCREEN_LUA)
        else:
            cfunctions.Log_Warning("TCPHandler.handle_read: unknown emergency screen version: " + str(version))
        
        #GZIP compress
        encoder = zlib.compressobj(8, zlib.DEFLATED, 16+zlib.MAX_WBITS, zlib.DEF_MEM_LEVEL, 0)
        emergenyScreenDataGZIP = encoder.compress(emergenyScreenData)
        emergenyScreenDataGZIP += encoder.flush()
        #create packet + send
        packet = packets.ClientPacket(consts.S_MSG_GET_EMERGENCY_SCREEN_DATA)
        packet.data = emergenyScreenDataGZIP
        return packet
    except Exception as e:
        cfunctions.Log_Error("HandleEmergencyScreenData: " + str(e))

