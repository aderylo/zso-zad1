#include <iostream>
#include <set>
#include <map>
#include <elfio/elfio.hpp>
#include <utils.hpp>

using namespace ELFIO;

struct mapping
{
    std::map<int, int> forward;
    std::map<int, int> backward;

    void swap_values( int fst, int snd )
    {
        int inv_fst = backward[fst];
        int inv_snd = backward[snd];

        forward[inv_fst] = snd;
        forward[inv_snd] = fst;
        backward[fst]    = inv_snd;
        backward[snd]    = inv_fst;
    }

    void insert( int key, int val )
    {
        forward[key]  = val;
        backward[val] = key;
    }
};

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
    writer.set_entry(0x0);

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

    mapping symbol_map;

    // copy symbols that point to non zero objects and create sections for them
    for ( int idx = 0; idx < old_symbols.get_symbols_num(); idx++ ) {
        utils::Symbol old_sym;
        utils::get_symbol_wrapper( old_symbols, idx, old_sym );

        if ( old_sym.section_index == SHN_UNDEF || old_sym.section_index == SHN_ABS )
            continue;
        section* old_sec = reader.sections[old_sym.section_index];
        section* new_sec = writer.sections.add( old_sec->get_name() + "." + old_sym.name );

        utils::configure_section_header( new_sec, old_sec->get_type(), old_sec->get_flags(),
                                         old_sec->get_address(), 0x0, old_sec->get_addr_align(),
                                         old_sec->get_entry_size() );

        if ( old_sym.size > 0 ) {
            size_t offset_into_old_sec = old_sym.value - old_sec->get_address();
            new_sec->set_data( old_sec->get_data() + offset_into_old_sec, old_sym.size );

            Elf_Word sym_idx = new_symbols.add_symbol( new_strings,
                                                       old_sym.name.c_str(), // name
                                                       old_sym.value,        // offset into section
                                                       old_sym.size,         // object size
                                                       old_sym.bind,  // bind type (usualy DEFAULT)
                                                       old_sym.type,  // type : FUNC | OBJECT
                                                       old_sym.other, // other
                                                       new_sec->get_index() );

            symbol_map.insert( idx, sym_idx );
        }
    }

    // second pass, add symbols that are purely labels to places in memory
    for ( int i = 0; i < old_symbols.get_symbols_num(); i++ ) {
        utils::Symbol old_sym, new_sym;
        utils::get_symbol_wrapper( old_symbols, i, old_sym );

        if ( old_sym.size == 0 && old_sym.value != 0 ) {
            for ( int j = 0; j < new_symbols.get_symbols_num(); j++ ) {
                utils::get_symbol_wrapper( new_symbols, j, new_sym );
                if ( new_sym.section_index == SHN_UNDEF )
                    continue;

                if ( old_sym.value >= new_sym.value &&
                     old_sym.value <= new_sym.value + new_sym.size ) {
                    Elf64_Addr offset = old_sym.value - new_sym.value;

                    Elf_Word sym_idx =
                        new_symbols.add_symbol( new_strings,
                                                old_sym.name.c_str(), // name
                                                offset,               // offset into section
                                                old_sym.size,         // object size
                                                old_sym.bind,         // bind type (usualy DEFAULT)
                                                old_sym.type,         // type : FUNC | OBJECT
                                                old_sym.other,        // other
                                                new_sym.section_index );

                    symbol_map.insert( i, sym_idx );
                }
            }
        }
    }

    std::function<void( int, int )> f = [&symbol_map]( int x, int y ) {
        symbol_map.swap_values( x, y );
    };
    new_symbols.arrange_local_symbols( f );

    // Relocs
    auto old_rel_sections  = utils::get_sections_by_type( reader, SHT_REL );
    auto old_rela_sections = utils::get_sections_by_type( reader, SHT_RELA );

    size_t non_rel_section_count = writer.sections.size();
    for ( int sec_idx = 0; sec_idx < non_rel_section_count; sec_idx++ ) {
        section* psec = writer.sections[sec_idx];

        if ( psec->get_type() != SHT_PROGBITS )
            continue;

        section* new_rel_sec = writer.sections.add( ".rel" + psec->get_name() );
        utils::configure_section_header( new_rel_sec, SHT_REL, SHF_INFO_LINK, 0x0,
                                         new_symtab_sec->get_index(), 0x4, 0x8 );
                                         new_rel_sec->set_info(sec_idx);
        relocation_section_accessor new_rel_acc( writer, new_rel_sec );

        for ( auto old_rel_sec : old_rel_sections ) {
            relocation_section_accessor old_rel_acc( reader, old_rel_sec );
            for ( int rel_idx = 0; rel_idx < old_rel_acc.get_entries_num(); rel_idx++ ) {

                utils::Relocation old_rel;
                utils::get_entry_wrapper( old_rel_acc, rel_idx, old_rel );

                if ( psec->get_address() <= old_rel.offset &&
                     old_rel.offset <= psec->get_address() + psec->get_size() ) {
                    // relocation applies to current section
                    Elf64_Addr offset = old_rel.offset - psec->get_address();
                    // we need to get symbol the new relocation is ought to be pointing to;
                    Elf_Word new_sym_idx = symbol_map.forward[old_rel.symbol];

                    if ( old_rel.type == R_386_32 ) { // S + A
                        new_rel_acc.add_entry( offset, new_sym_idx, old_rel.type, old_rel.addend );
                    }
                }
            }
        }
    }

    // zero out value of object/func symbols since they point to begining of their sections
    // and we do not need their original virtual address anymore
    for ( int sym_idx = 0; sym_idx < new_symbols.get_symbols_num(); sym_idx++ ) {
        utils::Symbol sym;
        utils::get_symbol_wrapper( new_symbols, sym_idx, sym );
        if ( sym.size > 0 ) {
            sym.value = 0x0;
        }
    }

    writer.save( "./result.elf" );

    return 0;
}
