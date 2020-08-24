#coding: utf-8
"""
MIT License
Copyright (c) 2019-2020 dkw72n
Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:
The above copyright notice and this permission notice shall be included in all
copies or substantial portions of the Software.
THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
SOFTWARE.
"""

"""

启动exe, 在运行前注入dll

"""
import sys
from subprocess import list2cmdline
import time
import ctypes
from _ctypes import Structure, Union, POINTER
from ctypes import *
from ctypes.wintypes import *
import os
import argparse
import locale
locale.setlocale(locale.LC_ALL, '')
parser = argparse.ArgumentParser(formatter_class=argparse.RawTextHelpFormatter)
parser.add_argument('dll', help=u"被注入的dll")
parser.add_argument('--param', help=u"dll 参数")
parser.add_argument('command', help=u"被测程序")
parser.add_argument('args', nargs=argparse.REMAINDER, help=u"命令行参数")
_mswindows = (sys.platform == "win32")

ULONGLONG = c_ulonglong
SIZE_T = c_size_t
LONGLONG = c_longlong
ULONG_PTR = c_void_p
LPBYTE = POINTER(c_char)
LPVOID = c_void_p

class _LARGE_INTEGER(Structure):
  _fields_ = [
    ("LowPart", DWORD),
    ("HighPart", LONG),
  ]
    
class LARGE_INTEGER(Union):
  _anonymous_ = ('_u',)
  _fields_ = [
    ("_u", _LARGE_INTEGER),
    ("u", _LARGE_INTEGER),
    ("QuadPart", LONGLONG)
  ]
    
class IO_COUNTERS(Structure):
  _fields_ = [
    ("ReadOperationCount", ULONGLONG),
    ("WriteOperationCount", ULONGLONG),
    ("OtherOperationCount", ULONGLONG),
    ("ReadTransferCount", ULONGLONG),
    ("WriteTransferCount", ULONGLONG),
    ("OtherTransferCount", ULONGLONG)
  ]

class JOBOBJECT_BASIC_LIMIT_INFORMATION(Structure):
  _fields_ = [
    ('PerProcessUserTimeLimit', LARGE_INTEGER),
    ('PerJobUserTimeLimit', LARGE_INTEGER),
    ('LimitFlags', DWORD),
    ('MinimumWorkingSetSize', SIZE_T),
    ('MaximumWorkingSetSize', SIZE_T),
    ('ActiveProcessLimit', DWORD),
    ('Affinity', ULONG_PTR),
    ('PriorityClass', DWORD),
    ('SchedulingClass', DWORD)
  ]

class JOBOBJECT_EXTENDED_LIMIT_INFORMATION(Structure):
   _fields_ = [
    ('BasicLimitInformation', JOBOBJECT_BASIC_LIMIT_INFORMATION),
    ('IoInfo', IO_COUNTERS),
    ('ProcessMemoryLimit', SIZE_T),
    ('JobMemoryLimit', SIZE_T),
    ('PeakProcessMemoryUsed', SIZE_T),
    ('PeakJobMemoryUsed', SIZE_T)
  ]
  
class SECURITY_ATTRIBUTES(Structure):
  _fields_ = [
    ("ReadOperationCount", ULONGLONG),
    ("WriteOperationCount", ULONGLONG),
    ("OtherOperationCount", ULONGLONG),
    ("ReadTransferCount", ULONGLONG),
    ("WriteTransferCount", ULONGLONG),
    ("OtherTransferCount", ULONGLONG)
  ]

class STARTUPINFOW(Structure):
  _fields_ = [
    ("cb", DWORD),
    ("lpReserved",LPWSTR),
    ("lpDesktop", LPWSTR),
    ("lpTitle", LPWSTR),
    ("dwX", DWORD),
    ("dwY", DWORD),
    ("dwXSize", DWORD),
    ("dwYSize", DWORD),
    ("dwXCountChars", DWORD),
    ("dwYCountChars", DWORD),
    ("dwFillAttribute", DWORD),
    ("dwFlags", DWORD),
    ("wShowWindow", WORD),
    ("cbReserved2", WORD),
    ("lpReserved2", LPBYTE),
    ("hStdInput", HANDLE),
    ("hStdOutput", HANDLE),
    ("hStdError", HANDLE)
  ]

class PROCESS_INFORMATION(Structure):
  _fields_ = [
    ("hProcess", HANDLE),
    ("hThread", HANDLE),
    ("dwProcessId", DWORD),
    ("dwThreadId", DWORD)
  ]
  
CreateProcessW = windll.kernel32.CreateProcessW
CreateProcessW.restype = BOOL
CreateProcessW.argtypes = [
  c_wchar_p,                        # lpApplicationName
  c_wchar_p,                        # lpCommandLine
  POINTER(SECURITY_ATTRIBUTES),     # lpProcessAttributes
  POINTER(SECURITY_ATTRIBUTES),     # lpThreadAttributes
  BOOL,                             # bInheritHandles
  DWORD,                            # dwCreationFlags
  c_void_p,                         # lpEnvironment
  c_wchar_p,                        # lpCurrentDirectory
  POINTER(STARTUPINFOW),            # lpStartupInfo
  POINTER(PROCESS_INFORMATION)      # lpProcessInformation
  ]

VirtualAllocEx = windll.kernel32.VirtualAllocEx
VirtualAllocEx.restype = LPVOID
VirtualAllocEx.argtypes = [
  HANDLE,                           # hProcess,
  LPVOID,                           # lpAddress,
  SIZE_T,                           # dwSize,
  DWORD,                            # flAllocationType,
  DWORD                             # flProtect
  ]

