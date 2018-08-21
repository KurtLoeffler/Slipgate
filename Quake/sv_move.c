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
// sv_move.c -- monster movement

#include "quakedef.h"

#define STEPSIZE 18

/*
=============
SV_CheckBottom

Returns false if any part of the bottom of the entity is off an edge that
is not a staircase.

=============
*/
int c_yes, c_no;

bool SV_CheckBottom(edict_t* ent)
{
	vec3_t mins, maxs, start, stop;
	trace_t trace;
	int x, y;
	float mid, bottom;

	mins = Vec_Add(ent->v.origin, ent->v.mins);
	maxs = Vec_Add(ent->v.origin, ent->v.maxs);

	// if all of the points under the corners are solid world, don't bother
	// with the tougher checks
	// the corners must be within 16 of the midpoint
	start.z = mins.z - 1;
	for (x = 0; x<=1; x++)
		for (y = 0; y<=1; y++)
		{
			start.x = x ? maxs.x : mins.x;
			start.y = y ? maxs.y : mins.y;
			if (SV_PointContents(start) != CONTENTS_SOLID)
				goto realcheck;
		}

	c_yes++;
	return true; // we got out easy

realcheck:
	c_no++;
	//
	// check it for real...
	//
	start.z = mins.z;

	// the midpoint must be within 16 of the bottom
	start.x = stop.x = (mins.x + maxs.x)*0.5;
	start.y = stop.y = (mins.y + maxs.y)*0.5;
	stop.z = start.z - 2*STEPSIZE;
	trace = SV_Move(start, Vec_Zero(), Vec_Zero(), stop, true, ent);

	if (trace.fraction == 1.0)
		return false;
	mid = bottom = trace.endpos.z;

	// the corners must be within 16 of the midpoint
	for (x = 0; x<=1; x++)
		for (y = 0; y<=1; y++)
		{
			start.x = stop.x = x ? maxs.x : mins.x;
			start.y = stop.y = y ? maxs.y : mins.y;

			trace = SV_Move(start, Vec_Zero(), Vec_Zero(), stop, true, ent);

			if (trace.fraction != 1.0 && trace.endpos.z > bottom)
				bottom = trace.endpos.z;
			if (trace.fraction == 1.0 || mid - trace.endpos.z > STEPSIZE)
				return false;
		}

	c_yes++;
	return true;
}


/*
=============
SV_movestep

Called by monster program code.
The move will be adjusted for slopes and stairs, but if the move isn't
possible, no move is done, false is returned, and
pr_global_struct->trace_normal is set to the normal of the blocking wall
=============
*/
bool SV_movestep(edict_t* ent, vec3_t move, bool relink)
{
	float dz;
	vec3_t oldorg, neworg, end;
	trace_t trace;
	int i;
	edict_t* enemy;

	// try the move
	oldorg = ent->v.origin;
	neworg = Vec_Add(ent->v.origin, move);

	// flying monsters don't step up
	if ((int)ent->v.flags & (FL_SWIM | FL_FLY))
	{
		// try one move with vertical motion, then one without
		for (i = 0; i<2; i++)
		{
			neworg = Vec_Add(ent->v.origin, move);
			enemy = PROG_TO_EDICT(ent->v.enemy);
			if (i == 0 && enemy != sv.edicts)
			{
				dz = ent->v.origin.z - PROG_TO_EDICT(ent->v.enemy)->v.origin.z;
				if (dz > 40)
					neworg.z -= 8;
				if (dz < 30)
					neworg.z += 8;
			}
			trace = SV_Move(ent->v.origin, ent->v.mins, ent->v.maxs, neworg, false, ent);

			if (trace.fraction == 1)
			{
				if (((int)ent->v.flags & FL_SWIM) && SV_PointContents(trace.endpos) == CONTENTS_EMPTY)
					return false; // swim monster left water

				ent->v.origin = trace.endpos;
				if (relink)
					SV_LinkEdict(ent, true);
				return true;
			}

			if (enemy == sv.edicts)
				break;
		}

		return false;
	}

	// push down from a step height above the wished position
	neworg.z += STEPSIZE;
	end = neworg;
	end.z -= STEPSIZE*2;

	trace = SV_Move(neworg, ent->v.mins, ent->v.maxs, end, false, ent);

	if (trace.allsolid)
		return false;

	if (trace.startsolid)
	{
		neworg.z -= STEPSIZE;
		trace = SV_Move(neworg, ent->v.mins, ent->v.maxs, end, false, ent);
		if (trace.allsolid || trace.startsolid)
			return false;
	}
	if (trace.fraction == 1)
	{
		// if monster had the ground pulled out, go ahead and fall
		if ((int)ent->v.flags & FL_PARTIALGROUND)
		{
			ent->v.origin = Vec_Add(ent->v.origin, move);
			if (relink)
				SV_LinkEdict(ent, true);
			ent->v.flags = (int)ent->v.flags & ~FL_ONGROUND;
			// Con_Printf ("fall down\n");
			return true;
		}

		return false; // walked off an edge
	}

	// check point traces down for dangling corners
	ent->v.origin = trace.endpos;

	if (!SV_CheckBottom(ent))
	{
		if ((int)ent->v.flags & FL_PARTIALGROUND)
		{ // entity had floor mostly pulled out from underneath it
			// and is trying to correct
			if (relink)
				SV_LinkEdict(ent, true);
			return true;
		}
		ent->v.origin = oldorg;
		return false;
	}

	if ((int)ent->v.flags & FL_PARTIALGROUND)
	{
		// Con_Printf ("back on ground\n");
		ent->v.flags = (int)ent->v.flags & ~FL_PARTIALGROUND;
	}
	ent->v.groundentity = EDICT_TO_PROG(trace.ent);

	// the move is ok
	if (relink)
		SV_LinkEdict(ent, true);
	return true;
}


