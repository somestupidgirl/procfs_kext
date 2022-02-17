/*
kextgizmos osdictionary_util


Dual-licensed under the MIT and zLib licenses.


Copyright 2018 Phillip & Laura Dennis-Jordan

Permission is hereby granted, free of charge, to any person obtaining a copy of
this software and associated documentation files (the "Software"), to deal in
the Software without restriction, including without limitation the rights to
use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies
of the Software, and to permit persons to whom the Software is furnished to do
so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all
copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
SOFTWARE.



Copyright (c) 2018 Phillip & Laura Dennis-Jordan

This software is provided 'as-is', without any express or implied warranty. In
no event will the authors be held liable for any damages arising from the use
of this software.

Permission is granted to anyone to use this software for any purpose, including
commercial applications, and to alter it and redistribute it freely, subject
to the following restrictions:

1. The origin of this software must not be misrepresented; you must not claim
that you wrote the original software. If you use this software in a product,
an acknowledgment in the product documentation would be appreciated but is not
required.

2. Altered source versions must be plainly marked as such, and must not be
misrepresented as being the original software.

3. This notice may not be removed or altered from any source distribution.

*/

#include "osdictionary_util.hpp"
#include <libkern/c++/OSDictionary.h>

#define KLog(...) ({ IOLog(__VA_ARGS__); kprintf(__VA_ARGS); })

void log_dictionary_contents(OSDictionary* table)
{
	KLog("MCTTrigger6USB::matchPropertyTable:\n");
	if (table == nullptr)
	{
		KLog("[NULL dictionary]\n");
	}
	else
	{
		OSCollectionIterator* iter = OSCollectionIterator::withCollection(table);
		
		while (OSObject* obj = iter->getNextObject())
		{
			OSString* key = OSDynamicCast(OSString, obj);
			if (key != nullptr)
			{
				OSObject* obj = table->getObject(key);
				OSNumber* num = OSDynamicCast(OSNumber, obj);
				OSBoolean* boolean = OSDynamicCast(OSBoolean, obj);
				OSString* str = OSDynamicCast(OSString, obj);
				if (num != nullptr)
				{
					KLog("'%s' -> %llu (0x%llx, %lld)\n", key->getCStringNoCopy(), num->unsigned64BitValue(), num->unsigned64BitValue(), num->unsigned64BitValue());
				}
				else if (boolean != nullptr)
				{
					KLog("'%s' -> %s\n", key->getCStringNoCopy(), boolean->getValue() ? "true" : "false");
				}
				else if (str != nullptr)
				{
					KLog("'%s' -> '%s'\n", key->getCStringNoCopy(), str->getCStringNoCopy());
				}
				else if (obj == nullptr)
				{
					KLog("'%s' -> NULL\n", key->getCStringNoCopy());
				}
				else
				{
					KLog("'%s' -> [%s @ %p]\n", key->getCStringNoCopy(), obj->getMetaClass()->getClassName(), obj);
				}
			}
			else
			{
				KLog("[%s @ %p]\n", obj->getMetaClass()->getClassName(), obj);
			}
		}
		OSSafeReleaseNULL(iter);
	}
}
