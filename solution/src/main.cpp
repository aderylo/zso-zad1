#include <iostream>
#include <elfio/elfio.hpp>
#include <set>
#include <map>

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

std::vector<section*> get_reloc_sections( const elfio& elf_file )
{

    Elf_Half              sec_num = elf_file.sections.size();
    std::vector<section*> result;

    for ( int i = 0; i < sec_num; ++i ) {
        section* psec = elf_file.sections[i];
        Elf_Word type = psec->get_type();
        if ( type == SHT_REL || type == SHT_RELA ) {
            result.push_back( psec );
        }
    }

    return result;
}

struct Relocation
{
    Elf64_Addr offset;
    Elf_Word   symbol_idx;
    Elf_Word   type;
    Elf_Sxword addend;

    Relocation( Elf64_Addr offset, Elf_Word symbol_idx, Elf_Word type, Elf_Sxword addend )
        : offset( offset ), symbol_idx( symbol_idx ), type( type ), addend( addend )
    {
    }
};

std::vector<Relocation> get_all_relocations( const elfio& elf_file )
{
    std::vector<section*>   reloc_secs = get_reloc_sections( elf_file );
    std::vector<Relocation> result;

    for ( section* reloc_sec : reloc_secs ) {
        relocation_section_accessor relocs( elf_file, reloc_sec );

        for ( uint j = 0; j < relocs.get_entries_num(); j++ ) {
            Elf64_Addr rel_offset;
            Elf_Word   rel_symbol_idx;
            Elf_Word   rel_type;
            Elf_Sxword rel_addend;
            relocs.get_entry( j, rel_offset, rel_symbol_idx, rel_type, rel_addend );
            result.push_back( Relocation( rel_offset, rel_symbol_idx, rel_type, rel_addend ) );
        }
    }

    return result;
}


// std::vector<Symbol> get_symtab_snapshot(const elfio & elf_file){

// }

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
    new_symtab_sec->set_info( old_symtab_sec->get_info() );
    new_symtab_sec->set_link( new_strtab_sec->get_index() );

    symbol_section_accessor symbols( reader, old_symtab_sec );
    symbol_section_accessor new_symbols( writer, new_symtab_sec );
    string_section_accessor new_strings( new_strtab_sec );

    std::map<int, int> old_symbol_idx_to_new; 

    // copy all funcitons
    for ( unsigned int j = 0; j < symbols.get_symbols_num(); ++j ) {
        std::string   sym_name;
        Elf64_Addr    sym_value;
        Elf_Xword     sym_size;
        unsigned char sym_bind;
        unsigned char sym_type;
        Elf_Half      sym_section_index;
        unsigned char sym_other;
        symbols.get_symbol( j, sym_name, sym_value, sym_size, sym_bind, sym_type, sym_section_index,
                            sym_other );

        if ( sym_section_index == SHN_UNDEF || sym_section_index == SHN_ABS )
            continue;
        // get section in which referenced object resides
        section* psec = reader.sections[sym_section_index];

        // create a section for this object
        std::string sec_name = psec->get_name() + "." + sym_name;
        section*    f_sec    = writer.sections.add( sec_name );
        f_sec->set_type( psec->get_type() );
        f_sec->set_flags( psec->get_flags() );
        f_sec->set_addr_align( psec->get_addr_align() );

        if ( sym_size > 0 ) { // add instructions/data if symbol points to it
            size_t offset_into_psec = sym_value - psec->get_address();
            if ( offset_into_psec > psec->get_size() )
                throw std::runtime_error( "trying to read data for fuction "
                                          "outside of its orignal section!" );

            f_sec->set_data( psec->get_data() + offset_into_psec, sym_size );
        }

        // create relevant symbol table entry
        Elf_Word idx = new_symbols.add_symbol( new_strings,
                                sym_name.c_str(),  // name
                                sym_value,         // offset into section
                                sym_size,          // object size
                                sym_bind,          // bind type (usualy DEFAULT)
                                sym_type,          // type : FUNC | OBJECT
                                sym_other,         // other
                                f_sec->get_index() // index to a section
        );
        std::cout<<idx<<std::endl;
    }

    // We have necessary sections now its time to do relocations

    // reverse relocations
    std::vector<Relocation> old_relocations = get_all_relocations( reader );
    for ( int j = 0; j < new_symbols.get_symbols_num(); j++ ) {
        std::string   sym_name;
        Elf64_Addr    sym_value;
        Elf_Xword     sym_size;
        unsigned char sym_bind;
        unsigned char sym_type;
        Elf_Half      sym_section_index;
        unsigned char sym_other;
        symbols.get_symbol( j, sym_name, sym_value, sym_size, sym_bind, sym_type, sym_section_index,
                            sym_other );
        section*                    sym_sec = writer.sections[sym_section_index];
        section*                    rel_sec = writer.sections.add( ".rel" + sym_sec->get_name() );
        relocation_section_accessor rel_sec_acc( writer, rel_sec );

        // for ( auto old_rel : old_relocations ) {
        //     if ( sym_value <= old_rel.offset && old_rel.offset <= sym_value + sym_size ) {
        //         Elf64_Addr sec_offset = old_rel.offset - sym_value;

        //         if ( old_rel.type == R_386_32 ) {


                    
        //             rel_sec_acc.add_entry( sec_offset,
        //                                    old_rel.symbol_idx, // should change that
        //                                    old_rel.type, 
        //                                    old_rel.addend );
        //         }
        //     }
        // }
    }

    // Create additional section at the end of the file
    section* note_sec = reader.sections.add( ".note.ELFIO" );
    note_sec->set_type( SHT_NOTE );
    note_section_accessor note_writer( reader, note_sec );
    note_writer.add_note( 0x01, "Created by ELFIO", "My data", 8 );

    writer.save( "./result.elf" );

    return 0;
}
