#pragma once

#include <cstdint>
#include <type_traits>

namespace compressedInts::utils
{

template <uint32_t TotalBits>
    requires(TotalBits <= 32)
struct TypeWithTotalBits
{
    using type = std::conditional_t<TotalBits <= 8, uint8_t, std::conditional_t<TotalBits <= 16, uint16_t, uint32_t>>;
};

} // namespace compressedInts::utils