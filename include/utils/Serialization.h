#pragma once

#include "network/Message.h"  
#include <vector>
#include <optional>

namespace LocalTether::Utils {

std::vector<uint8_t> serializeInputPayload(const Network::InputPayload& payload);

std::optional<Network::InputPayload> deserializeInputPayload(const uint8_t* data, size_t length);

}