# Лабораторная работа №5

**Тема:** Системы модульного тестирования

**Студент:** Приходько Максим Максимович, ИУ8-24

**Дата:** 05.05.2026

Окружение: WSL2 Ubuntu 24.04, gcc 13.3.0, cmake 3.28.3, GoogleTest release-1.8.1, lcov 2.0, Coveralls. CI на GitHub Actions (Linux, gcc + clang) — Travis CI из исходного задания заменён на GitHub Actions по тем же причинам, что и в lab04.

## Tutorial

### Подготовка репозитория

В качестве отправной точки взято содержимое `lab04` (мульти-модульный CMake-проект Formatter Inc. + CI). Создан публичный репозиторий `maxopetya/lab05`, исходники перенесены, удалён `appveyor.yml` (Windows-сборка в lab05 не требуется).

```sh
$ git clone https://github.com/maxopetya/lab04.git ~/lab05-work
$ cd ~/lab05-work && rm -rf .git appveyor.yml REPORT.md README.md
$ git init -b master
```

### Подключение GoogleTest

Подключён как git submodule в `third-party/gtest`, зафиксирована версия `release-1.8.1` (как в задании):

```sh
$ mkdir third-party
$ git submodule add https://github.com/google/googletest third-party/gtest
$ cd third-party/gtest && git checkout release-1.8.1
```

### Туториальный тест на `print`

В `tests/test1.cpp` — тест из учебного материала: запись строки в файл через `print`, чтение обратно, сверка.

```cpp
TEST(Print, InFileStream) {
  std::string filepath = "file.txt";
  std::string text = "hello";
  std::ofstream out{filepath};
  print(text, out);
  out.close();

  std::string result;
  std::ifstream in{filepath};
  in >> result;
  EXPECT_EQ(result, text);
}
```

### CMake: цель `check`

В корневой `CMakeLists.txt` добавлены опции `BUILD_TESTS` и `COVERAGE`, политика `CMP0079 NEW` (чтобы из корня можно было дописывать линковочные флаги цели из подкаталога), и блок сборки тестов:

```cmake
if(BUILD_TESTS)
  enable_testing()
  add_subdirectory(third-party/gtest)
  foreach(_gtarget gtest gtest_main gmock gmock_main)
    if(TARGET ${_gtarget})
      target_compile_options(${_gtarget} PRIVATE -Wno-error)
    endif()
  endforeach()
  file(GLOB ${PROJECT_NAME}_TEST_SOURCES tests/*.cpp)
  add_executable(check ${${PROJECT_NAME}_TEST_SOURCES})
  target_link_libraries(check ${PROJECT_NAME} banking gmock_main)
  add_test(NAME check COMMAND check)
endif()
```

Подавление `-Werror` для целей gtest потребовалось из-за ошибки `‘dummy’ may be used uninitialized` в `gtest-death-test.cc` под gcc 13 — старый код gtest 1.8.1 не учитывает свежий анализ маяка инициализации.

### Сборка и прогон

```sh
$ cmake -H. -B_build -DBUILD_TESTS=ON -DCMAKE_POLICY_VERSION_MINIMUM=3.5
$ cmake --build _build
$ _build/check
[==========] 16 tests from 3 test cases ran. (1 ms total)
[  PASSED  ] 16 tests.
```

Флаг `-DCMAKE_POLICY_VERSION_MINIMUM=3.5` сохранён из lab04 — gtest 1.8.1 требует совместимости с CMake < 3.5.

## Homework

### Задание 1 — `banking/CMakeLists.txt`

Исходники `Account.{h,cpp}`, `Transaction.{h,cpp}` взяты из `tp-labs/lab05/banking`. В исходном задании `CMakeList.txt` пуст — написан с нуля:

```cmake
cmake_minimum_required(VERSION 3.4)
project(banking)
set(CMAKE_CXX_STANDARD 11)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

add_library(banking STATIC
  ${CMAKE_CURRENT_SOURCE_DIR}/Account.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/Transaction.cpp
)

target_include_directories(banking PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
```

В корневой `CMakeLists.txt` добавлен `add_subdirectory(banking)`.

### Задание 2 — модульные тесты с моками

`tests/banking_tests.cpp`. Реальный класс `Account` тестируется напрямую, через мок-объекты (`MockAccount`, `MockTransaction`) проверяется логика `Transaction::Make` без реальных побочных эффектов.

Mock на `Account` — все четыре виртуальных метода через `MOCK_*METHOD*` (gtest 1.8.1 — старый синтаксис):

```cpp
class MockAccount : public Account {
 public:
  MockAccount(int id, int balance) : Account(id, balance) {}
  MOCK_CONST_METHOD0(GetBalance, int());
  MOCK_METHOD1(ChangeBalance, void(int diff));
  MOCK_METHOD0(Lock, void());
  MOCK_METHOD0(Unlock, void());
};

class MockTransaction : public Transaction {
 public:
  MOCK_METHOD3(SaveToDataBase, void(Account&, Account&, int));
};
```

Сценарии для `Transaction::Make`:

