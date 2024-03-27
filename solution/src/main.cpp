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

bool parse_text_section( const elfio& src, elfio& dst )
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

void parse_rodata_sections( const elfio& src, elfio& dst )
{
    auto rodata_sections = utils::get_sections_by_flags( src, SHF_ALLOC );
    auto sym_sections    = utils::get_sections_by_type( src, SHT_SYMTAB );

    for ( auto rodata_sec : rodata_sections ) {
        Elf64_Addr start = rodata_sec->get_address();
        Elf64_Addr end   = rodata_sec->get_address() + rodata_sec->get_size();

        section*       new_sec;
        utils::Section new_sec_hdr;
        new_sec_hdr.type      = SHT_PROGBITS;
        new_sec_hdr.flags     = rodata_sec->get_flags();
        new_sec_hdr.link      = 0x0;
        new_sec_hdr.addralign = rodata_sec->get_addr_align();
        new_sec_hdr.entsize   = rodata_sec->get_entry_size();
        new_sec_hdr.info      = 0x0;

        // get all symbols that point to functions in exectuable sections
        std::vector<utils::Symbol> symbols;

        auto processSymSection = [&]( auto& sym_sec, std::vector<utils::Symbol>& result ) {
            symbol_section_accessor sym_sec_acc( src, sym_sec );
            auto                    sv = utils::get_symtab_view( sym_sec_acc );
            sv                         = utils::filter_symtab_view_by_range( sv, start, end );
            std::copy_if( sv.begin(), sv.end(), std::back_inserter( result ),
                          []( const utils::Symbol& sym ) { return sym.size != 0; } );
        };
        std::for_each( sym_sections.begin(), sym_sections.end(),
                       [&]( auto& sym_section ) { processSymSection( sym_section, symbols ); } );
        std::sort( symbols.begin(), symbols.end(),
                   []( const utils::Symbol& a, const utils::Symbol& b ) {
                       return ( a.value == b.value ) ? ( a.size > b.size ) : ( a.value < b.value );
                   } );

        // disect .rodata into functions
        disect_section_template( dst, rodata_sec, new_sec_hdr, ".rodata.", symbols );
    }
}

void parse_data_sections( const elfio& src, elfio& dst )
{
    auto data_sections = utils::get_sections_by_regex( src, ".*data.*" );
    auto sym_sections  = utils::get_sections_by_type( src, SHT_SYMTAB );

    for ( auto data_sec : data_sections ) {
        if ( data_sec->get_flags() != ( SHF_ALLOC | SHF_WRITE ) ||
             data_sec->get_type() != SHT_PROGBITS )
            continue;

        print( data_sec->get_name() );

        Elf64_Addr start = data_sec->get_address();
        Elf64_Addr end   = data_sec->get_address() + data_sec->get_size();

        section*       new_sec;
        utils::Section new_sec_hdr;
        new_sec_hdr.type      = SHT_PROGBITS;
        new_sec_hdr.flags     = data_sec->get_flags();
        new_sec_hdr.link      = 0x0;
        new_sec_hdr.addralign = data_sec->get_addr_align();
        new_sec_hdr.entsize   = data_sec->get_entry_size();
        new_sec_hdr.info      = 0x0;

        // get all symbols that point to functions in exectuable sections
        std::vector<utils::Symbol> symbols;

        auto processSymSection = [&]( auto& sym_sec, std::vector<utils::Symbol>& result ) {
            symbol_section_accessor sym_sec_acc( src, sym_sec );
            auto                    sv = utils::get_symtab_view( sym_sec_acc );
            sv                         = utils::filter_symtab_view_by_range( sv, start, end );
            std::copy_if( sv.begin(), sv.end(), std::back_inserter( result ),
                          []( const utils::Symbol& sym ) { return sym.size != 0; } );
        };
        std::for_each( sym_sections.begin(), sym_sections.end(),
                       [&]( auto& sym_section ) { processSymSection( sym_section, symbols ); } );
        std::sort( symbols.begin(), symbols.end(),
                   []( const utils::Symbol& a, const utils::Symbol& b ) {
                       return ( a.value == b.value ) ? ( a.size > b.size ) : ( a.value < b.value );
                   } );

        // disect .rodata into functions
        disect_section_template( dst, data_sec, new_sec_hdr, ".data.", symbols );
    }
}

