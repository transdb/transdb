import cfunctions

class Crypt:
    def __init__(self):
        self.m_cryptKey = bytearray(b"T2%o9^24C2r14}:p63zU")
        self.m_keyLen = len(self.m_cryptKey)
        self.m_send_i = 0
        self.m_send_j = 0
        self.m_recv_i = 0
        self.m_recv_j = 0
    
    def DecryptRecv(self, data):
        try:
            returnData = bytearray(data)
            for t in xrange(0, len(returnData)):
                self.m_recv_i %= self.m_keyLen;
                x = ((returnData[t] - self.m_recv_j) ^ self.m_cryptKey[self.m_recv_i]) % 256;
                self.m_recv_i += 1;
                self.m_recv_j = returnData[t];
                returnData[t] = x;
            return returnData
        except Exception as e:
            cfunctions.Log_Error("Crypt.DecryptRecv: " + str(e))
    
    def EncryptSend(self, data):
        try:
            returnData = bytearray(data)
            for t in xrange(0, len(returnData)):
                self.m_send_i %= self.m_keyLen;
                returnData[t] = self.m_send_j = ((returnData[t] ^ self.m_cryptKey[self.m_send_i]) + self.m_send_j) % 256;
                self.m_send_i += 1;
            return returnData
        except Exception as e:
            cfunctions.Log_Error("Crypt.EncryptSend: " + str(e))
