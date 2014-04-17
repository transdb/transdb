import time
import socket
import struct
import zlib
import Queue
import threading
import transDB
import SocketServer
import common
import pickle
import asyncore
import json
import cfunctions
import random
import smtplib
import packets
import crypto
import statistics
from email.MIMEMultipart import MIMEMultipart
from email.MIMEText import MIMEText
from datetime import datetime
from collections import OrderedDict
from operator import itemgetter, attrgetter

C_MSG_ADD_TO_LEADEBOARD           = 1
S_MSG_ADD_TO_LEADEBOARD           = 2
C_MSG_GET_LEADERBOARD             = 3
S_MSG_GET_LEADERBOARD             = 4
C_MSG_GET_EMERGENCY_SCREEN_DATA   = 5
S_MSG_GET_EMERGENCY_SCREEN_DATA   = 6
C_MSG_GET_DAILY_CHALLENGE         = 7
S_MSG_GET_DAILY_CHALLENGE         = 8

#used x keys
X_KEY_FOR_LEADERBOARDS_MIN  = 1
X_KEY_FOR_LEADERBOARDS_MAX  = 65535
X_KEY_FOR_SETTINGS          = 100000
X_KEY_FOR_STATS             = 200000

#used y keys
SETTINGS_Y_KEY_EMERGENCY_SCREEN = 1
SETTINGS_Y_KEY_GAME_IDS = 2
SETTINGS_Y_DAILY_CHALLENGE_LEVELS = 3
SETTINGS_Y_DAILY_CHALLENGE_ACTIVE_LEVELS = 4
SETTINGS_Y_DAILY_CHALLENGE_LAST_ACTIVE_LEVEL = 5
SETTINGS_Y_DAILY_CHALLENGE_OPCODE_STATS = 10

#dictionary holding data
gameIDsDict = {}

#for daily challenges
#5am GMT
dailyChallangeStartTime = 5
#seconds
useTimeInterval = None
remainingTime = 0

#list of active level IDs (strings)
dailyChallengeActiveLevels = []
lastDailyChallengeActiveLevel = 0

#stats
stats = statistics.Stats()

class User:
    def __init__(self, userID, userName, score):
        self.userID = userID
        self.userName = userName
        self.score = score

    def __repr__(self):
        return "userID: "+str(self.userID)+", userName: "+self.userName+", score: "+str(self.score)

def resetDailyChallengeActiveLevels():
    """ Create list from definitions """
    try:
        tmpData = transDB.readData(X_KEY_FOR_SETTINGS, SETTINGS_Y_DAILY_CHALLENGE_LEVELS)
        if len(tmpData) != 0:
            global dailyChallengeActiveLevels
            dailyChallengeActiveLevels = tmpData.split(",")
            random.shuffle(dailyChallengeActiveLevels)
            saveDailyChallengeActiveLevels()
    except Exception as e:
        cfunctions.Log_Error("resetDailyChallengeActiveLevels: " + str(e))

def saveDailyChallengeActiveLevels():
    """ Save  dailyChallengeActiveLevels to DB """
    try:
        global dailyChallengeActiveLevels
        tmpString = ','.join([str(x) for x in dailyChallengeActiveLevels])
        transDB.writeData(X_KEY_FOR_SETTINGS, SETTINGS_Y_DAILY_CHALLENGE_ACTIVE_LEVELS, tmpString)
    except Exception as e:
        cfunctions.Log_Error("saveDailyChallengeActiveLevels: " + str(e))

def loadDailyChallengeActiveLevels():
    """ Load  dailyChallengeActiveLevels from DB """
    try:
        #load daily challenge active level
        tmpData = transDB.readData(X_KEY_FOR_SETTINGS, SETTINGS_Y_DAILY_CHALLENGE_ACTIVE_LEVELS)
        if len(tmpData) != 0:
            global dailyChallengeActiveLevels
            dailyChallengeActiveLevels = tmpData.split(",")
    except Exception as e:
        cfunctions.Log_Error("loadDailyChallengeActiveLevels: " + str(e))

def loadLastDailyChallengeActiveLevel():
    try:
        #load last DailyChallenge active level
        tmpData = transDB.readData(X_KEY_FOR_SETTINGS, SETTINGS_Y_DAILY_CHALLENGE_LAST_ACTIVE_LEVEL)
        if len(tmpData) != 0:
            global lastDailyChallengeActiveLevel
            lastDailyChallengeActiveLevel = int(tmpData)
    except Exception as e:
        cfunctions.Log_Error("loadDailyChallengeActiveLevels: " + str(e))