WriteProcessMemory = windll.kernel32.WriteProcessMemory
WriteProcessMemory.restype = BOOL
WriteProcessMemory.argtypes = [
  HANDLE,                           # hProcess,
  LPVOID,                           # lpBaseAddress,
  c_wchar_p,                        # lpBuffer,
  SIZE_T,                           # nSize,
  POINTER(SIZE_T),                  # lpNumberOfBytesWritten
  ]

QueueUserAPC = windll.kernel32.QueueUserAPC
QueueUserAPC.restype = DWORD
QueueUserAPC.argtypes = [
  c_void_p,                         # pfnAPC,
  HANDLE,                           # hThread,
  ULONG_PTR,                        # dwData
  ]

ResumeThread = windll.kernel32.ResumeThread
ResumeThread.restype = DWORD
ResumeThread.argtypes = [HANDLE]

CreateJobObjectW = windll.kernel32.CreateJobObjectW
CreateJobObjectW.restype = HANDLE
CreateJobObjectW.argtypes = [c_void_p, c_wchar_p]

GetCurrentProcess = windll.kernel32.GetCurrentProcess
GetCurrentProcess.restype = HANDLE

AssignProcessToJobObject = windll.kernel32.AssignProcessToJobObject
AssignProcessToJobObject.restype = BOOL
AssignProcessToJobObject.argtypes = [HANDLE, HANDLE]

GetLastError = windll.kernel32.GetLastError
OpenJobObjectW = windll.kernel32.OpenJobObjectW
TerminateJobObject = windll.kernel32.TerminateJobObject
TerminateJobObject.restype = BOOL
TerminateJobObject.argtypes = [HANDLE, c_uint]

SetInformationJobObject = windll.kernel32.SetInformationJobObject
SetInformationJobObject.restype = BOOL
SetInformationJobObject.argtypes = [HANDLE, c_int, c_void_p, DWORD]

WaitForSingleObject = windll.kernel32.WaitForSingleObject
WaitForSingleObject.restype = DWORD
WaitForSingleObject.argtypes = [HANDLE, DWORD]

GetExitCodeProcess = windll.kernel32.GetExitCodeProcess
GetExitCodeProcess.restype = BOOL
GetExitCodeProcess.argtypes = [HANDLE, POINTER(DWORD)]

INFINITE = 0xffffffff
MEM_COMMIT = 0x00001000
MEM_RESERVE = 0x00002000
PAGE_READWRITE = 0x04
CREATE_SUSPENDED = 0x00000004
JOB_OBJECT_LIMIT_KILL_ON_JOB_CLOSE = 0x00002000
JobObjectBasicLimitInformation = 2
JobObjectExtendedLimitInformation = 9

def activate_subprocess_reaver():
  hProcess = GetCurrentProcess()
  hJob = CreateJobObjectW(None, 'Local\\py_win_job_%d_%d' % (hProcess, time.time()))
  if hJob == 0:
    return False, ("CreateJobObjectW", GetLastError())
  info = JOBOBJECT_EXTENDED_LIMIT_INFORMATION()
  info.BasicLimitInformation.LimitFlags = JOB_OBJECT_LIMIT_KILL_ON_JOB_CLOSE
  # print(sizeof(info))
  if not SetInformationJobObject(hJob, JobObjectExtendedLimitInformation, pointer(info), sizeof(info)):
    return False, ("SetInformationJobObject", GetLastError())
  if not AssignProcessToJobObject(hJob, hProcess):
    return False, ("AssignProcessToJobObject", GetLastError())
  return True, None

def create_process_suspended(cmd):
  si = STARTUPINFOW()
  si.cb = sizeof(si)
  pi = PROCESS_INFORMATION()
  if CreateProcessW(None, list2cmdline(cmd), None, None, True, CREATE_SUSPENDED, None, None, pointer(si), pointer(pi)):
    return pi.hProcess, pi.hThread
  return None, None
  
def resume_suspended_process(proc, thread):
  if thread:
    ResumeThread(thread)
  pass
  
def inject_dll(hProcess, hThread, dll):
  print("[+] inject_dll: hProcess=%x, hThread=%x, dll=%s" % (hProcess, hThread, dll))
  if not hProcess: return
  p = VirtualAllocEx(hProcess, None, 1 << 12, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE);
  name = os.path.abspath(dll)
  if not p:
    print("[!] VirtualAllocEx Failed")
    return
  print("[-] addr is %016x" % p)
  if not WriteProcessMemory(hProcess, p, name, len(name) * sizeof(c_wchar_p), None):
    print("[!] WriteProcessMemory Failed")
    return
  if not QueueUserAPC(windll.kernel32.LoadLibraryW, hThread, p): # FIXME: should use address from remote process instead
    print("[!] QueueUserAPC Failed")
    return
  return True

def main(dll, param, cmd):
  """
  ok, y = activate_subprocess_reaver()
  if not ok:
    print("[!] %s Failed: %d" % y)
    return
  """
  proc, thread = create_process_suspended(cmd)
  print("[+] injected: ",inject_dll(proc, thread, dll))
  resume_suspended_process(proc, thread)
  WaitForSingleObject(proc, INFINITE)
  code = DWORD(0)
  GetExitCodeProcess(proc, pointer(code))
  print("[+] exit code:", code)
  
if __name__ == '__main__':
  opts = parser.parse_args()
  main(opts.dll, opts.param, [opts.command] + opts.args)
  pass