#include <iostream>
#include <elfio/elfio.hpp>
#include <set>
#include <map>
#include <regex>

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

    Elf_Word __symtab_idx;
};

Symbol
add_symbol( symbol_section_accessor& sym_acc, string_section_accessor& str_acc, Symbol symbol )
{
    symbol.__symtab_idx =
        sym_acc.add_symbol( str_acc, symbol.name.c_str(), symbol.value, symbol.size, symbol.bind,
                            symbol.type, symbol.other, symbol.section_index );
    return symbol;
}

Symbol add_function_symbol( symbol_section_accessor& sym_acc,
                            string_section_accessor& str_acc,
                            section*                 fn_section )
{
    utils::Symbol fn_symbol;
    fn_symbol.value         = 0x0;
    fn_symbol.name          = fn_section->get_name();
    fn_symbol.bind          = STB_GLOBAL;
    fn_symbol.section_index = fn_section->get_index();
    fn_symbol.size          = fn_section->get_size();
    fn_symbol.type          = STT_FUNC;
    fn_symbol.other         = STV_DEFAULT;

    return add_symbol( sym_acc, str_acc, fn_symbol );
}

Symbol add_rodata_symbol( symbol_section_accessor& sym_acc,
                          string_section_accessor& str_acc,
                          Elf64_Word               original_addr,
                          section*                 ro_object_section )
{
    utils::Symbol symbol;
    symbol.value         = 0x0;
    symbol.name          = std::to_string( original_addr ) + "r";
    symbol.bind          = STB_LOCAL;
    symbol.section_index = ro_object_section->get_index();
    symbol.size          = ro_object_section->get_size();
    symbol.type          = STT_OBJECT;
    symbol.other         = STV_DEFAULT;

    return add_symbol( sym_acc, str_acc, symbol );
}

Symbol add_bss_symbol( symbol_section_accessor& sym_acc,
                       string_section_accessor& str_acc,
                       Elf64_Word               original_addr,
                       section*                 bss_obj_section )
{
    utils::Symbol symbol;
    symbol.value         = 0x0;
    symbol.name          = std::to_string( original_addr ) + "B";
    symbol.bind          = STB_GLOBAL;
    symbol.section_index = bss_obj_section->get_index();
    symbol.size          = bss_obj_section->get_size();
    symbol.type          = STT_OBJECT;
    symbol.other         = STV_DEFAULT;

    return add_symbol( sym_acc, str_acc, symbol );
}

Symbol add_data_symbol( symbol_section_accessor& sym_acc,
                        string_section_accessor& str_acc,
                        Elf64_Word               original_addr,
                        section*                 data_obj_section )
{
    utils::Symbol symbol;
    symbol.value         = 0x0;
    symbol.name          = std::to_string( original_addr ) + "d";
    symbol.bind          = STB_LOCAL;
    symbol.section_index = data_obj_section->get_index();
    symbol.size          = data_obj_section->get_size();
    symbol.type          = STT_OBJECT;
    symbol.other         = STV_DEFAULT;

    return add_symbol( sym_acc, str_acc, symbol );
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
        symbol.__symtab_idx = j;
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
    auto                isOfType = [type]( Symbol s ) { return ( s.type == type ); };

    std::copy_if( symtab_view.begin(), symtab_view.end(), std::back_inserter( res ), isOfType );
    return res;
}

std::vector<Symbol> filter_symtab_view_by_regex( const std::vector<Symbol>& symtab_view,
                                                 const std::string&         pattern_str )
{
    std::vector<Symbol> res;
    std::regex          pattern( pattern_str );
    auto nameMatches = [pattern]( Symbol s ) { return ( std::regex_match( s.name, pattern ) ); };

    std::copy_if( symtab_view.begin(), symtab_view.end(), std::back_inserter( res ), nameMatches );
    return res;
}

std::vector<Symbol> filter_symtab_view_by_contains_addr( const std::vector<Symbol>& symtab_view,
                                                         Elf64_Addr                 addr )
{
    std::vector<Symbol> res;
    auto contains = [addr]( Symbol s ) { return ( s.value <= addr && addr <= s.value + s.size ); };

    std::copy_if( symtab_view.begin(), symtab_view.end(), std::back_inserter( res ), contains );
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