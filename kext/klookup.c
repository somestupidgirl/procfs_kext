//
// klookup.c
//
// Kernel symbol lookup function
//
////////////////////////////////////////////////////////////////////////////////
// License: 0BSD                                                              //
//                                                                            //
// Copyright (C) 2021 by Syncretic                                            //
//                                                                            //
// Permission to use, copy, modify, and/or distribute this software for any   //
// purpose with or without fee is hereby granted.                             //
//                                                                            //
// THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES   //
// WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF           //
// MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR    //
// ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES     //
// WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN      //
// ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF OR //
// IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.                //
//                                                                            //
////////////////////////////////////////////////////////////////////////////////

#include "klookup.h"
#include <IOKit/IOLib.h>
#include <mach-o/nlist.h>

#define KERNEL_BASE           0xffffff8000200000   // Base address of the kernel, per the Mach-O file on disk
#define LB_SEG_PRELINK_TEXT   "__PRELINK_TEXT"     // Segment name for PRELINK_TEXT (used by Big Sur and later)

//////////////////////////////////////////////////////////////////////
//
// Find a 64-bit segment by name
//
//////////////////////////////////////////////////////////////////////
static struct segment_command_64 *FindSegment64(struct mach_header_64 *MachHeader, const char *SegmentName)
{
   struct   load_command         *LoadCommands;
   struct   segment_command_64   *Segment = NULL;
   struct   segment_command_64   *tmpSegment;

   // Get the address of the list of load commands
   LoadCommands = (struct load_command *)((uint64_t)MachHeader + sizeof(struct mach_header_64));
   while ((uint64_t)LoadCommands < (uint64_t)MachHeader + (uint64_t)MachHeader->sizeofcmds)
   {
      if (LoadCommands->cmd == LC_SEGMENT_64)
      {
         // evaluate segment
         tmpSegment = (struct segment_command_64 *)LoadCommands;
         if (!strcmp(tmpSegment->segname, SegmentName))
         {
            Segment = tmpSegment;
            break;
         }
      }
        
      // next load command
      LoadCommands = (struct load_command *)((uint64_t)LoadCommands + (uint64_t)LoadCommands->cmdsize);
   }

   return Segment;
}


