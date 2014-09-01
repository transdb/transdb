/*
 * EasySQL Server
 * Copyright (C) 2010 Miroslav 'Wayland' Kudrnac
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU Affero General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU Affero General Public License for more details.
 *
 * You should have received a copy of the GNU Affero General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 *
 */

#include "StdAfx.h"

static std::map<uint32, std::string> g_errtext_map;

struct st_map_errno_to_sqlstate
{
	uint32			m_mysql_errno;
	const char *	m_odbc_state;
	const char *	m_jdbc_state;
};

struct st_map_errtxt_to_errno
{
	const char *	m_mysql_err_text;
	uint32			m_mysql_errno;
};

struct st_map_errtxt_to_errno errtxt_map[] =
{
	#include "mysqld_ername.h"
};

struct st_map_errno_to_sqlstate sqlstate_map[] =
{
	#include "sql_state.h"
};

Exception::Exception(uint32 errNumber, const std::string & sErrMessage)
{
	m_ErrNumber		= errNumber;
	m_sErrMessage	= sErrMessage;
	m_sSqlState		= getSqlStatePriv(errNumber);
}

const char * Exception::getSqlStatePriv(uint32 errNumber)
{
	uint32 first					= 0;
	uint32 end						= array_elements(sqlstate_map) - 1;
	uint32 mid						= 0;
	st_map_errno_to_sqlstate * map	= NULL;

	/* Do binary search in the sorted array */
	while(first != end)
	{
		mid = (first + end) / 2;
		map = sqlstate_map + mid;
		if(map->m_mysql_errno < errNumber)
			first = mid + 1;
		else
			end = mid;
	}
	map = sqlstate_map + first;

	if(map->m_mysql_errno == errNumber)
		return map->m_odbc_state;
	else
		return "HY000";						/* General error */
}

void Trace::check(bool oCondition, uint32 errNumber)
{
	check(oCondition, errNumber, "");
}

void Trace::check(bool oCondition, uint32 errNumber, const std::string & add)
{
	if(oCondition == false)
	{
		throw getError(errNumber, add);
	}
}

Exception Trace::getError(uint32 errNumber, const std::string & add)
{
	std::string sErrMessage = getErrMessagePriv(errNumber);
	if(add.size() && sErrMessage.size())
	{
		sErrMessage.resize(sErrMessage.size() + add.size());
		//sprintf_s((char*)sErrMessage.data(), sErrMessage.capacity(), sErrMessage.c_str(), add.c_str(), errNumber);
	}
	return Exception(errNumber, sErrMessage);
}

const std::string & Trace::getErrMessagePriv(uint32 errNumber)
{
	std::map<uint32, std::string>::const_iterator itr = g_errtext_map.find(errNumber);
	assert(itr != g_errtext_map.end());
	return itr->second;
}

uint32 Trace::getErrNumberByErrName(const std::string & sErrName)
{
	st_map_errtxt_to_errno * map = NULL;
	for(uint32 i = 0;i < array_elements(errtxt_map);++i)
	{
		map = errtxt_map + i;
		if(sErrName.compare(map->m_mysql_err_text) == 0)
		{
			return map->m_mysql_errno;
		}
	}
	return -1;
}

void Trace::LoadErrMessages()
{
	Log.Notice(__FUNCTION__, "Loading error messages.");

    std::ifstream rStream("w:\\SVN\\EasySQL\\src\\EasySQLShared\\errmsg.txt");
	assert(rStream.is_open());

	std::string sLine;
	std::vector<std::string> sTokens;
	std::vector<std::string> sTokensMain;

	while(rStream.eof() != true)
	{
		//get line
		getline(rStream, sLine);
		
		//try get main token
		sTokensMain.clear();
        sTokensMain = CommonFunctions::StrSplit(sLine, " ");
		if(sTokensMain.empty())
			continue;

		sLine = sTokensMain.at(0);
		
		//get err number by text
		uint32 errNumber = getErrNumberByErrName(sLine);
		if(errNumber != -1)
		{
			do
			{
				bool oBreak = false;
				getline(rStream, sLine);
				
				sTokens.clear();
				sTokens = CommonFunctions::StrSplit(sLine, "\"");

				for(std::vector<std::string>::iterator itr = sTokens.begin();itr != sTokens.end();++itr)
				{
					//trim
					itr->erase(itr->find_last_not_of(" ") + 1);
					itr->erase(0, itr->find_first_not_of(" "));

					//load english only
					if(itr->compare("eng") == 0)
					{
						g_errtext_map[errNumber] = (*++itr);
						oBreak = true;
						break;
					}
				}

				if(oBreak)
				{
					break;
				}

				//try get main token
				sTokensMain.clear();
				sTokensMain = CommonFunctions::StrSplit(sLine, " ");
				if(sTokensMain.empty())
					continue;

				sLine = sTokensMain.at(0);

			}while(getErrNumberByErrName(sLine) == -1);
		}
	}

	rStream.close();
}