/*
Copyright (C) 1996-2001 Id Software, Inc.
Copyright (C) 2002-2009 John Fitzgibbons and others
Copyright (C) 2010-2014 QuakeSpasm developers

This program is free software; you can redistribute it and/or
modify it under the terms of the GNU General Public License
as published by the Free Software Foundation; either version 2
of the License, or (at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.

See the GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program; if not, write to the Free Software
Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.

*/
// sv_user.c -- server code for moving users

#include "quakedef.h"

extern cvar_t sv_friction;
cvar_t sv_edgefriction = { "edgefriction", "2", CVAR_NONE };
extern cvar_t sv_stopspeed;

cvar_t sv_altnoclip = { "sv_altnoclip","1",CVAR_ARCHIVE }; //johnfitz

/*
==================
SV_UserFriction

==================
*/
void SV_UserFriction(edict_t* player)
{
	//vec3_t vel;
	float speed, newspeed, control;
	vec3_t start, stop;
	float friction;
	trace_t trace;

	//vel = velocity;

	speed = sqrt(player->v.velocity.x*player->v.velocity.x + player->v.velocity.y*player->v.velocity.y);
	if (!speed)
		return;

	// if the leading edge is over a dropoff, increase friction
	start.x = stop.x = player->v.origin.x + player->v.velocity.x/speed*16;
	start.y = stop.y = player->v.origin.y + player->v.velocity.y/speed*16;
	start.z = player->v.origin.z + player->v.mins.z;
	stop.z = start.z - 34;

	trace = SV_Move(start, Vec_Zero(), Vec_Zero(), stop, true, player);

	if (trace.fraction == 1.0)
		friction = sv_friction.value*sv_edgefriction.value;
	else
		friction = sv_friction.value;

	// apply friction
	control = speed < sv_stopspeed.value ? sv_stopspeed.value : speed;
	newspeed = speed - host_frametime*control*friction;

	if (newspeed < 0)
		newspeed = 0;
	newspeed /= speed;

	player->v.velocity.x = player->v.velocity.x*newspeed;
	player->v.velocity.y = player->v.velocity.y*newspeed;
}

/*
==============
SV_Accelerate
==============
*/
cvar_t sv_maxspeed = { "sv_maxspeed", "320", CVAR_NOTIFY|CVAR_SERVERINFO };
cvar_t sv_accelerate = { "sv_accelerate", "10", CVAR_NONE };
void SV_Accelerate(edict_t* player, float wishspeed, const vec3_t wishdir)
{
	float addspeed, accelspeed, currentspeed;

	currentspeed = Vec_Dot(player->v.velocity, wishdir);
	addspeed = wishspeed - currentspeed;
	if (addspeed <= 0)
		return;
	accelspeed = sv_accelerate.value*host_frametime*wishspeed;
	if (accelspeed > addspeed)
		accelspeed = addspeed;

	player->v.velocity = Vec_MulAdd(player->v.velocity, accelspeed, wishdir);
}
/*
void SV_AirAccelerate(float wishspeed, vec3_t wishveloc)
{
	int i;
	float addspeed, wishspd, accelspeed, currentspeed;
	return;
	wishspd = Vec_Normalize(&wishveloc);
	if (wishspd > 30)
		wishspd = 30;
	currentspeed = Vec_Dot(*velocity, wishveloc);
	addspeed = wishspd - currentspeed;
	if (addspeed <= 0)
		return;
	// accelspeed = sv_accelerate.value * host_frametime;
	accelspeed = sv_accelerate.value*wishspeed * host_frametime;
	if (accelspeed > addspeed)
		accelspeed = addspeed;

	for (i = 0; i<3; i++)
		velocity->values[i] += accelspeed*wishveloc.values[i];
}
*/

void DropPunchAngle(edict_t* player)
{
	float len;

	len = Vec_Normalize(&player->v.punchangle);

	len -= 10*host_frametime;
	if (len < 0)
		len = 0;
	player->v.punchangle = Vec_Scale(player->v.punchangle, len);
}

/*
===================
SV_WaterMove

===================
*/
void SV_WaterMove(edict_t* player, usercmd_t cmd)
{
	int i;
	vec3_t wishvel;
	float speed, newspeed, wishspeed, addspeed, accelspeed;

	//
	// user intentions
	//
	vec3_t forward, right, up;
	Vec_AngleVectors(player->v.v_angle, &forward, &right, &up);

	for (i = 0; i<3; i++)
		wishvel.values[i] = forward.values[i]*cmd.forwardmove + right.values[i]*cmd.sidemove;

	if (!cmd.forwardmove && !cmd.sidemove && !cmd.upmove)
		wishvel.z -= 60; // drift towards bottom
	else
		wishvel.z += cmd.upmove;

	wishspeed = Vec_Length(wishvel);
	if (wishspeed > sv_maxspeed.value)
	{
		wishvel = Vec_Scale(wishvel, sv_maxspeed.value/wishspeed);
		wishspeed = sv_maxspeed.value;
	}
	wishspeed *= 0.7;

	//
	// water friction
	//
	speed = Vec_Length(player->v.velocity);
	if (speed)
	{
		newspeed = speed - host_frametime * speed * sv_friction.value;
		if (newspeed < 0)
			newspeed = 0;
		player->v.velocity = Vec_Scale(player->v.velocity, newspeed/speed);
	}
	else
		newspeed = 0;

	//
	// water acceleration
	//
	if (!wishspeed)
		return;

	addspeed = wishspeed - newspeed;
	if (addspeed <= 0)
		return;

	Vec_Normalize(&wishvel);
	accelspeed = sv_accelerate.value * wishspeed * host_frametime;
	if (accelspeed > addspeed)
		accelspeed = addspeed;

	player->v.velocity = Vec_MulAdd(player->v.velocity, accelspeed, wishvel);
}

