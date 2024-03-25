/*
Copyright (C) 2001-present by Serge Lamikhov-Center

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in
all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
THE SOFTWARE.
*/

#include <iostream>
#include <elfio/elfio.hpp>

using namespace ELFIO;

section* get_symtab_section( const ELFIO::elfio& elf_file )
{
    Elf_Half sec_num = elf_file.sections.size();
    std::cout << "Number of sections: " << sec_num << std::endl;
    for ( int i = 0; i < sec_num; ++i ) {
        section* psec = elf_file.sections[i];
        if ( psec->get_type() == SHT_SYMTAB ) {
            return psec;
        }
    }

    throw std::runtime_error( "No SYMTAB section found!" );
}

int main( int argc, char** argv )
{
    if ( argc != 2 ) {
        std::cout << "Usage: add_section <elf_file>" << std::endl;
        return 1;
    }

    // Create an elfio reader
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

    // create empty string table
    section* new_strtab_sec = writer.sections.add( ".strtab" );
    new_strtab_sec->set_type( SHT_STRTAB );
    new_strtab_sec->set_addr_align( 0x1 );

    // create empty symbol table
    section* old_symtab_sec = get_symtab_section( reader );
    section* new_symtab_sec = writer.sections.add( ".symtab" );
    new_symtab_sec->set_type( old_symtab_sec->get_type() );
    new_symtab_sec->set_entry_size( old_symtab_sec->get_entry_size() );
    new_symtab_sec->set_flags( old_symtab_sec->get_flags() );
    new_symtab_sec->set_addr_align( old_symtab_sec->get_addr_align() );
    // this is ought to be changed info shall be the index of first global symbol
    new_symtab_sec->set_info(old_symtab_sec->get_info());
    new_symtab_sec->set_link(new_strtab_sec->get_index());

    symbol_section_accessor symbols( reader, old_symtab_sec );
    symbol_section_accessor new_symbols( writer, new_symtab_sec );
    string_section_accessor new_strings( new_strtab_sec );

    // copy all funcitons
    for ( unsigned int j = 0; j < symbols.get_symbols_num(); ++j ) {
        std::string   sym_name;
        Elf64_Addr    sym_value;
        Elf_Xword     sym_size;
        unsigned char sym_bind;
        unsigned char sym_type;
        Elf_Half      sym_section_index;
        unsigned char sym_other;
        symbols.get_symbol( j, sym_name, sym_value, sym_size, sym_bind,
                            sym_type, sym_section_index, sym_other );

        if ( sym_type == STT_FUNC ) {
            // create section for a given function
            std::string sec_name = ".text." + sym_name;
            section*    f_sec    = writer.sections.add( sec_name );
            f_sec->set_type( SHT_PROGBITS );
            f_sec->set_flags( SHF_ALLOC | SHF_EXECINSTR );
            f_sec->set_addr_align( 0x1 );

            // get original section data
            section*    psec           = reader.sections[sym_section_index];
            size_t      psec_data_size = psec->get_size();
            const char* psec_data_ptr  = psec->get_data();

            size_t offset_into_psec = sym_value - psec->get_address();

            if ( offset_into_psec > psec_data_size )
                throw std::runtime_error( "trying to read data for fuction "
                                          "outside of its orignal section!" );

            f_sec->set_data( psec_data_ptr + offset_into_psec, sym_size );

            // create relevant symbol table entry
            new_symbols.add_symbol( new_strings,
                                    sym_name.c_str(), // name
                                    0x0,              // offset into section
                                    sym_size,         // object size
                                    sym_bind,  // bind type (usualy DEFAULT)
                                    sym_type,  // type : FUNC | OBJECT
                                    sym_other, // other
                                    f_sec->get_index() // index to a section
            );
        }
    }



    // Create additional section at the end of the file
    section* note_sec = reader.sections.add( ".note.ELFIO" );
    note_sec->set_type( SHT_NOTE );
    note_section_accessor note_writer( reader, note_sec );
    note_writer.add_note( 0x01, "Created by ELFIO", "My data", 8 );

    writer.save( "./result.elf" );

    return 0;
}
