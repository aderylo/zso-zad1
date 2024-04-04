#ifndef SYMBOLIZE_HPP
#define SYMBOLIZE_HPP

#include <exception>

#include <elfio/elfio.hpp>
#include <section_utils.hpp>
#include <symbol_utils.hpp>
#include <relocation_utils.hpp>
#include <utils.hpp>

#define print( x ) std::cout << ( x ) << std::endl

utils::Relocation recreate_relocation_entry( elfio&                     new_file,
                                             elfio&                     org_file,
                                             section*                   fn_sec,
                                             symbol_section_accessor&   new_symtab_acc,
                                             string_section_accessor&   new_strtab_acc,
                                             utils::ElfMemoryLayout     org_mem_layout,
                                             std::vector<utils::Symbol> org_symtab_view,
                                             utils::Relocation          org_rel )
{
    print( "REL_TYPE: " + std::to_string( org_rel.type ) + "\n" );

    utils::Symbol org_sym      = org_symtab_view[org_rel.symbol];
    Elf32_Addr    org_obj_addr = org_sym.value;
    Elf_Xword     org_obj_size = org_sym.size;
    Elf_Sword     addend       = utils::resolve_rel_addend( org_file, org_rel.offset );

    std::cout << "sym val: " << std::hex << org_obj_addr << std::endl;
    std::cout << "bytes at off: " << std::hex << addend << std::endl;
    std::cout << "bytes at off: " << std::dec << addend << std::endl;

    // -- adjust addend, in exec file bytes at rel.offset is actual symbol value
    addend -= org_obj_addr;
    org_obj_addr += addend;

    std::cout << "ADDEND: " << std::dec << addend << std::endl;
    std::cout << "Adjusted addr: " << std::hex << org_obj_addr << std::endl;

    if ( org_rel.type == R_386_PC32 ) { // S + A - PC
        // cheating I know where it points
        org_obj_addr = org_sym.value;
    }

    utils::PointerClass obj_class = utils::classify_pointer( org_mem_layout, org_obj_addr );

    // -- sometimes relocations do not point to objects but sections so grab obj size;
    if ( org_obj_size == 0 ) {
        for ( auto sym : org_symtab_view ) {
            if ( sym.value == org_obj_addr && sym.size > 0 ) {
                org_sym      = sym;
                org_obj_size = sym.size;
            }
        }
    }

    auto current_symtab_view = utils::get_symtab_view( new_symtab_acc );
    auto existing_sym =
        utils::filter_symtab_view_by_contains_addr( current_symtab_view, org_obj_addr );

    // -- handle relocation for function call
    if ( obj_class == utils::TEXT ) {
        if ( existing_sym.empty() )
            throw std::runtime_error( "Functions ought to have symbols beforehand!\n" );

        auto referenced_sym = existing_sym.front();
        return utils::Relocation( org_rel.offset - fn_sec->get_address(),
                                  referenced_sym.__symtab_idx, org_rel.type, 0x0 );
    }

    // -- handle ptr to object in rodata
    if ( obj_class == utils::RODATA ) {
        if ( existing_sym.empty() ) { // add section (with data) and symbol pointing to it
            // section header
            section* new_rodata_sec = utils::add_rodata_section( new_file, org_obj_addr );

            // link data to section
            size_t offset_into_rodata = org_obj_addr - org_mem_layout.rodata.addr;
            // new_rodata_sec->set_data( org_mem_layout.rodata.data + offset_into_rodata,
            //                           org_obj_size );

            // add symbol pointing to section representing object
            utils::Symbol new_rodata_sym = utils::add_rodata_symbol( new_symtab_acc, new_strtab_acc,
                                                                     org_obj_addr, new_rodata_sec );
            existing_sym.push_back( new_rodata_sym );
        }

        // add relocation to symbol
        utils::Symbol referenced_sym = existing_sym.front();
        return utils::Relocation( org_rel.offset - fn_sec->get_address(),
                                  referenced_sym.__symtab_idx, org_rel.type, 0x0 );
    }

    // -- handle ptr to object in BSS

    if ( obj_class == utils::BSS ) {
        if ( existing_sym.empty() ) { // add section (with data) and symbol pointing to it
            section* new_bss_sec = utils::add_bss_section( new_file, org_obj_addr );

            // link unutilized data to section
            new_bss_sec->set_size( org_obj_size );

            // add symbol pointing to section representing object
            utils::Symbol new_bss_sym =
                utils::add_bss_symbol( new_symtab_acc, new_strtab_acc, org_obj_addr, new_bss_sec );

            existing_sym.push_back( new_bss_sym );
        }

        utils::Symbol referenced_sym = existing_sym.front();
        return utils::Relocation( org_rel.offset - fn_sec->get_address(),
                                  referenced_sym.__symtab_idx, org_rel.type, 0x0 );
    }

    // -- handle ptr to object in DATA

    if ( obj_class == utils::DATA ) {
        if ( existing_sym.empty() ) { // add section (with data) and symbol pointing to it
            // section header
            section* new_data_sec = utils::add_data_section( new_file, org_obj_addr );

            // link data to section
            size_t      offset_into_data = org_obj_addr - org_mem_layout.data.addr;
            size_t      file_offset      = org_mem_layout.data.offset + offset_into_data;
            const char* data             = utils::get_data_by_offset( org_file, file_offset );
            new_data_sec->set_data( data, org_obj_size );

            // add symbol pointing to section representing object
            utils::Symbol new_data_sym = utils::add_data_symbol( new_symtab_acc, new_strtab_acc,
                                                                 org_obj_addr, new_data_sec );
            existing_sym.push_back( new_data_sym );
        }

        // add relocation to symbol
        utils::Symbol referenced_sym = existing_sym.front();
        return utils::Relocation( org_rel.offset - fn_sec->get_address(),
                                  referenced_sym.__symtab_idx, org_rel.type, 0x0 );
    }

    std::runtime_error( "Unclassified object!!!\n" );
    return utils::Relocation( 0x0, 0x0, 0x0, 0x0 );
}

