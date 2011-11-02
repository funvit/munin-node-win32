/* This file is part of munin-node-win32
* Copyright (C) 2006-2008 Jory Stone (jcsston@jory.info)
* Copyright (C) 2007 Steve Schnepp <steve.schnepp@gmail.com> 
*
* This program is free software; you can redistribute it and/or
* modify it under the terms of the GNU General Public License
* as published by the Free Software Foundation; either version 2
* of the License, or (at your option) any later version.
*
* This program is distributed in the hope that it will be useful,
* but WITHOUT ANY WARRANTY; without even the implied warranty of
* MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
* GNU General Public License for more details.
*
* You should have received a copy of the GNU General Public License
* along with this program; if not, write to the Free Software
* Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
*/

#include "StdAfx.h"
#include "CpuMuninNodePlugin.h"

// dunno this is good :(
#define HZ 100

#define SystemBasicInformation 0
#define SystemPerformanceInformation 2
#define SystemTimeInformation 3

#define Li2Double(x) ((double)((x).HighPart) * 4.294967296E9 + (double)((x).LowPart))

// Initialisation
CpuMuninNodePlugin::CpuMuninNodePlugin()
{
  dbIdleTime = 0;
  dbSystemTime = 0;
  liOldIdleTime.QuadPart = 0;
  liOldSystemTime.QuadPart = 0;
  NtQuerySystemInformation = (PROCNTQSI)GetProcAddress(GetModuleHandle(_T("ntdll")), "NtQuerySystemInformation");

  // Setup first call
  CalculateCpuLoad();
}

CpuMuninNodePlugin::~CpuMuninNodePlugin()
{

}
/*
int CpuMuninNodePlugin::GetConfig(char *buffer, int len) 
{
  int ret = 0;

  SYSTEM_INFO SystemInfo;
  GetSystemInfo(&SystemInfo);

  ret = _snprintf(buffer, len, "graph_title CPU usage\n"
    "graph_category system\n"
    "graph_info This graph shows how CPU time is spent.\n"
    "graph_args --base 1000 -r --lower-limit 0 --upper-limit %d\n"
	"graph_order system user idle\n"
    "graph_vlabel %%\n"
	"graph_scale no\n"
	"graph_period second\n", 100*SystemInfo.dwNumberOfProcessors);
  buffer += ret;
  len -= ret;

  ret = _snprintf(buffer, len, "system.label system\n"
    "system.draw AREA\n"
    "system.max 5000\n"
    "system.min 0\n"
    "system.type DERIVE\n"
    "system.warning 30\n"
    "system.critical 50\n"
    "system.info CPU time spent by the kernel in system activities\n"
    "user.label user\n"
    "user.draw STACK\n"
    "user.min 0\n"
    "user.max 5000\n"
    "user.warning 80\n"
    "user.type DERIVE\n"
    "user.info CPU time spent by normal programs and daemons\n"
    "idle.label idle\n"
    "idle.draw STACK\n"
    "idle.min 0\n"
    "idle.max 5000\n"
    "idle.type DERIVE\n"
    "idle.info Idle CPU time\n");
  buffer += ret;
  len -= ret;

  strncat(buffer, ".\n", len);

  return 0;
}*/
int CpuMuninNodePlugin::GetConfig(char *buffer, int len) 
{
  strncpy(buffer, 
    "graph_args -l 0 --vertical-label percent --upper-limit 100\n"
    "graph_title Cpu usage\n"
    "graph_category system\n"
    "graph_info This graph shows what the machine uses its cpu for.\n"
    "graph_order cpu_user\n"
    //"graph_order cpu_system cpu_user\n"
    "cpu_user.label user\n"
    "cpu_user.draw AREA\n"
    "cpu_user.info CPU used by user-space applications.\n"
    //"cpu_system.label system\n"
    //"cpu_system.draw STACK\n"
    //"cpu_system.info CPU used by kernel.\n"
    ".\n", len);
  return 0;
}

