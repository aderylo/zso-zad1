#ifndef DISASSEMBLE_HPP
#define DISASSEMBLE_HPP

#include <vector>
#include <map>

#include <utils.hpp>
#include <section_utils.hpp>
#include <symbol_utils.hpp>
#include <relocation_utils.hpp>

bool dissect_section_template( elfio&                     dst,
                               section*                   dst_symtab,
                               section*                   dst_strtab,
                               section*                   src_sec,
                               utils::Section             sec_hdr_template,
                               std::string                sec_name_prefix,
                               std::vector<utils::Symbol> src_symbols,
                               std::map<int, int>&        virtual_addr_map )
{
    symbol_section_accessor dst_symtab_acc( dst, dst_symtab );
    string_section_accessor dst_strtab_acc( dst_strtab );

    Elf64_Addr start  = src_sec->get_address();
    Elf64_Addr end    = src_sec->get_address() + src_sec->get_size();
    Elf64_Addr offset = 0, chunk_size = 0;
    section*   new_sec;

    for ( auto sym : src_symbols ) {
        if ( sym.size == 0 )
            continue;

        if ( ( start + offset ) < sym.value ) {
            // add empty space before symbol refrence
            sec_hdr_template.name = sec_name_prefix + std::to_string( start + offset );
            sec_hdr_template.addr = 0x0;
            chunk_size            = sym.value - ( start + offset );

            new_sec                                = utils::add_section( dst, sec_hdr_template );
            virtual_addr_map[new_sec->get_index()] = ( start + offset );

            new_sec->set_data( src_sec->get_data() + offset, chunk_size );
            utils::add_function_symbol( dst_symtab_acc, dst_strtab_acc, new_sec );
            offset += chunk_size;
        }

        if ( start + offset == sym.value && sym.value + sym.size <= end ) { // add function
            sec_hdr_template.name = sec_name_prefix + sym.name;
            sec_hdr_template.addr = 0x0;

            new_sec                                = utils::add_section( dst, sec_hdr_template );
            virtual_addr_map[new_sec->get_index()] = sym.value;

            new_sec->set_data( src_sec->get_data() + offset, sym.size );
            utils::add_function_symbol( dst_symtab_acc, dst_strtab_acc, new_sec );
            offset += sym.size;
        }
    }

    if ( start + offset < end ) { // add any leftovers
        sec_hdr_template.name = sec_name_prefix + std::to_string( start + offset );
        sec_hdr_template.addr = 0x0;

        new_sec                                = utils::add_section( dst, sec_hdr_template );
        virtual_addr_map[new_sec->get_index()] = start + offset;

        new_sec->set_data( src_sec->get_data() + offset, end - ( start + offset ) );
        utils::add_function_symbol( dst_symtab_acc, dst_strtab_acc, new_sec );
    }

    return true;
}

bool recreate_functions( const elfio&        src,
                         elfio&              dst,
                         section*            dst_symtab,
                         section*            dst_strtab,
                         std::map<int, int>& virtual_addr_map )
{
    auto text_sections = utils::get_sections_by_flags( src, SHF_ALLOC | SHF_EXECINSTR );
    auto sym_sections  = utils::get_sections_by_type( src, SHT_SYMTAB );

    for ( auto text_sec : text_sections ) {
        Elf64_Addr start  = text_sec->get_address();
        Elf64_Addr end    = text_sec->get_address() + text_sec->get_size();
        Elf64_Addr offset = 0, chunk_size = 0;

        section*       new_sec;
        utils::Section new_sec_hdr;
        new_sec_hdr.type      = SHT_PROGBITS;
        new_sec_hdr.flags     = ( SHF_ALLOC | SHF_EXECINSTR );
        new_sec_hdr.link      = 0x0;
        new_sec_hdr.addralign = text_sec->get_addr_align();
        new_sec_hdr.entsize   = text_sec->get_entry_size();
        new_sec_hdr.info      = 0x0;

        // get all symbols that point to functions in exectuable sections
        std::vector<utils::Symbol> symbols;

        auto processSymSection = [&]( auto& sym_sec, std::vector<utils::Symbol>& result ) {
            symbol_section_accessor sym_sec_acc( src, sym_sec );
            auto                    sv = utils::get_symtab_view( sym_sec_acc );
            sv                         = utils::filter_symtab_view_by_range( sv, start, end );
            sv                         = utils::filter_symtab_view_by_type( sv, STT_FUNC );
            std::copy_if( sv.begin(), sv.end(), std::back_inserter( result ),
                          []( const utils::Symbol& sym ) { return sym.size != 0; } );
        };
        std::for_each( sym_sections.begin(), sym_sections.end(),
                       [&]( auto& sym_section ) { processSymSection( sym_section, symbols ); } );
        std::sort( symbols.begin(), symbols.end(),
                   []( const utils::Symbol& a, const utils::Symbol& b ) {
                       return ( a.value == b.value ) ? ( a.size > b.size ) : ( a.value < b.value );
                   } );

        // disect .text into functions
        dissect_section_template( dst, dst_symtab, dst_strtab, text_sec, new_sec_hdr, ".text.",
                                  symbols, virtual_addr_map );
    }

    return true;
}

#endif // DISASSEMBLE_HPP