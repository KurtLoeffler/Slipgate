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
// sv_phys.c

#include "quakedef.h"

/*


pushmove objects do not obey gravity, and do not interact with each other or trigger fields, but block normal movement and push normal objects when they move.

onground is set for toss objects when they come to a complete rest.  it is set for steping or walking objects

doors, plats, etc are SOLID_BSP, and MOVETYPE_PUSH
bonus items are SOLID_TRIGGER touch, and MOVETYPE_TOSS
corpses are SOLID_NOT and MOVETYPE_TOSS
crates are SOLID_BBOX and MOVETYPE_TOSS
walking monsters are SOLID_SLIDEBOX and MOVETYPE_STEP
flying/floating monsters are SOLID_SLIDEBOX and MOVETYPE_FLY

solid_edge items only clip against bsp models.

*/

cvar_t sv_friction = { "sv_friction","4",CVAR_NOTIFY|CVAR_SERVERINFO };
cvar_t sv_stopspeed = { "sv_stopspeed","100",CVAR_NONE };
cvar_t sv_gravity = { "sv_gravity","800",CVAR_NOTIFY|CVAR_SERVERINFO };
cvar_t sv_maxvelocity = { "sv_maxvelocity","2000",CVAR_NONE };
cvar_t sv_nostep = { "sv_nostep","0",CVAR_NONE };
cvar_t sv_freezenonclients = { "sv_freezenonclients","0",CVAR_NONE };


#define MOVE_EPSILON 0.01

void SV_Physics_Toss(edict_t* ent);

#define STEPSIZE 18

vec3_t _oldmins;
//vec3_t _oldmaxs;
vec3_t _oldabsmin;
vec3_t _oldsize;
void ShrinkStepHeight(edict_t* ent)
{
	_oldmins = ent->v.mins;
	//_oldmaxs = ent->v.maxs;
	_oldabsmin = ent->v.absmin;
	_oldsize = ent->v.size;
	ent->v.mins.z += STEPSIZE;
	//ent->v.maxs.z -= STEPSIZE/2.0;
	ent->v.absmin.z += STEPSIZE;
	ent->v.size.z -= STEPSIZE;
	//ent->v.origin.z += STEPSIZE/2.0;
}

void RestoreStepHeight(edict_t* ent)
{
	ent->v.mins.z = _oldmins.z;
	//ent->v.maxs.z = _oldmaxs.z;
	ent->v.absmin.z = _oldabsmin.z;
	ent->v.size.z = _oldsize.z;
	//ent->v.origin.z -= STEPSIZE/2.0;
}

/*
================
SV_CheckAllEnts
================
*/
void SV_CheckAllEnts(void)
{
	int e;
	edict_t* check;

	// see if any solid entities are inside the final position
	check = NEXT_EDICT(sv.edicts);
	for (e = 1; e<sv.num_edicts; e++, check = NEXT_EDICT(check))
	{
		if (!check->id)
			continue;
		if (check->v.movetype == MOVETYPE_PUSH
			|| check->v.movetype == MOVETYPE_NONE
			|| check->v.movetype == MOVETYPE_NOCLIP)
			continue;

		if (SV_TestEntityPosition(check))
			Con_Printf("entity in invalid position\n");
	}
}

/*
================
SV_CheckVelocity
================
*/
void SV_CheckVelocity(edict_t* ent)
{
	int i;

	//
	// bound velocity
	//
	for (i = 0; i<3; i++)
	{
		if (IS_NAN(ent->v.velocity.values[i]))
		{
			Con_Printf("Got a NaN velocity on %s\n", PR_GetString(ent->v.classname));
			ent->v.velocity.values[i] = 0;
		}
		if (IS_NAN(ent->v.origin.values[i]))
		{
			Con_Printf("Got a NaN origin on %s\n", PR_GetString(ent->v.classname));
			ent->v.origin.values[i] = 0;
		}
		if (ent->v.velocity.values[i] > sv_maxvelocity.value)
			ent->v.velocity.values[i] = sv_maxvelocity.value;
		else if (ent->v.velocity.values[i] < -sv_maxvelocity.value)
			ent->v.velocity.values[i] = -sv_maxvelocity.value;
	}
}

/*
=============
SV_RunThink

Runs thinking code if time.  There is some play in the exact time the think
function will be called, because it is called before any movement is done
in a frame.  Not used for pushmove objects, because they must be exact.
Returns false if the entity removed itself.
=============
*/
bool SV_RunThink(edict_t* ent)
{
	float thinktime;
	float oldframe; //johnfitz
	int i; //johnfitz

	thinktime = ent->v.nextthink;
	if (thinktime <= 0 || thinktime > sv.time + host_frametime)
		return true;

	if (thinktime < sv.time)
		thinktime = sv.time; // don't let things stay in the past.
								// it is possible to start that way
								// by a trigger with a local time.

	oldframe = ent->v.frame; //johnfitz

	ent->v.nextthink = 0;
	pr_global_struct->time = thinktime;
	pr_global_struct->self = EDICT_TO_PROG(ent);
	pr_global_struct->other = EDICT_TO_PROG(sv.edicts);
	PR_ExecuteProgram(ent->v.think);

	//johnfitz -- PROTOCOL_FITZQUAKE
	//capture interval to nextthink here and send it to client for better
	//lerp timing, but only if interval is not 0.1 (which client assumes)
	ent->sendinterval = false;
	if (ent->id && ent->v.nextthink && (ent->v.movetype == MOVETYPE_STEP || ent->v.frame != oldframe))
	{
		i = Q_rint((ent->v.nextthink-thinktime)*255);
		if (i >= 0 && i < 256 && i != 25 && i != 26) //25 and 26 are close enough to 0.1 to not send
			ent->sendinterval = true;
	}
	//johnfitz

	return ent->id != 0;
}

