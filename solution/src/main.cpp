#include <iostream>
#include <set>
#include <map>
#include <elfio/elfio.hpp>
#include <section_utils.hpp>
#include <symbol_utils.hpp>
#include <relocation_utils.hpp>
#include <algorithm>
#include <utils.hpp>
#include <exception>

using namespace ELFIO;

#define print( x ) std::cout << ( x ) << std::endl

bool disect_section_template( elfio&                     dst,
                              section*                   dst_symtab,
                              section*                   dst_strtab,
                              section*                   src_sec,
                              utils::Section             sec_hdr_template,
                              std::string                sec_name_prefix,
                              std::vector<utils::Symbol> src_symbols )
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
            sec_hdr_template.addr = ( start + offset );
            chunk_size            = sym.value - ( start + offset );

            new_sec = utils::add_section( dst, sec_hdr_template );
            new_sec->set_data( src_sec->get_data() + offset, chunk_size );
            utils::add_function_symbol( dst_symtab_acc, dst_strtab_acc, new_sec );
            offset += chunk_size;
        }

        if ( start + offset == sym.value && sym.value + sym.size <= end ) { // add function
            sec_hdr_template.name = sec_name_prefix + sym.name;
            sec_hdr_template.addr = sym.value;

            new_sec = utils::add_section( dst, sec_hdr_template );
            new_sec->set_data( src_sec->get_data() + offset, sym.size );
            utils::add_function_symbol( dst_symtab_acc, dst_strtab_acc, new_sec );
            offset += sym.size;
        }
    }

    if ( start + offset < end ) { // add any leftovers
        sec_hdr_template.name = sec_name_prefix + std::to_string( start + offset );
        sec_hdr_template.addr = start + offset;

        new_sec = utils::add_section( dst, sec_hdr_template );
        new_sec->set_data( src_sec->get_data() + offset, end - ( start + offset ) );
        utils::add_function_symbol( dst_symtab_acc, dst_strtab_acc, new_sec );
    }

    return true;
}

bool recreate_functions( const elfio& src, elfio& dst, section* dst_symtab, section* dst_strtab )
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
        disect_section_template( dst, dst_symtab, dst_strtab, text_sec, new_sec_hdr, ".text.",
                                 symbols );
    }

    return true;
}

std::vector<utils::Relocation>
create_rel_tab_for_fn( elfio&                         file,
                       symbol_section_accessor&       symtab_acc,
                       string_section_accessor&       strtab_acc,
                       utils::ElfMemoryLayout         mem_layout,
                       std::vector<utils::Relocation> original_relocs,
                       std::vector<utils::Symbol>     original_symbols,
                       section*                       fn_sec )
{
    Elf64_Addr                     fn_addr = fn_sec->get_address();
    Elf_Xword                      fn_size = fn_sec->get_size();
    std::vector<utils::Relocation> new_relocs;

    for ( auto org_rel : original_relocs ) {
        if ( !( fn_addr <= org_rel.offset && org_rel.offset <= fn_addr + fn_size ) )
            continue;

        auto                org_sym   = original_symbols[org_rel.symbol];
        Elf64_Addr          obj_addr  = org_sym.value;
        Elf_Xword           obj_size  = org_sym.size;
        utils::PointerClass obj_class = utils::classify_pointer( mem_layout, obj_addr );

        auto current_symtab_view = utils::get_symtab_view( symtab_acc );
        auto existing_sym =
            utils::filter_symtab_view_by_contains_addr( current_symtab_view, obj_addr );

        if ( obj_class == utils::TEXT ) {
            if ( existing_sym.empty() )
                std::cerr << "Funcitons ought to have symbols beforehand!";
            else {
                new_relocs.push_back( utils::Relocation( obj_addr - existing_sym.at( 0 ).value,
                                                         existing_sym.front().__symtab_idx,
                                                         org_rel.type, 0x0 ) );
            }
        }
        else if ( obj_class == utils::RODATA ) {
            if ( existing_sym.empty() ) {
                auto new_obj_sec = utils::add_rodata_section( file, obj_addr );
                // need: offset to an object;
                // new_obj_sec.set_data()
                auto new_obj_sym =
                    utils::add_rodata_symbol( symtab_acc, strtab_acc, obj_addr, new_obj_sec );
                existing_sym.push_back( new_obj_sym );
            }

            utils::Relocation new_rel_entry( obj_addr - existing_sym.front().value,
                                             existing_sym.front().__symtab_idx, org_rel.type, 0x0 );
            new_relocs.push_back( new_rel_entry );
        }
        else if ( obj_class == utils::GOT ) {
            // idk
        }
        else if ( obj_class == utils::DATA ) {
            if ( existing_sym.empty() ) {
                auto new_obj_sec = utils::add_data_section( file, obj_addr );
                auto new_obj_sym =
                    utils::add_data_symbol( symtab_acc, strtab_acc, obj_addr, new_obj_sec );
                existing_sym.push_back( new_obj_sym );
            }

            utils::Relocation new_rel_entry( obj_addr - existing_sym.front().value,
                                             existing_sym.front().__symtab_idx, org_rel.type, 0x0 );
            new_relocs.push_back( new_rel_entry );
        }
        else if ( obj_class == utils::BSS ) {
            if ( existing_sym.empty() ) {
                auto new_obj_sec = utils::add_bss_section( file, obj_addr );
                auto new_obj_sym =
                    utils::add_bss_symbol( symtab_acc, strtab_acc, obj_addr, new_obj_sec );
                existing_sym.push_back( new_obj_sym );
            }

            utils::Relocation new_rel_entry( obj_addr - existing_sym.front().value,
                                             existing_sym.front().__symtab_idx, org_rel.type, 0x0 );
            new_relocs.push_back( new_rel_entry );
        }
    }

    return new_relocs;
}

