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

#ifndef TOKENIZER_H
#define TOKENIZER_H

static const char * g_keyword_table[] =
{
	"AND", "ALL", "AVG", "BY", "BETWEEN", "COUNT", "CASEWHEN",
	"DISTINCT", "EXISTS", "EXCEPT", "FALSE", "FROM",
	"GROUP", "IF", "INTO", "IFNULL", "IS", "IN", "INTERSECT", "INNER",
	"LEFT", "LIKE", "MAX", "MIN", "NULL", "NOT", "ON", "ORDER", "OR",
	"OUTER", "PRIMARY", "SELECT", "SET", "SUM", "TO", "TRUE",
	"UNIQUE", "UNION", "VALUES", "WHERE", "CONVERT", "CAST",
	"CONCAT", "MINUS", "CALL"
};

class Tokenizer
{
public:
	Tokenizer();
	static void FillKeyword();

	void Init(const std::string & sQuery);
	void back();
	void getThis(const std::string & match);
	bool wasValue();
	bool wasLongName();
	bool wasName();
	std::string getLongNameFirst();
	std::string getLongNameLast();
	std::string getName();
	std::string const & getstring();
	uint32 getType();
	uint32 getPosition();
	uint32 getSize();
	std::string getPart(uint32 begin, uint32 end);
	std::string const & getCommand() { return m_sCommand; }

private:
	void getToken();

	static const int NAME = 1, LONG_NAME = 2, SPECIAL = 3, NUMBER = 4, FLOAT = 5, STRING = 6, LONG = 7;
	static const int QUOTED_IDENTIFIER = 9, REMARK_LINE = 10, REMARK = 11;

	std::string		m_sCommand;
	uint32			m_Length;
	uint32			m_Index;
	uint32			m_Type;
	std::string		m_sToken;
	std::string		m_sLongNameFirst;
	std::string		m_sLongNameLast;
	bool			m_bWait;
};

#endif