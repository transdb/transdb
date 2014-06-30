import struct


class TransDBPacket:
    """ Packet class """
    def __init__(self, opcode):
        self.opcode = opcode
        self.data = ''
    
    def createPacket(self):
        packet = struct.pack('<IH', len(self.data), self.opcode)
        packet += str(self.data)
        return packet
