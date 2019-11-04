#pragma once

#include <tao/json.hpp>
#include <base/plugins/base_plugin.h>

namespace wolf {
namespace to {
class string : public base_plugin {
 protected:
  void process(json &&message) override {
    output(json(tao::json::to_string(message)).copy_all(message));
  }
};

}
}


