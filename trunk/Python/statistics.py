import struct
import pickle
import json
import smtplib
from operator import itemgetter
from email import Encoders

import transDB
import cfunctions
import consts
from email.MIMEMultipart import MIMEMultipart
from email.MIMEBase import MIMEBase
from email.MIMEText import MIMEText
from email.Utils import COMMASPACE, formatdate


class Stats:
    def __init__(self):
        self.opcodesCount = {}
        self.userStats = {}
    
    def Reset(self):
        try:
            #reset opcodes
            self.opcodesCount = {}
            opcodesCountData = pickle.dumps(self.opcodesCount)
            transDB.writeData(consts.X_KEY_FOR_SETTINGS, consts.SETTINGS_Y_DAILY_CHALLENGE_OPCODE_STATS, opcodesCountData)
            #reset user stats
            self.userStats = {}
            transDB.deleteData(consts.X_KEY_FOR_STATS, 0)
        except Exception as e:
            cfunctions.Log_Error("Stats.Reset: " + str(e))

    def loadStatsFromDB(self):
        """ Load statistics.Stats class from DB """
        try:
            #load opcodes stats
            opcodesCountData = transDB.readData(consts.X_KEY_FOR_SETTINGS, consts.SETTINGS_Y_DAILY_CHALLENGE_OPCODE_STATS)
            if len(opcodesCountData) != 0:
                self.opcodesCount = pickle.loads(opcodesCountData)
            
            #load user stats
            data = transDB.readData(consts.X_KEY_FOR_STATS, 0)
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
            transDB.writeData(consts.X_KEY_FOR_SETTINGS, consts.SETTINGS_Y_DAILY_CHALLENGE_OPCODE_STATS, opcodesCountData)
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

    def SendByEmail(self):
        """ dumps stats and send by email  """
        try:
            #header
            fromaddr = "kudrnac@hypervhosting.cz"
            toaddr = ["miroslav.kudrnac@geewa.com", "denis.timofeev@geewa.com", "jozef.kral@geewa.com", "tomas.pojkar@geewa.com", "tomas.kafka@geewa.com", "nikola.bornova@geewa.com"]
            msg = MIMEMultipart()
            msg['From'] = fromaddr
            msg['To'] = COMMASPACE.join(toaddr)
            msg['Date'] = formatdate(localtime=True)
            msg['Subject'] = "Daily challenge stats"

            #body - opcodesStats
            body = json.dumps(self.opcodesCount) + '\n\n'
            msg.attach(MIMEText(body, 'plain'))

            #attachment - userStats
            jsonStr = self.dumpUserStatsToJSON()
            part = MIMEBase('application', "octet-stream")
            part.set_payload(jsonStr)
            Encoders.encode_base64(part)
            part.add_header('Content-Disposition', 'attachment; filename="userStats.json"')
            msg.attach(part)
            
            #send email
            server = smtplib.SMTP('vps4u.cz', 25)
            server.login("kudrnac@hypervhosting.cz", "102111036")
            server.sendmail(fromaddr, toaddr, msg.as_string())
            server.quit()
        
        except Exception as e:
            cfunctions.Log_Error("Stats.SendByEmail: " + str(e))

    def ResetDailyChallengeStats(self):
        try:
            """ Send email with daily stats and reset stats """
            
            #send by email
            self.SendByEmail()
            
            #reset stats
            self.Reset()
        
        except Exception as e:
            cfunctions.Log_Error("Stats.ResetDailyChallengeStats: " + str(e))

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
            transDB.writeData(consts.X_KEY_FOR_STATS, userID, userData)
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