/*
==================
SV_Impact

Two entities have touched, so run their touch functions
==================
*/
void SV_Impact(edict_t* e1, edict_t* e2)
{
	int old_self, old_other;

	old_self = pr_global_struct->self;
	old_other = pr_global_struct->other;

	pr_global_struct->time = sv.time;
	if (e1->v.touch && e1->v.solid != SOLID_NOT)
	{
		pr_global_struct->self = EDICT_TO_PROG(e1);
		pr_global_struct->other = EDICT_TO_PROG(e2);
		PR_ExecuteProgram(e1->v.touch);
	}

	if (e2->v.touch && e2->v.solid != SOLID_NOT)
	{
		pr_global_struct->self = EDICT_TO_PROG(e2);
		pr_global_struct->other = EDICT_TO_PROG(e1);
		PR_ExecuteProgram(e2->v.touch);
	}

	pr_global_struct->self = old_self;
	pr_global_struct->other = old_other;
}


/*
==================
ClipVelocity

Slide off of the impacting object
returns the blocked flags (1 = floor, 2 = step / wall)
==================
*/
#define STOP_EPSILON 0.1

int ClipVelocity(vec3_t in, vec3_t normal, vec3_t* out, float overbounce)
{
	float backoff;
	float change;
	int i, blocked;

	blocked = 0;
	if (normal.z > 0)
		blocked |= 1; // floor
	if (!normal.z)
		blocked |= 2; // step

	backoff = Vec_Dot(in, normal) * overbounce;

	for (i = 0; i<3; i++)
	{
		change = normal.values[i]*backoff;
		out->values[i] = in.values[i] - change;
		if (out->values[i] > -STOP_EPSILON && out->values[i] < STOP_EPSILON)
			out->values[i] = 0;
	}

	return blocked;
}


/*
============
SV_FlyMove

The basic solid body movement clip that slides along multiple planes
Returns the clipflags if the velocity was modified (hit something solid)
1 = floor
2 = wall / step
4 = dead stop
If steptrace is not NULL, the trace of any vertical wall hit will be stored
============
*/
#define MAX_CLIP_PLANES 5
int SV_FlyMove(edict_t* ent, float time, float footoffset, trace_t* steptrace)
{
	int bumpcount, numbumps;
	vec3_t dir;
	float d;
	int numplanes;
	vec3_t planes[MAX_CLIP_PLANES];
	vec3_t primal_velocity, original_velocity, new_velocity;
	int i, j;
	trace_t trace;
	vec3_t end;
	float time_left;
	int blocked;

	numbumps = 4;//Fixing physics. Was 4;

	blocked = 0;
	original_velocity = ent->v.velocity;
	primal_velocity = ent->v.velocity;
	numplanes = 0;

	time_left = time;

	for (bumpcount = 0; bumpcount<numbumps; bumpcount++)
	{
		if (!ent->v.velocity.x && !ent->v.velocity.y && !ent->v.velocity.z)
			break;

		for (i = 0; i<3; i++)
			end.values[i] = ent->v.origin.values[i] + time_left * ent->v.velocity.values[i];

		vec3_t mins = ent->v.mins;
		vec3_t maxs = ent->v.maxs;
		mins.z += footoffset;

		trace = SV_Move(ent->v.origin, mins, maxs, end, false, ent);

		//This is somehow falsely reporting allsolid in low ceiling areas when mins is modified by footoffset...
		if (trace.allsolid && !footoffset)
		{ // entity is trapped in another solid
			ent->v.velocity = Vec_Zero();
			return 3;
		}

		if (trace.fraction > 0)
		{ // actually covered some distance
			ent->v.origin = trace.endpos;
			original_velocity = ent->v.velocity;
			numplanes = 0;
		}

		if (trace.fraction == 1)
			break; // moved the entire distance

		if (!trace.ent)
			Sys_Error("SV_FlyMove: !trace.ent");

		if (trace.plane.normal.z > 0.7)
		{
			blocked |= 1; // floor
			if (trace.ent->v.solid == SOLID_BSP)
			{
				ent->v.flags = (int)ent->v.flags | FL_ONGROUND;
				ent->v.groundentity = EDICT_TO_PROG(trace.ent);
			}
		}
		if (!trace.plane.normal.z)
		{
			blocked |= 2; // step
			if (steptrace)
				*steptrace = trace; // save for player extrafriction
		}

		//
		// run the impact function
		//
		SV_Impact(ent, trace.ent);
		if (!ent->id)
			break; // removed by the impact function


		time_left -= time_left * trace.fraction;

		// cliped to another plane
		if (numplanes >= MAX_CLIP_PLANES)
		{ // this shouldn't really happen
			ent->v.velocity = Vec_Zero();
			return 3;
		}

		planes[numplanes] = trace.plane.normal;
		numplanes++;

		//
		// modify original_velocity so it parallels all of the clip planes
		//
		for (i = 0; i<numplanes; i++)
		{
			ClipVelocity(original_velocity, planes[i], &new_velocity, 1);
			for (j = 0; j<numplanes; j++)
				if (j != i)
				{
					if (Vec_Dot(new_velocity, planes[j]) < 0)
						break; // not ok
				}
			if (j == numplanes)
				break;
		}

		if (i != numplanes)
		{ // go along this plane
			ent->v.velocity = new_velocity;
		}
		else
		{ // go along the crease
			if (numplanes != 2)
			{
				// Con_Printf ("clip velocity, numplanes == %i\n",numplanes);
				ent->v.velocity = Vec_Zero();
				return 7;
			}
			dir = Vec_Cross(planes[0], planes[1]);
			d = Vec_Dot(dir, ent->v.velocity);
			ent->v.velocity = Vec_Scale(dir, d);
		}

		//
		// if original velocity is against the original velocity, stop dead
		// to avoid tiny occilations in sloping corners
		//
		if (Vec_Dot(ent->v.velocity, primal_velocity) <= 0)
		{
			ent->v.velocity = Vec_Zero();
			break;
		}
	}

	return blocked;
}


