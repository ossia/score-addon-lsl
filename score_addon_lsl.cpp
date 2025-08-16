#include "score_addon_lsl.hpp"

#include <score/plugins/FactorySetup.hpp>

#include <LSL/LSLProtocolFactory.hpp>

score_addon_lsl::score_addon_lsl() = default;
score_addon_lsl::~score_addon_lsl() = default;

std::vector<score::InterfaceBase*> score_addon_lsl::factories(
    const score::ApplicationContext& ctx, const score::InterfaceKey& key) const
{
  return instantiate_factories<
      score::ApplicationContext,
      FW<Device::ProtocolFactory, Protocols::LSLProtocolFactory>>(ctx, key);
}

#include <score/plugins/PluginInstances.hpp>
SCORE_EXPORT_PLUGIN(score_addon_lsl)