| Тест | Что проверяется |
|---|---|
| `SameAccountThrows` | `from.id == to.id` → `std::logic_error` |
| `NegativeSumThrows` | `sum < 0` → `std::invalid_argument` |
| `TooSmallSumThrows` | `sum < 100` → `std::logic_error` |
| `BigFeeReturnsFalse` | `fee*2 > sum` → `false`, без `Lock`/`SaveToDataBase` |
| `SuccessfulTransfer` | `Lock/Unlock` × 2, `Credit`, `Debit` (успех), `SaveToDataBase` |
| `RollbackOnFailedDebit` | `Debit` возвращает `false` → `ChangeBalance(-sum)` (откат) |
| `RealSaveToDataBaseCoverage` | реальный `SaveToDataBase`, stdout перехвачен через `CaptureStdout` |

Для `Account` — пять тестов на состояния (`InitialState`, `ChangeWithoutLockThrows`, `LockAndChange`, `DoubleLockThrows`, `UnlockRevertsToLockedState`) плюс `PolymorphicDelete` — нужен, чтобы покрыть deleting destructor (`_ZN7AccountD0Ev`), генерируемый компилятором для `delete` через указатель на базовый класс. Аналогично `TransactionTest.PolymorphicDelete`.

Итог:

```sh
$ _build/check 2>&1 | tail -3
[==========] 16 tests from 3 test cases ran. (1 ms total)
[  PASSED  ] 16 tests.
```

### Замер покрытия

В корневом `CMakeLists.txt` — флаги покрытия только для целевых библиотек (`banking`, `print`), без gtest и тестов:

```cmake
if(COVERAGE AND CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
  target_compile_options(banking PRIVATE --coverage -O0 -g)
  target_link_libraries(banking PUBLIC --coverage)
  target_compile_options(print PRIVATE --coverage -O0 -g)
  target_link_libraries(print PUBLIC --coverage)
endif()
```

Локальный замер через gcov:

```sh
$ cmake -H. -B_build -DBUILD_TESTS=ON -DCOVERAGE=ON -DCMAKE_POLICY_VERSION_MINIMUM=3.5
$ cmake --build _build && _build/check > /dev/null
$ cd _build/banking/CMakeFiles/banking.dir
$ gcov -b Account.cpp.gcda
File '.../banking/Account.cpp'
Lines executed:100.00% of 15
Branches executed:100.00% of 8
$ gcov -b Transaction.cpp.gcda
File '.../banking/Transaction.cpp'
Lines executed:100.00% of 35
Branches executed:100.00% of 34
```

100% по строкам и веткам для обеих библиотек `banking`.

### Задание 3 — CI

`.github/workflows/ci.yml`. Две job-ы:

1. `build` — матрица `gcc`/`clang` на `ubuntu-latest`: configure → build → `ctest --output-on-failure`.
2. `coverage` — отдельная gcc-сборка с `-DCOVERAGE=ON`, после прогона тестов lcov собирает `coverage.info`, фильтруются третьи стороны и тесты, результат уходит в Coveralls через `coverallsapp/github-action@v2`:

```yaml
- name: Capture coverage
  run: |
    lcov --capture --directory _build --output-file coverage.info \
      --ignore-errors mismatch,negative,gcov,unused
    lcov --remove coverage.info '*/third-party/*' '*/tests/*' '/usr/*' \
      --output-file coverage.info --ignore-errors unused

- name: Upload to Coveralls
  uses: coverallsapp/github-action@v2
  with:
    github-token: ${{ secrets.GITHUB_TOKEN }}
    file: coverage.info
    format: lcov
```

### Задание 4 — Coveralls.io

Репозиторий `maxopetya/lab05` подключён к Coveralls. После первого зелёного прогона `coverage` отчёт публикуется на `coveralls.io/github/maxopetya/lab05`, бейдж появляется в `README.md`.

| Job             | Платформа | Компилятор | Результат |
|-----------------|-----------|------------|-----------|
| `linux-gcc`     | Linux     | gcc        | success   |
| `linux-clang`   | Linux     | clang      | success   |
| `coverage`      | Linux     | gcc        | success   |

## Ссылки

- Репозиторий лабы: <https://github.com/maxopetya/lab05>
- GitHub Actions: <https://github.com/maxopetya/lab05/actions>
- Coveralls: <https://coveralls.io/github/maxopetya/lab05>
- Каталог в учебном репозитории: <https://github.com/maxopetya/TIPM-LABS/tree/master/Lab05>

## Вывод

К проекту lab04 подключён GoogleTest как git submodule, написаны 16 модульных тестов на библиотеку `banking` и туториальный тест на `print`. Логика `Transaction::Make` проверяется через mock-объекты `Account` и `Transaction` с использованием `MOCK_METHOD*`, `EXPECT_CALL`, `WillOnce(Return(...))`. Покрытие `Account.cpp` и `Transaction.cpp` — 100% по строкам и веткам (gcov). CI на GitHub Actions гоняет тесты на gcc и clang, отдельная job замеряет покрытие через lcov и публикует его на Coveralls.io.
