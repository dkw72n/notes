# ref: https://learn.microsoft.com/en-us/windows/win32/wer/collecting-user-mode-dumps

import os
from winreg import *

appName = "hh.exe"
dumpFolder = "%LOCALAPPDATA%\CrashDumps"

DUMPTYPE_CUSTOM = 0
DUMPTYPE_MINIDUMP = 1
DUMPTYPE_FULLDUMP = 2

MiniDumpNormal = 0x00000000
MiniDumpWithDataSegs = 0x00000001
MiniDumpWithFullMemory = 0x00000002
MiniDumpWithHandleData = 0x00000004
MiniDumpFilterMemory = 0x00000008
MiniDumpScanMemory = 0x00000010
MiniDumpWithUnloadedModules = 0x00000020
MiniDumpWithIndirectlyReferencedMemory = 0x00000040
MiniDumpFilterModulePaths = 0x00000080
MiniDumpWithProcessThreadData = 0x00000100
MiniDumpWithPrivateReadWriteMemory = 0x00000200
MiniDumpWithoutOptionalData = 0x00000400
MiniDumpWithFullMemoryInfo = 0x00000800
MiniDumpWithThreadInfo = 0x00001000
MiniDumpWithCodeSegs = 0x00002000
MiniDumpWithoutAuxiliaryState = 0x00004000
MiniDumpWithFullAuxiliaryState = 0x00008000
MiniDumpWithPrivateWriteCopyMemory = 0x00010000
MiniDumpIgnoreInaccessibleMemory = 0x00020000
MiniDumpWithTokenInformation = 0x00040000
MiniDumpWithModuleHeaders = 0x00080000
MiniDumpFilterTriage = 0x00100000
MiniDumpWithAvxXStateContext = 0x00200000
MiniDumpWithIptTrace = 0x00400000
MiniDumpScanInaccessiblePartialPages = 0x00800000
MiniDumpValidTypeFlags = 0x01ffffff
  
with CreateKey(HKEY_LOCAL_MACHINE, "SOFTWARE\Microsoft\Windows\Windows Error Reporting\LocalDumps") as localDumps:
  with CreateKey(localDumps, appName) as app:
    SetValueEx(app, "DumpFolder", 0, REG_EXPAND_SZ, dumpFolder)
    SetValueEx(app, "DumpType", 0, REG_DWORD, DUMPTYPE_CUSTOM)
    SetValueEx(app, "CustomDumpFlags", 0, REG_DWORD, MiniDumpWithPrivateWriteCopyMemory)
