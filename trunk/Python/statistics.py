import transDB
import cfunctions
import clientSocket
import struct
import pickle
import json
from operator import itemgetter, attrgetter

class Stats:
    def __init__(self):
        self.opcodesCount = {}
        self.userStats = {}
    
    def Reset(self):
        try:
            #reset opcodes
            self.opcodesCount = {}
            opcodesCountData = pickle.dumps(self.opcodesCount)
            transDB.writeData(clientSocket.X_KEY_FOR_SETTINGS, clientSocket.SETTINGS_Y_DAILY_CHALLENGE_OPCODE_STATS, opcodesCountData)
            #reset user stats
            self.userStats = {}
            transDB.deleteData(clientSocket.X_KEY_FOR_STATS, 0)
        except Exception as e:
            cfunctions.Log_Error("Stats.Reset: " + str(e))

    def loadStatsFromDB(self):
        """ Load statistics.Stats class from DB """
        try:
            #load opcodes stats
            opcodesCountData = transDB.readData(clientSocket.X_KEY_FOR_SETTINGS, clientSocket.SETTINGS_Y_DAILY_CHALLENGE_OPCODE_STATS)
            if len(opcodesCountData) != 0:
                self.opcodesCount = pickle.loads(opcodesCountData)
            
            #load user stats
            data = transDB.readData(clientSocket.X_KEY_FOR_STATS, 0)
            if len(data) != 0:
                offset = 0
                while offset < len(data):
                    #key|recordSize|record
                    key = struct.unpack_from("<Q", data[offset:])[0]
                    offset += 8
                    recordSize = struct.unpack_from("<H", data[offset:])[0]
                    offset += 2
                    recordData = data[offset:offset + recordSize]
                    offset += recordSize
                    #get data
                    structSize = struct.calcsize("<II")
                    numberOfGames, userNameLen  = struct.unpack_from("<II", recordData)
                    userName = recordData[structSize:structSize + userNameLen]
                    self.userStats[key] = (userName, numberOfGames)
        except Exception as e:
            cfunctions.Log_Error("Stats.loadStatsFromDB: " + str(e))

    def saveDailyChallengeOpcodeStats(self):
        """ Save opcode stats to DB """
        try:
            opcodesCountData = pickle.dumps(self.opcodesCount, pickle.HIGHEST_PROTOCOL)
            transDB.writeData(clientSocket.X_KEY_FOR_SETTINGS, clientSocket.SETTINGS_Y_DAILY_CHALLENGE_OPCODE_STATS, opcodesCountData)
        except Exception as e:
            cfunctions.Log_Error("Stats.saveDailyChallengeOpcodeStats: " + str(e))

    def dumpUserStatsToJSON(self):
        """ Dump user stats to JSON """
        try:
            userList = []
            for key, value in self.userStats.items():
                userList.append({"userID":key, "userName": value[0], "games":value[1]})
            userList = sorted(userList, key=itemgetter('games'), reverse=True)
            return json.dumps(userList, ensure_ascii=False)
        except Exception as e:
            cfunctions.Log_Error("Stats.dumpUserStatsToJSON: " + str(e))

    def UpdatePlayedGames(self, userID, userName):
        """ UpdatePlayedGames for user and save to DB """
        try:
            numberOfGames = 1
            if userID in self.userStats:
                _, numberOfGames = self.userStats[userID]
                numberOfGames += 1
            
            #save to memory
            self.userStats[userID] = (userName, numberOfGames)
            
            #save to DB
            userData = struct.pack("<II", numberOfGames, len(userName)) + userName
            transDB.writeData(clientSocket.X_KEY_FOR_STATS, userID, userData)
        except Exception as e:
            cfunctions.Log_Error("Stats.UpdatePlayedGames: " + str(e))
    
    def UpdateOpcodesCount(self, opcode):
        """ UpdateOpcodesCount update opcode counter """
        try:
            if opcode in self.opcodesCount:
                self.opcodesCount[opcode] += 1
            else:
                self.opcodesCount[opcode] = 1
        except Exception as e:
            cfunctions.Log_Error("Stats.UpdateOpcodesCount: " + str(e))


