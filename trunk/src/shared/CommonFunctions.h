/*
 * Game server
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

#ifndef COMMONFUNCTIONS_H
#define COMMONFUNCTIONS_H

vector<string> StrSplit(const string & src, const string & sep);

void replace(std::string &str, const char* find, const char* rep, uint32 limit = 0);

long Sync_Add(volatile long* value);
long Sync_Sub(volatile long* value);
void Sync_Set(volatile long* value, long setValue);

string FormatOutputString(const char * Prefix, const char * Description, bool useTimeStamp);

inline bool isWhiteSpace(const unsigned char c) 
{
    return ::isspace(c) != 0;
}

inline bool isNewline(const unsigned char c) 
{
    return (c == '\n') || (c == '\r');
}

inline bool isDigit(const unsigned char c) 
{
    return ::isdigit(c) != 0;
}

inline bool isLetter(const unsigned char c) 
{
    return ::isalpha(c) != 0;
}

inline bool isSlash(const char c) 
{
    return (c == '\\') || (c == '/');
}

inline bool isQuote(const char c) 
{
    return (c == '\'') || (c == '\"');
}

inline uint32 hex2number(const char *pHexNumber)
{
    uint32 ret;
    stringstream ss;
    ss << std::hex << pHexNumber;
    ss >> ret;
    return ret;
}

/// Fastest Method of float2int32
static inline int float2int32(const float value)
{
#if !defined(X64) && defined(WIN32)
	int i;
	__asm {
		fld value
		frndint
		fistp i
	}
	return i;
#else
	union { int asInt[2]; double asDouble; } n;
	n.asDouble = value + 6755399441055744.0;

#if USING_BIG_ENDIAN
	return n.asInt [1];
#else
	return n.asInt [0];
#endif
#endif
}

inline static unsigned int MakeIP(const char * str)
{
    // convert the input IP address to an integer
    unsigned int address = inet_addr(str);
	return address;
}

void SetThreadName(const char* format, ...);

#endif
