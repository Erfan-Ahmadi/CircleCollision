#pragma once

#include <iostream>
#include <vector>

#include <vulkan/vulkan.h>

constexpr int INIT_WIDTH = 1280;
constexpr int INIT_HEIGHT = 720;

constexpr uint64_t	instance_count = (1 << 11);
#define Log(str) std::cout << str << std::endl