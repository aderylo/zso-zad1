#include <iostream>
#include <set>
#include <map>
#include <elfio/elfio.hpp>
#include <algorithm>
#include <utils.hpp>

using namespace ELFIO;

section* get_section_by_name( const elfio& elf_file, const std::string& name )
{
    for ( int j = 0; j < elf_file.sections.size(); j++ ) {
        section* psec = elf_file.sections[j];
        if ( psec->get_name() == name )
            return psec;
    }

    return nullptr;
}

bool parse_text_section( const elfio& src, elfio& dst )
{
    section*                text_sec = get_section_by_name( src, ".text" );
    section*                sym_sec  = utils::get_sections_by_type( src, SHT_SYMTAB ).front();
    symbol_section_accessor src_sym_acc( src, sym_sec );

    Elf64_Addr start  = text_sec->get_address();
    Elf64_Addr end    = text_sec->get_address() + text_sec->get_size();
    Elf64_Addr offset = 0;

    // get all symbols that point to something in .text section
    std::vector<utils::Symbol> src_syms_in_text =
        utils::get_symbols_in_range( src_sym_acc, start, end );

    // srot symbols by their virutal address
    std::sort( src_syms_in_text.begin(), src_syms_in_text.end(),
               []( const utils::Symbol& a, const utils::Symbol& b ) { return a.value < b.value; } );

    std::cout << src_syms_in_text.size() << std::endl;

    for ( auto sym : src_syms_in_text ) {
        section*    new_sec;
        std::string new_sec_name;
        size_t      offset_into_text, new_sec_size;

        if ( start + offset < sym.value ) {
            // add whatever is before this symbol
            section* new_sec = dst.sections.add( ".text" + std::to_string( start + offset ) + "f" );
            utils::configure_section_header(
                new_sec, SHT_PROGBITS, text_sec->get_flags(), text_sec->get_address(),
                text_sec->get_link(), text_sec->get_addr_align(), text_sec->get_entry_size() );

            new_sec->set_data( text_sec->get_data() + offset, sym.value - ( start + offset ) );
        }

        // add section for the symbol
        section* new_sec2 = dst.sections.add( ".text." + sym.name );
        utils::configure_section_header( new_sec2, SHT_PROGBITS, text_sec->get_flags(),
                                         text_sec->get_address(), text_sec->get_link(),
                                         text_sec->get_addr_align(), text_sec->get_entry_size() );

        new_sec2->set_data( text_sec->get_data() + sym.value - start, sym.size );
        offset = sym.value - start + sym.size;
    }

    if ( start + offset < end ) {
        // add whatever is leftover
        section* new_sec3 = dst.sections.add( ".text." + start );
        utils::configure_section_header( new_sec3, SHT_PROGBITS, text_sec->get_flags(),
                                         text_sec->get_address(), text_sec->get_link(),
                                         text_sec->get_addr_align(), text_sec->get_entry_size() );

        new_sec3->set_data( text_sec->get_data() + offset, end - ( start + offset ) );
    }

    return true;
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

    parse_text_section( reader, writer );

    writer.save( "./result.elf" );

    return 0;
}
