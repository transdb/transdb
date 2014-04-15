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

#ifndef ENUMS_H
#define ENUMS_H

namespace SocketEnums
{
	enum OUTPACKET_RESULT
	{
		OUTPACKET_RESULT_SUCCESS			= 1,
		OUTPACKET_RESULT_NO_ROOM_IN_BUFFER	= 2,
		OUTPACKET_RESULT_NOT_CONNECTED		= 3,
		OUTPACKET_RESULT_SOCKET_ERROR		= 4
	};

	enum PacketFlags
	{
		ePF_NULL							= 0,
		ePF_COMPRESSED						= 1
	};

	enum Opcodes
	{
		OP_NULL								= 0,

        C_MSG_WRITE_DATA                    = 1,
        S_MSG_WRITE_DATA                    = 2,
        
        C_MSG_READ_DATA                     = 3,
        S_MSG_READ_DATA                     = 4,

		C_MSG_DELETE_DATA   				= 5,
		S_MSG_DELETE_DATA                   = 6,
        
        C_MSG_GET_ALL_X                     = 7,
        S_MSG_GET_ALL_X                     = 8,
        
        C_MSG_PONG                          = 9,
        S_MSG_PING                          = 10,
        
        C_MSG_GET_ALL_Y                     = 11,
        S_MSG_GET_ALL_Y                     = 12,

		C_MSG_STATUS						= 13,
		S_MSG_STATUS						= 14,
        
        C_MSG_GET_ACTIVITY_ID               = 15,
        S_MSG_GET_ACTIVITY_ID               = 16,

		OP_NUM
	};
}

#endif