void SV_WaterJump(edict_t* player)
{
	if (sv.time > player->v.teleport_time
		|| !player->v.waterlevel)
	{
		player->v.flags = (int)player->v.flags & ~FL_WATERJUMP;
		player->v.teleport_time = 0;
	}
	player->v.velocity.x = player->v.movedir.x;
	player->v.velocity.y = player->v.movedir.y;
}

/*
===================
SV_NoclipMove -- johnfitz

new, alternate noclip. old noclip is still handled in SV_AirMove
===================
*/
void SV_NoclipMove(edict_t* player, usercmd_t cmd)
{
	vec3_t forward, right, up;
	Vec_AngleVectors(player->v.v_angle, &forward, &right, &up);

	player->v.velocity.x = forward.x*cmd.forwardmove + right.x*cmd.sidemove;
	player->v.velocity.y = forward.y*cmd.forwardmove + right.y*cmd.sidemove;
	player->v.velocity.z = forward.z*cmd.forwardmove + right.z*cmd.sidemove;
	player->v.velocity.z += cmd.upmove*2; //doubled to match running speed

	if (Vec_Length(player->v.velocity) > sv_maxspeed.value)
	{
		Vec_Normalize(&player->v.velocity);
		player->v.velocity = Vec_Scale(player->v.velocity, sv_maxspeed.value);
	}
}

/*
===================
SV_AirMove
===================
*/
void SV_AirMove(edict_t* player, usercmd_t cmd)
{
	int i;
	vec3_t wishvel, wishdir;
	float wishspeed;
	float fmove, smove;

	vec3_t forward, right, up;
	Vec_AngleVectors(player->v.angles, &forward, &right, &up);

	fmove = cmd.forwardmove;
	smove = cmd.sidemove;

	// hack to not let you back into teleporter
	if (sv.time < player->v.teleport_time && fmove < 0)
		fmove = 0;

	for (i = 0; i<3; i++)
		wishvel.values[i] = forward.values[i]*fmove + right.values[i]*smove;

	if ((int)player->v.movetype != MOVETYPE_WALK)
		wishvel.z = cmd.upmove;
	else
		wishvel.z = 0;

	wishdir = wishvel;
	wishspeed = Vec_Normalize(&wishdir);
	if (wishspeed > sv_maxspeed.value)
	{
		wishvel = Vec_Scale(wishvel, sv_maxspeed.value/wishspeed);
		wishspeed = sv_maxspeed.value;
	}

	if (player->v.movetype == MOVETYPE_NOCLIP)
	{ // noclip
		player->v.velocity = wishvel;
	}
	else if ((int)player->v.flags & FL_ONGROUND)
	{
		SV_UserFriction(player);
		SV_Accelerate(player, wishspeed, wishdir);
	}
	else
	{ // not on ground, so little effect on velocity
		SV_Accelerate(player, wishspeed*0.5, wishdir);
		//Con_Printf("Off ground %i\n", host_framecount);
	}
}

/*
===================
SV_ClientThink

the move fields specify an intended velocity in pix/sec
the angle fields specify an exact angular motion in degrees
===================
*/
void SV_ClientThink(edict_t* player, usercmd_t cmd)
{
	vec3_t v_angle;

	if (player->v.movetype == MOVETYPE_NONE)
		return;

	DropPunchAngle(player);

	//
	// if dead, behave differently
	//
	if (player->v.health <= 0)
		return;

	//
	// angles
	// show 1/3 the pitch angle and all the roll angle

	v_angle = Vec_Add(player->v.v_angle, player->v.punchangle);
	player->v.angles.roll = V_CalcRoll(player->v.angles, player->v.velocity)*4;
	if (!player->v.fixangle)
	{
		player->v.angles.pitch = -v_angle.pitch/3;
		player->v.angles.yaw = v_angle.yaw;
	}

	if ((int)player->v.flags & FL_WATERJUMP)
	{
		SV_WaterJump(player);
		return;
	}
	//
	// walk
	//
		//johnfitz -- alternate noclip
	if (player->v.movetype == MOVETYPE_NOCLIP && sv_altnoclip.value)
		SV_NoclipMove(player, cmd);
	else if (player->v.waterlevel >= 2 && player->v.movetype != MOVETYPE_NOCLIP)
		SV_WaterMove(player, cmd);
	else
		SV_AirMove(player, cmd);
	//johnfitz
}


