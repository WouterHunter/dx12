#pragma once

#define GLM_FORCE_AVX2
#define GLM_FORCE_SILENT_WARNINGS
#include <glm/glm.hpp>

using i8 = int8_t;
using i16 = int16_t;
using i32 = int32_t;
using i64 = int64_t;

using u8 = uint8_t;
using u16 = uint16_t;
using u32 = uint32_t;
using u64 = uint64_t;

using b8 = bool;

using f32 = float;
using f64 = double;

using size_t = uint64_t;
using ptr_t = uintptr_t;

using glm::vec2;
using glm::vec3;
using glm::vec4;
using glm::mat2;
using glm::mat3;
using glm::mat4;
using glm::quat;
using glm::ivec2;
using glm::ivec3;
using glm::ivec4;