//////////////////////////////////////////////////////////////////////
//
// Find the address of a symbol by name.
//
// Returns the symbol's associated address, or
//         NULL if <symbol> was not found.
//
//////////////////////////////////////////////////////////////////////
void *SymbolLookup(const char *Symbol)
{
   // For effiency, we keep SymbolTable, StringTable, and NameList across invocations
   static struct  symtab_command    *SymbolTable = NULL;
   static void                      *StringTable = NULL;
   static struct  nlist_64          *NameList = NULL;
   vm_offset_t                      SlideAddress = 0;
   int64_t                          Slide;
   struct      mach_header_64       *MachHeader;
   struct      segment_command_64   *LinkEdit;
   struct      load_command         *LoadCommands;
   struct      nlist_64             *tmpNameList;
   void                             *Address;
   uint64_t                         i;

   //
   // First, see if we've already found the symbol table and name list.
   // (For effiency, we only parse the kernel's Mach-O structure once.)
   //
   if (SymbolTable == NULL || StringTable == NULL || NameList == NULL)
   {
      //
      // Calculate the kernel slide (ASLR):
      //
      // Get the un-slid address of the printf function
      //
      vm_kernel_unslide_or_perm_external((unsigned long long)(void *)printf, &SlideAddress);
      //
      // Now calculate the difference between that and the slid address of printf
      //
      Slide = (long long)(void *)printf - SlideAddress;
      //
      // Now that we know the slide, we can figure out where the kernel's Mach-O structure is.
      //
      MachHeader = (struct mach_header_64 *)(Slide + KERNEL_BASE);

      // Check for a valid MACH-O header
      if (MachHeader->magic != MH_MAGIC_64)
      {
         printf("\n\n****** ********* ********* Latebloom KLOOKUP: BAD MAGIC HEADER\n\n");
         IOLog("latebloom: Bad Mach-O Magic Header\n");
         return NULL;
      }

      //
      // If there's a __PRELINK_TEXT segment and it contains a vmaddr, we use that as the
      // starting point to search for the __LINKEDIT segment.  (This appears to be a Big Sur
      // addition, copying modified segments (such as __LINKEDIT) into a normally-empty
      // __PRELINK_TEXT segment in memory;  the __PRELINK_TEXT segment is present in the
      // Mach-O kernel file, but it's empty on disk.)
      //
      // Since __PRELINK_TEXT is apparently present in Catalina (and earlier?), but seems to
      // be bogus (creates page faults) in those versions, we check for OS versions >= Big Sur.
      //
      if (version_major >= BIGSUR_XNU_MAJOR_VERSION)        // Only look at __PRELINK_TEXT on BS or later (Darwin version 20+)
      {
         // Find __PRELINK_TEXT
         if ((LinkEdit = FindSegment64(MachHeader, LB_SEG_PRELINK_TEXT)) != NULL)
         {
            // If we found it, use its vmaddr as our Mach-O header
            MachHeader = (struct mach_header_64 *)(LinkEdit->vmaddr);
         }
         // If we didn't find __PRELINK_TEXT, just use the original Mach-O header.
      }

      // find the __LINKEDIT segment
      if (!(LinkEdit = FindSegment64(MachHeader, SEG_LINKEDIT)))
      {
         printf("\n\n****** ********* ********* Latebloom KLOOKUP: __LINKEDIT NOT FOUND\n\n");
         IOLog("latebloom: __LINKEDIT not found\n");
         return NULL;
      }

      //
      // Find the symbol table (LC_SYMTAB command)
      //
      SymbolTable = NULL;              // assume failure
      // Get the list of load commands
      LoadCommands = (struct load_command *)((uint64_t)MachHeader + sizeof(struct mach_header_64));
      // Loop through the list of load commands, looking for LC_SYMTAB
      while ((uint64_t)LoadCommands < (uint64_t)MachHeader + (uint64_t)MachHeader->sizeofcmds)
      {
         if (LoadCommands->cmd == LC_SYMTAB)
         {
            // Found the symbol table
            SymbolTable = (struct symtab_command *)LoadCommands;
            break;
         }
         // Jump to the next load command
         LoadCommands = (struct load_command *)((uint64_t)LoadCommands + (uint64_t)LoadCommands->cmdsize);
      }

      // Did we find the symbol table?
      if (SymbolTable == NULL)
      {
         printf("\n\n****** ********* ********* Latebloom KLOOKUP: LC_SYMTAB NOT FOUND\n\n");
         IOLog("latebloom: LC_SYMTAB not found\n");
         return NULL;
      }

      // Get the address of the string table
      StringTable = (void *)((int64_t)(LinkEdit->vmaddr - LinkEdit->fileoff) + SymbolTable->stroff);
      // Get the address of the name list
      NameList = (struct nlist_64 *)((int64_t)(LinkEdit->vmaddr - LinkEdit->fileoff) + SymbolTable->symoff);
   }  // End if (SymbolTable == NULL || StringTable == NULL || NameList == NULL)

   //
   // Now loop through the name list until we find a match for <Symbol>
   //
   for (i = 0, Address = NULL, tmpNameList = NameList; i < SymbolTable->nsyms; ++i, ++tmpNameList)
   {
      char *str = (char *)(StringTable + tmpNameList->n_un.n_strx);

      if (!strcmp(str, Symbol))
      {
         // Found it - save its associated value (address)
         Address = (void *)tmpNameList->n_value;
         break;
      }
   }

   // Did we find <Symbol> in the name list?
   if (Address == NULL)
   {
      printf("\n\n****** ********* ********* Latebloom KLOOKUP: SYMBOL '%s' NOT FOUND\n\n", Symbol);
      IOLog("latebloom: Symbol '%s' not found\n", Symbol);
   }

   // Return either <Symbol>'s associated address, or NULL if we didn't find it.
   return Address;
}
