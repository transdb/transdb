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

static INLINE bool isWhiteSpace(const char c)
{
    return isspace(c) != 0;
}

static INLINE bool isNewline(const char c)
{
    return (c == '\n') || (c == '\r');
}

static INLINE bool isDigit(const char c)
{
    return isdigit(c) != 0;
}

static INLINE bool isLetter(const char c)
{
    return isalpha(c) != 0;
}

static INLINE bool isSlash(const char c)
{
    return (c == '\\') || (c == '/');
}

static INLINE bool isQuote(const char c)
{
    return (c == '\'') || (c == '\"');
}

static std::unordered_map<std::string, uint32> g_keyword_map;

Tokenizer::Tokenizer()
{
	m_Index			= 0;
	m_Type			= 0;
	m_bWait			= false;
}

void Tokenizer::Init(const std::string & sQuery)
{
	m_sCommand			= sQuery;
	m_Length			= (uint32)m_sCommand.size();
	m_Index				= 0;
	m_Type				= 0;
	m_bWait				= false;
	m_sToken			= "";
	m_sLongNameFirst	= "";
	m_sLongNameLast		= "";
}

void Tokenizer::back()
{
	assert(m_bWait == false);
	m_bWait = true;
}

void Tokenizer::getThis(const std::string & match)
{
	getToken();

	if(stricmp(m_sToken.c_str(), match.c_str()) != 0)
	{
		Trace::check(false, ER_PARSE_ERROR, m_sToken);
	}
}

bool Tokenizer::wasValue()
{
	if(m_Type == STRING || m_Type == NUMBER || m_Type == FLOAT)
		return true;

	if(stricmp(m_sToken.c_str(), "NULL") == 0)
		return true;

	if(stricmp(m_sToken.c_str(), "TRUE") == 0 || stricmp(m_sToken.c_str(), "FALSE") == 0)
		return true;

	return false;
}

bool Tokenizer::wasLongName()
{
	return m_Type == LONG_NAME;
}

bool Tokenizer::wasName()
{
	if(m_Type == QUOTED_IDENTIFIER)
		return true;

	if(m_Type != NAME) 
		return false;

	return g_keyword_map.find(m_sToken) != g_keyword_map.end();
}

std::string Tokenizer::getLongNameFirst()
{
	return m_sLongNameFirst;
}

std::string Tokenizer::getLongNameLast()
{
	return m_sLongNameLast;
}

std::string Tokenizer::getName()
{
	getToken();

	if(wasName() == false)
	{
		Trace::check(false, ER_PARSE_ERROR, m_sToken);
	}

	return m_sToken;
}

std::string const & Tokenizer::getstring()
{
	getToken();
	return m_sToken;
}

uint32 Tokenizer::getType()
{
	return 0;
}

uint32 Tokenizer::getPosition()
{
	return m_Index;
}

uint32 Tokenizer::getSize()
{
	return m_Length;
}

std::string Tokenizer::getPart(uint32 begin, uint32 end)
{
	return m_sCommand.substr(begin, end);
}

