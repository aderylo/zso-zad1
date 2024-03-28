#include <iostream>
#include <set>
#include <map>
#include <elfio/elfio.hpp>
#include <section_utils.hpp>
#include <symbol_utils.hpp>
#include <relocation_utils.hpp>
#include <algorithm>
#include <utils.hpp>

using namespace ELFIO;

#define print( x ) std::cout << ( x ) << std::endl

bool disect_section_template( elfio&                     dst,
                              section*                   src_sec,
                              utils::Section             sec_hdr_template,
                              std::string                sec_name_prefix,
                              std::vector<utils::Symbol> symbols )
{
    Elf64_Addr start  = src_sec->get_address();
    Elf64_Addr end    = src_sec->get_address() + src_sec->get_size();
    Elf64_Addr offset = 0, chunk_size = 0;
    section*   new_sec;

    for ( auto sym : symbols ) {
        if ( sym.size == 0 )
            continue;

        if ( ( start + offset ) < sym.value ) {
            // add empty space before symbol refrence
            sec_hdr_template.name = sec_name_prefix + std::to_string( start + offset );
            sec_hdr_template.addr = ( start + offset );
            chunk_size            = sym.value - ( start + offset );

            new_sec = utils::add_section( dst, sec_hdr_template );
            new_sec->set_data( src_sec->get_data() + offset, chunk_size );
            offset += chunk_size;
        }

        if ( start + offset == sym.value && sym.value + sym.size <= end ) { // add function
            sec_hdr_template.name = sec_name_prefix + sym.name;
            sec_hdr_template.addr = sym.value;

            new_sec = utils::add_section( dst, sec_hdr_template );
            new_sec->set_data( src_sec->get_data() + offset, sym.size );
            offset += sym.size;
        }
    }

    if ( start + offset < end ) { // add any leftovers
        sec_hdr_template.name = sec_name_prefix + std::to_string( start + offset );
        sec_hdr_template.addr = start + offset;

        new_sec = utils::add_section( dst, sec_hdr_template );
        new_sec->set_data( src_sec->get_data() + offset, end - ( start + offset ) );
    }

    return true;
}

bool recreate_functions( const elfio& src, elfio& dst )
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
        disect_section_template( dst, text_sec, new_sec_hdr, ".text.", symbols );
    }

    return true;
}

bool is_addr_inside_function( const elfio& elf_file, Elf64_Addr addr )
{
    for ( int j = 0; j < elf_file.segments.size(); j++ ) {
        auto pseg  = elf_file.segments[j];
        auto start = pseg->get_virtual_address();
        auto end   = start + pseg->get_memory_size();

        if ( start <= addr && addr <= end && ( pseg->get_flags() & SHF_EXECINSTR ) ) {
            return true;
        }
    }

    return false;
}

std::vector<utils::Relocation> get_merged_reltab_views( const elfio& elf_file )
{
    auto src_reloc_secs = utils::get_sections_by_type( elf_file, SHT_REL );

    std::vector<utils::Relocation> src_reltab_views_merged;

    for ( auto src_rel_sec : src_reloc_secs ) {
        auto acc             = relocation_section_accessor( elf_file, src_rel_sec );
        auto src_reltab_view = utils::get_reltab_view( acc );
        src_reltab_views_merged.insert( src_reltab_views_merged.end(), src_reltab_view.begin(),
                                        src_reltab_view.end() );
    }

    return src_reltab_views_merged;
}

std::vector<utils::Symbol> get_first_symtab_view( const elfio& elf_file )
{
    auto src_symtabs = utils::get_sections_by_type( elf_file, SHT_SYMTAB );
    if ( src_symtabs.empty() )
        return {};
    else {
        auto src_symtab_acc = symbol_section_accessor( elf_file, src_symtabs.front() );
        return utils::get_symtab_view( src_symtab_acc );
    }
}

