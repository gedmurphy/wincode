/*!

    @file Core/Tables/KnownValues.cpp

    @brief Values worth recognising that no SDK header names, like the fill
           patterns debug heaps and runtime checks leave in memory, and the
           exception codes compilers and runtimes raise.

    @details Written by hand, unlike the generated tables beside it, since none
             of these come from a header. Each fill pattern's listed at 32 and
             16 bits. The parser reads a 64-bit one like 0xCCCCCCCCCCCCCCCC as
             the 32-bit one it repeats.

             Keep it sorted by value, as every table must be.

    @author Ged Murphy

    @copyright Copyright (c) 2026 Ged Murphy

    This file is part of WinCode.

    WinCode is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by the
    Free Software Foundation, either version 3 of the License, or (at your
    option) any later version.

    WinCode is distributed in the hope that it will be useful, but
    WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY
    or FITNESS FOR A PARTICULAR PURPOSE. See the GNU General Public License for
    more details.

    You should have received a copy of the GNU General Public License along
    with WinCode. If not, see <https://www.gnu.org/licenses/>.

*/

#include "Tables.hpp"


static const TableEntry k_KnownValueEntries[] =
{
    { 0x0000ABABu, 0, "Past the end of a heap block (0xAB fill, 16 bits)",
      "The debug heap's guard bytes after a HeapAlloc block, read 16 bits at a time. Reading them means reading past the end of an allocation." },
    { 0x0000CCCCu, 0, "Uninitialised stack (0xCC fill, 16 bits)",
      "Stack memory MSVC's run-time checks (/RTC) filled before the code wrote it, read 16 bits at a time: a local variable used before it was set." },
    { 0x0000CDCDu, 0, "Unwritten heap memory (0xCD fill, 16 bits)",
      "Memory the debug CRT allocated but nothing has written yet, read 16 bits at a time: a new object's member used before it was set." },
    { 0x0000DDDDu, 0, "Freed heap memory (0xDD fill, 16 bits)",
      "Memory the debug CRT has freed, read 16 bits at a time: a use after free." },
    { 0x0000FDFDu, 0, "Heap guard bytes (0xFD fill, 16 bits)",
      "The debug CRT's guard bytes either side of an allocation, read 16 bits at a time. Finding them changed means a buffer overrun or underrun." },
    { 0x0000FEEEu, 0, "Freed heap memory (0xFEEE fill, 16 bits)",
      "Memory HeapFree has released, filled by the heap when a debugger is attached, read 16 bits at a time: a use after free." },
    { 0x406D1388u, 0, "MS_VC_EXCEPTION",
      "Raised to tell a debugger a thread's name, the way it was done before SetThreadDescription. Harmless: a debugger takes it and carries on." },
    { 0xABABABABu, 0, "Past the end of a heap block (0xAB fill)",
      "The debug heap's guard bytes after a HeapAlloc block. Reading them means reading past the end of an allocation." },
    { 0xBAADF00Du, 0, "Unwritten HeapAlloc memory (0xBAADF00D)",
      "Memory HeapAlloc or LocalAlloc returned that nothing has written yet, filled by the heap when a debugger is attached." },
    { 0xC06D007Eu, 0, "Delay-load: DLL not found",
      "A delay-loaded DLL couldn't be found when it was first used: VcppException(ERROR_SEVERITY_ERROR, ERROR_MOD_NOT_FOUND). The exception's DelayLoadInfo names the DLL." },
    { 0xC06D007Fu, 0, "Delay-load: function not found",
      "A function in a delay-loaded DLL couldn't be found when it was first called: VcppException(ERROR_SEVERITY_ERROR, ERROR_PROC_NOT_FOUND). The DLL loaded, but doesn't export it." },
    { 0xCCCCCCCCu, 0, "Uninitialised stack (0xCC fill)",
      "Stack memory MSVC's run-time checks (/RTC) filled before the code wrote it: a local variable used before it was set. 0xCC is also the INT 3 instruction, so executing it breaks into the debugger." },
    { 0xCDCDCDCDu, 0, "Unwritten heap memory (0xCD fill)",
      "Memory the debug CRT allocated but nothing has written yet: a new object's member used before it was set." },
    { 0xDDDDDDDDu, 0, "Freed heap memory (0xDD fill)",
      "Memory the debug CRT has freed: a use after free." },
    { 0xDEADBEEFu, 0, "Dead beef (0xDEADBEEF)",
      "A conventional marker many programs write over freed or invalid memory. Not from Windows itself, but it usually means the same thing: memory that should no longer be used." },
    { 0xE0434352u, 0, "EXCEPTION_COMPLUS",
      "A managed (.NET) exception raised by the CLR, 'CCR' in its low bytes. Load SOS and use !pe to see the managed exception." },
    { 0xE0434F4Du, 0, "EXCEPTION_COMPLUS (CLR 1.x and 2.0)",
      "A managed (.NET) exception raised by CLR 1.x or 2.0, 'COM' in its low bytes. Load SOS and use !pe to see the managed exception." },
    { 0xE06D7363u, 0, "EH_EXCEPTION_NUMBER",
      "A C++ exception thrown by code built with MSVC, 'msc' in its low bytes. Its parameters are a magic number, the thrown object and its ThrowInfo; in WinDbg, .exr shows them and !analyze names the type." },
    { 0xFDFDFDFDu, 0, "Heap guard bytes (0xFD fill)",
      "The debug CRT's guard bytes either side of an allocation. Finding them changed means a buffer overrun or underrun." },
    { 0xFEEEFEEEu, 0, "Freed heap memory (0xFEEE fill)",
      "Memory HeapFree has released, filled by the heap when a debugger is attached: a use after free." },
};

const TableSpan g_KnownValueTable =
{
    k_KnownValueEntries,
    ARRAYSIZE(k_KnownValueEntries),
    nullptr
};
