#ifndef SECTION_UTILS_HPP
#define SECTION_UTILS_HPP

#include <regex>
#include <string>
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
    Elf_Word    info;
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
    psec->set_info( sec_hdr.info );

    return psec;
}

section* add_fun_section(elfio & elf_file, const std::string& name){
        Section sec_hdr;
    sec_hdr.addr      = 0x0;
    sec_hdr.addralign = 0x1;
    sec_hdr.entsize   = 0x0;
    sec_hdr.flags     = ( SHF_ALLOC | SHF_EXECINSTR );;
    sec_hdr.type      = SHT_PROGBITS;
    sec_hdr.name      = name;
    sec_hdr.link      = 0x0;
    sec_hdr.info      = 0x0;
    return add_section( elf_file, sec_hdr );
}

section* add_rel_section( elfio& elf_file, const std::string& name, Elf_Word link, Elf_Word info )
{
    Section sec_hdr;
    sec_hdr.addr      = 0x0;
    sec_hdr.addralign = 0x4;
    sec_hdr.entsize   = 0x8;
    sec_hdr.flags     = SHF_INFO_LINK;
    sec_hdr.type      = SHT_REL;
    sec_hdr.name      = name;
    sec_hdr.link      = link;
    sec_hdr.info      = info;
    return add_section( elf_file, sec_hdr );
}

section* add_bss_section( elfio& elf_file, Elf64_Word addr )
{
    Section sec_hdr;
    sec_hdr.addr      = addr;
    sec_hdr.addralign = 0x4;
    sec_hdr.entsize   = 0x0;
    sec_hdr.flags     = ( SHF_WRITE | SHF_ALLOC );
    sec_hdr.info      = 0x0;
    sec_hdr.link      = 0x0;
    sec_hdr.name      = ".bss." + std::to_string( addr ) + "B";
    sec_hdr.type      = SHT_NOBITS;

    return add_section( elf_file, sec_hdr );
}

section* add_data_section( elfio& elf_file, Elf64_Word addr )
{
    Section sec_hdr;
    sec_hdr.addr      = addr;
    sec_hdr.addralign = 0x1;
    sec_hdr.entsize   = 0x0;
    sec_hdr.flags     = ( SHF_WRITE | SHF_ALLOC );
    sec_hdr.info      = 0x0;
    sec_hdr.link      = 0x0;
    sec_hdr.name      = ".data." + std::to_string( addr ) + "D";
    sec_hdr.type      = SHT_PROGBITS;

    return add_section( elf_file, sec_hdr );
}

section* add_rodata_section( elfio& elf_file, Elf64_Word addr )
{
    Section sec_hdr;
    sec_hdr.addr      = addr;
    sec_hdr.addralign = 0x4;
    sec_hdr.entsize   = 0x0;
    sec_hdr.flags     = ( SHF_ALLOC );
    sec_hdr.info      = 0x0;
    sec_hdr.link      = 0x0;
    sec_hdr.name      = ".rodata." + std::to_string( addr ) + "r";
    sec_hdr.type      = SHT_PROGBITS;

    return add_section( elf_file, sec_hdr );
}

std::vector<section*> get_sections_by_regex( const elfio& elf_file, const std::string& pattern_str )
{
    std::vector<section*> result;
    std::regex            pattern( pattern_str );

    for ( int j = 0; j < elf_file.sections.size(); ++j ) {
        section* psec = elf_file.sections[j];
        if ( std::regex_match( psec->get_name(), pattern ) )
            result.push_back( psec );
    }

    return result;
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

std::vector<section*> get_sections_containing_addr( const elfio& elf_file, Elf64_Addr addr )
{
    std::vector<section*> result;

    for ( int j = 0; j < elf_file.sections.size(); j++ ) {
        auto psec  = elf_file.sections[j];
        auto start = psec->get_address();
        auto end   = start + psec->get_size();

        if ( start <= addr && addr <= end )
            result.push_back( psec );
    }

    return result;
}

void configure_section_header( section*   section,
                               Elf_Word   type,
                               Elf_Xword  flags,
                               Elf64_Addr addr,
                               Elf_Word   link,
                               Elf_Xword  addralign,
                               Elf_Xword  entsize )
{
    section->set_type( type );
    section->set_flags( flags );
    section->set_address( addr );
    section->set_link( link );
    section->set_addr_align( addralign );
    section->set_entry_size( entsize );
}

} // namespace utils

#endif // SECTION_UTILS_HPP