def loadDataFromDatabase():
    try:
        #load stats
        stats.loadStatsFromDB()
        
        #load used gameids
        dataList = transDB.readData(X_KEY_FOR_SETTINGS, SETTINGS_Y_KEY_GAME_IDS)
        if len(dataList) == 0:
            return
        
        #load levels where are challenges
        loadDailyChallengeActiveLevels()
        
        #load last DailyChallenge active level
        loadLastDailyChallengeActiveLevel()
        
        #load levels which will be used for daily chalanges
        global dailyChallengeActiveLevels
        if len(dailyChallengeActiveLevels) == 0:
            resetDailyChallengeActiveLevels()
        
        #create list
        gameIDList = pickle.loads(dataList)
        for gameID in gameIDList:
            data = transDB.readData(gameID, 0)
            offset = 0
            while offset < len(data):
                key = struct.unpack_from("<Q", data[offset:])[0]
                offset += 8
                recordSize = struct.unpack_from("<H", data[offset:])[0]
                offset += 2
                recordData = data[offset:offset+recordSize]
                offset += recordSize
                
                #deserialize user
                user = pickle.loads(recordData)
                #get leadeboard dict by gameID
                leadeboardDict = GetLeaderBoardDict(gameID)
                #add to leaderboard map
                usersDict = leadeboardDict.get(user.score)
                if usersDict is None:
                    usersDict = {}
                    leadeboardDict[user.score] = usersDict
                usersDict[user.userID] = user
    
    except Exception as e:
        cfunctions.Log_Error("loadDataFromDatabase: " + str(e))

def GetLeaderBoardDict(gameID):
    try:
        #get leadeboard dict by gameID
        leadeboardDict = gameIDsDict.get(gameID)
        if leadeboardDict is None:
            leadeboardDict = {}
            gameIDsDict[gameID] = leadeboardDict
            #save used gameIDs to DB
            dataList = pickle.dumps(gameIDsDict.keys(), pickle.HIGHEST_PROTOCOL)
            transDB.writeDataNoWait(X_KEY_FOR_SETTINGS, SETTINGS_Y_KEY_GAME_IDS, dataList)
        return leadeboardDict
    except Exception as e:
        cfunctions.Log_Error("GetLeaderBoardDict: " + str(e))

def AddUserToLeaderBoard(gameID, userID, userName, score):
    try:
        #get leadeboard dict by gameID
        leadeboardDict = GetLeaderBoardDict(gameID)
        
        #create or update user
        userData = transDB.readData(gameID, userID)
        if len(userData) == 0:
            user = User(userID, userName, score)
        else:
            user = pickle.loads(userData)
            if score > user.score:
                #remove from userDict by old score
                usersDict = leadeboardDict.get(user.score)
                if not usersDict is None:
                    usersDict.pop(user.userID, None)
                    #if no item left delete dict with users
                    if len(usersDict) == 0:
                        leadeboardDict.pop(user.score, None)
                
                #set new score
                user.score = score
            else:
                #no score change return user
                return user
        
        #try to get userDict and add user to leaderboard dict
        usersDict = leadeboardDict.get(score)
        if usersDict is None:
            usersDict = {}
            leadeboardDict[score] = usersDict
        #add
        usersDict[user.userID] = user
        
        #save to DB - x=gameID, y=userID
        userData = pickle.dumps(user, pickle.HIGHEST_PROTOCOL)
        transDB.writeDataNoWait(gameID, user.userID, userData)
        
        return user
    except Exception as e:
        cfunctions.Log_Error("AddUserToLeaderBoard: " + str(e))

def GetUsers(leadeboardDict, users, keys, position, myPosition, myuserID):
    try:
        #fix positions
        if position < 0:
            position = 0
        if position > len(keys):
            position = len(keys) - 1
        if myPosition > len(keys):
            myPosition = len(keys)
        
        #user
        user = None
        
        #set step
        step = 1 if position < myPosition else -1
        
        #get users around me
        for i in xrange(position, myPosition, step):
            score = keys[i]
            usersDict = leadeboardDict[score]
            #get my info
            if myuserID in usersDict:
                user = usersDict[myuserID]
            else:
                #get 1st user info
                for userID in usersDict.keys():
                    user = usersDict[userID]
                    break
            #add user to list
            if not user is None:
                #list with keys is from 0 but we want positions from 1 (i + 1)
                users[user.userID] = (i + 1, user.score, len(usersDict) - 1, user.userName)

    except Exception as e:
        cfunctions.Log_Error("GetUsers: " + str(e))

