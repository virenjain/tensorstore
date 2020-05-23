// Copyright 2020 The TensorStore Authors
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//      http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

/// Tests for data_type.h

#include "tensorstore/data_type.h"

#include <gmock/gmock.h>
#include <gtest/gtest.h>
#include "absl/base/macros.h"
#include <nlohmann/json.hpp>
#include "tensorstore/index.h"
#include "tensorstore/internal/elementwise_function.h"
#include "tensorstore/util/status_testutil.h"

namespace {

using tensorstore::AllocateAndConstructShared;
using tensorstore::DataType;
using tensorstore::DataTypeIdOf;
using tensorstore::DataTypeOf;
using tensorstore::Index;
using tensorstore::IsElementType;
using tensorstore::MatchesStatus;
using tensorstore::StaticDataTypeCast;
using tensorstore::unchecked;
using tensorstore::internal::IterationBufferKind;
using tensorstore::internal::IterationBufferPointer;

using namespace tensorstore::data_types;  // NOLINT

namespace is_element_type_tests {
struct ClassT {};
union UnionT {};
enum class EnumT {};
static_assert(IsElementType<int>::value, "");
static_assert(IsElementType<void>::value, "");
static_assert(IsElementType<const void>::value, "");
static_assert(IsElementType<const int>::value, "");
static_assert(IsElementType<const int*>::value, "");
static_assert(IsElementType<const int* const>::value, "");
static_assert(!IsElementType<volatile int>::value, "");
static_assert(!IsElementType<int[]>::value, "");
static_assert(!IsElementType<int(int)>::value, "");
static_assert(!IsElementType<int&>::value, "");
static_assert(!IsElementType<const int&>::value, "");
static_assert(!IsElementType<volatile int&>::value, "");
static_assert(!IsElementType<int&&>::value, "");
static_assert(IsElementType<ClassT>::value, "");
static_assert(IsElementType<UnionT>::value, "");
static_assert(IsElementType<EnumT>::value, "");
static_assert(IsElementType<int ClassT::*>::value, "");
static_assert(IsElementType<int (ClassT::*)(int)>::value, "");
}  // namespace is_element_type_tests

struct X {
  int value = 3;
  ~X() { value = 5; }
};

TEST(ElementOperationsTest, UnsignedIntBasic) {
  DataType r = DataTypeOf<unsigned int>();
  EXPECT_EQ(r->type, typeid(unsigned int));
  EXPECT_EQ(r->size, sizeof(unsigned int));
  EXPECT_EQ(r->alignment, alignof(unsigned int));
}

TEST(ElementOperationsTest, UnsignedIntStaticDynamicConversion) {
  DataType r = DataTypeOf<unsigned int>();

  // Verify that the conversion succeeds.
  StaticDataTypeCast<unsigned int, unchecked>(r);
  StaticDataTypeCast<unsigned int, unchecked>(DataTypeOf<unsigned int>());
}

TEST(ElementOperationsTest, UnsignedIntConstruct) {
  DataType r = DataTypeOf<unsigned int>();

  alignas(alignof(
      unsigned int)) unsigned char dest_char_arr[sizeof(unsigned int) * 5];
  unsigned int* dest_arr = reinterpret_cast<unsigned int*>(&dest_char_arr[0]);
  r->construct(5, dest_arr);
  // Unsigned int constructor doesn't actually do anything.

  r->destroy(5, dest_arr);
  // Unsigned int destructor doesn't actually do anything.
}

TEST(ElementOperationsTest, UnsignedIntCompareEqual) {
  DataType r = DataTypeOf<unsigned int>();

  unsigned int arr1[5] = {1, 2, 2, 5, 6};
  unsigned int arr2[5] = {1, 2, 3, 4, 6};
  // Call the strided_function variant generated by
  // SimpleElementwiseFunction.
  EXPECT_EQ(
      0, r->compare_equal[IterationBufferKind::kStrided](
             nullptr, 0, IterationBufferPointer{arr1, sizeof(unsigned int) * 2},
             IterationBufferPointer{arr2, sizeof(unsigned int)},
             /*status=*/nullptr));
  EXPECT_EQ(
      2, r->compare_equal[IterationBufferKind::kStrided](
             nullptr, 2, IterationBufferPointer{arr1, sizeof(unsigned int) * 2},
             IterationBufferPointer{arr2, sizeof(unsigned int)},
             /*status=*/nullptr));
  EXPECT_EQ(
      2, r->compare_equal[IterationBufferKind::kStrided](
             nullptr, 3, IterationBufferPointer{arr1, sizeof(unsigned int) * 2},
             IterationBufferPointer{arr2, sizeof(unsigned int)},
             /*status=*/nullptr));
}

TEST(ElementOperationsTest, UnsignedIntCopyAssign) {
  DataType r = DataTypeOf<unsigned int>();

  unsigned int source_arr[] = {1, 2, 3, 4, 5};
  unsigned int dest_arr[5] = {0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF,
                              0xFFFFFFFF};
  // Call the strided_function variant generated by
  // SimpleElementwiseFunction.
  EXPECT_EQ(2, r->copy_assign[IterationBufferKind::kStrided](
                   nullptr, 2,
                   IterationBufferPointer{source_arr, sizeof(unsigned int) * 2},
                   IterationBufferPointer{dest_arr, sizeof(unsigned int)},
                   /*status=*/nullptr));
  EXPECT_THAT(dest_arr,
              ::testing::ElementsAre(1, 3, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF));

  EXPECT_EQ(
      2,
      r->copy_assign[IterationBufferKind::kStrided](
          nullptr, 2, IterationBufferPointer{source_arr, sizeof(unsigned int)},
          IterationBufferPointer{dest_arr + 1, sizeof(unsigned int) * 2},
          /*status=*/nullptr));
  EXPECT_THAT(dest_arr,
              ::testing::ElementsAre(1, 1, 0xFFFFFFFF, 2, 0xFFFFFFFF));

  EXPECT_EQ(2, r->copy_assign[IterationBufferKind::kStrided](
                   nullptr, 2,
                   IterationBufferPointer{source_arr, sizeof(unsigned int)},
                   IterationBufferPointer{dest_arr + 1, sizeof(unsigned int)},
                   /*status=*/nullptr));
  EXPECT_THAT(dest_arr, ::testing::ElementsAre(1, 1, 2, 2, 0xFFFFFFFF));
}

TEST(ElementOperationsTest, UnsignedIntMoveAssign) {
  DataType r = DataTypeOf<unsigned int>();

  unsigned int source_arr[] = {1, 2, 3, 4, 5};
  unsigned int dest_arr[5] = {0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF,
                              0xFFFFFFFF};
  // Call the strided_function variant generated by
  // SimpleElementwiseFunction.
  r->move_assign[IterationBufferKind::kStrided](
      nullptr, 2, IterationBufferPointer{source_arr, sizeof(unsigned int) * 2},
      IterationBufferPointer{dest_arr, sizeof(unsigned int)},
      /*status=*/nullptr);
  EXPECT_THAT(dest_arr,
              ::testing::ElementsAre(1, 3, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF));

  r->move_assign[IterationBufferKind::kStrided](
      nullptr, 2, IterationBufferPointer{source_arr, sizeof(unsigned int)},
      IterationBufferPointer{dest_arr + 1, sizeof(unsigned int) * 2},
      /*status=*/nullptr);
  EXPECT_THAT(dest_arr,
              ::testing::ElementsAre(1, 1, 0xFFFFFFFF, 2, 0xFFFFFFFF));

  r->move_assign[IterationBufferKind::kStrided](
      nullptr, 2, IterationBufferPointer{source_arr, sizeof(unsigned int)},
      IterationBufferPointer{dest_arr + 1, sizeof(unsigned int)},
      /*status=*/nullptr);
  EXPECT_THAT(dest_arr, ::testing::ElementsAre(1, 1, 2, 2, 0xFFFFFFFF));
}

TEST(ElementOperationsTest, UnsignedIntInitialize) {
  DataType r = DataTypeOf<unsigned int>();
  unsigned int dest_arr[5] = {0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF,
                              0xFFFFFFFF};
  // Call the strided_function variant generated by
  // SimpleElementwiseFunction.
  r->initialize[IterationBufferKind::kStrided](
      nullptr, 2, IterationBufferPointer{dest_arr, sizeof(unsigned int) * 2},
      /*status=*/nullptr);
  EXPECT_THAT(dest_arr,
              ::testing::ElementsAre(0, 0xFFFFFFFF, 0, 0xFFFFFFFF, 0xFFFFFFFF));

  r->initialize[IterationBufferKind::kStrided](
      nullptr, 2, IterationBufferPointer{dest_arr + 3, sizeof(unsigned int)},
      /*status=*/nullptr);
  EXPECT_THAT(dest_arr, ::testing::ElementsAre(0, 0xFFFFFFFF, 0, 0, 0));
}

TEST(ElementOperationsTest, UnsignedIntAppendToString) {
  DataType r = DataTypeOf<unsigned int>();
  std::string s = " ";
  unsigned int value = 5;
  r->append_to_string(&s, &value);
  EXPECT_EQ(" 5", s);
}

TEST(StaticElementRepresentationDeathTest, UnsignedInt) {
  EXPECT_DEBUG_DEATH((StaticDataTypeCast<unsigned int, unchecked>(
                         DataType(DataTypeOf<float>()))),
                     "StaticCast is not valid");
}

TEST(ElementOperationsTest, Class) {
  DataType r = DataTypeOf<X>();

  alignas(alignof(unsigned int)) unsigned char dest_char_arr[sizeof(X) * 2];
  X* dest_arr = reinterpret_cast<X*>(&dest_char_arr[0]);
  r->construct(2, dest_arr);
  EXPECT_EQ(3, dest_arr[0].value);
  EXPECT_EQ(3, dest_arr[1].value);
  r->destroy(2, dest_arr);
  EXPECT_EQ(5, dest_arr[0].value);
  EXPECT_EQ(5, dest_arr[1].value);

  EXPECT_EQ(0, r->compare_equal[IterationBufferKind::kStrided](
                   nullptr, 0, IterationBufferPointer{dest_arr, Index(0)},
                   IterationBufferPointer{dest_arr, Index(0)},
                   /*status=*/nullptr));

  EXPECT_EQ(0, r->compare_equal[IterationBufferKind::kStrided](
                   nullptr, 1, IterationBufferPointer{dest_arr, Index(0)},
                   IterationBufferPointer{dest_arr, Index(0)},
                   /*status=*/nullptr));
}

TEST(DataTypeTest, Construct) {
  DataType r;
  EXPECT_FALSE(r.valid());
  EXPECT_EQ(DataType(), r);
  r = DataTypeOf<float>();
  EXPECT_EQ(r, DataTypeOf<float>());
  EXPECT_TRUE(r.valid());
}

TEST(DataTypeTest, Comparison) {
  EXPECT_TRUE(DataTypeOf<int>() == DataTypeOf<int>());
  EXPECT_FALSE(DataTypeOf<int>() != DataTypeOf<int>());
  EXPECT_FALSE(DataTypeOf<float>() == DataTypeOf<int>());
  EXPECT_TRUE(DataTypeOf<float>() != DataTypeOf<int>());
  EXPECT_TRUE(DataType(DataTypeOf<float>()) != DataType(DataTypeOf<int>()));
  EXPECT_TRUE(DataType(DataTypeOf<float>()) == typeid(float));
  EXPECT_FALSE(DataType(DataTypeOf<float>()) == typeid(int));
  EXPECT_TRUE(DataType(DataTypeOf<float>()) != typeid(int));
  EXPECT_FALSE(DataType(DataTypeOf<float>()) != typeid(float));
  EXPECT_TRUE(typeid(float) == DataType(DataTypeOf<float>()));
  EXPECT_FALSE(typeid(float) != DataType(DataTypeOf<float>()));
  EXPECT_FALSE(DataType(DataTypeOf<int>()) != DataType(DataTypeOf<int>()));
  EXPECT_TRUE(DataType(DataTypeOf<int>()) == DataType(DataTypeOf<int>()));
}

TEST(AllocateAndConsructSharedTest, Destructor) {
  auto x = std::make_shared<int>();
  {
    auto ptr = AllocateAndConstructShared<std::shared_ptr<int>>(
        1, tensorstore::default_init);
    static_assert(std::is_same<std::shared_ptr<std::shared_ptr<int>>,
                               decltype(ptr)>::value,
                  "");
    ptr.get()[0] = x;
    EXPECT_EQ(2, x.use_count());
  }
  EXPECT_EQ(1, x.use_count());
}

TEST(AllocateAndConsructSharedTest, ValueInitialization) {
  auto ptr = AllocateAndConstructShared<int>(2, tensorstore::value_init);
  EXPECT_EQ(0, ptr.get()[0]);
  EXPECT_EQ(0, ptr.get()[1]);
}

// Thread sanitizer considers `operator new` allocation failure an error, and
// prevents this death test from working.
#if !defined(THREAD_SANITIZER)
TEST(AllocateAndConsructSharedDeathTest, OutOfMemory) {
  const auto allocate = [] {
    AllocateAndConstructShared<int>(0xFFFFFFFFFFFFFFF,
                                    tensorstore::default_init);
  };
#if ABSL_HAVE_EXCEPTIONS
  EXPECT_THROW(allocate(), std::bad_alloc);
#else
  EXPECT_DEATH(allocate(), "");
#endif
}
#endif  // defined(THREAD_SANITIZER)

TEST(DataTypeTest, Name) {
  EXPECT_EQ("bool", DataType(DataTypeOf<bool_t>()).name());
  EXPECT_EQ("byte", DataType(DataTypeOf<byte_t>()).name());
  EXPECT_EQ("char", DataType(DataTypeOf<char_t>()).name());
  EXPECT_EQ("int8", DataType(DataTypeOf<int8_t>()).name());
  EXPECT_EQ("uint8", DataType(DataTypeOf<uint8_t>()).name());
  EXPECT_EQ("int16", DataType(DataTypeOf<int16_t>()).name());
  EXPECT_EQ("uint16", DataType(DataTypeOf<uint16_t>()).name());
  EXPECT_EQ("int32", DataType(DataTypeOf<int32_t>()).name());
  EXPECT_EQ("uint32", DataType(DataTypeOf<uint32_t>()).name());
  EXPECT_EQ("int64", DataType(DataTypeOf<int64_t>()).name());
  EXPECT_EQ("uint64", DataType(DataTypeOf<uint64_t>()).name());
  EXPECT_EQ("float16", DataType(DataTypeOf<float16_t>()).name());
  EXPECT_EQ("float32", DataType(DataTypeOf<float32_t>()).name());
  EXPECT_EQ("float64", DataType(DataTypeOf<float64_t>()).name());
  EXPECT_EQ("complex64", DataType(DataTypeOf<complex64_t>()).name());
  EXPECT_EQ("complex128", DataType(DataTypeOf<complex128_t>()).name());
  EXPECT_EQ("string", DataType(DataTypeOf<string_t>()).name());
  EXPECT_EQ("ustring", DataType(DataTypeOf<ustring_t>()).name());
  EXPECT_EQ("json", DataType(DataTypeOf<json_t>()).name());
}

TEST(DataTypeTest, PrintToOstream) {
  EXPECT_EQ("int64", StrCat(DataTypeOf<std::int64_t>()));
  EXPECT_EQ("<unspecified>", StrCat(DataType()));
}

TEST(DataTypeTest, GetDataType) {
  using tensorstore::GetDataType;
  EXPECT_EQ(DataTypeOf<int8_t>(), GetDataType("int8"));
  EXPECT_EQ(DataTypeOf<uint8_t>(), GetDataType("uint8"));
  EXPECT_EQ(DataTypeOf<int16_t>(), GetDataType("int16"));
  EXPECT_EQ(DataTypeOf<uint16_t>(), GetDataType("uint16"));
  EXPECT_EQ(DataTypeOf<int32_t>(), GetDataType("int32"));
  EXPECT_EQ(DataTypeOf<uint32_t>(), GetDataType("uint32"));
  EXPECT_EQ(DataTypeOf<int64_t>(), GetDataType("int64"));
  EXPECT_EQ(DataTypeOf<uint64_t>(), GetDataType("uint64"));
  EXPECT_EQ(DataTypeOf<float32_t>(), GetDataType("float32"));
  EXPECT_EQ(DataTypeOf<float64_t>(), GetDataType("float64"));
  EXPECT_EQ(DataTypeOf<complex64_t>(), GetDataType("complex64"));
  EXPECT_EQ(DataTypeOf<complex128_t>(), GetDataType("complex128"));
  EXPECT_EQ(DataTypeOf<string_t>(), GetDataType("string"));
  EXPECT_EQ(DataTypeOf<bool_t>(), GetDataType("bool"));
  EXPECT_EQ(DataTypeOf<char_t>(), GetDataType("char"));
  EXPECT_EQ(DataTypeOf<byte_t>(), GetDataType("byte"));
  EXPECT_EQ(DataTypeOf<json_t>(), GetDataType("json"));
  EXPECT_EQ(DataType(), GetDataType("foo"));
}

TEST(DataTypeCastTest, Basic) {
  EXPECT_THAT(StaticDataTypeCast<int>(DataType()),
              ::testing::Optional(DataTypeOf<int>()));
  EXPECT_THAT(
      StaticDataTypeCast<std::int32_t>(DataType(DataTypeOf<float>())),
      MatchesStatus(absl::StatusCode::kInvalidArgument,
                    "Cannot cast data type of float32 to data type of int32"));
}

static_assert(DataTypeIdOf<int> == DataTypeIdOf<int32_t>);
static_assert(DataTypeIdOf<unsigned int> == DataTypeIdOf<uint32_t>);
static_assert(DataTypeIdOf<long long> == DataTypeIdOf<int64_t>);
static_assert(DataTypeIdOf<unsigned long long> == DataTypeIdOf<uint64_t>);
static_assert(sizeof(long) == 4 || sizeof(long) == 8);
static_assert(sizeof(long) != 4 || DataTypeIdOf<long> == DataTypeIdOf<int32_t>);
static_assert(sizeof(long) != 4 ||
              DataTypeIdOf<unsigned long> == DataTypeIdOf<uint32_t>);
static_assert(sizeof(long) == 4 || DataTypeIdOf<long> == DataTypeIdOf<int64_t>);
static_assert(sizeof(long) == 4 ||
              DataTypeIdOf<unsigned long> == DataTypeIdOf<uint64_t>);

}  // namespace
