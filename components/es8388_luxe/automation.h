#pragma once

#include "esphome/core/automation.h"

#include "es8388_luxe.h"

namespace esphome::es8388_luxe {

template<typename... Ts> class ReinitializeAction : public Action<Ts...>, public Parented<ES8388Luxe> {
 public:
  void play(Ts... x) override { this->parent_->reinitialize(); }
};

template<typename... Ts> class SetOutputRouteAction : public Action<Ts...>, public Parented<ES8388Luxe> {
 public:
  TEMPLATABLE_VALUE(OutputRoute, route)

  void play(Ts... x) override { this->parent_->set_output_route(this->route_.value(x...)); }
};

}  // namespace esphome::es8388_luxe
