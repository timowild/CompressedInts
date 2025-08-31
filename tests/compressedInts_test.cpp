#include <gtest/gtest.h>

#include <compressedInts/compressedInts.hpp>

#include <cstdint>
#include <type_traits>

int main(int argc, char* argv[])
{
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}

enum class Values
{
    V1,
    V2,
    V3
};

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

TEST(CompressedInts, ContainsValueName)
{
    constexpr compressedInts::CompressedInts<Values, {Values::V1, 2}, {Values::V2, 4}> test{};

    static_assert(test.containsValueName(Values::V1));
    static_assert(test.containsValueName(Values::V2));
    static_assert(!test.containsValueName(Values::V3));
}

TEST(CompressedInts, setAndGetValueOneValueName)
{
    constexpr uint8_t V1_BITS_NEEDED_MAX = 26;

    []<uint32_t... V1BitsNeededSeq>(std::integer_sequence<uint32_t, V1BitsNeededSeq...>)
    {
        const auto f = []<uint32_t V1BitsNeeded>()
        {
            compressedInts::CompressedInts<Values, {Values::V1, V1BitsNeeded}> test{};

            for (uint32_t i = 0; i < (static_cast<uint32_t>(1) << V1BitsNeeded); ++i)
            {
                test.template setValue<Values::V1>(i);
                EXPECT_EQ(test.template getValue<Values::V1>(), i);

                EXPECT_EQ(test.getData(), i);
            }

            test.template setValue<Values::V1>((static_cast<uint32_t>(1) << V1BitsNeeded) + 1);
            EXPECT_EQ(test.template getValue<Values::V1>(), 1); // Overflow (upper bits will be cut off)

            static_assert(std::is_same_v<typename compressedInts::utils::TypeWithTotalBits<V1BitsNeeded>::type, decltype(test.getData())>);
        };

        (f.template operator()<V1BitsNeededSeq>(), ...);
    }(integer_sequence_from_to<uint32_t, 1, V1_BITS_NEEDED_MAX + 1>{});
}

TEST(CompressedInts, setAndGetValueMultipleValueNames)
{
    constexpr uint8_t V1_BITS_NEEDED_MAX = 9;
    constexpr uint8_t V2_BITS_NEEDED_MAX = 9;
    constexpr uint8_t V3_BITS_NEEDED_MAX = 9;

    []<uint32_t... V1BitsNeededSeq, uint32_t... V2BitsNeededSeq, uint32_t... V3BitsNeededSeq>(
        std::integer_sequence<uint32_t, V1BitsNeededSeq...>, std::integer_sequence<uint32_t, V2BitsNeededSeq...>,
        std::integer_sequence<uint32_t, V3BitsNeededSeq...>)
    {
        const auto f = []<uint32_t V1BitsNeeded>()
        {
            const auto f2 = []<uint32_t V2BitsNeeded>()
            {
                const auto f3 = []<uint32_t V3BitsNeeded>()
                {
                    compressedInts::CompressedInts<Values, {Values::V1, V1BitsNeeded}, {Values::V2, V2BitsNeeded},
                                                   {Values::V3, V3BitsNeeded}>
                        test{};

                    test.template setValue<Values::V2>(0);
                    test.template setValue<Values::V3>(0);

                    for (uint32_t i = 0; i < (1 << V1BitsNeeded); ++i)
                    {
                        test.template setValue<Values::V1>(i);
                        EXPECT_EQ(test.template getValue<Values::V1>(), i);

                        EXPECT_EQ(test.getData(), i);
                    }

                    test.template setValue<Values::V1>((1 << V1BitsNeeded) + 1);
                    EXPECT_EQ(test.template getValue<Values::V1>(), 1); // Overflow (upper bits will be cut off)

                    test.template setValue<Values::V1>(0);

                    for (uint32_t i = 0; i < (1 << V2BitsNeeded); ++i)
                    {
                        test.template setValue<Values::V2>(i);
                        EXPECT_EQ(test.template getValue<Values::V2>(), i);

                        EXPECT_EQ(test.getData(), i << V1BitsNeeded);
                    }

                    test.template setValue<Values::V2>((1 << V2BitsNeeded) + 1);
                    EXPECT_EQ(test.template getValue<Values::V2>(), 1); // Overflow (upper bits will be cut off)

                    test.template setValue<Values::V2>(0);

                    for (uint32_t i = 0; i < (1 << V3BitsNeeded); ++i)
                    {
                        test.template setValue<Values::V3>(i);
                        EXPECT_EQ(test.template getValue<Values::V3>(), i);

                        EXPECT_EQ(test.getData(), i << (V1BitsNeeded + V2BitsNeeded));
                    }

                    test.template setValue<Values::V3>((1 << V3BitsNeeded) + 1);
                    EXPECT_EQ(test.template getValue<Values::V3>(), 1); // Overflow (upper bits will be cut off)

                    constexpr auto totalBits = V1BitsNeeded + V2BitsNeeded + V3BitsNeeded;

                    if constexpr (totalBits <= 32)
                    {
                        static_assert(
                            std::is_same_v<typename compressedInts::utils::TypeWithTotalBits<totalBits>::type, decltype(test.getData())>);
                    }
                };

                (f3.template operator()<V3BitsNeededSeq>(), ...);
            };

            (f2.template operator()<V2BitsNeededSeq>(), ...);
        };

        (f.template operator()<V1BitsNeededSeq>(), ...);
    }(integer_sequence_from_to<uint32_t, 1, V1_BITS_NEEDED_MAX + 1>{}, integer_sequence_from_to<uint32_t, 1, V2_BITS_NEEDED_MAX + 1>{},
      integer_sequence_from_to<uint32_t, 1, V3_BITS_NEEDED_MAX + 1>{});
}