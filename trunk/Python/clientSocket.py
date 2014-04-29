import socket
import struct
import zlib
import asyncore
import platform

import common
import cfunctions
import crypto
import leaderboard

#must be here because of pickle
class User:
    def __init__(self, userID, userName, score):
        self.userID = userID
        self.userName = userName
        self.score = score
    
    def __repr__(self):
        return "userID: "+str(self.userID)+", userName: "+self.userName+", score: "+str(self.score)

#leaderboard class
leaderboard = leaderboard.Leaderboard()

#packet handle
packetHandler = [None,                                  #NULL                               = 0
                 leaderboard.HandleAddToLeaderBoard,    #C_MSG_ADD_TO_LEADEBOARD            = 1
                 None,                                  #S_MSG_ADD_TO_LEADEBOARD            = 2
                 leaderboard.HandleGetLeaderboard,      #C_MSG_GET_LEADERBOARD              = 3
                 None,                                  #S_MSG_GET_LEADERBOARD              = 4
                 common.HandleEmergencyScreenData,      #C_MSG_GET_EMERGENCY_SCREEN_DATA    = 5
                 None,                                  #S_MSG_GET_EMERGENCY_SCREEN_DATA    = 6
                 leaderboard.HandleGetDailyChallenge,   #C_MSG_GET_DAILY_CHALLENGE          = 7
                 None                                   #S_MSG_GET_DAILY_CHALLENGE          = 8
                 ]

#socket timeout func
if platform.system()  == "Linux":
    set_keepalive_func = common.set_keepalive_linux
elif platform.system()  == "Darwin":
    set_keepalive_func = common.set_keepalive_osx
else:
    set_keepalive_func = None

class TCPHandler(asyncore.dispatcher_with_send):
    def __init__(self, sock):
        asyncore.dispatcher_with_send.__init__(self, sock)
        #init crypto
        self.crypt = crypto.Crypt()
        #set socket timeout
        set_keepalive_func(sock, after_idle_sec=10, interval_sec=5, max_fails=3)
            
    def handle_close(self):
        """ Handle socket close event """
        self.close()
    
    def handle_read(self):
        """ Handle socket read event """
        try:
            packetHeader = ''
            #wait for header
            headerLen = struct.calcsize("<IHI")
            while len(packetHeader) < headerLen:
                chunk = self.recv(headerLen - len(packetHeader))
                if chunk == '':
                    raise RuntimeError("Socket connection broken")
                packetHeader = packetHeader + chunk
    
            #decrypt packet header
            packetHeader = self.crypt.DecryptRecv(packetHeader)
        
            #get rest of the packet data
            size, opcode, crc32 = struct.unpack("<IHI", packetHeader)
            data = ''
            while len(data) < size:
                chunk = self.recv(size - len(data))
                if chunk == '':
                    raise RuntimeError("Socket connection broken")
                data = data + chunk
    
            #check crc32
            dataCrc32 = zlib.crc32(data) & 0xffffffff
            if dataCrc32 != crc32:
                cfunctions.Log_Warning("TCPHandler.handle_read: Crc32 does not match disconnecting socket.")
                self.handle_close()
                return
        
            #leaderboard opcode counter
            leaderboard.stats.UpdateOpcodesCount(opcode)
    
            #handle opcode
            if len(packetHandler) > opcode and not packetHandler[opcode] is None:
                packet = packetHandler[opcode](data)
                packetData = packet.createPacket(self.crypt)
                self.send(packetData)
            else:
                cfunctions.Log_Warning("TCPHandler.handle_read: Unknown opcode: " + str(opcode))
            
        #handle socket errors - disconnect socket
        except socket.error as e:
            self.handle_close()
        #ignore
        except RuntimeError as e:
            pass
        #log other exceptions
        except Exception as e:
            cfunctions.Log_Error("TCPHandler.handle_read: " + str(e))


class TCPServer(asyncore.dispatcher):
    def __init__(self, host, port):
        asyncore.dispatcher.__init__(self)
        self.create_socket(socket.AF_INET, socket.SOCK_STREAM)
        self.set_reuse_addr()
        self.bind((host, port))
        self.listen(5)
    
    def handle_accept(self):
        """ Handle socket accept """
        try:
            pair = self.accept()
            if pair is not None:
                sock, addr = pair
                handler = TCPHandler(sock)
        except Exception as e:
            cfunctions.Log_Error("TCPServer.handle_accept: " + str(e))