/*
============
SV_AddGravity

============
*/
void SV_AddGravity(edict_t* ent)
{
	float ent_gravity;
	eval_t* val;

	val = GetEdictFieldValue(ent, "gravity");
	if (val && val->_float)
		ent_gravity = val->_float;
	else
		ent_gravity = 1.0;

	ent->v.velocity.z -= ent_gravity * sv_gravity.value * host_frametime;
}


/*
===============================================================================

PUSHMOVE

===============================================================================
*/

/*
============
SV_PushEntity

Does not change the entities velocity at all
============
*/
trace_t SV_PushEntity(edict_t* ent, vec3_t push)
{
	trace_t trace;
	vec3_t end;

	end = Vec_Add(ent->v.origin, push);

	if (ent->v.movetype == MOVETYPE_FLYMISSILE)
		trace = SV_Move(ent->v.origin, ent->v.mins, ent->v.maxs, end, MOVE_MISSILE, ent);
	else if (ent->v.solid == SOLID_TRIGGER || ent->v.solid == SOLID_NOT)
		// only clip against bmodels
		trace = SV_Move(ent->v.origin, ent->v.mins, ent->v.maxs, end, MOVE_NOMONSTERS, ent);
	else
		trace = SV_Move(ent->v.origin, ent->v.mins, ent->v.maxs, end, MOVE_NORMAL, ent);

	ent->v.origin = trace.endpos;
	SV_LinkEdict(ent, true);

	if (trace.ent)
		SV_Impact(ent, trace.ent);

	return trace;
}


