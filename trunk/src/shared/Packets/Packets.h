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

#ifndef PACKETS_H
#define PACKETS_H

#include "../Defines.h"
#include "ByteBuffer.h"
#include "StackBuffer.h"

class Packet : public ByteBuffer
{
public:
    INLINE Packet() : ByteBuffer(), m_opcode(0), m_bufferPool(-1) { }
	INLINE Packet(const uint16 &opcode, const size_t &res) : ByteBuffer(res), m_opcode(opcode), m_bufferPool(-1) {}
	INLINE Packet(const size_t &res) : ByteBuffer(res), m_opcode(0), m_bufferPool(-1) { }
    INLINE Packet(const Packet &packet) : ByteBuffer(packet), m_opcode(packet.m_opcode), m_bufferPool(-1) {}

    //! Clear packet and set opcode all in one mighty blow
    INLINE void Initialize(const uint16 &opcode, size_t newres = 200)
    {
        clear();
		m_storage.reserve(newres);
        m_opcode = opcode;
    }

    INLINE const uint16 &GetOpcode() const          { return m_opcode; }
    INLINE void SetOpcode(const uint16 &opcode)     { m_opcode = opcode; }

protected:
    uint16 m_opcode;

public:
	int8 m_bufferPool;
};

class StackPacket : public StackBuffer
{
public:
	INLINE StackPacket(const uint16 &opcode, uint8* ptr, const size_t &sz) : StackBuffer(ptr, sz), m_opcode(opcode) { }

	//! Clear packet and set opcode all in one mighty blow
	INLINE void Initialize(const uint16 &opcode)
	{
		StackBuffer::Clear();
		m_opcode = opcode;
	}

    INLINE const uint16 &GetOpcode() const          { return m_opcode; }
    INLINE void SetOpcode(const uint16 &opcode)     { m_opcode = opcode; }

protected:
	uint16 m_opcode;
};

#endif
