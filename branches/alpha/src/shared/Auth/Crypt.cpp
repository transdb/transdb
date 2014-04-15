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

#include "Crypt.h"

Crypt::Crypt()
{
	m_keyLen = strlen(g_pCryptKey);
	m_send_i = 0;
	m_send_j = 0;
	m_recv_i = 0;
	m_recv_j = 0;
}

void Crypt::DecryptRecv(uint8 * pData, size_t size)
{
	uint8 x;
	for(size_t t = 0;t < size;++t) 
	{
		m_recv_i %= m_keyLen;
		x = (pData[t] - m_recv_j) ^ g_pCryptKey[m_recv_i];
		++m_recv_i;
		m_recv_j = pData[t];
		pData[t] = x;
	}
}

void Crypt::EncryptSend(uint8 * pData, size_t size)
{
	for(size_t t = 0;t < size;++t) 
	{
		m_send_i %= m_keyLen;
		pData[t] = m_send_j = (pData[t] ^ g_pCryptKey[m_send_i]) + m_send_j;
        ++m_send_i;
	}
}