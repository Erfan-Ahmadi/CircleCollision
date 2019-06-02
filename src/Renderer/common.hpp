#pragma once

#include <iostream>
#include <vector>

#include <vulkan/vulkan.h>

constexpr uint64_t	instance_count = (1 << 13);
#define Log(str) std::cout << str << std::endl