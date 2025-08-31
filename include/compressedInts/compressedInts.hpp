#pragma once

#include <compressedInts/utils.hpp>

#include <array>
#include <cstdint>
#include <limits.h>
#include <span>
#include <type_traits>

namespace compressedInts
{

template <typename ValueNamesEnum>
struct Holder
{
    static_assert(std::is_enum_v<ValueNamesEnum>, "ValueNamesEnum needs to be an enum");

    ValueNamesEnum m_valueName;
    uint8_t m_bitsNeeded;
};

template <typename ValueNamesEnum, Holder<ValueNamesEnum>... Values>
class CompressedInts
{
private:
    template <Holder<ValueNamesEnum> First, Holder<ValueNamesEnum>... Rest>
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
    static_assert(((Values.m_bitsNeeded > 0 && Values.m_bitsNeeded < sizeof(uint32_t) * CHAR_BIT) && ...),
                  "All holder bitsNeeded need to be > 0 && < sizeof(uint32_t) * CHAR_BIT");
    static_assert(sizeof(uint8_t) * CHAR_BIT == CHAR_BIT, "CHAR_BIT is incorrectly set");

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

        const auto computeBitsNeededAndOffsetsHelper = [&]<Holder<ValueNamesEnum> HolderV>()
        {
            bitsNeededAndOffsets[index] = BitsNeededAndOffset{.m_bitsNeeded = HolderV.m_bitsNeeded, .m_offset = oldBitsNeededPos};
            ++index;
            oldBitsNeededPos += HolderV.m_bitsNeeded;
        };

        (computeBitsNeededAndOffsetsHelper.template operator()<Values>(), ...);

        return bitsNeededAndOffsets;
    }

