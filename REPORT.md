# Лабораторная работа №5

**Тема:** Системы модульного тестирования

**Студент:** Приходько Максим Максимович, ИУ8-24

**Дата:** 05.05.2026

Окружение: WSL2 Ubuntu 24.04, gcc 13.3.0, cmake 3.28.3, GoogleTest release-1.8.1, lcov 2.0. CI на GitHub Actions, покрытие — Coveralls. Travis CI заменён на GitHub Actions, как и в lab04.

## Tutorial

В качестве отправной точки взято содержимое `lab04`, репозиторий перенацелен на `maxopetya/lab05`. GoogleTest подключён как git submodule, версия `release-1.8.1`:

```sh
$ git submodule add https://github.com/google/googletest third-party/gtest
$ cd third-party/gtest && git checkout release-1.8.1
```

В корневой `CMakeLists.txt` добавлены опции `BUILD_TESTS`, `COVERAGE` и блок сборки тестов:

```cmake
if(BUILD_TESTS)
  enable_testing()
  add_subdirectory(third-party/gtest)
  file(GLOB ${PROJECT_NAME}_TEST_SOURCES tests/*.cpp)
  add_executable(check ${${PROJECT_NAME}_TEST_SOURCES})
  target_link_libraries(check ${PROJECT_NAME} banking gmock_main)
  add_test(NAME check COMMAND check)
endif()
```

Туториальный тест `tests/test1.cpp` проверяет `print` в `ofstream` — записывает строку в файл, читает её обратно и сверяет. Дополнительно — тест в `std::ostringstream` для покрытия второй перегрузки `print`.

Под gcc 13 gtest 1.8.1 ловит `-Werror=maybe-uninitialized` в `gtest-death-test.cc`. Для целей gtest добавлен `-Wno-error`. Также сохранён `-DCMAKE_POLICY_VERSION_MINIMUM=3.5` (минимально требуемая cmake-политика для gtest 1.8.1, как и в lab04).

```sh
$ cmake -H. -B_build -DBUILD_TESTS=ON -DCMAKE_POLICY_VERSION_MINIMUM=3.5
$ cmake --build _build && _build/check
[==========] 17 tests from 3 test cases ran. (1 ms total)
[  PASSED  ] 17 tests.
```

## Homework

### `banking/CMakeLists.txt`

В исходном задании пуст. Написан с нуля: статическая библиотека из `Account.cpp` и `Transaction.cpp`, заголовки наружу через `target_include_directories(... PUBLIC ...)`. В корневой `CMakeLists.txt` добавлен `add_subdirectory(banking)`.

### Тесты с моками

`tests/banking_tests.cpp`. `Account` тестируется напрямую — пять сценариев на состояния (ctor, `Lock`/`Unlock`, `ChangeBalance` с/без блокировки, повторный `Lock`).

Логика `Transaction::Make` проверяется через моки. `MockAccount` реализует все четыре виртуальных метода `Account`, `MockTransaction` подменяет `SaveToDataBase`:

```cpp
class MockAccount : public Account {
 public:
  MockAccount(int id, int balance) : Account(id, balance) {}
  MOCK_CONST_METHOD0(GetBalance, int());
  MOCK_METHOD1(ChangeBalance, void(int));
  MOCK_METHOD0(Lock, void());
  MOCK_METHOD0(Unlock, void());
};

class MockTransaction : public Transaction {
 public:
  MOCK_METHOD3(SaveToDataBase, void(Account&, Account&, int));
};
```

Сценарии `Make`: одинаковые id (`logic_error`), отрицательная сумма (`invalid_argument`), сумма `< 100` (`logic_error`), большая комиссия (`return false`), успешный перевод (вызовы `Lock`/`Unlock`/`Credit`/`Debit`/`SaveToDataBase`), откат при неудачном `Debit` (`ChangeBalance(-sum)`). Отдельный тест с реальным `SaveToDataBase` и `CaptureStdout` нужен для покрытия — мок отключал бы реальный вывод.

Для покрытия удаляющих деструкторов `_ZN7AccountD0Ev` и `_ZN7TransactionD0Ev`, генерируемых при `delete` через указатель на базовый класс, добавлены тесты `PolymorphicDelete`.

### Покрытие

Флаги `--coverage` навешиваются только на целевые библиотеки `banking` и `print`, без gtest и тестов:

```cmake
if(COVERAGE AND CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
  target_compile_options(banking PRIVATE --coverage -O0 -g)
  target_link_libraries(banking PUBLIC --coverage)
  target_compile_options(print PRIVATE --coverage -O0 -g)
  target_link_libraries(print PUBLIC --coverage)
endif()
```

Локально через gcov: `Account.cpp` — 100% строк (15/15) и веток (8/8), `Transaction.cpp` — 100% строк (35/35) и веток (34/34).

### CI

`.github/workflows/ci.yml` — две job-ы:

1. `build` — матрица `gcc`/`clang`, configure → build → `ctest --output-on-failure`.
2. `coverage` — отдельная gcc-сборка с `-DCOVERAGE=ON`, после прогона тестов lcov собирает `coverage.info`, отбрасываются `third-party`, `tests`, `/usr` и результат уходит в Coveralls через `coverallsapp/github-action@v2`.

| Job           | Платформа | Компилятор | Результат |
|---------------|-----------|------------|-----------|
| `linux-gcc`   | Linux     | gcc        | success   |
| `linux-clang` | Linux     | clang      | success   |
| `coverage`    | Linux     | gcc        | success   |

Coveralls на master — 98.11% (52/53). Единственная незакрытая строка — служебная, в коде `banking` и `print` покрытие 100%.

## Ссылки

- Репозиторий: <https://github.com/maxopetya/lab05>
- GitHub Actions: <https://github.com/maxopetya/lab05/actions>
- Coveralls: <https://coveralls.io/github/maxopetya/lab05>
- Каталог в учебном репозитории: <https://github.com/maxopetya/TIPM-LABS/tree/master/Lab05>

## Вывод

К проекту lab04 подключён GoogleTest как git submodule, написано 17 модульных тестов на библиотеки `banking` и `print`. Логика `Transaction::Make` проверена через `MockAccount` и `MockTransaction`, `Account` — напрямую. Покрытие `banking` — 100% по строкам и веткам, общее по проекту — 98.11% (Coveralls). CI на GitHub Actions гоняет тесты на gcc и clang, отдельная job замеряет покрытие и публикует на Coveralls.io.
