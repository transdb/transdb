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

#ifndef TRACE_H
#define TRACE_H

#define array_elements(A) ((uint32)(sizeof(A) / sizeof(A[0])))

class Exception
{
public:
	Exception(uint32 errNumber, const std::string & sErrMessage);

	const std::string & getErrMessage()	{ return m_sErrMessage; }
	const uint32 & getErrNumber()       { return m_ErrNumber; }
	const std::string & getSqlState()	{ return m_sSqlState; }

private:
	const char * getSqlStatePriv(uint32 errNumber);

	std::string m_sErrMessage;
	std::string m_sSqlState;
	uint32      m_ErrNumber;
};

class Trace
{
public:
	static void check(bool oCondition, uint32 errNumber);
	static void check(bool oCondition, uint32 errNumber, const std::string & add);
	static Exception getError(uint32 errNumber, const std::string & add);

	static void LoadErrMessages();

private:
	static uint32 getErrNumberByErrName(const std::string & sErrName);
	static const std::string & getErrMessagePriv(uint32 errNumber);
};

#endif