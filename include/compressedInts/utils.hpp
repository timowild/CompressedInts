#pragma once

#include <cstdint>
#include <type_traits>

namespace compressedInts::utils
{

namespace details
{
/// Increases all values of the index_sequence by a value of "Add"
/// For example: <1,2,3,4> + 2 => <3,4,5,6>
template <typename T, class IntegerSequence, T Add>
struct increase_integer_sequence;

template <typename T, T... Idx, T Add>
struct increase_integer_sequence<T, std::integer_sequence<T, Idx...>, Add>
{
    using type = std::integer_sequence<T, (Idx + Add)...>;
};
} // namespace details

/// integer sequence of type size_t of the range [Start, End)
template <typename T, T Start, T End>
requires(Start <= End)
using integer_sequence_from_to = typename details::increase_integer_sequence<T, std::make_integer_sequence<T, End - Start>, Start>::type;

template <uint32_t TotalBits>
    requires(TotalBits <= 32)
struct TypeWithTotalBits
{
    using type = std::conditional_t<TotalBits <= 8, uint8_t, std::conditional_t<TotalBits <= 16, uint16_t, uint32_t>>;
};

} // namespace compressedInts