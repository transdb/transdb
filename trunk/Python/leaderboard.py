import time
import struct
import zlib
import pickle
import json
import random
from random import randint
from operator import itemgetter

import transDB
import common
import cfunctions
import packets
import statistics
import consts
import clientSocket


class Leaderboard:
    def __init__(self):
        #stat class
        self.stats = statistics.Stats()
        #dictionary holding data
        self.gameIDsDict = {}
        #for time measurement
        self.timeDict = {}
        self.banTable = [8654585614, 8650784135, 8659413645, 8664423249,
                         8592925224, 8661139638, 8637683649, 8654585614,
                         8640859146, 8619348861, 8617418356, 8656218460
                         8654585614, 8671767897]
        #for daily challenges
        #5am GMT
        self.dailyChallangeStartTime = 5
        #seconds
        self.useTimeInterval = None
        self.remainingTime = 0
        #list of active level IDs (strings)
        self.dailyChallengeActiveLevels = []
        self.lastDailyChallengeActiveLevel = 0
        
        #set if use timeinterval or daily timer
        if self.useTimeInterval is None:
            self.dailyChallengeTicked = False
            self.remainingTime = common.calcRemainingTimeFromDayHour(self.dailyChallangeStartTime)
        else:
            self.remainingTime = self.useTimeInterval
            self.dailyChallengeTicked = int(time.time())

    def TickForDailyChallenge(self):
        """ Tick every XX seconds and check for reset daily challenge and stats """
        try:
            #get time
            timeStamp = int(time.time())
            timeStruct = time.gmtime(timeStamp)
            
            #use time not time interval
            if self.useTimeInterval is None:
                #update remaining time in sec for client
                self.remainingTime = common.calcRemainingTimeFromDayHour(self.dailyChallangeStartTime)
                
                #check time
                if timeStruct.tm_hour == self.dailyChallangeStartTime and self.dailyChallengeTicked == False:
                    self.ResetDailyChallenge()
                    self.dailyChallengeTicked = True
                
                #reset time
                if timeStruct.tm_hour != self.dailyChallangeStartTime and self.dailyChallengeTicked == True:
                    self.dailyChallengeTicked = False
            else:
                #use time interval
                if self.remainingTime <= 0:
                    #reset and reset timer
                    self.ResetDailyChallenge()
                    self.remainingTime = self.useTimeInterval
                else:
                    #update remainign time and save last time
                    self.remainingTime -= (timeStamp - self.dailyChallengeTicked)
                    self.dailyChallengeTicked = int(time.time())
        
        except Exception as e:
            cfunctions.Log_Error("Leaderboard.TickForDailyChallenge: " + str(e))
    
    def ResetDailyChallenge(self):
        """ Handle reset of leaderboard and daily challenge """
        try:
            #reset stats
            self.stats.ResetDailyChallengeStats()
            
            #reload challenges and prepare new one
            if len(self.dailyChallengeActiveLevels) != 0:
                #save last active daily challenge level
                self.lastDailyChallengeActiveLevel = int(self.dailyChallengeActiveLevels[0])
                transDB.writeData(consts.X_KEY_FOR_SETTINGS, consts.SETTINGS_Y_DAILY_CHALLENGE_LAST_ACTIVE_LEVEL, str(self.lastDailyChallengeActiveLevel))
                
                #reset leaderboards
                #level ID looks like 0/1 - actual/last one
                #               level ID 00 - 99
                #               mod ID 00 - 99
                #example        10101
                activeLevel = int(self.dailyChallengeActiveLevels[0])
                
                #get old leadeboard dict by gameID and remove from dict
                leadeboardDict = self.GetLeaderBoardDict(activeLevel)
                self.gameIDsDict.pop(activeLevel, None)
                
                #read all data from leaderboard then delete and insert to new level ID
                leaderboardData = transDB.readData(activeLevel, 0)
                transDB.deleteData(activeLevel, 0)
                #change level
                activeLevel += 10000
                transDB.deleteData(activeLevel, 0)
                transDB.writeDataNum(activeLevel, leaderboardData)
                
                #save to gameIDsDict
                self.GetLeaderBoardDict(activeLevel)
                self.gameIDsDict[activeLevel] = leadeboardDict
                
                #reload from DB we want to edit it in DB and make it apply next day
                self.loadDailyChallengeActiveLevels()
                
                #remove 1st one and save to db
                self.dailyChallengeActiveLevels.pop(0)
                self.saveDailyChallengeActiveLevels()
            
            #load levels + shuffle and save to DB
            if len(self.dailyChallengeActiveLevels) == 0:
                self.resetDailyChallengeActiveLevels()
            
            #delete data from new active level - there can be old data
            activeLevel = int(self.dailyChallengeActiveLevels[0])
            transDB.deleteData(activeLevel, 0)
        
        except Exception as e:
            cfunctions.Log_Error("Leaderboard.ResetDailyChallenge: " + str(e))
    
    def loadDataFromDatabase(self):
        """ Load Leaderboard class from DB """
        try:
            #load stats
            self.stats.loadStatsFromDB()
            
            #load levels where are challenges
            self.loadDailyChallengeActiveLevels()
            
            #load last DailyChallenge active level
            self.loadLastDailyChallengeActiveLevel()
            
            #load levels which will be used for daily challenges
            if len(self.dailyChallengeActiveLevels) == 0:
                self.resetDailyChallengeActiveLevels()
        
        except Exception as e:
            cfunctions.Log_Error("Leaderboard.loadDataFromDatabase: " + str(e))

    def resetDailyChallengeActiveLevels(self):
        """ Create list from definitions """
        try:
            tmpData = transDB.readData(consts.X_KEY_FOR_SETTINGS, consts.SETTINGS_Y_DAILY_CHALLENGE_LEVELS)
            if len(tmpData) != 0:
                self.dailyChallengeActiveLevels = tmpData.split(",")
                random.shuffle(self.dailyChallengeActiveLevels)
                self.saveDailyChallengeActiveLevels()
        except Exception as e:
            cfunctions.Log_Error("Leaderboard.resetDailyChallengeActiveLevels: " + str(e))

    def saveDailyChallengeActiveLevels(self):
        """ Save  dailyChallengeActiveLevels to DB """
        try:
            tmpString = ','.join([str(x) for x in self.dailyChallengeActiveLevels])
            transDB.writeData(consts.X_KEY_FOR_SETTINGS, consts.SETTINGS_Y_DAILY_CHALLENGE_ACTIVE_LEVELS, tmpString)
        except Exception as e:
            cfunctions.Log_Error("Leaderboard.saveDailyChallengeActiveLevels: " + str(e))

    def loadDailyChallengeActiveLevels(self):
        """ Load  dailyChallengeActiveLevels from DB """
        try:
            #load daily challenge active level
            tmpData = transDB.readData(consts.X_KEY_FOR_SETTINGS, consts.SETTINGS_Y_DAILY_CHALLENGE_ACTIVE_LEVELS)
            if len(tmpData) != 0:
                self.dailyChallengeActiveLevels = tmpData.split(",")
        except Exception as e:
            cfunctions.Log_Error("Leaderboard.loadDailyChallengeActiveLevels: " + str(e))

    def loadLastDailyChallengeActiveLevel(self):
        """ Load last active level in daily challenge """
        try:
            #load last DailyChallenge active level
            tmpData = transDB.readData(consts.X_KEY_FOR_SETTINGS, consts.SETTINGS_Y_DAILY_CHALLENGE_LAST_ACTIVE_LEVEL)
            if len(tmpData) != 0:
                self.lastDailyChallengeActiveLevel = int(tmpData)
        except Exception as e:
            cfunctions.Log_Error("Leaderboard.loadDailyChallengeActiveLevels: " + str(e))

    def LoadLeaderboardDictByGameID(self, gameID):
        """ Load leaderboard dict from DB """
        try:
            #ret value
            leadeboardDict = {}
            #load from DB
            data = transDB.readData(gameID, 0)
            offset = 0
            #parse
            while offset < len(data):
                key = struct.unpack_from("<Q", data[offset:])[0]
                offset += 8
                recordSize = struct.unpack_from("<H", data[offset:])[0]
                offset += 2
                recordData = data[offset:offset+recordSize]
                offset += recordSize
                
                #deserialize user
                user = pickle.loads(recordData)
                
                #check bantable - set rnd low score
                if user.userID in self.banTable:
                    user.score = randint(1, 20)
                
                #add to leaderboard map
                usersDict = leadeboardDict.get(user.score)
                if usersDict is None:
                    usersDict = {}
                    leadeboardDict[user.score] = usersDict
                usersDict[user.userID] = user
            
            return leadeboardDict
        except Exception as e:
            cfunctions.Log_Error("Leaderboard.LoadLeaderboardDictByGameID: " + str(e))

    def GetLeaderBoardDict(self, gameID):
        """ Get leaderboard dict by gameID """
        try:
            #get leadeboard dict by gameID
            leadeboardDict = self.gameIDsDict.get(gameID)
            if leadeboardDict is None:
                #load leadeboardDict from DB
                leadeboardDict = self.LoadLeaderboardDictByGameID(gameID)
                self.gameIDsDict[gameID] = leadeboardDict
            return leadeboardDict
        except Exception as e:
            cfunctions.Log_Error("Leaderboard.GetLeaderBoardDict: " + str(e))

    def GetUsers(self, leadeboardDict, users, keys, position, myPosition, myuserID):
        """ Grab users from leaderboard by criteria """
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
            cfunctions.Log_Error("Leaderboard.GetUsers: " + str(e))

    def CreateLeaderboardJSON(self, gameID, userID):
        """ Create leaderboard list serializable to JSON for userID and gameID """
        try:
            #get leadeboard dict by gameID
            leadeboardDict = self.GetLeaderBoardDict(gameID)
            
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
                    self.GetUsers(leadeboardDict, users, keys, 0, topUsersCountIfNoResult, userID)
                else:
                    #get top x users
                    self.GetUsers(leadeboardDict, users, keys, 0, topUsersCount, userID)
                    #get my position
                    self.GetUsers(leadeboardDict, users, keys, userPosition, userPosition+1, userID)
                    #get users before me
                    positionBefore = userPosition - usersAround
                    self.GetUsers(leadeboardDict, users, keys, positionBefore, userPosition, userID)
                    #get users after me
                    positionAfter = userPosition + usersAround
                    self.GetUsers(leadeboardDict, users, keys, userPosition+1, positionAfter+1, userID)
            else:
                #user is not in leaderboard return top 8 users
                self.GetUsers(leadeboardDict, users, keys, 0, topUsersCountIfNoResult, userID)
            
            #create list for json
            sortedUsers = []
            for userID in users.keys():
                position, score, numOfUsers, userName = users[userID]
                sortedUsers.append({"position":position, "userID":userID, "userName": userName, "score":score, "numOfUsers":numOfUsers})
            
            #sort by position
            sortedUsers = sorted(sortedUsers, key=itemgetter('position'))
            return sortedUsers
        except Exception as e:
            cfunctions.Log_Error("Leaderboard.CreateLeaderboardJSON: " + str(e))

    def AddUserToLeaderBoard(self, gameID, userID, userName, score):
        """ Hplfunction for add user to leaderboard """
        try:
            #get leadeboard dict by gameID
            leadeboardDict = self.GetLeaderBoardDict(gameID)
            
            #create or update user
            userData = transDB.readData(gameID, userID)
            if len(userData) == 0:
                user = clientSocket.User(userID, userName, score)
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
            transDB.writeData(gameID, user.userID, userData)
            
            return user
        except Exception as e:
            cfunctions.Log_Error("Leaderboard.AddUserToLeaderBoard: " + str(e))

    def HandleAddToLeaderBoard(self, data):
        """ Handle C_MSG_ADD_TO_LEADEBOARD opcode """
        try:
            #get gameID (levelID), userID, score
            structSize = struct.calcsize("<HQI")
            gameID, userID, score = struct.unpack_from("<HQI", data)
            userName = data[structSize:-1]
            
            #check time - avoid cheating score
            if userID in self.timeDict:
                startTime = self.timeDict[userID]
                self.timeDict[userID] = time.time()
                gameTime = int(time.time() - startTime)
                cheater = False
                gameMode = ((int(self.dailyChallengeActiveLevels[0]) / 100) % 100)
                if gameMode == 2:
                    if (gameTime < 600 and score > 600):
                        cheater = True
                    if cheater == True:
                        cfunctions.Log_Warning("Possible cheating - userID: "+str(userID)+", userName: "+userName+", score: "+str(score)+", gameTime: "+str(gameTime))
            
            #check bantable - set rnd low score
            if userID in self.banTable:
                score = randint(1, 20)
            
            #score limit
            if score > 10000:
                score = randint(1, 20)
            
            #check data from client - ignore bad data
            if gameID == 0 or userID == 0:
                #send dummy data to client
                packet = packets.ClientPacket(consts.S_MSG_ADD_TO_LEADEBOARD)
                packet.data = struct.pack("<II", 0, 0)
                return packet
            else:
                #stats
                self.stats.UpdatePlayedGames(userID, userName)
                
                #create or update user
                user = self.AddUserToLeaderBoard(gameID, userID, userName, score)
                
                #get user position and count of users
                leadeboardDict = self.GetLeaderBoardDict(gameID)
                keys = sorted(leadeboardDict.keys(), reverse=True)
                userPosition = keys.index(user.score) + 1
                usersCount = len(keys)
                
                #send ok to client
                packet      = packets.ClientPacket(consts.S_MSG_ADD_TO_LEADEBOARD)
                packet.data = struct.pack("<II", userPosition, usersCount)
                return packet
        except Exception as e:
            cfunctions.Log_Error("Leaderboard.HandleAddToLeaderBoard: " + str(e))

    def HandleGetLeaderboard(self, data):
        """ Handle C_MSG_GET_LEADERBOARD opcode """
        try:
            #get gameID (levelID), userID
            gameID, userID = struct.unpack_from("<HQ", data)
            #save time when showed leadeboard
            self.timeDict[userID] = time.time()
            #create JSON per active levels
            jsonDict = {}
            jsonDict[gameID]                                = self.CreateLeaderboardJSON(gameID, userID)
            jsonDict[self.lastDailyChallengeActiveLevel]    = self.CreateLeaderboardJSON(self.lastDailyChallengeActiveLevel + 10000, userID)
            #dump to JSON
            jsonStr = json.dumps(jsonDict)
            #GZIP compress
            encoder = zlib.compressobj(8, zlib.DEFLATED, 16+zlib.MAX_WBITS, zlib.DEF_MEM_LEVEL, 0)
            jsonStrGZIP = encoder.compress(jsonStr)
            jsonStrGZIP += encoder.flush()
            #create packet + send
            packet      = packets.ClientPacket(consts.S_MSG_GET_LEADERBOARD)
            packet.data = jsonStrGZIP
            return packet
        except Exception as e:
            cfunctions.Log_Error("Leaderboard.HandleGetLeaderboard: " + str(e))

    def HandleGetDailyChallenge(self, data):
        """ Handle C_MSG_GET_DAILY_CHALLENGE opcode  """
        try:
            #get active level
            level = 0
            #
            if len(self.dailyChallengeActiveLevels) != 0:
                level = int(self.dailyChallengeActiveLevels[0])
            
            #create and send packet
            packet      = packets.ClientPacket(consts.S_MSG_GET_DAILY_CHALLENGE)
            packet.data = struct.pack("<HHI", level, self.lastDailyChallengeActiveLevel, abs(self.remainingTime))
            return packet
        except Exception as e:
            cfunctions.Log_Error("Leaderboard.HandleGetDailyChallenge: " + str(e))



























