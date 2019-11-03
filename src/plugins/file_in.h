//
// Created by lamanchy on 17.7.19.
//

#ifndef WOLF_FILE_IN_H
#define WOLF_FILE_IN_H

#include <base/plugins/threaded_plugin.h>
#include <base/options/base_option.h>
#include "istream_in.h"
namespace wolf {

template<typename Serializer>
class file_in : public istream_in<Serializer> {
 public:
  explicit file_in(const option<std::string> &file_name) : istream_in<Serializer>(file) {
    file.open(file_name->value(), std::ios_base::binary);
  }

 private:
  std::ifstream file;
};

}

#endif //WOLF_FILE_IN_H
