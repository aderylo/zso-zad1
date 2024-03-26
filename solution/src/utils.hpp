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

} // namespace utils