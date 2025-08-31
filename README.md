# CompressedInts

Integer variables are ubiquitous in computer programmes. In most cases, however, only a fraction of the total value range of an integer data type is used, resulting in a considerable waste of storage space.

CompressedInts is a pure header library for C++20 that reduces this waste of memory space by storing (compressing) multiple small integers in one large integer data type using bit shift operations. Compared to my other library [MultipleInt](https://github.com/timowild/MultipleInt), CompressedInts does not require the number of bits needed for each small integer to be identical.

## Example

CompressedInts provides a single class named `CompressedInts` in the namespace `compressedInts`, where this class expects a `ValueNameEnum` (= user-defined enum type for naming each small integer) and a variadic template of `Holder<ValueNamesEnum>`, where each a `ValueNameEnum`-constant and `BitsNeeded` for storing the small integer is required.

```cpp
#include <compressedInts/compressedInts.hpp>

enum class ValueNames
{
    V1,
    V2,
    V3
};

int main()
{
    static constexpr uint32_t V1_BITS_NEEDED = 3;
    static constexpr uint32_t V2_BITS_NEEDED = 10;

    compressedInts::CompressedInts<ValueNames, {ValueNames::V1, V1_BITS_NEEDED}, 
                                               {ValueNames::V2, V2_BITS_NEEDED}> container{};

    const bool containsV1 = container.containsValueName(ValueNames::V1); // true
    const bool containsV2 = container.containsValueName(ValueNames::V2); // true
    const bool containsV3 = container.containsValueName(ValueNames::V3); // false

    container.setValue<ValueNames::V1>(1);
    const auto v1GetValue = container.getValue<ValueNames::V1>(); // 1

    container.setValue<ValueNames::V2>(265);
    const auto v2GetValue = container.getValue<ValueNames::V2>(); // 265

    // Get plain data of internal backing storage
    const auto storedPlainData = container.getData(); // 265 << V1_BITS_NEEDED + 1 = 2121

    return 0;
}
```

Further examples can be found in the [tests](tests)-folder.

## Installation

Since CompressedInts is a header-only library, no installation is required. Copying the files from the [include](include)-folder into your project is sufficient.

## Licensing

MIT - See [LICENSE](LICENSE)
