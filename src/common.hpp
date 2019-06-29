#pragma once

#include <iostream>
#include "Renderer/renderer_helper.h"

#ifdef _WIN32
#define VK_USE_PLATFORM_WIN32_KHR
#define NOMINMAX
#endif

#ifdef __linux__ 
		// Get Linux File Path
#elif _WIN32
#include<Windows.h>
#endif

typedef uint32_t size;

constexpr int screen_width = 1280;
constexpr int screen_height = 720;

constexpr size		instance_count = 1 << 14;
constexpr float		relative_velocity = 0.5f;
constexpr float		relative_scale = 1.0f;

constexpr bool mouse_bounding_enabled = false;
constexpr bool mouse_drawing_enabled = true;

static float mouse_draw_radius = 30.0f;

static int max_size = relative_scale * glm::sqrt((screen_width * screen_height) / instance_count) * 0.5f;
static int min_size = max_size / 3;
static const int max_collisions = 20;

#define MAX_TITLE_CHARS 128
static char title[MAX_TITLE_CHARS];

#define log(str) std::cout << str << std::endl

namespace files
{
	static std::string get_app_path()
	{
		char current_path[FILENAME_MAX];
#ifdef __linux__ 
		// Get Linux File Path
#elif _WIN32
		GetModuleFileName(0, current_path, sizeof(current_path));
#endif
		return std::string(current_path);
	}
}

namespace simd
{
	// Code From https://stackoverflow.com/questions/36932240
	inline __m256 pack_left_256(const __m256& src, const unsigned int& mask)
	{
		uint64_t expanded_mask = _pdep_u64(mask, 0x0101010101010101);
		expanded_mask *= 0xFF;

		const uint64_t identity_indices = 0x0706050403020100;
		uint64_t wanted_indices = _pext_u64(identity_indices, expanded_mask);

		__m128i bytevec = _mm_cvtsi64_si128(wanted_indices);
		__m256i shufmask = _mm256_cvtepu8_epi32(bytevec);

		return _mm256_permutevar8x32_ps(src, shufmask);
	}

	inline __m256i pack_left_256_indices(const unsigned int& mask)
	{
		uint64_t expanded_mask = _pdep_u64(mask, 0x0101010101010101);
		expanded_mask *= 0xFF;

		const uint64_t identity_indices = 0x0706050403020100;
		uint64_t wanted_indices = _pext_u64(identity_indices, expanded_mask);

		__m128i bytevec = _mm_cvtsi64_si128(wanted_indices);

		return _mm256_cvtepu8_epi32(bytevec);
	}
}