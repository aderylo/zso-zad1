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
#include <algorithm>
#include <fstream>
#include <iostream>
#include <iterator>
#include <memory>

#include <tuple>

#include <LIEF/ELF.hpp>

using namespace LIEF::ELF;

uint64_t get_symbol_file_offset(LIEF::ELF::Symbol &symbol) {
  auto section = *symbol.section();
  return section.file_offset() + symbol.value() - section.virtual_address();
}

size_t get_section_idx(Binary &binary, Section &section) {
  size_t idx = 0, res = 0;
  for (auto s : binary.sections()) {
    if (s.name() == section.name()) {
      res = idx;
      break;
    }
    idx++;
  }

  return res;
}

Section create_function_section(Symbol &symbol) {
  auto s = Section();
  s.name(".text." + symbol.name());
  s.type(ELF_SECTION_TYPES::SHT_PROGBITS);
  s.virtual_address(0x0);
  s.file_offset(get_symbol_file_offset(symbol));
  s.size(symbol.size());
  s.alignment(0x1);

  return s;
}

Section create_variable_section(Symbol &symbol) {}

int main(int argc, char **argv) {
  std::cout << "ELF Section rename" << '\n';
  if (argc != 3) {
    std::cerr << "Usage: " << argv[0] << "<binary> <binary output name>" << '\n';
    return -1;
  }

  std::unique_ptr<Binary> binary = Parser::parse(argv[1]);

  Binary::it_symbols symbol_table = binary->symbols();

  for (auto &symbol : symbol_table) {
    if (symbol.is_function()) {
      Section new_s = create_function_section(symbol);
      auto s = binary->add(new_s, false);  // offset and size do not persists
      binary->get_section(new_s.name())->file_offset(get_symbol_file_offset(symbol));
      binary->get_section(new_s.name())->size(symbol.size());
      symbol.shndx(get_section_idx(*binary, *s) - 1);  // because shstrtab will be relocated to last
      symbol.value(0x0);  // since it points to section that represents function
    } else {
    }
  }

  LIEF::ELF::Builder builder{*binary};
  LIEF::ELF::Builder::config_t config;
  config.force_relocate = false;
  builder.set_config(config);
  builder.build();
  builder.write(argv[2]);

  return 0;
}