/*
============
SV_PushMove
============
*/
void SV_PushMove(edict_t* pusher, float movetime)
{
	int i, e;
	edict_t* check, *block;
	vec3_t mins, maxs, move;
	vec3_t entorig, pushorig;
	int num_moved;
	edict_t **moved_edict; //johnfitz -- dynamically allocate
	vec3_t* moved_from; //johnfitz -- dynamically allocate
	int mark; //johnfitz

	if (!pusher->v.velocity.x && !pusher->v.velocity.y && !pusher->v.velocity.z)
	{
		pusher->v.ltime += movetime;
		return;
	}

	move = Vec_Scale(pusher->v.velocity, movetime);
	mins = Vec_Add(pusher->v.absmin, move);
	maxs = Vec_Add(pusher->v.absmax, move);

	pushorig = pusher->v.origin;

	// move the pusher to it's final position

	pusher->v.origin = Vec_Add(pusher->v.origin, move);
	pusher->v.ltime += movetime;
	SV_LinkEdict(pusher, false);

	//johnfitz -- dynamically allocate
	mark = Hunk_LowMark();
	moved_edict = (edict_t **)Hunk_Alloc(sv.num_edicts*sizeof(edict_t *));
	moved_from = (vec3_t *)Hunk_Alloc(sv.num_edicts*sizeof(vec3_t));
	//johnfitz

// see if any solid entities are inside the final position
	num_moved = 0;
	check = NEXT_EDICT(sv.edicts);
	for (e = 1; e<sv.num_edicts; e++, check = NEXT_EDICT(check))
	{
		if (!check->id)
			continue;
		if (check->v.movetype == MOVETYPE_PUSH
			|| check->v.movetype == MOVETYPE_NONE
			|| check->v.movetype == MOVETYPE_NOCLIP)
			continue;

		bool isstanding = ((int)check->v.flags & FL_ONGROUND) && PROG_TO_EDICT(check->v.groundentity) == pusher;
		// if the entity is standing on the pusher, it will definately be moved
		if (!isstanding)
		{
			if (check->v.absmin.x >= maxs.x
				|| check->v.absmin.y >= maxs.y
				|| check->v.absmin.z >= maxs.z
				|| check->v.absmax.x <= mins.x
				|| check->v.absmax.y <= mins.y
				|| check->v.absmax.z <= mins.z)
				continue;

			// see if the ent's bbox is inside the pusher's final position
			if (!SV_TestEntityPosition(check))
				continue;
		}

//#if 0
		// remove the onground flag for non-players
		if (check->v.movetype != MOVETYPE_WALK)
			check->v.flags = (int)check->v.flags & ~FL_ONGROUND;
//#endif

		entorig = check->v.origin;
		moved_from[num_moved] = check->v.origin;
		moved_edict[num_moved] = check;
		num_moved++;

		if (check->v.movetype == MOVETYPE_WALK)
		{

		}
		else
		{

		}
		
		// try moving the contacted entity
		pusher->v.solid = SOLID_NOT;
		SV_PushEntity(check, move);
		pusher->v.solid = SOLID_BSP;

		//check->v.mins = Vec_Add(check->v.mins, Vec(0, 0, -2));
		//check->v.maxs = Vec_Add(check->v.maxs, Vec(0, 0, 2));

		//SV_PushEntity(check, Vec_Zero());

		//check->v.mins = origmins;
		//check->v.maxs = origmaxs;

		//bool blkd = !Vec_Equal(startpos, check->v.origin);
		// if it is still inside the pusher, block
		//pusher->v.solid = SOLID_NOT;
		block = SV_TestEntityPosition(check);
		//pusher->v.solid = SOLID_BSP;
		//block = 0;

		if (block)
		{ 
			// fail the move
			if (check->v.mins.x == check->v.maxs.x)
				continue;
			if (check->v.solid == SOLID_NOT || check->v.solid == SOLID_TRIGGER)
			{ // corpse
				check->v.mins.x = check->v.mins.y = 0;
				check->v.maxs = check->v.mins;
				continue;
			}

			check->v.origin = entorig;
			SV_LinkEdict(check, true);

			pusher->v.origin = pushorig;
			SV_LinkEdict(pusher, false);
			pusher->v.ltime -= movetime;

			// if the pusher has a "blocked" function, call it
			// otherwise, just stay in place until the obstacle is gone
			if (pusher->v.blocked)
			{
				pr_global_struct->self = EDICT_TO_PROG(pusher);
				pr_global_struct->other = EDICT_TO_PROG(check);
				PR_ExecuteProgram(pusher->v.blocked);
			}

			// move back any entities we already moved
			for (i = 0; i<num_moved; i++)
			{
				moved_edict[i]->v.origin = moved_from[i];
				SV_LinkEdict(moved_edict[i], false);
			}
			Hunk_FreeToLowMark(mark); //johnfitz
			return;
		}

		//pusher->v.solid = SOLID_NOT;
		//SV_PushEntity(check, move);
		//pusher->v.solid = SOLID_BSP;
	}

	Hunk_FreeToLowMark(mark); //johnfitz

}

/*
================
SV_Physics_Pusher

================
*/
void SV_Physics_Pusher(edict_t* ent)
{
	float thinktime;
	float oldltime;
	float movetime;

	oldltime = ent->v.ltime;

	thinktime = ent->v.nextthink;
	if (thinktime < ent->v.ltime + host_frametime)
	{
		movetime = thinktime - ent->v.ltime;
		if (movetime < 0)
			movetime = 0;
	}
	else
		movetime = host_frametime;

	if (movetime)
	{
		SV_PushMove(ent, movetime); // advances ent->v.ltime if not blocked
	}

	if (thinktime > oldltime && thinktime <= ent->v.ltime)
	{
		ent->v.nextthink = 0;
		pr_global_struct->time = sv.time;
		pr_global_struct->self = EDICT_TO_PROG(ent);
		pr_global_struct->other = EDICT_TO_PROG(sv.edicts);
		PR_ExecuteProgram(ent->v.think);
		if (!ent->id)
			return;
	}

}


/*
===============================================================================

CLIENT MOVEMENT

===============================================================================
*/

/*
=============
SV_CheckStuck

This is a big hack to try and fix the rare case of getting stuck in the world
clipping hull.
=============
*/
void SV_CheckStuck(edict_t* ent)
{
	int i, j;
	int z;
	vec3_t org;

	if (!SV_TestEntityPosition(ent))
	{
		ent->v.oldorigin = ent->v.origin;
		return;
	}

	org = ent->v.origin;
	ent->v.origin = ent->v.oldorigin;
	if (!SV_TestEntityPosition(ent))
	{
		Con_DPrintf("Unstuck.\n");
		SV_LinkEdict(ent, true);
		return;
	}

	for (z = 0; z< 18; z++)
		for (i = -1; i <= 1; i++)
			for (j = -1; j <= 1; j++)
			{
				ent->v.origin.x = org.x + i;
				ent->v.origin.y = org.y + j;
				ent->v.origin.z = org.z + z;
				if (!SV_TestEntityPosition(ent))
				{
					Con_DPrintf("Unstuck.\n");
					SV_LinkEdict(ent, true);
					return;
				}
			}

	ent->v.origin = org;
	Con_DPrintf("player is stuck.\n");
}


