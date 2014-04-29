import struct
import zlib
import cfunctions

class TransDBPacket:
    """ Packet class """
    def __init__(self, opcode):
        self.opcode = opcode
        self.data = ''
    
    def createPacket(self):
        packet = struct.pack('<IH', len(self.data), self.opcode)
        packet += str(self.data)
        return packet

class ClientPacket:
    """ Packet class """
    def __init__(self, opcode):
        self.opcode = opcode
        self.data = ''
    
    def createPacket(self, crypt):
        try:
            #calc crc as unsigned
            crc32 = zlib.crc32(self.data) & 0xffffffff
            #create packet header
            packetHeader = struct.pack('<IHI', len(self.data), self.opcode, crc32)
            #encrypt header
            packet = crypt.EncryptSend(packetHeader)
            #add data
            packet += str(self.data)
            return packet
        except Exception as e:
            cfunctions.Log_Error("ClientPacket.createPacket: " + str(e))
