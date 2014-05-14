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

#include "Log.h"

createFileSingleton(ScreenLog);

ScreenLog::ScreenLog() : m_pFileLog(NULL)
{
	m_log_level		= 3;
#ifdef WIN32	
	m_stderr_handle	= GetStdHandle(STD_ERROR_HANDLE);
	m_stdout_handle	= GetStdHandle(STD_OUTPUT_HANDLE);
#endif
}

ScreenLog::~ScreenLog()
{
    delete m_pFileLog;
}

void ScreenLog::CreateFileLog(std::string sFileName)
{
    CreateFileLog(sFileName.c_str());
}

void ScreenLog::CreateFileLog(const char * filename)
{
	delete m_pFileLog;
    m_pFileLog = new FileLog(filename, true);
}

void ScreenLog::Color(unsigned int color)
{
#ifndef WIN32
#ifndef MAC
		static const char* colorstrings[TBLUE+1] = {
			"",
				"\033[22;31m",
				"\033[22;32m",
				"\033[01;33m",
				//"\033[22;37m",
				"\033[0m",
				"\033[01;37m",
				"\033[1;34m",
		};
		fputs(colorstrings[color], stdout);
#endif
#else	
	SetConsoleTextAttribute(m_stdout_handle, (WORD)color);
#endif
}

void ScreenLog::Time()
{
	printf("%02u:%02u:%02u ", (uint32)g_localTime.tm_hour, (uint32)g_localTime.tm_min, (uint32)g_localTime.tm_sec);
}

void ScreenLog::Line()
{
	LOCK_LOG;
	putchar('\n');
	UNLOCK_LOG;
}

void ScreenLog::Notice(const char * source, const char * format, ...)
{
    if(m_log_level < 0)
        return;
    
    if(m_pFileLog)
    {
        //create message
        size_t len;
        char out[2048];
        
        va_list ap;
        va_start(ap, format);
        len = sprintf(out, "N %s: ", source);
        vsnprintf(&out[len], sizeof(out) - len, format, ap);
        va_end(ap);
        
        //write to disk
        m_pFileLog->write(out);
    }
    else
    {    
        /* notice is old loglevel 0/string */
        LOCK_LOG;
        va_list ap;
        va_start(ap, format);
        Time();
        fputs("N ", stdout);
        if(*source)
        {
            Color(TWHITE);
            fputs(source, stdout);
            putchar(':');
            putchar(' ');
            Color(TNORMAL);
        }
        
        vprintf(format, ap);
        putchar('\n');
        va_end(ap);
        Color(TNORMAL);
        UNLOCK_LOG;
    }
}

void ScreenLog::Warning(const char * source, const char * format, ...)
{
    if(m_log_level < 2)
        return;
    
    if(m_pFileLog)
    {
        //create message
        size_t len;
        char out[2048];
        
        va_list ap;
        va_start(ap, format);
        len = sprintf(out, "W %s: ", source);
        vsnprintf(&out[len], sizeof(out) - len, format, ap);
        va_end(ap);
        
        //write to disk
        m_pFileLog->write(out);
    }
    else
    {        
        /* warning is old loglevel 2/detail */
        LOCK_LOG;
        va_list ap;
        va_start(ap, format);
        Time();
        Color(TYELLOW);
        fputs("W ", stdout);
        if(*source)
        {
            Color(TWHITE);
            fputs(source, stdout);
            putchar(':');
            putchar(' ');
            Color(TYELLOW);
        }
        
        vprintf(format, ap);
        putchar('\n');
        va_end(ap);
        Color(TNORMAL);
        UNLOCK_LOG;
    }
}

void ScreenLog::Success(const char * source, const char * format, ...)
{
    if(m_log_level < 2)
        return;
    
    if(m_pFileLog)
    {
        //create message
        size_t len;
        char out[2048];
        
        va_list ap;
        va_start(ap, format);
        len = sprintf(out, "S %s: ", source);
        vsnprintf(&out[len], sizeof(out) - len, format, ap);
        va_end(ap);
        
        //write to disk
        m_pFileLog->write(out);
    }
    else
    {        
        LOCK_LOG;
        va_list ap;
        va_start(ap, format);
        Time();
        Color(TGREEN);
        fputs("S ", stdout);
        if(*source)
        {
            Color(TWHITE);
            fputs(source, stdout);
            putchar(':');
            putchar(' ');
            Color(TGREEN);
        }
        
        vprintf(format, ap);
        putchar('\n');
        va_end(ap);
        Color(TNORMAL);
        UNLOCK_LOG;
    }
}

void ScreenLog::Error(const char * source, const char * format, ...)
{
    if(m_log_level < 1)
        return;
    
    if(m_pFileLog)
    {
        //create message
        size_t len;
        char out[2048];
        
        va_list ap;
        va_start(ap, format);
        len = sprintf(out, "E %s: ", source);
        vsnprintf(&out[len], sizeof(out) - len, format, ap);
        va_end(ap);
        
        //write to disk
        m_pFileLog->write(out);
    }
    else
    {        
        LOCK_LOG;
        va_list ap;
        va_start(ap, format);
        Time();
        Color(TRED);
        fputs("E ", stdout);
        if(*source)
        {
            Color(TWHITE);
            fputs(source, stdout);
            putchar(':');
            putchar(' ');
            Color(TRED);
        }
        
        vprintf(format, ap);
        putchar('\n');
        va_end(ap);
        Color(TNORMAL);
        UNLOCK_LOG;
    }
}

void ScreenLog::Debug(const char * source, const char * format, ...)
{
    if(m_log_level < 3)
        return;
    
    if(m_pFileLog)
    {
        //create message
        size_t len;
        char out[2048];
        
        va_list ap;
        va_start(ap, format);
        len = sprintf(out, "D %s: ", source);
        vsnprintf(&out[len], sizeof(out) - len, format, ap);
        va_end(ap);
        
        //write to disk
        m_pFileLog->write(out);
    }
    else
    {
        LOCK_LOG;
        va_list ap;
        va_start(ap, format);
        Time();
        Color(TBLUE);
        fputs("D ", stdout);
        if(*source)
        {
            Color(TWHITE);
            fputs(source, stdout);
            putchar(':');
            putchar(' ');
            Color(TBLUE);
        }
        
        vprintf(format, ap);
        putchar('\n');
        va_end(ap);
        Color(TNORMAL);
        UNLOCK_LOG;
    }
}

void FileLog::write(const char* format, ...)
{
	if(!m_file)
		return;

	m_lock.lock();
	va_list ap;
	va_start(ap, format);
	char out[4096];

	tm aTm;
	time_t t = time(NULL);
	localtime(&t, &aTm);
	sprintf(out, "[%-4d-%02d-%02d %02d:%02d:%02d] ",aTm.tm_year+1900,aTm.tm_mon+1,aTm.tm_mday,aTm.tm_hour,aTm.tm_min,aTm.tm_sec);
	size_t l = strlen(out);
	vsnprintf(&out[l], sizeof(out) - l, format, ap);

	fprintf(m_file, "%s\n", out);
	fflush(m_file);

	va_end(ap);
	m_lock.unlock();
}

void FileLog::Open()
{
	m_file = fopen(m_filename.c_str(), "a");
	assert(m_file);
}

void FileLog::Close()
{
	if(!m_file) 
		return;

	fflush(m_file);
	fclose(m_file);
	m_file = NULL;
}

FileLog::FileLog(const char * filename, bool open) : m_file(NULL), m_filename(filename)
{
	if(open)
	{
		Open();
	}
}

FileLog::~FileLog()
{
	if(m_file)
	{
		Close();
	}
}