def CreateLeaderboardJSON(gameID, userID):
    try:
        #get leadeboard dict by gameID
        leadeboardDict = GetLeaderBoardDict(gameID)
        
        #cfg - max users on page is 8
        topUsersCountIfNoResult = 8
        topUsersCount = 3
        usersAround = 2
        
        #dictionary
        users = {}
        #sorted leadeboardDict keys - score
        keys = sorted(leadeboardDict.keys(), reverse=True)
        
        #if user is none return only top x users
        userData = transDB.readData(gameID, userID)
        if len(userData) != 0:
            user = pickle.loads(userData)
            #get user index in dictionary
            userPosition = keys.index(user.score)
            #if user is in top 6 return top 8 it fits to device screen
            if userPosition in xrange(0, topUsersCountIfNoResult - usersAround):
                #
                GetUsers(leadeboardDict, users, keys, 0, topUsersCountIfNoResult, userID)
            else:
                #get top x users
                GetUsers(leadeboardDict, users, keys, 0, topUsersCount, userID)
                #get my position
                GetUsers(leadeboardDict, users, keys, userPosition, userPosition+1, userID)
                #get users before me
                positionBefore = userPosition - usersAround
                GetUsers(leadeboardDict, users, keys, positionBefore, userPosition, userID)
                #get users after me
                positionAfter = userPosition + usersAround
                GetUsers(leadeboardDict, users, keys, userPosition+1, positionAfter+1, userID)
        else:
            #user is not in leaderboard return top 8 users
            GetUsers(leadeboardDict, users, keys, 0, topUsersCountIfNoResult, userID)
        
        #create list for json
        sortedUsers = []
        for userID in users.keys():
            position, score, numOfUsers, userName = users[userID]
            sortedUsers.append({"position":position, "userID":userID, "userName": userName, "score":score, "numOfUsers":numOfUsers})
        
        #sort by position
        sortedUsers = sorted(sortedUsers, key=itemgetter('position'))
        return sortedUsers
    except Exception as e:
        cfunctions.Log_Error("CreateLeaderboardJSON: " + str(e))


