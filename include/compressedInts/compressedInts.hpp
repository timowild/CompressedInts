#pragma once

#include <compressedInts/utils.hpp>

#include <array>
#include <cstdint>
#include <limits.h>
#include <span>
#include <type_traits>

namespace compressedInts
{

template <typename V>
struct Holder
{
    static_assert(std::is_enum_v<V>, "V needs to be an enum");

    V m_valueName;
    uint8_t m_bitsNeeded;
};

template <typename V, Holder<V>... Values>
class CompressedInts
{
private:
    template <Holder<V> First, Holder<V>... Rest>
    static consteval bool allHolderValueNamesAreDifferent()
    {
        if constexpr (sizeof...(Rest) == 0)
        {
            return true;
        }
        else
        {
            return ((First.m_valueName != Rest.m_valueName) && ...) && allHolderValueNamesAreDifferent<Rest...>();
        }
    }

    static_assert(sizeof...(Values) > 0, "At least one Holder needed");
    static_assert(allHolderValueNamesAreDifferent<Values...>(), "All holder valueNames need to be different");
    static_assert(((Values.m_bitsNeeded > 0) && ...), "All holder bitsNeeded need to be > 0");

    static consteval uint32_t totalNumberOfBitsNeeded() { return (static_cast<uint32_t>(Values.m_bitsNeeded) + ...); }

    static constexpr int32_t BITS_NEEDED_AND_OFFSET_NOT_FOUND_INDEX = -1;
    static constexpr uint32_t BUFFER_SIZE_IN_BYTES = ((totalNumberOfBitsNeeded() + CHAR_BIT - 1) / CHAR_BIT); /*Round up*/

    struct BitsNeededAndOffset
    {
        uint8_t m_bitsNeeded;
        uint8_t m_offset;
    };

    static consteval std::array<BitsNeededAndOffset, sizeof...(Values)> computeBitsNeededAndOffsets()
    {
        std::array<BitsNeededAndOffset, sizeof...(Values)> bitsNeededAndOffsets{};
        uint32_t index{0};
        uint8_t oldBitsNeededPos{0};

        const auto computeBitsNeededAndOffsetsHelper = [&]<Holder<V> HolderV>()
        {
            bitsNeededAndOffsets[index] = BitsNeededAndOffset{.m_bitsNeeded = HolderV.m_bitsNeeded, .m_offset = oldBitsNeededPos};
            ++index;
            oldBitsNeededPos += HolderV.m_bitsNeeded;
        };

        (computeBitsNeededAndOffsetsHelper.template operator()<Values>(), ...);

        return bitsNeededAndOffsets;
    }

    template <V ValueName>
    static consteval int32_t getBitsNeededAndOffsetIndex()
    {
        constexpr auto getBitsNeededAndOffsetIndexHelper = []<int32_t Idx, Holder<V> First, Holder<V>... Rest>(const auto& impl)
        {
            if (First.m_valueName == ValueName)
            {
                return Idx;
            }
            else
            {
                if constexpr (sizeof...(Rest) > 0)
                {
                    return impl.template operator()<Idx + 1, Rest...>(impl);
                }
                else
                {
                    return BITS_NEEDED_AND_OFFSET_NOT_FOUND_INDEX;
                }
            }
        };

        return getBitsNeededAndOffsetIndexHelper.template operator()<0, Values...>(getBitsNeededAndOffsetIndexHelper);
    }

    static constexpr std::array<BitsNeededAndOffset, sizeof...(Values)> BITS_NEEDED_AND_OFFSETS = computeBitsNeededAndOffsets();

public:
    static constexpr bool containsValueName(V valueName) { return ((Values.m_valueName == valueName) || ...); }

    constexpr CompressedInts() = default;

    template <V ValueName>
    constexpr void setValue(uint32_t value)
    {
        constexpr auto bitsNeededAndOffsetIndex = getBitsNeededAndOffsetIndex<ValueName>();

        if constexpr (bitsNeededAndOffsetIndex == BITS_NEEDED_AND_OFFSET_NOT_FOUND_INDEX)
        {
            return;
        }
        else
        {
            // clang-format off
            const auto leastSignificantBlockCallback = [&value, this]<uint32_t Idx, uint32_t Shift, uint32_t Mask>()
            {
                m_backingStorage[Idx] &= static_cast<uint8_t>(~Mask); // Delete old value
                m_backingStorage[Idx] |= static_cast<uint8_t>((static_cast<uint8_t>(value) << Shift) & Mask);
            };

            const auto mainBlocksCallback = [&value, this]<uint32_t Idx, uint32_t Shift>()
            { 
                m_backingStorage[Idx] = static_cast<uint8_t>(value >> Shift);
            };

            const auto mostSignificantBlockCallback = [&value, this]<uint32_t Idx, uint32_t Shift, uint32_t Mask>()
            {
                m_backingStorage[Idx] &= static_cast<uint8_t>(~Mask); // Delete old value
                m_backingStorage[Idx] |= static_cast<uint8_t>((value >> Shift) & Mask);
            };
            // clang-format on

            compute<bitsNeededAndOffsetIndex>(leastSignificantBlockCallback, mainBlocksCallback, mostSignificantBlockCallback);
        }
    }

