#include <iostream>
#include <elfio/elfio.hpp>
#include <set>
#include <map>
#include <stdexcept>

using namespace ELFIO;

namespace utils {

struct Relocation
{
    Elf64_Addr offset;
    Elf_Word   symbol;
    unsigned   type;
    Elf_Sxword addend;

    Relocation( Elf64_Addr offset_val,
                Elf_Word   symbol_val,
                unsigned   type_val,
                Elf_Sxword addend_val )
    {
        offset = offset_val;
        symbol = symbol_val;
        type   = type_val;
        addend = addend_val;
    }
};

void add_rel_entry( relocation_section_accessor& accessor, Relocation new_reloc )
{
    accessor.add_entry( new_reloc.offset, new_reloc.symbol, new_reloc.type );
}

Relocation get_relocation_by_idx( const relocation_section_accessor& accessor, Elf_Word index )
{
    Relocation entry( 0, 0, 0, 0 );
    if ( !accessor.get_entry( index, entry.offset, entry.symbol, entry.type, entry.addend ) )
        throw std::runtime_error( "No relocation entry with such index!" );

    return entry;
}

std::vector<Relocation> get_reltab_view( const relocation_section_accessor& accessor )
{
    std::vector<Relocation> res;

    for ( int j = 0; j < accessor.get_entries_num(); j++ ) {
        auto reloc = get_relocation_by_idx( accessor, j );
        res.push_back( reloc );
    }

    return res;
}

} // namespace utils