/*
=============
SV_CheckWater
=============
*/
bool SV_CheckWater(edict_t* ent)
{
	vec3_t point;
	int cont;

	point.x = ent->v.origin.x;
	point.y = ent->v.origin.y;
	point.z = ent->v.origin.z + ent->v.mins.z + 1;

	ent->v.waterlevel = 0;
	ent->v.watertype = CONTENTS_EMPTY;
	cont = SV_PointContents(point);
	if (cont <= CONTENTS_WATER)
	{
		ent->v.watertype = cont;
		ent->v.waterlevel = 1;
		point.z = ent->v.origin.z + (ent->v.mins.z + ent->v.maxs.z)*0.5;
		cont = SV_PointContents(point);
		if (cont <= CONTENTS_WATER)
		{
			ent->v.waterlevel = 2;
			point.z = ent->v.origin.z + ent->v.view_ofs.z;
			cont = SV_PointContents(point);
			if (cont <= CONTENTS_WATER)
				ent->v.waterlevel = 3;
		}
	}

	return ent->v.waterlevel > 1;
}

/*
============
SV_WallFriction

============
*/
void SV_WallFriction(edict_t* ent, trace_t* trace)
{
	vec3_t forward, right, up;
	float d, i;
	vec3_t into, side;

	Vec_AngleVectors(ent->v.v_angle, &forward, &right, &up);
	d = Vec_Dot(trace->plane.normal, forward);

	d += 0.5;
	if (d >= 0)
		return;

	// cut the tangential velocity
	i = Vec_Dot(trace->plane.normal, ent->v.velocity);
	into = Vec_Scale(trace->plane.normal, i);
	side = Vec_Sub(ent->v.velocity, into);

	ent->v.velocity.x = side.x * (1 + d);
	ent->v.velocity.y = side.y * (1 + d);
}

/*
=====================
SV_TryUnstick

Player has come to a dead stop, possibly due to the problem with limited
float precision at some angle joins in the BSP hull.

Try fixing by pushing one pixel in each direction.

This is a hack, but in the interest of good gameplay...
======================
*/
int SV_TryUnstick(edict_t* ent, vec3_t oldvel)
{
	int i;
	vec3_t oldorg;
	vec3_t dir;
	int clip;
	trace_t steptrace;

	oldorg = ent->v.origin;
	dir = Vec_Zero();

	for (i = 0; i<8; i++)
	{
		// try pushing a little in an axial direction
		switch (i)
		{
		case 0: dir.x = 2; dir.y = 0; break;
		case 1: dir.x = 0; dir.y = 2; break;
		case 2: dir.x = -2; dir.y = 0; break;
		case 3: dir.x = 0; dir.y = -2; break;
		case 4: dir.x = 2; dir.y = 2; break;
		case 5: dir.x = -2; dir.y = 2; break;
		case 6: dir.x = 2; dir.y = -2; break;
		case 7: dir.x = -2; dir.y = -2; break;
		}

		SV_PushEntity(ent, dir);

		// retry the original move
		ent->v.velocity.x = oldvel.x;
		ent->v.velocity.y = oldvel.y;
		ent->v.velocity.z = 0;
		clip = SV_FlyMove(ent, 0.1, 0, &steptrace);

		if (fabs(oldorg.y - ent->v.origin.y) > 4
			|| fabs(oldorg.x - ent->v.origin.x) > 4)
		{
			//Con_DPrintf ("unstuck!\n");
			return clip;
		}

		// go back to the original pos and try again
		ent->v.origin = oldorg;
	}

	ent->v.velocity = Vec_Zero();
	return 7; // still not moving
}


trace_t Ground(edict_t* ent)
{
	//ShrinkStepHeight(ent);

	vec3_t footmins = ent->v.mins;
	vec3_t footmaxs = ent->v.maxs;
	footmins.z += STEPSIZE;
	footmaxs.z = footmins.z+STEPSIZE;
	vec3_t footstart = ent->v.origin;
	vec3_t footend = footstart;
	footend.z -= STEPSIZE;
	footend.z -= 0.1;

	//RestoreStepHeight(ent);

	trace_t downtrace = SV_Move(footstart, footmins, footmaxs, footend, MOVE_NORMAL, ent);

	if (downtrace.plane.normal.z > 0.7)
	{
		ent->v.flags = (int)ent->v.flags | FL_ONGROUND;
		ent->v.groundentity = EDICT_TO_PROG(downtrace.ent);

		float dist = Vec_Dot(ent->v.velocity, downtrace.plane.normal);
		if (dist < 0)
		{
			ent->v.velocity = Vec_Sub(ent->v.velocity, Vec_Scale(downtrace.plane.normal, dist));
		}
	}

	if (((int)ent->v.flags&FL_ONGROUND) && downtrace.fraction < 1)
	{
		ent->v.origin.z = downtrace.endpos.z;
		//ent->v.origin.z -= 1;
		ent->v.origin.z += STEPSIZE;
		SV_LinkEdict(ent, true);
	}
	return downtrace;
}
/*
=====================
SV_WalkMove

Only used by players
======================
*/

