/*
	#libprofile (Tiny code profiler for various game consoles)
 
	profiler.h
 
	Copyright (c) 2010, Ian Callaghan <ian (at) unrom (dot) com>

	Permission is hereby granted, free of charge, to any person obtaining a copy of this software and associated 
	documentation files (the "Software"), to deal in the Software without restriction, including without
	limitation the rights to use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of
	the Software, and to permit persons to whom the Software is furnished to do so, subject to the following
	conditions:

	The above copyright notice and this permission notice shall be included in all copies or substantial
	portions of the Software.
	
	THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT
	LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO
	EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN
	AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE
	OR OTHER DEALINGS IN THE SOFTWARE.
*/

//#define GAMECUBE
#define WII
//#define IPHONE
//#define PSP

#include <stdio.h>
#include "types.h"
#include "timer.h"
#include "profiler.h"
	/*static u64 __ticks_to_us(u64 ticks)
	{
		return (((ticks)* 8) / (TIMER_CLK / 125000));
	}

	static u64 __ticks_to_cycles(u64 ticks)
	{
		return (((ticks)* ((CPU_CLK * 2) / TIMER_CLK)) / 2);
	}
	*/
	void profiler_create(profiler_t* pjob, char* pname)
	{
		pjob->active = 0;
		pjob->no_hits = 0;
		pjob->total_time = 0;
		pjob->min_time = 0;
		pjob->max_time = 0;
		pjob->start_time = 0;
		pjob->name = pname;
	}

	void profiler_start(profiler_t* pjob)
	{
		pjob->active = 1;
		pjob->start_time = timer_getthetime();
	};

	void profiler_stop(profiler_t* pjob)
	{
		u64 stop_time;
		u64 start_time;
		u64 duration;

		stop_time = timer_getthetime();

		if (pjob->active)
		{
			start_time = pjob->start_time;
			duration = stop_time - start_time;
			pjob->total_time += duration;

			if (pjob->min_time == 0 || duration < pjob->min_time)
				pjob->min_time = duration;

			if (duration > pjob->max_time)
				pjob->max_time = duration;

			pjob->no_hits += 1;
			pjob->active = 0;
		}
	};

	void profiler_reset(profiler_t* pjob)
	{
		pjob->active = 0;
		pjob->no_hits = 0;
		pjob->total_time = 0;
		pjob->min_time = 0;
		pjob->max_time = 0;
		pjob->start_time = 0;
	};

	/*
	void profiler_output(profiler_t* pjob)
	{
		char* name;
		u32 hits;
		u64 total_us, total_cycles;
		u64 min_us, min_cycles;
		u64 max_us, max_cycles;

		name = pjob->name;
		hits = pjob->no_hits;

		min_us = __ticks_to_us(pjob->min_time);
		min_cycles = __ticks_to_cycles(pjob->min_time);

		max_us = __ticks_to_us(pjob->max_time);
		max_cycles = __ticks_to_cycles(pjob->max_time);

		total_us = __ticks_to_us(pjob->total_time);
		total_cycles = __ticks_to_cycles(pjob->total_time);
	}
	*/
