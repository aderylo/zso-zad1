#include <iostream>
#include <elfio/elfio.hpp>
#include <set>
#include <map>

using namespace ELFIO;

namespace utils {

struct Relocation
{
    Elf64_Addr offset;
    Elf_Word   symbol;
    unsigned   type;
    Elf_Sxword addend;
};

struct Symbol
{
    std::string   name;
    Elf64_Addr    value;
    Elf_Xword     size;
    unsigned char bind;
    unsigned char type;
    Elf_Half      section_index;
    unsigned char other;
};

struct Section
{
    std::string name;
    Elf_Word    type;
    Elf_Xword   flags;
    Elf64_Addr  addr;
    Elf_Word    link;
    Elf_Xword   addralign;
    Elf_Xword   entsize;
};

section* add_section( elfio& elf_file, const Section& sec_hdr )
{
    section* psec = elf_file.sections.add( sec_hdr.name );

    psec->set_type( sec_hdr.type );
    psec->set_flags( sec_hdr.flags );
    psec->set_address( sec_hdr.addr );
    psec->set_link( sec_hdr.link );
    psec->set_addr_align( sec_hdr.addralign );
    psec->set_entry_size( sec_hdr.entsize );

    return psec;
}

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

bool get_entry_wrapper( const relocation_section_accessor& accessor,
                        Elf_Word                           index,
                        Relocation&                        entry )
{
    return accessor.get_entry( index, entry.offset, entry.symbol, entry.type, entry.addend );
}

bool get_symbol_wrapper( const symbol_section_accessor& accessor,
                         Elf_Xword                      index,
                         Symbol&                        symbolInfo )
{
    return accessor.get_symbol( index, symbolInfo.name, symbolInfo.value, symbolInfo.size,
                                symbolInfo.bind, symbolInfo.type, symbolInfo.section_index,
                                symbolInfo.other );
}

Elf_Xword get_symbol_by_refrence( const symbol_section_accessor& accessor,
                                  Elf64_Addr                     value,
                                  Elf_Xword                      size,
                                  Symbol&                        symbol )
{
    for ( int i = 0; i < accessor.get_symbols_num(); i++ ) {
        get_symbol_wrapper( accessor, i, symbol );
        if ( symbol.size == size && symbol.value == value ) {
            return i;
        }
    }

    return -1;
    ;
}

std::vector<Symbol> get_symbols_in_range( const symbol_section_accessor& accessor,
                                          Elf64_Addr                     lower_bound,
                                          Elf64_Addr                     upper_bound )
{
    std::vector<Symbol> result;
    Symbol              symbol;

    for ( int j = 0; j < accessor.get_symbols_num(); j++ ) {
        utils::get_symbol_wrapper( accessor, j, symbol );
        if ( symbol.value >= lower_bound && symbol.value <= upper_bound ) {
            result.push_back( symbol );
        }
    }

    return result;
}

void configure_section_header( section*   section,
                               Elf_Word   type,
                               Elf_Xword  flags,
                               Elf64_Addr addr,
                               Elf_Word   link,
                               Elf_Xword  addralign,
                               Elf_Xword  entsize )
{
    section->set_type( type );
    section->set_flags( flags );
    section->set_address( addr );
    section->set_link( link );
    section->set_addr_align( addralign );
    section->set_entry_size( entsize );
}

std::vector<section*> get_sections_by_type( const elfio& elf_file, Elf_Word type )
{
    std::vector<section*> result;
    Elf_Half              sec_num = elf_file.sections.size();

    for ( int i = 0; i < sec_num; ++i ) {
        section* psec = elf_file.sections[i];
        if ( type == psec->get_type() ) {
            result.push_back( psec );
        }
    }

    return result;
}

} // namespace utils