std::vector<utils::Relocation> recreate_reltab( elfio&                         new_file,
                                                elfio&                         org_file,
                                                symbol_section_accessor&       new_symtab_acc,
                                                string_section_accessor&       new_strtab_acc,
                                                utils::ElfMemoryLayout         org_mem_layout,
                                                section*                       sec,
                                                std::vector<utils::Symbol>     org_symtab_view,
                                                std::vector<utils::Relocation> org_reltab_view )
{
    Elf64_Addr                     fn_addr = sec->get_address();
    Elf_Xword                      fn_size = sec->get_size();
    std::vector<utils::Relocation> new_relocs;

    // a hac for now to use original relocs
    for ( auto org_rel : org_reltab_view ) {
        if ( !( fn_addr <= org_rel.offset && org_rel.offset <= fn_addr + fn_size ) )
            continue;

        // if ( org_rel.type != R_386_32 )
        //     continue;

        utils::Relocation new_rel =
            recreate_relocation_entry( new_file, org_file, sec, new_symtab_acc, new_strtab_acc,
                                       org_mem_layout, org_symtab_view, org_rel );
        new_relocs.push_back( new_rel );
    }

    return new_relocs;
}

void recreate_relocations( elfio&    new_file,
                           elfio&    org_file,
                           section*& new_symtab_sec,
                           section*& new_strtab_sec )
{
    symbol_section_accessor new_symtab_acc( new_file, new_symtab_sec );
    string_section_accessor new_strtab_acc( new_strtab_sec );

    // -- grab recreated function sections from a new file
    auto new_fn_sections = utils::get_sections_by_flags( new_file, SHF_ALLOC | SHF_EXECINSTR );

    // -- grab symtab view from original file
    section* src_symtab_sec = utils::get_sections_by_type( org_file, SHT_SYMTAB ).front();
    symbol_section_accessor    org_symtab_acc( org_file, src_symtab_sec );
    std::vector<utils::Symbol> org_symtab_view = utils::get_symtab_view( org_symtab_acc );

    // -- grab all relocations from original file
    std::vector<utils::Relocation> org_reltab_view;
    std::vector<section*>          org_rel_secs = utils::get_sections_by_type( org_file, SHT_REL );
    for ( auto org_rel_sec : org_rel_secs ) {
        auto acc = relocation_section_accessor( org_file, org_rel_sec );
        auto tmp = utils::get_reltab_view( acc );
        org_reltab_view.insert( org_reltab_view.end(), tmp.begin(), tmp.end() );
    }

    // -- grab memory layout of original file
    auto org_mem_layout = utils::reconstructMemoryLayout( org_file );

    // -- for each recreated function, recrate reltab if needed
    for ( section* new_fn_sec : new_fn_sections ) {
        auto new_reltab_view =
            recreate_reltab( new_file, org_file, new_symtab_acc, new_strtab_acc, org_mem_layout,
                             new_fn_sec, org_symtab_view, org_reltab_view );

        if ( !new_reltab_view.empty() ) {
            // -- create relocation table section header
            auto reltab_sec =
                utils::add_rel_section( new_file, ".rel" + new_fn_sec->get_name(),
                                        new_symtab_sec->get_index(), new_fn_sec->get_index() );
            relocation_section_accessor rel_tab_acc( new_file, reltab_sec );

            // -- populate the table
            for ( auto rel_entry : new_reltab_view ) {
                utils::add_rel_entry( rel_tab_acc, rel_entry );
            }
        }
    }
}

#endif // SYMBOLIZE_HPP