    template <ValueNamesEnum ValueName>
    static consteval int32_t getBitsNeededAndOffsetIndex()
    {
        constexpr auto getBitsNeededAndOffsetIndexHelper =
            []<int32_t Idx, Holder<ValueNamesEnum> First, Holder<ValueNamesEnum>... Rest>(const auto& impl)
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
    static constexpr bool containsValueName(ValueNamesEnum valueName) { return ((Values.m_valueName == valueName) || ...); }

    constexpr CompressedInts() = default;

    template <ValueNamesEnum ValueName>
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
            const auto leastSignificantPartialBlockCallback = [&value, this]<uint32_t Idx, uint32_t Shift, uint32_t Mask>()
            {
                m_backingStorage[Idx] &= static_cast<uint8_t>(~Mask); // Delete old value
                m_backingStorage[Idx] |= static_cast<uint8_t>((static_cast<uint8_t>(value) << Shift) & Mask);
            };

            const auto mainCompleteBlocksCallback = [&value, this]<uint32_t Idx, uint32_t Shift, uint32_t BytesToRead>()
            {
                using CastT = typename utils::TypeWithTotalBits<BytesToRead * CHAR_BIT>::type;

                *reinterpret_cast<CastT*>(&m_backingStorage[Idx]) = static_cast<CastT>(value >> Shift);
            };

            const auto mostSignificantPartialBlockCallback = [&value, this]<uint32_t Idx, uint32_t Shift, uint32_t Mask>()
            {
                m_backingStorage[Idx] &= static_cast<uint8_t>(~Mask); // Delete old value
                m_backingStorage[Idx] |= static_cast<uint8_t>((value >> Shift) & Mask);
            };
            // clang-format on

            compute<bitsNeededAndOffsetIndex>(leastSignificantPartialBlockCallback, mainCompleteBlocksCallback,
                                              mostSignificantPartialBlockCallback);
        }
    }

    template <ValueNamesEnum ValueName, std::integral T = uint32_t>
    constexpr T getValue(T defaultValue = T{}) const
    {
        constexpr auto bitsNeededAndOffsetIndex = getBitsNeededAndOffsetIndex<ValueName>();

        if constexpr (bitsNeededAndOffsetIndex == BITS_NEEDED_AND_OFFSET_NOT_FOUND_INDEX)
        {
            return defaultValue;
        }
        else
        {
            T value{0};

            // clang-format off
            const auto leastSignificantPartialBlockCallback = [&value, this]<uint32_t Idx, uint32_t Shift, uint32_t Mask>()
            { 
                value |= static_cast<uint8_t>((m_backingStorage[Idx] & Mask) >> Shift);
            };

            const auto mainCompleteBlocksCallback = [&value, this]<uint32_t Idx, uint32_t Shift, uint32_t BytesToRead>()
            { 
                using CastT = typename utils::TypeWithTotalBits<BytesToRead * CHAR_BIT>::type;

                value |= static_cast<uint32_t>(*reinterpret_cast<const CastT*>(&m_backingStorage[Idx])) << Shift;
            };

            const auto mostSignificantPartialBlockCallback = [&value, this]<uint32_t Idx, uint32_t Shift, uint32_t Mask>()
            { 
                value |= (static_cast<uint32_t>(m_backingStorage[Idx]) & Mask) << Shift;
            };
            // clang-format on

            compute<bitsNeededAndOffsetIndex>(leastSignificantPartialBlockCallback, mainCompleteBlocksCallback,
                                              mostSignificantPartialBlockCallback);

            return value;
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
    static constexpr void compute(const auto& leastSignificantPartialBlockCallback, const auto& mainCompleteBlocksCallback,
                                  const auto& mostSignificantPartialBlockCallback)
    {
        static_assert(BitsNeededAndOffsetIndex >= 0 && BitsNeededAndOffsetIndex < sizeof...(Values));

        constexpr auto bitsNeededAndOffset = BITS_NEEDED_AND_OFFSETS[BitsNeededAndOffsetIndex];

        constexpr auto indexStart = static_cast<uint32_t>(bitsNeededAndOffset.m_offset) / CHAR_BIT;
        // Inclusive
        constexpr auto indexEnd = (static_cast<uint32_t>(bitsNeededAndOffset.m_offset) + bitsNeededAndOffset.m_bitsNeeded - 1) / CHAR_BIT;

        constexpr auto bitsUsedByAnotherValueNameInStartPartialBlock = bitsNeededAndOffset.m_offset - indexStart * CHAR_BIT; // [0, 7]

        if constexpr (indexStart == indexEnd) // Only one uint8_t-block is used
        {
            static_assert(bitsNeededAndOffset.m_bitsNeeded <= CHAR_BIT);

            constexpr auto idx = indexStart;
            constexpr auto shift = bitsUsedByAnotherValueNameInStartPartialBlock;

            if constexpr (bitsNeededAndOffset.m_bitsNeeded < CHAR_BIT) // Only a part of the block is used
            {
                constexpr auto mask = static_cast<uint8_t>((static_cast<uint16_t>(1) << bitsNeededAndOffset.m_bitsNeeded) - 1)
                                      << bitsUsedByAnotherValueNameInStartPartialBlock;

                leastSignificantPartialBlockCallback.template operator()<idx, shift, mask>();
            }
            else // Whole block is used
            {
                constexpr auto bytesToRead = sizeof(uint8_t);

                mainCompleteBlocksCallback.template operator()<idx, shift, bytesToRead>();
            }
        }
        else
        {
            // Least significant partial block
            if constexpr (bitsUsedByAnotherValueNameInStartPartialBlock > 0) // Partial block?
            {
                constexpr auto bitsUsedByThisValueNameInPartialStartBlockMask =
                    static_cast<uint8_t>(~((1 << bitsUsedByAnotherValueNameInStartPartialBlock) - 1));

                constexpr auto idx = indexStart;
                constexpr auto shift = bitsUsedByAnotherValueNameInStartPartialBlock;
                constexpr auto mask = bitsUsedByThisValueNameInPartialStartBlockMask;

                leastSignificantPartialBlockCallback.template operator()<idx, shift, mask>();
            }

            constexpr auto bitsUsedByThisValueNameInPartialStartBlock =
                bitsUsedByAnotherValueNameInStartPartialBlock == 0 ? 0 : CHAR_BIT - bitsUsedByAnotherValueNameInStartPartialBlock; // [0, 7]

            constexpr auto bitsUsedByThisValueNameInPartialEndBlock =
                (bitsNeededAndOffset.m_bitsNeeded - bitsUsedByThisValueNameInPartialStartBlock) % CHAR_BIT;

            constexpr auto indexStartMainCompleteBlocks = bitsUsedByAnotherValueNameInStartPartialBlock > 0 ? indexStart + 1 : indexStart;
            constexpr auto indexEndMainCompleteBlocks = bitsUsedByThisValueNameInPartialEndBlock > 0 ? indexEnd - 1 : indexEnd;

            // Main complete blocks
            if constexpr (indexStartMainCompleteBlocks <= indexEndMainCompleteBlocks)
            {
                constexpr auto indexRangeCount = indexEndMainCompleteBlocks - indexStartMainCompleteBlocks + 1;

                constexpr auto multipleOf32Bits = indexRangeCount / sizeof(uint32_t);
                constexpr auto remainder16Bits = (indexRangeCount / sizeof(uint16_t)) % sizeof(uint16_t);
                constexpr auto remainder8Bits = indexRangeCount % sizeof(uint16_t);

                static_assert(multipleOf32Bits <= 1, "Feature not added so far");

                if constexpr (multipleOf32Bits > 0)
                {
                    constexpr auto idx = indexStartMainCompleteBlocks;
                    constexpr auto shift = bitsUsedByThisValueNameInPartialStartBlock;
                    constexpr auto bytesToRead = sizeof(uint32_t);

                    mainCompleteBlocksCallback.template operator()<idx, shift, bytesToRead>();
                }

                if constexpr (remainder16Bits > 0)
                {
                    constexpr auto idx = indexStartMainCompleteBlocks + multipleOf32Bits * sizeof(uint32_t);
                    constexpr auto shift = bitsUsedByThisValueNameInPartialStartBlock + multipleOf32Bits * sizeof(uint32_t) * CHAR_BIT;
                    constexpr auto bytesToRead = sizeof(uint16_t);

                    mainCompleteBlocksCallback.template operator()<idx, shift, bytesToRead>();
                }

                if constexpr (remainder8Bits > 0)
                {
                    constexpr auto idx =
                        indexStartMainCompleteBlocks + multipleOf32Bits * sizeof(uint32_t) + remainder16Bits * sizeof(uint16_t);
                    constexpr auto shift = bitsUsedByThisValueNameInPartialStartBlock +
                                           (multipleOf32Bits * sizeof(uint32_t) + remainder16Bits * sizeof(uint16_t)) * CHAR_BIT;
                    constexpr auto bytesToRead = sizeof(uint8_t);

                    mainCompleteBlocksCallback.template operator()<idx, shift, bytesToRead>();
                }
            }

            // Most significant partial block
            if constexpr (bitsUsedByThisValueNameInPartialEndBlock > 0) // Partial block?
            {
                constexpr auto bitsUsedByThisValueNameInPartialEndBlockMask =
                    static_cast<uint8_t>(((1 << bitsUsedByThisValueNameInPartialEndBlock) - 1));

                constexpr auto idx = indexEnd;
                constexpr auto shift =
                    bitsUsedByThisValueNameInPartialStartBlock + (indexEndMainCompleteBlocks - indexStartMainCompleteBlocks + 1) * CHAR_BIT;
                constexpr auto mask = bitsUsedByThisValueNameInPartialEndBlockMask;

                mostSignificantPartialBlockCallback.template operator()<idx, shift, mask>();
            }
        }
    }

    std::array<uint8_t, BUFFER_SIZE_IN_BYTES> m_backingStorage;
};

} // namespace compressedInts
