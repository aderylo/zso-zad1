/* Copyright 2017 - 2024 R. Thomas
 * Copyright 2017 - 2024 Quarkslab
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */
#include <iostream>
#include <memory>
#include <iostream>
#include <fstream>
#include <algorithm>
#include <iterator>

#include <tuple>

#include <LIEF/ELF.hpp>

using namespace LIEF::ELF;

uint64_t get_symbol_file_offset(LIEF::ELF::Symbol &symbol)
{
  auto section = *symbol.section();
  return section.file_offset() + symbol.value() - section.virtual_address();
}

Section create_function_section(Symbol &symbol)
{
  auto s = Section();
  s.name(".text." + symbol.name());
  s.type(ELF_SECTION_TYPES::SHT_PROGBITS);
  s.virtual_address(0x0);
  s.file_offset(69);
  s.size(symbol.size());
  s.alignment(0x1);

  return s;
}

int main(int argc, char **argv)
{
  std::cout << "ELF Section rename" << '\n';
  if (argc != 3)
  {
    std::cerr << "Usage: " << argv[0] << "<binary> <binary output name>" << '\n';
    return -1;
  }

  std::unique_ptr<Binary> binary = Parser::parse(argv[1]);

  Binary::it_symbols symbol_table = binary->symbols();

  // binary->header().file_type(E_TYPE::ET_REL);

  for (auto &symbol : symbol_table)
  {
    if (symbol.is_function())
    {
      std::cout << symbol.name() << "\n";
      // std::cout << *symbol.section() << "\n";
      std::cout << symbol.section()->file_offset() << "\n";
      std::cout << symbol.section()->offset() << "\n";
      std::cout << get_symbol_file_offset(symbol) << "\n";

      Section new_s = create_function_section(symbol);
      std::cout << "Adding section... " << new_s.name() << "\n";
      std::cout << new_s.size() << "\n";
      binary->add(new_s, false);
      std::cout << new_s.size() << "\n";
    }
  }

  std::cout << binary->sections()[42].name() << "\n";
  std::cout << binary->sections()[42].size() << "\n";
  std::cout << binary->sections()[42].offset() << "\n";

  std::cout << binary->sections()[42].content().size_bytes();

  // for (size_t i = 0; i < 2; ++i)
  // {
  //   Section new_section{".test"};
  //   std::vector<uint8_t> data(100, 0);
  //   // new_section.content(std::move(data));
  //   new_section.file_offset(0x69);
  //   binary->add(new_section, false);
  //   std::cout << new_section.offset() << "\n";
  //   std::cout << new_section.file_offset() << "\n";

  // }
  // binary->write(argv[2]);

  LIEF::ELF::Builder builder{*binary};
  LIEF::ELF::Builder::config_t config;
  config.force_relocate = true;
  // config.rela = false;
  // config.static_symtab = false;
  // config.dt_hash = false;
  // config.dyn_str = false;
  // config.dt_hash = false;
  // config.dyn_str = false;
  // config.dynamic_section = false;
  // config.fini_array = false;
  // config.gnu_hash = false;
  // config.init_array = false;
  // config.interpreter = false;
  // config.jmprel = false;
  // config.notes = false;
  // config.preinit_array = false;
  // config.rela = false;
  // config.static_symtab = false;
  // config.sym_verdef = false;
  // config.sym_verneed = false;
  // config.sym_versym = false;
  // config.symtab = false;

  builder.set_config(config);
  builder.build();
  builder.write(argv[2]);

  return 0;
}