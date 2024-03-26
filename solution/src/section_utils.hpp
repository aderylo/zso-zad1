#include <elfio/elfio.hpp>

using namespace ELFIO;

namespace utils {

struct Section
{
    std::string name;
    Elf_Word    type;
    Elf_Xword   flags;
    Elf64_Addr  addr;
    Elf_Word    link;
    Elf_Xword   addralign;
    Elf_Xword   entsize;
};

section* add_section( elfio& elf_file, const Section& sec_hdr )
{
    section* psec = elf_file.sections.add( sec_hdr.name );

    psec->set_type( sec_hdr.type );
    psec->set_flags( sec_hdr.flags );
    psec->set_address( sec_hdr.addr );
    psec->set_link( sec_hdr.link );
    psec->set_addr_align( sec_hdr.addralign );
    psec->set_entry_size( sec_hdr.entsize );

    return psec;
}

section* get_section_by_name( const elfio& elf_file, const std::string& name )
{
    for ( int j = 0; j < elf_file.sections.size(); j++ ) {
        section* psec = elf_file.sections[j];
        if ( psec->get_name() == name )
            return psec;
    }

    return nullptr;
}

std::vector<section*> get_sections_by_type( const elfio& elf_file, Elf_Word type )
{
    std::vector<section*> result;
    Elf_Half              sec_num = elf_file.sections.size();

    for ( int i = 0; i < sec_num; ++i ) {
        section* psec = elf_file.sections[i];
        if ( type == psec->get_type() ) {
            result.push_back( psec );
        }
    }

    return result;
}

std::vector<section*> get_sections_by_flags( const elfio& elf_file, Elf_Xword flags )
{
    std::vector<section*> result;
    Elf_Half              sec_num = elf_file.sections.size();

    for ( int i = 0; i < sec_num; ++i ) {
        section* psec = elf_file.sections[i];
        if ( flags == psec->get_flags() ) {
            result.push_back( psec );
        }
    }

    return result;
}

} // utils