    template <V ValueName, typename T = uint32_t>
    constexpr T getValue(T defaultValue = T{}) const
    {
        constexpr auto bitsNeededAndOffsetIndex = getBitsNeededAndOffsetIndex<ValueName>();

        if constexpr (bitsNeededAndOffsetIndex == BITS_NEEDED_AND_OFFSET_NOT_FOUND_INDEX)
        {
            return defaultValue;
        }
        else
        {
            uint32_t value{0};

            // clang-format off
            const auto leastSignificantBlockCallback = [&value, this]<uint32_t Idx, uint32_t Shift, uint32_t Mask>()
            { 
                value |= static_cast<uint8_t>((m_backingStorage[Idx] & Mask) >> Shift);
            };

            const auto mainBlocksCallback = [&value, this]<uint32_t Idx, uint32_t Shift>()
            { 
                value |= static_cast<uint32_t>(m_backingStorage[Idx]) << Shift;
            };

            const auto mostSignificantBlockCallback = [&value, this]<uint32_t Idx, uint32_t Shift, uint32_t Mask>()
            { 
                value |= (static_cast<uint32_t>(m_backingStorage[Idx]) & Mask) << Shift;
            };
            // clang-format on

            compute<bitsNeededAndOffsetIndex>(leastSignificantBlockCallback, mainBlocksCallback, mostSignificantBlockCallback);

            return static_cast<T>(value);
        }
    }

    constexpr auto getData() const
    {
        if constexpr (BUFFER_SIZE_IN_BYTES == 1)
        {
            return static_cast<uint8_t>(m_backingStorage[0]);
        }
        else if constexpr (BUFFER_SIZE_IN_BYTES == 2)
        {
            return *reinterpret_cast<const uint16_t*>(m_backingStorage.data());
        }
        else if constexpr (BUFFER_SIZE_IN_BYTES == 3)
        {
            return static_cast<uint32_t>(m_backingStorage[2]) << (2 * CHAR_BIT) |
                   *reinterpret_cast<const uint16_t*>(m_backingStorage.data());
        }
        else if constexpr (BUFFER_SIZE_IN_BYTES == 4)
        {
            return *reinterpret_cast<const uint32_t*>(m_backingStorage.data());
        }
        else
        {
            return std::span<const uint8_t>{m_backingStorage};
        }
    }

private:
    template <uint32_t BitsNeededAndOffsetIndex>
    static constexpr void compute(const auto& leastSignificantBlockCallback, const auto& mainBlocksCallback,
                                  const auto& mostSignificantBlockCallback)
    {
        static_assert(BitsNeededAndOffsetIndex >= 0 && BitsNeededAndOffsetIndex < sizeof...(Values));

        constexpr auto bitsNeededAndOffset = BITS_NEEDED_AND_OFFSETS[BitsNeededAndOffsetIndex];

        constexpr auto indexStart = static_cast<uint32_t>(bitsNeededAndOffset.m_offset) / CHAR_BIT;
        constexpr auto indexEnd = (static_cast<uint32_t>(bitsNeededAndOffset.m_offset) + bitsNeededAndOffset.m_bitsNeeded - 1) / CHAR_BIT;

        constexpr auto remainingBitsInStartBlock = bitsNeededAndOffset.m_offset - indexStart * CHAR_BIT; // [0, 7]

        if constexpr (indexStart == indexEnd)
        {
            static_assert(bitsNeededAndOffset.m_bitsNeeded <= 8);

            constexpr auto mask = static_cast<uint8_t>((static_cast<uint16_t>(1) << bitsNeededAndOffset.m_bitsNeeded) - 1)
                                  << remainingBitsInStartBlock;

            leastSignificantBlockCallback.template operator()<indexStart, remainingBitsInStartBlock, mask>();
        }
        else
        {
            constexpr auto bitsUsedFromValueNameInStartBlock =
                remainingBitsInStartBlock == 0 ? CHAR_BIT : CHAR_BIT - remainingBitsInStartBlock; // [1, 7]

            // Least Significant Block
            {
                constexpr auto bitsUsedFromValueNameInStartBlockMask = static_cast<uint8_t>(~((1 << remainingBitsInStartBlock) - 1));

                leastSignificantBlockCallback
                    .template operator()<indexStart, remainingBitsInStartBlock, bitsUsedFromValueNameInStartBlockMask>();
            }

            // Main Blocks
            if constexpr (indexStart + 1 != indexEnd)
            {
                [&]<uint32_t... Idx>(std::integer_sequence<uint32_t, Idx...>) {
                    ((mainBlocksCallback
                          .template operator()<Idx, bitsUsedFromValueNameInStartBlock + (Idx - (indexStart + 1)) * CHAR_BIT>()),
                     ...);
                }(utils::integer_sequence_from_to<uint32_t, indexStart + 1, indexEnd>{});
            }

            constexpr auto bitsUsedFromValueNameInEndBlock =
                bitsNeededAndOffset.m_offset + bitsNeededAndOffset.m_bitsNeeded - indexEnd * CHAR_BIT; // [0, 7]

            // Most Significant Block
            if constexpr (bitsUsedFromValueNameInEndBlock != 0)
            {
                constexpr auto bitsUsedFromValueNameInEndBlockMask = static_cast<uint8_t>(((1 << bitsUsedFromValueNameInEndBlock) - 1));

                mostSignificantBlockCallback
                    .template operator()<indexEnd, bitsUsedFromValueNameInStartBlock + (indexEnd - indexStart - 1) * CHAR_BIT,
                                         bitsUsedFromValueNameInEndBlockMask>();
            }
        }
    }

    std::array<uint8_t, BUFFER_SIZE_IN_BYTES> m_backingStorage;
};

} // namespace compressedInts