class TCPHandler(asyncore.dispatcher_with_send):
    def __init__(self, sock, holder):
        asyncore.dispatcher_with_send.__init__(self, sock)
        self.holder = holder
        self.sock = sock
        self.crypt = crypto.Crypt()
        #set socket timeout
        common.set_keepalive_linux(self.sock, after_idle_sec=10, interval_sec=5, max_fails=3)
    
    def handle_read(self):
        """ Handle socket read event """
        try:
            global remainingTime
            global dailyChallengeActiveLevels
            global lastDailyChallengeActiveLevel
            
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
                self.close()
                return
        
            #counter
            stats.UpdateOpcodesCount(opcode)
    
            #leaderboard
            if opcode == C_MSG_ADD_TO_LEADEBOARD:
                #get gameID (levelID), userID, score
                structSize = struct.calcsize("<HQI")
                gameID, userID, score = struct.unpack_from("<HQI", data)
                userName = data[structSize:-1]
                
                #check data from client - ignore bad data
                if gameID == 0 or userID == 0:
                    #send dummy data to client
                    packet = packets.ClientPacket(S_MSG_ADD_TO_LEADEBOARD, self.crypt)
                    packet.data = struct.pack("<II", 0, 0)
                    packetData = packet.createPacket()
                    self.send(packetData)
                else:
                    #stats
                    stats.UpdatePlayedGames(userID, userName)
                    
                    #create or update user
                    user = AddUserToLeaderBoard(gameID, userID, userName, score)
                    
                    #get user position and count of users
                    leadeboardDict = GetLeaderBoardDict(gameID)
                    keys = sorted(leadeboardDict.keys(), reverse=True)
                    userPosition = keys.index(user.score) + 1
                    usersCount = len(keys)
                    
                    #send ok to client
                    packet = packets.ClientPacket(S_MSG_ADD_TO_LEADEBOARD, self.crypt)
                    packet.data = struct.pack("<II", userPosition, usersCount)
                    packetData = packet.createPacket()
                    self.send(packetData)
            
            #leaderboard
            elif opcode == C_MSG_GET_LEADERBOARD:
                #get gameID (levelID), userID
                gameID, userID = struct.unpack_from("<HQ", data, 0)
                #create JSON per active levels
                jsonDict = {}
                jsonDict[gameID] = CreateLeaderboardJSON(gameID, userID)
                jsonDict[lastDailyChallengeActiveLevel] = CreateLeaderboardJSON(lastDailyChallengeActiveLevel + 10000, userID)
                #dump to JSON
                jsonStr = json.dumps(jsonDict)
                #GZIP compress
                encoder = zlib.compressobj(8, zlib.DEFLATED, 16+zlib.MAX_WBITS, zlib.DEF_MEM_LEVEL, 0)
                jsonStrGZIP = encoder.compress(jsonStr)
                jsonStrGZIP += encoder.flush()
                #create packet + send
                packet = packets.ClientPacket(S_MSG_GET_LEADERBOARD, self.crypt)
                packet.data = jsonStrGZIP
                packetData = packet.createPacket()
                self.send(packetData)
            
            #emergency screen data
            elif opcode == C_MSG_GET_EMERGENCY_SCREEN_DATA:
                data = transDB.readData(X_KEY_FOR_SETTINGS, SETTINGS_Y_KEY_EMERGENCY_SCREEN)
                #GZIP compress
                encoder = zlib.compressobj(8, zlib.DEFLATED, 16+zlib.MAX_WBITS, zlib.DEF_MEM_LEVEL, 0)
                dataGZIP = encoder.compress(data)
                dataGZIP += encoder.flush()
                #create packet + send
                packet = packets.ClientPacket(S_MSG_GET_EMERGENCY_SCREEN_DATA, self.crypt)
                packet.data = dataGZIP
                packetData = packet.createPacket()
                self.send(packetData)
            
            #daily challenge
            elif opcode == C_MSG_GET_DAILY_CHALLENGE:
                #get active level
                level = 0
                #
                if len(dailyChallengeActiveLevels) != 0:
                    level = int(dailyChallengeActiveLevels[0])
                
                #create and send packet
                packet = packets.ClientPacket(S_MSG_GET_DAILY_CHALLENGE, self.crypt)
                packet.data = struct.pack("<HHI", level, lastDailyChallengeActiveLevel, remainingTime)
                packetData = packet.createPacket()
                self.send(packetData)
                
            else:
                cfunctions.Log_Warning("TCPHandler.handle_read: Unknown opcode: " + str(opcode))
        
        #ignore
        except socket.error as e:
            #timeout disconnect socket
            if e.errno == errno.ETIMEDOUT:
                self.handle_close()
            else:
                raise
                
        except RuntimeError as e:
            pass
    
        except Exception as e:
            cfunctions.Log_Error("TCPHandler.handle_read: " + str(e))


