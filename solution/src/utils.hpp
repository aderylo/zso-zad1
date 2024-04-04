#include <iostream>
#include <elfio/elfio.hpp>
#include <set>
#include <map>
#include <iomanip>
#include <sstream>

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

std::string formatTo08x( int number )
{
    std::stringstream ss;
    ss << std::setw( 8 ) << std::setfill( '0' ) << std::hex << number;
    return ss.str();
}

struct ElfSectionLayout
{
    Elf64_Addr  addr;
    Elf_Xword   size;
    const char* data;
};

struct ElfMemoryLayout
{
    ElfSectionLayout text;
    ElfSectionLayout rodata;
    ElfSectionLayout got;
    ElfSectionLayout bss;
    ElfSectionLayout stack;
    ElfSectionLayout data;
};

ElfMemoryLayout reconstructMemoryLayout( const elfio& elf_file )
{
    ElfMemoryLayout       layout;
    std::vector<section*> sections;
    Elf64_Addr            addr, size;
    const char*           data_ptr;

    // asuming file has section table
    // -- TEXT:
    addr     = SIZE_MAX;
    size     = 0;
    sections = utils::get_sections_by_flags( elf_file, SHF_EXECINSTR | SHF_ALLOC );
    for ( const auto& sec : sections ) {
        if ( addr > sec->get_address() ) {
            addr     = sec->get_address();
            data_ptr = sec->get_data();
        }
        size += sec->get_size();
    }
    layout.text = { addr, size, data_ptr };

    // -- RODATA (assuming .data.rel.ro is .ro)
    addr     = SIZE_MAX;
    size     = 0;
    sections = utils::get_sections_by_regex( elf_file, "^(?!.*rel).*\\.ro.*" );
    for ( const auto& sec : sections ) {
        if ( addr > sec->get_address() ) {
            addr     = sec->get_address();
            data_ptr = sec->get_data();
        }
        size += sec->get_size();
    }
    layout.rodata = { addr, size, data_ptr };

    // -- GOT
    addr     = SIZE_MAX;
    size     = 0;
    sections = utils::get_sections_by_regex( elf_file, "^(?!.*rel).*\\.got.*" );
    for ( const auto& sec : sections ) {
        if ( addr > sec->get_address() ) {
            addr     = sec->get_address();
            data_ptr = sec->get_data();
        }
        size += sec->get_size();
    }
    layout.got = { addr, size, data_ptr };

    // -- BSS
    addr     = SIZE_MAX;
    size     = 0;
    sections = utils::get_sections_by_regex( elf_file, "^(?!.*rel).*\\.bss.*" );
    for ( const auto& sec : sections ) {
        if ( addr > sec->get_address() ) {
            addr     = sec->get_address();
            data_ptr = sec->get_data();
        }
        size += sec->get_size();
    }
    layout.bss = { addr, size, data_ptr };

    // -- STACK
    addr     = SIZE_MAX;
    size     = 0;
    sections = utils::get_sections_by_regex( elf_file, "^(?!.*rel).*\\.stack.*" );
    for ( const auto& sec : sections ) {
        if ( addr > sec->get_address() ) {
            addr     = sec->get_address();
            data_ptr = sec->get_data();
        }
        size += sec->get_size();
    }
    layout.stack = { addr, size, data_ptr };

    // -- DATA
    addr     = SIZE_MAX;
    size     = 0;
    sections = utils::get_sections_by_regex( elf_file, "^(?!.*rel).*\\.data.*" );
    for ( const auto& sec : sections ) {
        std::cout << sec->get_name() << std::endl;
        if ( addr > sec->get_address() ) {
            addr     = sec->get_address();
            data_ptr = sec->get_data();
        }
        size += sec->get_size();
    }
    layout.data = { addr, size, data_ptr };

    return layout;
}

enum PointerClass
{
    UNCLASSIFIED = 0,
    TEXT,   // 1: .init .text
    RODATA, // 2: After .text section, but within segment 00
    GOT,    // 3: idk how to tell it but will see;
    BSS,    // 4: .bss
    STACK,  // 5: .stack
    DATA    // 6: .data
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
PointerClass classify_pointer( ElfMemoryLayout layout, Elf64_Addr addr )
{
    if ( layout.text.addr <= addr && addr <= layout.text.addr + layout.text.size )
        return PointerClass::TEXT;
    if ( layout.rodata.addr <= addr && addr <= layout.rodata.addr + layout.rodata.size )
        return PointerClass::RODATA;
    if ( layout.got.addr <= addr && addr <= layout.got.addr + layout.got.size )
        return PointerClass::GOT;
    if ( layout.bss.addr <= addr && addr <= layout.bss.addr + layout.bss.size )
        return PointerClass::BSS;
    if ( layout.stack.addr <= addr && addr <= layout.stack.addr + layout.stack.size )
        return PointerClass::STACK;
    if ( layout.data.addr <= addr && addr <= layout.data.addr + layout.data.size )
        return PointerClass::DATA;

    return PointerClass::UNCLASSIFIED;
}

} // namespace utils