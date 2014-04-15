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

#include "Defines.h"

vector<string> StrSplit(const string & src, const string & sep)
{
	std::vector<std::string>	tokens;
	std::string					item;
	std::stringstream			ss(src);
    
	while(std::getline(ss, item, sep[0]))
	{
		tokens.push_back(item);
	}
    
	return tokens;
}

void replace(std::string &str, const char* find, const char* rep, uint32 limit)
{
	uint32 i = 0;
	std::string::size_type pos = 0;
	while((pos = str.find(find, pos)) != std::string::npos)
	{
		str.erase(pos, strlen(find));
		str.insert(pos, rep);
		pos += strlen(rep);

		++i;
		if(limit != 0 && i == limit)
			break;
	}
}

long Sync_Add(volatile long* value)
{
#ifdef WIN32
	return InterlockedIncrement(value);
#else
	return __sync_add_and_fetch(value, 1);
#endif
}

long Sync_Sub(volatile long* value)
{
#ifdef WIN32	
	return InterlockedDecrement(value);
#else
	return __sync_sub_and_fetch(value, 1);
#endif
}

void Sync_Set(volatile long* value, long setValue)
{
#ifdef WIN32
    InterlockedExchange(value, setValue);
#else
    __sync_lock_test_and_set(value, setValue);
#endif    
}

string FormatOutputString(const char * Prefix, const char * Description, bool useTimeStamp)
{
	char p[MAX_PATH];
	memset(&p, 0, sizeof(p));
	tm *a;
	time_t t = time(NULL);
	a = gmtime(&t);
	strcat(p, Prefix);
	strcat(p, "/");
	strcat(p, Description);
	if(useTimeStamp)
	{
		char ftime[128];
		memset(&ftime, 0, sizeof(ftime));
		snprintf(ftime, sizeof(ftime), "-%-4d-%02d-%02d %02d-%02d-%02d", a->tm_year+1900, a->tm_mon+1, a->tm_mday, a->tm_hour, a->tm_min, a->tm_sec);
		strcat(p, ftime);
	}

	strcat(p, ".log");
	return string(p);
}

void SetThreadName(const char* format, ...)
{
	// This isn't supported on nix?
	va_list ap;
	va_start(ap, format);
    
	char thread_name[256];
	vsnprintf(thread_name, sizeof(thread_name), format, ap);
    
#ifdef WIN32
#pragma pack(push,8)
	typedef struct tagTHREADNAME_INFO
	{
		DWORD dwType;		// must be 0x1000
		LPCSTR szName;		// pointer to name (in user addr space)
		DWORD dwThreadID;	// thread ID (-1=caller thread)
		DWORD dwFlags;		// reserved for future use, must be zero
	} THREADNAME_INFO;
#pragma pack(pop)

	THREADNAME_INFO info;
	info.dwType = 0x1000;
	info.dwThreadID = GetCurrentThreadId();
	info.dwFlags = 0;
	info.szName = thread_name;

	__try
	{
#ifdef X64
		RaiseException(0x406D1388, 0, sizeof(info)/sizeof(DWORD), (ULONG_PTR*)&info);
#else
		RaiseException(0x406D1388, 0, sizeof(info)/sizeof(DWORD), (DWORD*)&info);
#endif
	}
	__except(EXCEPTION_CONTINUE_EXECUTION)
	{

	}
#else
#ifdef DEBUG
    pthread_setname_np(thread_name);
#endif
#endif

	va_end(ap);
}






