void Tokenizer::getToken()
{
	if(m_bWait)
	{
		m_bWait = false;
		return;
	}

	while(m_Index < m_Length && isWhiteSpace(m_sCommand[m_Index]))
	{
		++m_Index;
	}

	m_sToken = "";

	if(m_Index >= m_Length)
	{
		m_Type = 0;
		return;
	}

	bool point		= false; 
	bool digit		= false;
	bool exp		= false;
	bool afterexp	= false;
	bool end		= false;
	char c			= m_sCommand[m_Index];
	char cfirst		= '0';
    std::stringstream	name;

	if(isLetter(c))
	{
		m_Type = NAME;
	}
	else if(std::string("(),*=;+%").find(c) != std::string::npos)
	{
		m_Type = SPECIAL;
		++m_Index;
		m_sToken = c;
		return;
	}
	else if(isDigit(c))
	{
		m_Type = NUMBER;
		digit = true;
	}
	else if(std::string("!<>|/-").find(c) != std::string::npos)
	{
		cfirst = c;
		m_Type = SPECIAL;
	}
	//else if(c == '\"')
	//{
	//	m_Type = QUOTED_IDENTIFIER;
	//}
	else if(c == '\'' || c == '`' || c == '\"')
	{
		m_Type = STRING;
	}
	else if(c == '.')
	{
		m_Type = FLOAT;
		point = true;
	}
	else
	{
		Trace::check(false, ER_PARSE_ERROR, std::string("") + c);
		assert(false);
	}

	uint32 start = m_Index++;

	while(true)
	{
		if(m_Index >= m_Length)
		{
			c = ' ';
			end = true;
			Trace::check(m_Type != STRING && m_Type != QUOTED_IDENTIFIER, ER_PARSE_ERROR);
		}
		else
		{
			c = m_sCommand[m_Index];
		}

		switch(m_Type)
		{
		case NAME:
			{
				if(isLetter(c) || isDigit(c) || c == '_')
					break;

				m_sToken = m_sCommand.substr(start, (m_Index - start));

				if(c == '.')
				{
					m_sLongNameFirst = m_sToken;
					++m_Index;

					getToken();

					m_sLongNameLast = m_sToken;
					m_Type = LONG_NAME;
					m_sToken = m_sLongNameFirst + "." + m_sLongNameLast;
				}

				return;
			}break;
		//case QUOTED_IDENTIFIER:
		//	{
		//		if(c == '\"')
		//		{
		//			++m_Index;
		//			if(m_Index >= m_Length)
		//			{
		//				m_sToken = name.str();
		//				return;
		//			}

		//			c = m_sCommand[m_Index];

		//			if(c == '.')
		//			{
		//				m_sLongNameFirst = name.str();
		//				++m_Index;

		//				getToken();

		//				m_sLongNameLast = m_sToken;
		//				m_Type = LONG_NAME;
		//				m_sToken = m_sLongNameFirst + "." + m_sLongNameLast;
		//				return;
		//			}

		//			if(c == '\"')
		//			{
		//				m_sToken = name.str();
		//				return;
		//			}
		//		}

		//		name << c;

		//	}break;
		case STRING:
			{
				if(c == '\'' || c == '`' || c == '\"')
				{
					++m_Index;
					if(m_Index >= m_Length || m_sCommand[m_Index] != '\'' || m_sCommand[m_Index] != '`' || m_sCommand[m_Index] != '\"') 
					{
						m_sToken = name.str();
						return;
					}
				}

				name << c;

			}break;
		case REMARK:
			{
				if(end)
				{
					m_Type = 0;
					return;
				}
				else if(c == '*')
				{
					++m_Index;
					if(m_Index >= m_Length || m_sCommand[m_Index] != '\'') 
					{
						++m_Index;

						getToken();

						return;
					}
				}
			}break;
		case REMARK_LINE:
			{
				if(end)
				{
					m_Type = 0;
					return;
				}
				else if(c == '\r' || c == '\n')
				{
					getToken();

					return;
				}
			}break;
		case SPECIAL:
			{
				if(c == '/' && cfirst == '/')
				{
					m_Type = REMARK_LINE;
					break;
				}
				else if(c == '-' && cfirst == '-')
				{
					m_Type = REMARK_LINE;
					break;
				}
				else if(c == '*' && cfirst == '/')
				{
					m_Type = REMARK;
					break;
				}
				else if(std::string(">=|").find(c) != std::string::npos)
				{
					break;
				}

				m_sToken = m_sCommand.substr(start, (m_Index - start));
				return;

			}break;
		case FLOAT:
		case NUMBER:
			{
				if(isDigit(c))
				{
					digit = true;
				}
				else if(c == '.')
				{
					m_Type = FLOAT;

					if(point)
					{
						Trace::check(false, ER_PARSE_ERROR, ".");
					}

					point = true;
				}
				else if(c == 'E' || c == 'e')
				{
					if(exp)
					{
						Trace::check(false, ER_PARSE_ERROR, "E");
					}
					afterexp	= true;
					point		= true;
					exp			= true;
				}
				else if(c == '-' && afterexp)
				{
					afterexp = false;
				}
				else if(c == '+' && afterexp)
				{
					afterexp = false;
				}
				else
				{
					afterexp = false;

					if(digit == false)
					{
						if(point && start == m_Index - 1)
						{
							m_sToken = ".";
							m_Type = SPECIAL;
							return;
						}
						Trace::check(false, ER_PARSE_ERROR, std::string("") + c);
					}

					m_sToken = m_sCommand.substr(start, (m_Index - start));
					return;
				}
			}break;
		}
		++m_Index;
	}
}	

void Tokenizer::FillKeyword()
{
	for(uint32 i = 0;i < array_elements(g_keyword_table);++i)
	{
		g_keyword_map.insert(std::make_pair(g_keyword_table[i], i));
	}
}