void parse_bss_sections( const elfio& src, elfio& dst )
{
    auto bss_sections = utils::get_sections_by_regex( src, ".*bss.*" );

    for ( auto bss_sec : bss_sections ) {
        section*       new_sec;
        utils::Section new_sec_hdr;
        new_sec_hdr.name      = bss_sec->get_name();
        new_sec_hdr.type      = SHT_NOBITS;
        new_sec_hdr.flags     = bss_sec->get_flags();
        new_sec_hdr.link      = 0x0;
        new_sec_hdr.addralign = bss_sec->get_addr_align();
        new_sec_hdr.entsize   = bss_sec->get_entry_size();
        new_sec_hdr.addr      = bss_sec->get_address();
        new_sec_hdr.info      = 0x0;

        new_sec = utils::add_section( dst, new_sec_hdr );
        new_sec->set_size( bss_sec->get_size() );
    }
}

void copy_symtab( const elfio&             src,
                  elfio&                   dst,
                  symbol_section_accessor& dst_sym_acc,
                  string_section_accessor& dst_str_acc )
{
    auto                       sym_sections = utils::get_sections_by_type( src, SHT_SYMTAB );
    std::vector<utils::Symbol> symbols;

    // remove symbols to files and linker stuff
    auto processSymSection = [&]( auto& sym_sec, std::vector<utils::Symbol>& result ) {
        symbol_section_accessor sym_sec_acc( src, sym_sec );
        auto                    sv = utils::get_symtab_view( sym_sec_acc );
        std::copy_if( sv.begin(), sv.end(), std::back_inserter( result ),
                      []( const utils::Symbol& sym ) { return true; } );
    };
    std::for_each( sym_sections.begin(), sym_sections.end(),
                   [&]( auto& sym_section ) { processSymSection( sym_section, symbols ); } );
    symbols.erase( symbols.begin() + 0 );

    for ( auto old_sym : symbols ) {
        if ( old_sym.type != STT_SECTION && old_sym.section_index != SHN_ABS &&
             old_sym.section_index != SHN_UNDEF ) {
            for ( int j = 0; j < dst.sections.size(); j++ ) {
                section* section = dst.sections[j];

                if ( section->get_address() <= old_sym.value &&
                     section->get_address() + section->get_size() ) {
                    old_sym.section_index = section->get_index();
                    old_sym.value = old_sym.value - section->get_address();
                }
            }
        }

        utils::add_symbol( dst_sym_acc, dst_str_acc, old_sym );
    }

    dst_sym_acc.arrange_local_symbols();
}

void do_relocs( const elfio& src, elfio& dst, section* dst_symtab )
{
    auto                           sections         = utils::get_sections_by_regex( dst, ".*" );
    auto                           old_rel_sections = utils::get_sections_by_type( src, SHT_REL );
    std::vector<utils::Relocation> old_all_relocs;

    auto processRelSection = [&]( auto& rel_sec, std::vector<utils::Relocation> &all_relocs ) {
        relocation_section_accessor rel_sec_acc( src, rel_sec );
        auto                        rv = utils::get_reltab_view( rel_sec_acc );
        std::copy( rv.begin(), rv.end(), std::back_inserter( all_relocs ) );
    };
    std::for_each( old_rel_sections.begin(), old_rel_sections.end(),
                   [&]( auto& rel_sec ) { processRelSection( rel_sec, old_all_relocs ); } );

    for ( auto section : sections ) {
        if ( section->get_type() != SHT_PROGBITS ) {
            continue;
        }

        utils::Section new_rel_sec;
        new_rel_sec.addr      = 0x0;
        new_rel_sec.addralign = 0x4;
        new_rel_sec.entsize   = 0x8;
        new_rel_sec.flags     = SHF_INFO_LINK;
        new_rel_sec.type      = SHT_REL;
        new_rel_sec.name      = ".rel" + section->get_name();
        new_rel_sec.link      = dst_symtab->get_index();
        new_rel_sec.info      = section->get_index();

        auto                        new_rel_sec_ptr = utils::add_section( dst, new_rel_sec );
        relocation_section_accessor new_rel_sec_acc( dst, new_rel_sec_ptr );

        for ( auto old_rel : old_all_relocs ) {
            if (old_rel.type != R_386_32)
                continue;


            if ( section->get_address() <= old_rel.offset &&
                 section->get_address() + section->get_size() >= old_rel.offset ) {
                // apply relocation inside a section

                utils::Relocation new_rel;
                new_rel.offset = old_rel.offset - section->get_address();
                new_rel.symbol = old_rel.symbol;
                new_rel.type   = old_rel.type;
                new_rel.addend = old_rel.addend;

                utils::add_relocation( new_rel_sec_acc, new_rel );
                // maybe a bit diffrent depending on the type, think about it
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

    parse_text_section( reader, writer );
    parse_rodata_sections( reader, writer );
    parse_data_sections( reader, writer );
    parse_bss_sections( reader, writer );
    copy_symtab( reader, writer, new_symbols, new_strings );
    do_relocs( reader, writer, new_symtab_sec );

    writer.save( "./result.elf" );

    return 0;
}
