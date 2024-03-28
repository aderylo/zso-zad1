#include <iostream>
#include <elfio/elfio.hpp>
#include <set>
#include <map>

using namespace ELFIO;

namespace utils {

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

Elf_Word add_symbol( symbol_section_accessor& sym_acc,
                 string_section_accessor& str_acc,
                 const Symbol&            symbol )
{
    return sym_acc.add_symbol( str_acc, symbol.name.c_str(), symbol.value, symbol.size, symbol.bind,
                               symbol.type, symbol.other, symbol.section_index );
}

bool get_symbol_by_idx( const symbol_section_accessor& accessor, Elf_Xword index, Symbol& symbol )
{
    return accessor.get_symbol( index, symbol.name, symbol.value, symbol.size, symbol.bind,
                                symbol.type, symbol.section_index, symbol.other );
}

std::vector<Symbol> get_symtab_view( const symbol_section_accessor& accessor )
{
    std::vector<Symbol> res;
    Symbol              symbol;

    for ( int j = 0; j < accessor.get_symbols_num(); j++ ) {
        get_symbol_by_idx( accessor, j, symbol );
        res.push_back( symbol );
    }

    return res;
}

std::vector<Symbol> filter_symtab_view_by_range( const std::vector<Symbol>& symtab_view,
                                                 Elf64_Addr                 lower_bound,
                                                 Elf64_Addr                 upper_bound )
{
    std::vector<Symbol> res;
    auto                isInRange = [lower_bound, upper_bound]( Symbol s ) {
        return ( s.value >= lower_bound && s.value <= upper_bound );
    };

    std::copy_if( symtab_view.begin(), symtab_view.end(), std::back_inserter( res ), isInRange );
    return res;
}

std::vector<Symbol> filter_symtab_view_by_type( const std::vector<Symbol>& symtab_view,
                                                unsigned char              type )
{
    std::vector<Symbol> res;
    auto                isInRange = [type]( Symbol s ) { return ( s.type == type ); };

    std::copy_if( symtab_view.begin(), symtab_view.end(), std::back_inserter( res ), isInRange );
    return res;
}

std::vector<Symbol> get_symtab_view_in_range( const symbol_section_accessor& accessor,
                                              Elf64_Addr                     lower_bound,
                                              Elf64_Addr                     upper_bound )
{
    std::vector<Symbol> result;
    Symbol              symbol;

    for ( int j = 0; j < accessor.get_symbols_num(); j++ ) {
        get_symbol_by_idx( accessor, j, symbol );
        if ( symbol.value >= lower_bound && symbol.value <= upper_bound ) {
            result.push_back( symbol );
        }
    }

    return result;
}

} // namespace utils