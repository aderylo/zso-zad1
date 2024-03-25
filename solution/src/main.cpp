#include <iostream>
#include <set>
#include <map>
#include <elfio/elfio.hpp>
#include <utils.hpp>

using namespace ELFIO;

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
    writer.set_type( reader.get_type() );
    writer.set_machine( reader.get_machine() );

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

    // copy symbols and create sections for them
    for ( int idx = 0; idx < old_symbols.get_symbols_num(); idx++ ) {
        utils::Symbol old_sym;
        utils::get_symbol_wrapper( old_symbols, idx, old_sym );

        if ( old_sym.section_index == SHN_UNDEF || old_sym.section_index == SHN_ABS )
            continue;
        section* old_sec = reader.sections[old_sym.section_index];
        section* new_sec = writer.sections.add( old_sec->get_name() + "." + old_sym.name );

        utils::configure_section_header( new_sec, old_sec->get_type(), old_sec->get_flags(), 0x0,
                                         0x0, old_sec->get_addr_align(),
                                         old_sec->get_entry_size() );

        if ( old_sym.size > 0 ) {
            size_t offset_into_old_sec = old_sym.value - old_sec->get_address();
            new_sec->set_data( old_sec->get_data() + offset_into_old_sec, old_sym.size );
        }

        Elf_Word sec_idx = new_symbols.add_symbol( new_strings,
                                                   old_sym.name.c_str(), // name
                                                   old_sym.value,        // offset into section
                                                   old_sym.size,         // object size
                                                   old_sym.size,  // bind type (usualy DEFAULT)
                                                   old_sym.type,  // type : FUNC | OBJECT
                                                   old_sym.other, // other
                                                   new_sec->get_index() );
    }

    // Create additional section at the end of the file
    section* note_sec = reader.sections.add( ".note.ELFIO" );
    note_sec->set_type( SHT_NOTE );
    note_section_accessor note_writer( reader, note_sec );
    note_writer.add_note( 0x01, "Created by ELFIO", "My data", 8 );

    writer.save( "./result.elf" );

    return 0;
}
