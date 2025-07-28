#include <gtest/gtest.h>

#include <compressedInts/compressedInts.hpp>

int main(int argc, char* argv[])
{
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}

template <uint32_t TotalBits>
    requires(TotalBits <= 32)
struct TypeWithTotalBits
{
    using type = std::conditional_t<TotalBits <= 8, uint8_t, std::conditional_t<TotalBits <= 16, uint16_t, uint32_t>>;
};

enum class Values
{
    V1,
    V2,
    V3
};

TEST(CompressedInts, ContainsValueName)
{
    using namespace compressedInts;

    constexpr CompressedInts<Values, {Values::V1, 2}, {Values::V2, 4}> test{};

    static_assert(test.containsValueName(Values::V1));
    static_assert(test.containsValueName(Values::V2));
    static_assert(!test.containsValueName(Values::V3));
}

TEST(CompressedInts, setAndGetValue)
{
    constexpr uint8_t V1_BITS_NEEDED_MAX = 16;
    constexpr uint8_t V2_BITS_NEEDED_MAX = 16;

    []<uint32_t... V1BitsNeededSeq>(std::integer_sequence<uint32_t, V1BitsNeededSeq...>)
    {
        const auto f = []<uint32_t V1BitsNeeded, uint32_t... V2BitsNeededSeq>(std::integer_sequence<uint32_t, V2BitsNeededSeq...>)
        {
            const auto f2 = []<uint32_t V2BitsNeeded>()
            {
                compressedInts::CompressedInts<Values, {Values::V1, V1BitsNeeded}, {Values::V2, V2BitsNeeded}> test{};

                test.template setValue<Values::V2>(0);

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

                static_assert(std::is_same_v<typename TypeWithTotalBits<V1BitsNeeded + V2BitsNeeded>::type, decltype(test.getData())>);

                // Does not exist
                test.template setValue<Values::V3>(9);
                EXPECT_EQ(test.template getValue<Values::V3>(), 0);
            };

            (f2.template operator()<V2BitsNeededSeq>(), ...);
        };

        ((f.template operator()<V1BitsNeededSeq>(
             compressedInts::utils::integer_sequence_from_to<uint32_t, V1BitsNeededSeq, V2_BITS_NEEDED_MAX + 1>{})),
         ...);
    }(compressedInts::utils::integer_sequence_from_to<uint32_t, 1, V1_BITS_NEEDED_MAX + 1>{});
}