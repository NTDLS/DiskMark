///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//  Copyright © NetworkDLS 2023, All rights reserved
//
// THIS CODE AND INFORMATION IS PROVIDED "AS IS" WITHOUT WARRANTY OF 
// ANY KIND, EITHER EXPRESSED OR IMPLIED, INCLUDING BUT NOT LIMITED TO 
// THE IMPLIED WARRANTIES OF MERCHANTABILITY AND/OR FITNESS FOR A 
// PARTICULAR PURPOSE.
///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
#ifndef _NSWFL_H_
#define _NSWFL_H_
///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

#include <Windows.H>
#include <Stdio.H>
#include <Stdlib.H>

//The memory pool class is used to track memory allocations to ease the task of memory leak detection. It can be removed from release code with preprocessors.
#include "NSWFL_MemoryPool.H"

//Type conversions and parsers. Dates, strings, int, float, boolean, you name it.
#include "NSWFL_Conversion.H"

//Functions for working with WinAPI list boxes.
#include "NSWFL_ListBox.H"

//Its math. What did you expect?
#include "NSWFL_Math.H"

//Memory operations, clear, set, copy, etc.
#include "NSWFL_Memory.H"

//Everything you need to access the registry.
#include "NSWFL_Registry.H"

//String manipulation.
#include "NSWFL_String.H"

//Lots of functions for managing windows or anything with an HWND.
#include "NSWFL_Windows.H"

//File access functions.
#include "NSWFL_File.H"

//Debugging functions/
#include "NSWFL_Debug.H"

//It's a high precision timer.
#include "NSWFL_HighPrecisionTimer.H"

//A reasonable string builder for C++, finally!
#include "NSWFL_StringBuilder.H"

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
#endif