void SV_WalkMove(edict_t* ent)
{
	vec3_t upmove, downmove;
	vec3_t oldorg, oldvel;
	int clip;
	int oldonground;
	trace_t steptrace;

	oldorg = ent->v.origin;
	oldvel = ent->v.velocity;

	clip = SV_FlyMove(ent, host_frametime, 0, &steptrace);
	if (!(clip & 2))
	{
		Ground(ent);
		return; 
	}

	vec3_t nosteporg = ent->v.origin;
	vec3_t nostepvel = ent->v.velocity;

	ent->v.origin = oldorg;
	ent->v.velocity = oldvel;
	//ent->v.velocity.z = 0;

	//ShrinkStepHeight(ent);

	clip = SV_FlyMove(ent, host_frametime, STEPSIZE, &steptrace);

	if (clip & 2)
	{
		vec3_t ov = ent->v.velocity;
		SV_WallFriction(ent, &steptrace);
		ent->v.velocity = Vec_Lerp(ent->v.velocity, ov, 0.75);
	}

	//RestoreStepHeight(ent);

	trace_t groundtrace = Ground(ent);

	if (groundtrace.plane.normal.z <= 0.7)
	{
		// if the push down didn't end up on good ground, use the move without
		// the step up.  This happens near wall / slope combinations, and can
		// cause the player to hop up higher on a slope too steep to climb
		ent->v.origin = nosteporg;
		ent->v.velocity = nostepvel;
	}
}

#if 1
void SV_WalkMoveOld(edict_t* ent)
{
	vec3_t upmove, downmove;
	vec3_t oldorg, oldvel;
	vec3_t nosteporg, nostepvel;
	int clip;
	int oldonground;
	trace_t steptrace, downtrace;

	//
	// do a regular slide move unless it looks like you ran into a step
	//
	oldonground = (int)ent->v.flags & FL_ONGROUND;
	ent->v.flags = (int)ent->v.flags & ~FL_ONGROUND;

	oldorg = ent->v.origin;
	oldvel = ent->v.velocity;

	clip = SV_FlyMove(ent, host_frametime, 0, &steptrace);

	if (!(clip & 2))
		return; // move didn't block on a step

	if (!oldonground && ent->v.waterlevel == 0)
		return; // don't stair up while jumping

	if (ent->v.movetype != MOVETYPE_WALK)
		return; // gibbed by a trigger

	if (sv_nostep.value)
		return;

	if ((int)ent->v.flags & FL_WATERJUMP)
		return;

	nosteporg = ent->v.origin;
	nostepvel = ent->v.velocity;

	//
	// try moving up and forward to go up a step
	//
	ent->v.origin = oldorg; // back to start pos

	upmove = Vec_Zero();
	downmove = Vec_Zero();
	upmove.z = STEPSIZE;
	downmove.z = -STEPSIZE+oldvel.z*host_frametime;

	// move up
	SV_PushEntity(ent, upmove); // FIXME: don't link?

// move forward
	ent->v.velocity.x = oldvel.x;
	ent->v.velocity.y = oldvel.y;
	ent->v.velocity.z = 0;

	clip = SV_FlyMove(ent, host_frametime, 0, &steptrace);

	// check for stuckness, possibly due to the limited precision of floats
	// in the clipping hulls
	if (clip)
	{
		if (fabs(oldorg.y - ent->v.origin.y) < 0.03125
			&& fabs(oldorg.x - ent->v.origin.x) < 0.03125)
		{ // stepping up didn't make any progress
			//clip = SV_TryUnstick(ent, oldvel);
		}
	}

	// extra friction based on view angle
	if (clip & 2)
		SV_WallFriction(ent, &steptrace);

	// move down
	downtrace = SV_PushEntity(ent, downmove); // FIXME: don't link?

	if (downtrace.plane.normal.z > 0.7)
	{
		if (ent->v.solid == SOLID_BSP)
		{
			ent->v.flags = (int)ent->v.flags | FL_ONGROUND;
			ent->v.groundentity = EDICT_TO_PROG(downtrace.ent);
		}
	}
	else
	{
		// if the push down didn't end up on good ground, use the move without
		// the step up.  This happens near wall / slope combinations, and can
		// cause the player to hop up higher on a slope too steep to climb
		ent->v.origin = nosteporg;
		ent->v.velocity = nostepvel;
	}
}
#endif