void create_relocs( const elfio&             src,
                    elfio&                   dst,
                    section*                 sym_sec,
                    section*                 str_sec,
                    symbol_section_accessor& symtab_acc,
                    string_section_accessor& strtab_acc )
{
    auto fn_sections = utils::get_sections_by_flags( dst, SHF_ALLOC | SHF_EXECINSTR );

    // --- syms from original file
    section*                   src_sym_sec = utils::get_sections_by_type( src, SHT_SYMTAB ).front();
    symbol_section_accessor    src_sym_acc( src, src_sym_sec );
    std::vector<utils::Symbol> src_sym_view = utils::get_symtab_view( src_sym_acc );

    // --- rels from original file
    std::vector<utils::Relocation> src_rel_view;
    std::vector<section*>          src_rel_secs = utils::get_sections_by_type( src, SHT_REL );
    for ( auto src_rel_sec : src_rel_secs ) {
        auto acc = relocation_section_accessor( src, src_rel_sec );
        auto tmp = utils::get_reltab_view( acc );
        src_rel_view.insert( src_rel_view.end(), tmp.begin(), tmp.end() );
    }

    // --- create memory layout
    auto memory_layout = utils::reconstructMemoryLayout( src );

    // --- create relocs for functions
    for ( section* fn_sec : fn_sections ) {
        auto rel_tab_view = create_rel_tab_for_fn( dst, symtab_acc, strtab_acc, memory_layout,
                                                   src_rel_view, src_sym_view, fn_sec );

        if ( !rel_tab_view.empty() ) {
            auto rel_tab_sec = utils::add_rel_section( dst, ".rel" + fn_sec->get_name(),
                                                       sym_sec->get_index(), fn_sec->get_index() );
            relocation_section_accessor rel_tab_acc( dst, rel_tab_sec );
            for ( auto rel_entry : rel_tab_view ) {
                utils::add_rel_entry( rel_tab_acc, rel_entry );
            }
        }
    }
}

void fix_symtab( elfio& dst, section* dst_symtab, section* dst_strtab )
{
    symbol_section_accessor accessor( dst, dst_symtab );
    utils::mapping          symbol_idx_map;
    for ( int i = 0; i < dst_symtab->get_size(); i++ ) {
        symbol_idx_map.insert( i, i );
    }

    std::function<void( int, int )> f = [&symbol_idx_map]( int first, int second ) {
        symbol_idx_map.swap_values( first, second );
    };
    accessor.arrange_local_symbols( f );

    std::vector<section*> rel_sections = utils::get_sections_by_type( dst, SHT_REL );

    for ( auto rel_sec : rel_sections ) {
        relocation_section_accessor acc( dst, rel_sec );

        for ( int j = 0; j < acc.get_entries_num(); j++ ) {
            auto rel_entry   = utils::get_relocation_by_idx( acc, j );
            rel_entry.symbol = symbol_idx_map.forward[rel_entry.symbol];
        }
    }
}

void establish_entry_point( elfio& dst, section* dst_symtab, section* dst_strtab )
{
    symbol_section_accessor sym_acc( dst, dst_symtab );
    string_section_accessor str_acc( dst_strtab );
    auto                    entry = dst.get_entry();
    std::cout << entry << std::endl;

    auto sections = utils::get_sections_containing_addr( dst, entry );
    std::cout << sections.size() << std::endl;
    auto start_sec = sections.front();
    for ( int j = 0; j < dst.sections.size(); j++ ) {
        section* sec = dst.sections[j];

        if ( sec->get_address() == entry ) {
            utils::Symbol fn_symbol;
            fn_symbol.value         = 0x0;
            fn_symbol.name          = "_start";
            fn_symbol.bind          = STB_GLOBAL;
            fn_symbol.section_index = sec->get_index();
            fn_symbol.size          = sec->get_size();
            fn_symbol.type          = STT_FUNC;
            fn_symbol.other         = STV_DEFAULT;

            utils::add_symbol( sym_acc, str_acc, fn_symbol );
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
    writer.set_entry( reader.get_entry() );

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

    recreate_functions( reader, writer, new_symtab_sec, new_strtab_sec );
    create_relocs( reader, writer, new_symtab_sec, new_strtab_sec, new_symbols, new_strings );
    fix_symtab( writer, new_symtab_sec, new_strtab_sec );
    establish_entry_point( writer, new_symtab_sec, new_strtab_sec );

    writer.save( "./result.elf" );

    return 0;
}