class TCPServer(asyncore.dispatcher):
    def __init__(self, host, port):
        asyncore.dispatcher.__init__(self)
        self.create_socket(socket.AF_INET, socket.SOCK_STREAM)
        self.set_reuse_addr()
        self.bind((host, port))
        self.listen(128)
        #set if use timeinterval or daily timer
        global remainingTime
        global useTimeInterval
        if useTimeInterval is None:
            self.dailyChallengeTicked = False
            remainingTime = self.calcRemainingTimeFromDayHour(dailyChallangeStartTime)
        else:
            remainingTime = useTimeInterval
            self.dailyChallengeTicked = int(time.time())
    
    def handle_accept(self):
        """ Handle socket accept """
        try:
            pair = self.accept()
            if pair is not None:
                sock, addr = pair
                handler = TCPHandler(sock, self)
        except Exception as e:
            cfunctions.Log_Error("TCPServer.handle_accept: " + str(e))
    
    def calcRemainingTimeFromDayHour(self, dayHour):
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
            cfunctions.Log_Error("TCPServer.getRemainingTime: " + str(e))

    def saveDailyChallengeOpcodeStats(self):
        """ Save opcode stats to DB """
        try:
            stats.saveDailyChallengeOpcodeStats()
        except Exception as e:
            cfunctions.Log_Error("TCPServer.saveDailyChallengeOpcodeStats: " + str(e))

    def ResetDailyChallengeStats(self):
        try:
            """ Send email with daily stats and reset stats """
            
            #header
            fromaddr = "kudrnac@hypervhosting.cz"
            toaddr = ["miroslav.kudrnac@geewa.com", "denis.timofeev@geewa.com", "jozef.kral@geewa.com", "tomas.pojkar@geewa.com", "tomas.kafka@geewa.com", "nikola.bornova@geewa.com"]
            msg = MIMEMultipart()
            msg['From'] = fromaddr
            msg['To'] = ",".join(toaddr)
            msg['Subject'] = "Daily challenge stats"
            #body - userStats
            body = stats.dumpUserStatsToJSON()
            msg.attach(MIMEText(body, 'plain'))
            #body - opcodesStats
            body = '\n\n' + json.dumps(stats.opcodesCount)
            msg.attach(MIMEText(body, 'plain'))
        
            #send email
            server = smtplib.SMTP('vps4u.cz', 25)
            server.login("kudrnac@hypervhosting.cz", "102111036")
            server.sendmail(fromaddr, toaddr, msg.as_string())
            server.quit()
        
            #reset stats
            stats.Reset()

        except Exception as e:
            cfunctions.Log_Error("TCPServer.ResetDailyChallengeStats: " + str(e))

    def ResetDailyChallenge(self):
        """ Handle reset of leaderboard and daily challenge """
        try:
            #global
            global dailyChallengeActiveLevels
            global lastDailyChallengeActiveLevel
            
            #reset stats
            self.ResetDailyChallengeStats()
            
            #reload challenges and prepare new one
            if len(dailyChallengeActiveLevels) != 0:
                #save last active daily challenge level
                lastDailyChallengeActiveLevel = int(dailyChallengeActiveLevels[0])
                transDB.writeData(X_KEY_FOR_SETTINGS, SETTINGS_Y_DAILY_CHALLENGE_LAST_ACTIVE_LEVEL, str(lastDailyChallengeActiveLevel))
                
                #reset leaderboards
                #level ID looks like 0/1 - actual/last one
                #               level ID 00 - 99
                #               mod ID 00 - 99
                #example        10101
                activeLevel = int(dailyChallengeActiveLevels[0])
                
                #get old leadeboard dict by gameID and remove from dict
                leadeboardDict = GetLeaderBoardDict(activeLevel)
                gameIDsDict.pop(activeLevel, None)
                
                #read all data from leaderboard then delete and insert to new level ID
                leaderboardData = transDB.readData(activeLevel, 0)
                transDB.deleteData(activeLevel, 0)
                #change level
                activeLevel += 10000
                transDB.deleteData(activeLevel, 0)
                transDB.writeDataNum(activeLevel, leaderboardData)
                
                #save to gameIDsDict
                GetLeaderBoardDict(activeLevel)
                gameIDsDict[activeLevel] = leadeboardDict
                
                #reload from DB we want to edit it in DB and make it apply next day
                loadDailyChallengeActiveLevels()
                
                #remove 1st one and save to db
                dailyChallengeActiveLevels.pop(0)
                saveDailyChallengeActiveLevels()
            
            #load levels + shuffle and save to DB
            if len(dailyChallengeActiveLevels) == 0:
                resetDailyChallengeActiveLevels()
    
            #delete data from new active level - there can be old data
            activeLevel = int(dailyChallengeActiveLevels[0])
            transDB.deleteData(activeLevel, 0)
    
        except Exception as e:
            cfunctions.Log_Error("TCPServer.ResetDailyChallenge: " + str(e))
    
    def TickForDailyChallenge(self):
        try:
            global remainingTime
            global useTimeInterval
            
            #get time
            timeStamp = int(time.time())
            timeStruct = time.gmtime(timeStamp)
            
            #use time not time interval
            if useTimeInterval is None:
                #update remaining time in sec for client
                remainingTime = self.calcRemainingTimeFromDayHour(dailyChallangeStartTime)
                
                #check time
                if timeStruct.tm_hour == dailyChallangeStartTime and self.dailyChallengeTicked == False:
                    self.ResetDailyChallenge()
                    self.dailyChallengeTicked = True
                
                #reset time
                if timeStruct.tm_hour != dailyChallangeStartTime and self.dailyChallengeTicked == True:
                    self.dailyChallengeTicked = False
            else:
                #use time interval
                if remainingTime <= 0:
                    #reset and reset timer
                    self.ResetDailyChallenge()
                    remainingTime = useTimeInterval
                else:
                    #update remainign time and save last time
                    remainingTime -= (timeStamp - self.dailyChallengeTicked)
                    self.dailyChallengeTicked = int(time.time())
        
        except Exception as e:
            cfunctions.Log_Error("TCPServer.TickForDailyChallenge: " + str(e))











