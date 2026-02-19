#pragma once

#include <string>
#include <vector>
#include <cstdint>
#include <cstddef>

std::pair<const unsigned char*, size_t> GetResource(const std::string& resname);
const std::vector<std::string>& GetResourceList();