//============================================================================

/*
======================
SV_StepDirection

Turns to the movement direction, and walks the current distance if
facing it.

======================
*/
void PF_changeyaw(void);
bool SV_StepDirection(edict_t* ent, float yaw, float dist)
{
	vec3_t move, oldorigin;
	float delta;

	ent->v.ideal_yaw = yaw;
	PF_changeyaw();

	yaw = yaw*M_PI*2 / 360;
	move.x = cos(yaw)*dist;
	move.y = sin(yaw)*dist;
	move.z = 0;

	oldorigin = ent->v.origin;
	if (SV_movestep(ent, move, false))
	{
		delta = ent->v.angles.yaw - ent->v.ideal_yaw;
		if (delta > 45 && delta < 315)
		{ // not turned far enough, so don't take the step
			ent->v.origin = oldorigin;
		}
		SV_LinkEdict(ent, true);
		return true;
	}
	SV_LinkEdict(ent, true);

	return false;
}

/*
======================
SV_FixCheckBottom

======================
*/
void SV_FixCheckBottom(edict_t* ent)
{
	// Con_Printf ("SV_FixCheckBottom\n");

	ent->v.flags = (int)ent->v.flags | FL_PARTIALGROUND;
}



/*
================
SV_NewChaseDir

================
*/
#define DI_NODIR -1
void SV_NewChaseDir(edict_t* actor, edict_t* enemy, float dist)
{
	float deltax, deltay;
	float d[3];
	float tdir, olddir, turnaround;

	olddir = AngleMod((int)(actor->v.ideal_yaw/45)*45);
	turnaround = AngleMod(olddir - 180);

	deltax = enemy->v.origin.x - actor->v.origin.x;
	deltay = enemy->v.origin.y - actor->v.origin.y;
	if (deltax>10)
		d[1] = 0;
	else if (deltax<-10)
		d[1] = 180;
	else
		d[1] = DI_NODIR;
	if (deltay<-10)
		d[2] = 270;
	else if (deltay>10)
		d[2] = 90;
	else
		d[2] = DI_NODIR;

	// try direct route
	if (d[1] != DI_NODIR && d[2] != DI_NODIR)
	{
		if (d[1] == 0)
			tdir = d[2] == 90 ? 45 : 315;
		else
			tdir = d[2] == 90 ? 135 : 215;

		if (tdir != turnaround && SV_StepDirection(actor, tdir, dist))
			return;
	}

	// try other directions
	if (((rand()&3) & 1) ||  abs((int)deltay)>abs((int)deltax)) // ericw -- explicit int cast to suppress clang suggestion to use fabsf
	{
		tdir = d[1];
		d[1] = d[2];
		d[2] = tdir;
	}

	if (d[1]!=DI_NODIR && d[1]!=turnaround
		&& SV_StepDirection(actor, d[1], dist))
		return;

	if (d[2]!=DI_NODIR && d[2]!=turnaround
		&& SV_StepDirection(actor, d[2], dist))
		return;

	/* there is no direct path to the player, so pick another direction */

	if (olddir!=DI_NODIR && SV_StepDirection(actor, olddir, dist))
		return;

	if (rand()&1)  /*randomly determine direction of search*/
	{
		for (tdir = 0; tdir<=315; tdir += 45)
			if (tdir!=turnaround && SV_StepDirection(actor, tdir, dist))
				return;
	}
	else
	{
		for (tdir = 315; tdir >=0; tdir -= 45)
			if (tdir!=turnaround && SV_StepDirection(actor, tdir, dist))
				return;
	}

	if (turnaround != DI_NODIR && SV_StepDirection(actor, turnaround, dist))
		return;

	actor->v.ideal_yaw = olddir; // can't move

// if a bridge was pulled out from underneath a monster, it may not have
// a valid standing position at all

	if (!SV_CheckBottom(actor))
		SV_FixCheckBottom(actor);

}

/*
======================
SV_CloseEnough

======================
*/
bool SV_CloseEnough(edict_t* ent, edict_t* goal, float dist)
{
	int i;

	for (i = 0; i<3; i++)
	{
		if (goal->v.absmin.values[i] > ent->v.absmax.values[i] + dist)
			return false;
		if (goal->v.absmax.values[i] < ent->v.absmin.values[i] - dist)
			return false;
	}
	return true;
}

/*
======================
SV_MoveToGoal

======================
*/
void SV_MoveToGoal(void)
{
	edict_t* ent, *goal;
	float dist;

	ent = PROG_TO_EDICT(pr_global_struct->self);
	goal = PROG_TO_EDICT(ent->v.goalentity);
	dist = G_FLOAT(OFS_PARM0);

	if (!((int)ent->v.flags & (FL_ONGROUND|FL_FLY|FL_SWIM)))
	{
		G_FLOAT(OFS_RETURN) = 0;
		return;
	}

	// if the next step hits the enemy, return immediately
	if (PROG_TO_EDICT(ent->v.enemy) != sv.edicts &&  SV_CloseEnough(ent, goal, dist))
		return;

	// bump around...
	if ((rand()&3)==1 ||
		!SV_StepDirection(ent, ent->v.ideal_yaw, dist))
	{
		SV_NewChaseDir(ent, goal, dist);
	}
}