void CpuMuninNodePlugin::CalculateCpuLoad()
{
  if (NtQuerySystemInformation != NULL) {
    LONG status;
    SYSTEM_PERFORMANCE_INFORMATION SysPerfInfo;
    SYSTEM_TIME_INFORMATION SysTimeInfo;
    SYSTEM_BASIC_INFORMATION SysBaseInfo;

    // get number of processors in the system
    status = NtQuerySystemInformation(SystemBasicInformation, &SysBaseInfo, sizeof(SysBaseInfo), NULL);
    if (status != NO_ERROR)
      return;

    // get new system time
    status = NtQuerySystemInformation(SystemTimeInformation, &SysTimeInfo, sizeof(SysTimeInfo), NULL);
    if (status!=NO_ERROR)
      return;

    // get new CPU's idle time
    status = NtQuerySystemInformation(SystemPerformanceInformation, &SysPerfInfo, sizeof(SysPerfInfo), NULL);
    if (status != NO_ERROR)
      return;

    // if it's a first call - skip it
    if (liOldIdleTime.QuadPart != 0)
    {
      // CurrentValue = NewValue - OldValue
      double diffIdleTime = Li2Double(SysPerfInfo.liIdleTime) - Li2Double(liOldIdleTime);
      double diffSystemTime = Li2Double(SysTimeInfo.liKeSystemTime) - Li2Double(liOldSystemTime);

      // CurrentCpuIdle = IdleTime / SystemTime
      dbIdleTime = diffIdleTime / diffSystemTime;

      // CurrentCpuUsage% = 100 - (CurrentCpuIdle * 100) / NumberOfProcessors
      dbIdleTime = 100.0 - dbIdleTime * 100.0 / (double)SysBaseInfo.bKeNumberProcessors + 0.5;
    }

    // store new CPU's idle and system time
    liOldIdleTime = SysPerfInfo.liIdleTime;
    liOldSystemTime = SysTimeInfo.liKeSystemTime;
  }
}


int CpuMuninNodePlugin::GetValues(char *buffer, int len)
{ 
  CalculateCpuLoad();

  _snprintf(buffer, len, 
    "cpu_user.value %f\n"
    //"cpu_system.value %f\n"
    ".\n", this->dbIdleTime);
  return 0;

  /* cant build that
  int index = 0;
  int ret;
  NTSTATUS ntret;
  SYSTEM_PROCESSOR_TIMES spt[32];

  SYSTEM_INFO SystemInfo;
  GetSystemInfo(&SystemInfo);

  // We have array for 32 processor only :( 
  if (SystemInfo.dwNumberOfProcessors <= 32) {

	  ntret = NtQuerySystemInformation (SystemProcessorPerformanceInformation, (PVOID) spt,
		  sizeof spt[0] * SystemInfo.dwNumberOfProcessors, NULL);
	  if (ntret == NO_ERROR)
	  {
		  unsigned long long user_time = 0ULL, kernel_time = 0ULL, idle_time = 0ULL;
		  for (unsigned int i = 0; i < SystemInfo.dwNumberOfProcessors; i++)
		  {
			  kernel_time += (spt[i].KernelTime.QuadPart - spt[i].IdleTime.QuadPart) * HZ / 10000000ULL;
			  user_time += spt[i].UserTime.QuadPart * HZ / 10000000ULL;
			  idle_time += spt[i].IdleTime.QuadPart * HZ / 10000000ULL;
		  }
		  ret = _snprintf(buffer, len, "system.value %I64u\n", kernel_time);
		  len -= ret;
		  buffer += ret;
		  ret = _snprintf(buffer, len, "user.value %I64u\n", user_time);
		  len -= ret;
		  buffer += ret;
		  ret = _snprintf(buffer, len, "idle.value %I64u\n", idle_time);
		  len -= ret;
		  buffer += ret;
	  }  
  }
  */
/* Old wrong code...
  FILETIME IdleTime;
  FILETIME KernelTime;
  FILETIME UserTime;
  
  GetSystemTimes(&IdleTime, &KernelTime, &UserTime);

  ret = _snprintf(buffer, len, "system.value %I64u\n", (*(PDWORD64)&KernelTime - *(PDWORD64)&IdleTime) / 65536);
  len -= ret;
  buffer += ret;
  ret = _snprintf(buffer, len, "user.value %I64u\n", *(PDWORD64)&UserTime / 65536);
  len -= ret;
  buffer += ret;
  ret = _snprintf(buffer, len, "idle.value %I64u\n", *(PDWORD64)&IdleTime / 65536);
  len -= ret;
  buffer += ret;
*/

  //strncat(buffer, ".\n", len);
  //return 0;
}
