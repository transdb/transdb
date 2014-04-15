/*
 * Game Server
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
#ifndef _CRYPT_H
#define _CRYPT_H

#include "../Defines.h"

static const char * g_pCryptKey = "T2%o9^24C2r14}:p63zU";

class Crypt 
{
public:
	Crypt();

	void DecryptRecv(uint8 * pData, size_t size);
	void EncryptSend(uint8 * pData, size_t size);

private:
	uint8	m_send_i;
	uint8	m_send_j;
	uint8	m_recv_i;
	uint8	m_recv_j;
	size_t	m_keyLen;
};

#endif