/*
===================
SV_ReadClientMove
===================
*/
void SV_ReadClientMove(usercmd_t* move)
{
	int i;
	vec3_t angle;
	int bits;

	// read ping time
	host_client->ping_times[host_client->num_pings%NUM_PING_TIMES]
		= sv.time - MSG_ReadFloat();
	host_client->num_pings++;

	// read current angles
	for (i = 0; i<3; i++)
		//johnfitz -- 16-bit angles for PROTOCOL_FITZQUAKE
		if (sv.protocol == PROTOCOL_NETQUAKE)
			angle.values[i] = MSG_ReadAngle(sv.protocolflags);
		else
			angle.values[i] = MSG_ReadAngle16(sv.protocolflags);
	//johnfitz

	host_client->edict->v.v_angle = angle;

	// read movement
	move->forwardmove = MSG_ReadShort();
	move->sidemove = MSG_ReadShort();
	move->upmove = MSG_ReadShort();

	// read buttons
	bits = MSG_ReadByte();
	host_client->edict->v.button0 = bits & 1;
	host_client->edict->v.button2 = (bits & 2)>>1;

	i = MSG_ReadByte();
	if (i)
		host_client->edict->v.impulse = i;
}

/*
===================
SV_ReadClientMessage

Returns false if the client should be killed
===================
*/
bool SV_ReadClientMessage(void)
{
	int ret;
	int ccmd;
	const char* s;

	do
	{
	nextmsg:
		ret = NET_GetMessage(host_client->netconnection);
		if (ret == -1)
		{
			Sys_Printf("SV_ReadClientMessage: NET_GetMessage failed\n");
			return false;
		}
		if (!ret)
			return true;

		MSG_BeginReading();

		while (1)
		{
			if (!host_client->active)
				return false; // a command caused an error

			if (msg_badread)
			{
				Sys_Printf("SV_ReadClientMessage: badread\n");
				return false;
			}

			ccmd = MSG_ReadChar();

			switch (ccmd)
			{
			case -1:
				goto nextmsg; // end of message

			default:
				Sys_Printf("SV_ReadClientMessage: unknown command char\n");
				return false;

			case clc_nop:
				// Sys_Printf ("clc_nop\n");
				break;

			case clc_stringcmd:
				s = MSG_ReadString();
				ret = 0;
				if (q_strncasecmp(s, "status", 6) == 0)
					ret = 1;
				else if (q_strncasecmp(s, "god", 3) == 0)
					ret = 1;
				else if (q_strncasecmp(s, "notarget", 8) == 0)
					ret = 1;
				else if (q_strncasecmp(s, "fly", 3) == 0)
					ret = 1;
				else if (q_strncasecmp(s, "name", 4) == 0)
					ret = 1;
				else if (q_strncasecmp(s, "noclip", 6) == 0)
					ret = 1;
				else if (q_strncasecmp(s, "setpos", 6) == 0)
					ret = 1;
				else if (q_strncasecmp(s, "say", 3) == 0)
					ret = 1;
				else if (q_strncasecmp(s, "say_team", 8) == 0)
					ret = 1;
				else if (q_strncasecmp(s, "tell", 4) == 0)
					ret = 1;
				else if (q_strncasecmp(s, "color", 5) == 0)
					ret = 1;
				else if (q_strncasecmp(s, "kill", 4) == 0)
					ret = 1;
				else if (q_strncasecmp(s, "pause", 5) == 0)
					ret = 1;
				else if (q_strncasecmp(s, "spawn", 5) == 0)
					ret = 1;
				else if (q_strncasecmp(s, "begin", 5) == 0)
					ret = 1;
				else if (q_strncasecmp(s, "prespawn", 8) == 0)
					ret = 1;
				else if (q_strncasecmp(s, "kick", 4) == 0)
					ret = 1;
				else if (q_strncasecmp(s, "ping", 4) == 0)
					ret = 1;
				else if (q_strncasecmp(s, "give", 4) == 0)
					ret = 1;
				else if (q_strncasecmp(s, "ban", 3) == 0)
					ret = 1;

				if (ret == 1)
					Cmd_ExecuteString(s, src_client);
				else
					Con_DPrintf("%s tried to %s\n", host_client->name, s);
				break;

			case clc_disconnect:
				// Sys_Printf ("SV_ReadClientMessage: client disconnected\n");
				return false;

			case clc_move:
				SV_ReadClientMove(&host_client->cmd);
				break;
			}
		}
	} while (ret == 1);

	return true;
}


/*
==================
SV_RunClients
==================
*/
void SV_RunClients(void)
{
	int i;

	for (i = 0, host_client = svs.clients; i<svs.maxclients; i++, host_client++)
	{
		if (!host_client->active)
			continue;

		if (!SV_ReadClientMessage())
		{
			SV_DropClient(false); // client misbehaved...
			continue;
		}

		if (!host_client->spawned)
		{
			// clear client movement until a new packet is received
			memset(&host_client->cmd, 0, sizeof(host_client->cmd));
			continue;
		}

		// always pause in single player if in console or menus
		if (!sv.paused && (svs.maxclients > 1 || key_dest == key_game))
			SV_ClientThink(host_client->edict, host_client->cmd);
	}
}