/*
================
SV_Physics_Client

Player character actions
================
*/
void SV_Physics_Client(edict_t* ent, int num)
{
	if (!svs.clients[num-1].active)
		return; // unconnected slot

//
// call standard client pre-think
//
	pr_global_struct->time = sv.time;
	pr_global_struct->self = EDICT_TO_PROG(ent);
	PR_ExecuteProgram(pr_global_struct->PlayerPreThink);

	//
	// do a move
	//
	SV_CheckVelocity(ent);

	//
	// decide which move function to call
	//
	switch ((int)ent->v.movetype)
	{
	case MOVETYPE_NONE:
		if (!SV_RunThink(ent))
			return;
		break;

	case MOVETYPE_WALK:
		if (!SV_RunThink(ent))
			return;
		if (!SV_CheckWater(ent) && !((int)ent->v.flags & FL_WATERJUMP))
			SV_AddGravity(ent);

#if 0
		SV_CheckStuck(ent);
#endif

		
		SV_WalkMove(ent);
		
		break;

	case MOVETYPE_TOSS:
	case MOVETYPE_BOUNCE:
		SV_Physics_Toss(ent);
		break;

	case MOVETYPE_FLY:
		if (!SV_RunThink(ent))
			return;
		SV_FlyMove(ent, host_frametime, 0, NULL);
		break;

	case MOVETYPE_NOCLIP:
		if (!SV_RunThink(ent))
			return;
		ent->v.origin = Vec_MulAdd(ent->v.origin, host_frametime, ent->v.velocity);
		break;

	default:
		Sys_Error("SV_Physics_client: bad movetype %i", (int)ent->v.movetype);
	}

	//
	// call standard player post-think
	//
	SV_LinkEdict(ent, true);

	pr_global_struct->time = sv.time;
	pr_global_struct->self = EDICT_TO_PROG(ent);
	PR_ExecuteProgram(pr_global_struct->PlayerPostThink);
}

//============================================================================

/*
=============
SV_Physics_None

Non moving objects can only think
=============
*/
void SV_Physics_None(edict_t* ent)
{
	// regular thinking
	SV_RunThink(ent);
}

/*
=============
SV_Physics_Noclip

A moving object that doesn't obey physics
=============
*/
void SV_Physics_Noclip(edict_t* ent)
{
	// regular thinking
	if (!SV_RunThink(ent))
		return;

	ent->v.angles = Vec_MulAdd(ent->v.angles, host_frametime, ent->v.avelocity);
	ent->v.origin = Vec_MulAdd(ent->v.origin, host_frametime, ent->v.velocity);

	SV_LinkEdict(ent, false);
}

/*
==============================================================================

TOSS / BOUNCE

==============================================================================
*/

/*
=============
SV_CheckWaterTransition

=============
*/
void SV_CheckWaterTransition(edict_t* ent)
{
	int cont;

	cont = SV_PointContents(ent->v.origin);

	if (!ent->v.watertype)
	{ // just spawned here
		ent->v.watertype = cont;
		ent->v.waterlevel = 1;
		return;
	}

	if (cont <= CONTENTS_WATER)
	{
		if (ent->v.watertype == CONTENTS_EMPTY)
		{ // just crossed into water
			SV_StartSound(ent, 0, "misc/h2ohit1.wav", 255, 1);
		}
		ent->v.watertype = cont;
		ent->v.waterlevel = 1;
	}
	else
	{
		if (ent->v.watertype != CONTENTS_EMPTY)
		{ // just crossed into water
			SV_StartSound(ent, 0, "misc/h2ohit1.wav", 255, 1);
		}
		ent->v.watertype = CONTENTS_EMPTY;
		ent->v.waterlevel = cont;
	}
}

/*
=============
SV_Physics_Toss

Toss, bounce, and fly movement.  When onground, do nothing.
=============
*/
void SV_Physics_Toss(edict_t* ent)
{
	trace_t trace;
	vec3_t move;
	float backoff;

	// regular thinking
	if (!SV_RunThink(ent))
		return;

	// if onground, return without moving
	if (((int)ent->v.flags & FL_ONGROUND))
		return;

	SV_CheckVelocity(ent);

	// add gravity
	if (ent->v.movetype != MOVETYPE_FLY
		&& ent->v.movetype != MOVETYPE_FLYMISSILE)
		SV_AddGravity(ent);

	// move angles
	ent->v.angles = Vec_MulAdd(ent->v.angles, host_frametime, ent->v.avelocity);

	// move origin
	move = Vec_Scale(ent->v.velocity, host_frametime);
	trace = SV_PushEntity(ent, move);
	if (trace.fraction == 1)
		return;
	if (!ent->id)
		return;

	if (ent->v.movetype == MOVETYPE_BOUNCE)
		backoff = 1.5;
	else
		backoff = 1;

	ClipVelocity(ent->v.velocity, trace.plane.normal, &ent->v.velocity, backoff);

	// stop if on ground
	if (trace.plane.normal.z > 0.7)
	{
		if (ent->v.velocity.z < 60 || ent->v.movetype != MOVETYPE_BOUNCE)
		{
			ent->v.flags = (int)ent->v.flags | FL_ONGROUND;
			ent->v.groundentity = EDICT_TO_PROG(trace.ent);
			ent->v.velocity = Vec_Zero();
			ent->v.avelocity = Vec_Zero();
		}
	}

	// check for in water
	SV_CheckWaterTransition(ent);
}

