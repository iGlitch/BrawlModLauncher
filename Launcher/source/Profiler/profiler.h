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

#ifndef __PROFILER_H__
#define __PROFILER_H__

//#define GAMECUBE
#define WII
 
	typedef struct profiler_s
	{
		char*	name;
		u32		active;
		u32		no_hits;
		u64		total_time;
		u64		min_time;
		u64		max_time;
		u64		start_time;
	} profiler_t;

	void profiler_create(profiler_t* pjob, char* pname);
	//void profiler_output(profiler_t* pjob);
	void profiler_start(profiler_t* pjob);
	void profiler_stop(profiler_t* pjob);
	void profiler_reset(profiler_t* pjob);

#endif // __PROFILER_H__
