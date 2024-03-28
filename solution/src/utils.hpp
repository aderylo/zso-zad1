#include <iostream>
#include <elfio/elfio.hpp>
#include <set>
#include <map>

using namespace ELFIO;

namespace utils {

struct mapping
{
    std::map<int, int> forward;
    std::map<int, int> backward;

    void swap_values( int fst, int snd )
    {
        int inv_fst = backward[fst];
        int inv_snd = backward[snd];

        forward[inv_fst] = snd;
        forward[inv_snd] = fst;
        backward[fst]    = inv_snd;
        backward[snd]    = inv_fst;
    }

    void insert( int key, int val )
    {
        forward[key]  = val;
        backward[val] = key;
    }
};

enum PointerClass
{
    UNCLASSIFIED = 0,
    TEXT,          // 1: .init .text
    RODATA_OR_GOT, // 2: After .text section, but within segment 00
    BSS_OR_STACK,  // 3: .bss .stack
    DATA           // 4: .data
};

/** Usual layout of sections in segments:
 *  00  : .init .text .rodata .data.rel.ro .got 
 *  01  : .bss .stack
 *  02  : .data
 * 
 * Characteristic flags:
 *  E - if contains .init/.text sections
 *  RW - if contains .data/.data.rel.ro or .bss/.stack
 *  R - if cointains only .rodata
 * 
 *  Type: 
 *  all relevant segments have type LOAD;
 *  
 *  FileSize:
 *  .bss .stack : is always set to zero
 *  
 *  MemSize:
 *  .bss .stack : is never set to zero, rather large;
 * 
*/
PointerClass classify_pointer( const elfio& elf_file,
                               Elf64_Addr   text_start,
                               Elf64_Addr   text_end,
                               Elf64_Addr   addr )
{
    {
        for ( int j = 0; j < elf_file.segments.size(); j++ ) {
            auto segment = elf_file.segments[j];
            auto start   = segment->get_virtual_address();
            auto end     = start + segment->get_memory_size();

            if ( start == 0 || segment->get_memory_size() == 0 )
                continue; // empty or undefined segment

            if ( start <= addr && addr <= end ) {
                // @TODO: implement point scoring system instead of branching logic

                if ( j == 0 && text_start <= addr && addr <= text_end )
                    return TEXT;

                if ( j == 0 && addr >= text_end )
                    return RODATA_OR_GOT;

                if ( j == 1 && segment->get_memory_size() > 0 && segment->get_file_size() == 0 )
                    return BSS_OR_STACK;

                if ( j == 2 )
                    return DATA;
            }
        }

        return UNCLASSIFIED;
    }
}

} // namespace utils