/*
===============================================================================

STEPPING MOVEMENT

===============================================================================
*/

/*
=============
SV_Physics_Step

Monsters freefall when they don't have a ground entity, otherwise
all movement is done with discrete steps.

This is also used for objects that have become still on the ground, but
will fall if the floor is pulled out from under them.
=============
*/
void SV_Physics_Step(edict_t* ent)
{
	bool hitsound;

	// freefall if not onground
	if (!((int)ent->v.flags & (FL_ONGROUND | FL_FLY | FL_SWIM)))
	{
		if (ent->v.velocity.z < sv_gravity.value*-0.1)
			hitsound = true;
		else
			hitsound = false;

		SV_AddGravity(ent);
		SV_CheckVelocity(ent);
		SV_FlyMove(ent, host_frametime, 0, NULL);
		SV_LinkEdict(ent, true);

		if ((int)ent->v.flags & FL_ONGROUND) // just hit ground
		{
			if (hitsound)
				SV_StartSound(ent, 0, "demon/dland2.wav", 255, 1);
		}
	}

	// regular thinking
	SV_RunThink(ent);

	SV_CheckWaterTransition(ent);
}


//============================================================================

/*
================
SV_Physics

================
*/
void SV_Physics(void)
{
	int i;
	int entity_cap; // For sv_freezenonclients 
	edict_t* ent;

	// let the progs know that a new frame has started
	pr_global_struct->self = EDICT_TO_PROG(sv.edicts);
	pr_global_struct->other = EDICT_TO_PROG(sv.edicts);
	pr_global_struct->time = sv.time;
	PR_ExecuteProgram(pr_global_struct->StartFrame);

	/*
	if (qlua.state)
	{
		QLua_PushLuaFunction(&qlua.startframefunc);
		lua_pushnumber(qlua.state, 1);
		lua_pushnumber(qlua.state, 2);
		QLua_CallFunction(2, 1);
		int args = lua_gettop(qlua.state);
		double startframereturn = lua_tonumber(qlua.state, -1);
		lua_pop(qlua.state, 2);//Pop return and function table.
	}
	*/

	//SV_CheckAllEnts ();

	//
	// treat each object in turn
	//
	if (sv_freezenonclients.value)
		entity_cap = svs.maxclients + 1; // Only run physics on clients and the world
	else
		entity_cap = sv.num_edicts;

	ent = sv.edicts;
	for (i = 0; i<entity_cap; i++, ent = NEXT_EDICT(ent))
	{
		if (!ent->id)
			continue;

		if (pr_global_struct->force_retouch)
		{
			SV_LinkEdict(ent, true); // force retouch even for stationary
		}

		if (ent->v.movetype == MOVETYPE_PUSH)
		{
			SV_Physics_Pusher(ent);
		}
	}

	ent = sv.edicts;
	for (i = 0; i<entity_cap; i++, ent = NEXT_EDICT(ent))
	{
		if (!ent->id)
			continue;

		//if (pr_global_struct->force_retouch)
		//{
		// SV_LinkEdict(ent, true); // force retouch even for stationary
		//}

		if (i > 0 && i <= svs.maxclients)
			SV_Physics_Client(ent, i);
		else if (ent->v.movetype == MOVETYPE_PUSH)
		{
			//SV_Physics_Pusher(ent);
		}
		else if (ent->v.movetype == MOVETYPE_NONE)
			SV_Physics_None(ent);
		else if (ent->v.movetype == MOVETYPE_NOCLIP)
			SV_Physics_Noclip(ent);
		else if (ent->v.movetype == MOVETYPE_STEP)
			SV_Physics_Step(ent);
		else if (ent->v.movetype == MOVETYPE_TOSS
			|| ent->v.movetype == MOVETYPE_BOUNCE
			|| ent->v.movetype == MOVETYPE_FLY
			|| ent->v.movetype == MOVETYPE_FLYMISSILE)
			SV_Physics_Toss(ent);
		else
			Sys_Error("SV_Physics: bad movetype %i", (int)ent->v.movetype);
	}

	/*
	ent = sv.edicts;
	for (i = 0; i<entity_cap; i++, ent = NEXT_EDICT(ent))
	{
		if (ent->free)
			continue;

		if (ent->v.movetype == MOVETYPE_WALK)
		{
			if (((int)ent->v.flags&FL_ONGROUND))
			{
				//ent->v.velocity += ent->v.;
				ent->v.velocity = Vec_Add(ent->v.velocity, PROG_TO_EDICT(ent->v.groundentity)->v.velocity);
			}
		}
	}
	*/
	

	if (pr_global_struct->force_retouch)
		pr_global_struct->force_retouch--;

	if (!sv_freezenonclients.value)
		sv.time += host_frametime;
}
