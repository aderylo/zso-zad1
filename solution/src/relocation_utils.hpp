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

void add_relocation( relocation_section_accessor& accessor, Relocation new_reloc )
{
    accessor.add_entry( new_reloc.offset, new_reloc.symbol, new_reloc.type, new_reloc.addend );
}

bool get_relocation_by_idx( const relocation_section_accessor& accessor,
                            Elf_Word                           index,
                            Relocation&                        entry )
{
    return accessor.get_entry( index, entry.offset, entry.symbol, entry.type, entry.addend );
}

std::vector<Relocation> get_reltab_view( const relocation_section_accessor& accessor )
{
    std::vector<Relocation> res;
    Relocation              reloc;

    for ( int j = 0; j < accessor.get_entries_num(); j++ ) {
        get_relocation_by_idx( accessor, j, reloc );
        res.push_back( reloc );
    }

    return res;
}

} // namespace utils