void create_relocs( const elfio& src, elfio& dst, section* dst_symtab, section* dst_strtab )
{
    std::cout << std::hex;
    auto fun_sections   = utils::get_sections_by_flags( dst, SHF_ALLOC | SHF_EXECINSTR );
    auto dst_symtab_acc = symbol_section_accessor( dst, dst_symtab );
    auto dst_strtab_acc = string_section_accessor( dst_strtab );

    for ( auto fun_sec : fun_sections ) {
        Elf64_Addr fn_start    = fun_sec->get_address();
        Elf64_Addr fn_end      = fn_start + fun_sec->get_size();
        auto       new_rel_sec = utils::add_rel_section( dst, ".rel" + fun_sec->get_name(),
                                                         dst_symtab->get_index(), fun_sec->get_index() );
        auto       new_rel_acc = relocation_section_accessor( dst, new_rel_sec );

        // here you would dissasemble function and look for
        // other function calls and such, for now use provided relocs
        // Asumption there is only one symtab;
        std::vector<utils::Relocation> src_reltab_views_merged = get_merged_reltab_views( src );
        std::vector<utils::Symbol>     src_symtab_view         = get_first_symtab_view( src );

        for ( auto src_rel : src_reltab_views_merged ) {
            if ( !( src_rel.offset >= fn_start && src_rel.offset <= fn_end ) )
                continue;

            // resolve relocation to get address
            auto src_symbol = src_symtab_view[src_rel.symbol];
            auto virt_addr  = src_symbol.value;
            std::cout << "x";
            std::cout << virt_addr << std::endl;

            if ( is_addr_inside_function( src, virt_addr ) ) {
                // find called function in new file
                std::cout << "h";
                std::cout << virt_addr << std::endl;
                section* called_fn;
                auto     x = utils::get_sections_containing_addr( dst, virt_addr );
                if ( x.empty() )
                    continue;
                else
                    called_fn = x.front();

                std::cout << called_fn->get_name() << std::endl;

                // create symbol for it
                utils::Symbol fn_symbol;
                fn_symbol.value         = 0x0;
                fn_symbol.name          = called_fn->get_name();
                fn_symbol.bind          = STB_GLOBAL;
                fn_symbol.section_index = called_fn->get_index();
                fn_symbol.size          = called_fn->get_size();
                fn_symbol.type          = STT_FUNC;
                fn_symbol.other         = STV_DEFAULT;
                auto sidx = utils::add_symbol( dst_symtab_acc, dst_strtab_acc, fn_symbol );

                std::cout << fn_symbol.size << " " << virt_addr << std::endl;

                // create relocation to that symbol
                utils::Relocation new_rel_entry;
                new_rel_entry.offset = src_rel.offset - fn_start;
                new_rel_entry.type   = src_rel.type;
                new_rel_entry.symbol = sidx;

                new_rel_acc.add_entry( new_rel_entry.offset, new_rel_entry.symbol,
                                       new_rel_entry.type );
            }
        }
    }
}

int main( int argc, char** argv )
{
    if ( argc != 2 ) {
        std::cout << "Usage: add_section <elf_file>" << std::endl;
        return 1;
    }

    elfio reader;
    elfio writer;

    // Load ELF data
    if ( !reader.load( argv[1] ) ) {
        std::cout << "Can't find or process ELF file " << argv[1] << std::endl;
        return 2;
    }

    // Create header
    writer.create( reader.get_class(), reader.get_encoding() );
    writer.set_os_abi( reader.get_os_abi() );
    writer.set_type( ET_REL );
    writer.set_machine( reader.get_machine() );
    writer.set_entry( 0x0 );

    section* old_symtab_sec = utils::get_sections_by_type( reader, SHT_SYMTAB ).front();
    section* new_strtab_sec = writer.sections.add( ".strtab" );
    section* new_symtab_sec = writer.sections.add( ".symtab" );

    utils::configure_section_header( new_strtab_sec, SHT_STRTAB, 0x0, 0x0, 0x0, 0x1, 0x0 );
    utils::configure_section_header( new_symtab_sec, SHT_SYMTAB, 0x0, 0x0,
                                     new_strtab_sec->get_index(), old_symtab_sec->get_addr_align(),
                                     old_symtab_sec->get_entry_size() );

    symbol_section_accessor old_symbols( reader, old_symtab_sec );
    symbol_section_accessor new_symbols( writer, new_symtab_sec );
    string_section_accessor new_strings( new_strtab_sec );

    recreate_functions( reader, writer );
    create_relocs( reader, writer, new_symtab_sec, new_strtab_sec );

    // parse_rodata_sections( reader, writer );
    // parse_data_sections( reader, writer );
    // parse_bss_sections( reader, writer );
    // copy_symtab( reader, writer, new_symbols, new_strings );
    // do_relocs( reader, writer, new_symtab_sec );

    writer.save( "./result.elf" );

    return 0;
}
