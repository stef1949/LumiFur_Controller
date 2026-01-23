#include <unity.h>

#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <memory>
#include <new>
#include <vector>

struct Widget
{
  int a;
  float b;
};

struct alignas(16) AlignedBlock
{
  std::uint8_t data[16];
};

void setUp(void)
{
}

void tearDown(void)
{
}

void test_new_delete_single(void)
{
  Widget *w = new (std::nothrow) Widget{42, 3.5f};
  TEST_ASSERT_NOT_NULL(w);
  if (w)
  {
    TEST_ASSERT_EQUAL_INT(42, w->a);
    TEST_ASSERT_FLOAT_WITHIN(0.0001f, 3.5f, w->b);
  }
  delete w;
}

void test_new_delete_array(void)
{
  constexpr size_t count = 8;
  int *arr = new (std::nothrow) int[count];
  TEST_ASSERT_NOT_NULL(arr);
  if (arr)
  {
    for (size_t i = 0; i < count; ++i)
    {
      arr[i] = static_cast<int>(i * 3);
    }
    for (size_t i = 0; i < count; ++i)
    {
      TEST_ASSERT_EQUAL_INT(static_cast<int>(i * 3), arr[i]);
    }
  }
  delete[] arr;
}

void test_malloc_free_block(void)
{
  constexpr size_t bytes = 64;
  auto *mem = static_cast<std::uint8_t *>(std::malloc(bytes));
  TEST_ASSERT_NOT_NULL(mem);
  if (mem)
  {
    std::memset(mem, 0xAB, bytes);
    for (size_t i = 0; i < bytes; ++i)
    {
      TEST_ASSERT_EQUAL_UINT8(0xAB, mem[i]);
    }
  }
  std::free(mem);
}

void test_unique_ptr_reset(void)
{
  std::unique_ptr<int> ptr(new int(7));
  TEST_ASSERT_NOT_NULL(ptr.get());
  TEST_ASSERT_EQUAL_INT(7, *ptr);

  ptr.reset(new int(9));
  TEST_ASSERT_NOT_NULL(ptr.get());
  TEST_ASSERT_EQUAL_INT(9, *ptr);
}

void test_vector_reserve_and_push(void)
{
  std::vector<int> values;
  values.reserve(16);
  TEST_ASSERT_TRUE(values.capacity() >= 16);

  for (int i = 0; i < 16; ++i)
  {
    values.push_back(i);
  }

  TEST_ASSERT_EQUAL_UINT32(16, static_cast<std::uint32_t>(values.size()));
  for (int i = 0; i < 16; ++i)
  {
    TEST_ASSERT_EQUAL_INT(i, values[static_cast<size_t>(i)]);
  }
}

void test_vector_clear_keeps_capacity(void)
{
  std::vector<int> values(10, 1);
  const size_t capacity = values.capacity();
  values.clear();
  TEST_ASSERT_EQUAL_UINT32(0, static_cast<std::uint32_t>(values.size()));
  TEST_ASSERT_TRUE(values.capacity() >= capacity);
}

void test_aligned_allocation(void)
{
  AlignedBlock *block = new (std::nothrow) AlignedBlock{};
  TEST_ASSERT_NOT_NULL(block);
  if (block)
  {
    const std::uintptr_t addr = reinterpret_cast<std::uintptr_t>(block);
    const std::uintptr_t alignment = static_cast<std::uintptr_t>(alignof(AlignedBlock));
    TEST_ASSERT_EQUAL_UINT64(0, addr % alignment);
  }
  delete block;
}

void setup()
{
  UNITY_BEGIN();
  RUN_TEST(test_new_delete_single);
  RUN_TEST(test_new_delete_array);
  RUN_TEST(test_malloc_free_block);
  RUN_TEST(test_unique_ptr_reset);
  RUN_TEST(test_vector_reserve_and_push);
  RUN_TEST(test_vector_clear_keeps_capacity);
  RUN_TEST(test_aligned_allocation);
  UNITY_END();
}

int main()
{
  setup();
  return 0;
}

void loop()
{
  // Unity tests run once
}
