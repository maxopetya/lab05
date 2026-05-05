#include "Account.h"
#include "Transaction.h"

#include <gtest/gtest.h>
#include <gmock/gmock.h>

#include <stdexcept>
#include <string>

using ::testing::_;
using ::testing::AnyNumber;
using ::testing::NiceMock;
using ::testing::Return;

// --- Account: тестируем реальную реализацию ---

TEST(AccountTest, InitialState) {
  Account acc(7, 500);
  EXPECT_EQ(acc.id(), 7);
  EXPECT_EQ(acc.GetBalance(), 500);
}

TEST(AccountTest, ChangeWithoutLockThrows) {
  Account acc(1, 100);
  EXPECT_THROW(acc.ChangeBalance(10), std::runtime_error);
  EXPECT_EQ(acc.GetBalance(), 100);
}

TEST(AccountTest, LockAndChange) {
  Account acc(1, 100);
  acc.Lock();
  acc.ChangeBalance(50);
  EXPECT_EQ(acc.GetBalance(), 150);
  acc.ChangeBalance(-30);
  EXPECT_EQ(acc.GetBalance(), 120);
}

TEST(AccountTest, DoubleLockThrows) {
  Account acc(1, 100);
  acc.Lock();
  EXPECT_THROW(acc.Lock(), std::runtime_error);
}

TEST(AccountTest, UnlockRevertsToLockedState) {
  Account acc(1, 100);
  acc.Lock();
  acc.Unlock();
  EXPECT_THROW(acc.ChangeBalance(1), std::runtime_error);
  acc.Lock();
  acc.ChangeBalance(1);
  EXPECT_EQ(acc.GetBalance(), 101);
}

TEST(AccountTest, PolymorphicDelete) {
  Account* a = new Account(99, 0);
  delete a;
}

// --- Mock-объекты ---

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
  MOCK_METHOD3(SaveToDataBase, void(Account& from, Account& to, int sum));
};

// --- Transaction: тестируем через моки ---

TEST(TransactionTest, FeeAccessor) {
  Transaction tr;
  EXPECT_EQ(tr.fee(), 1);
  tr.set_fee(5);
  EXPECT_EQ(tr.fee(), 5);
}

TEST(TransactionTest, PolymorphicDelete) {
  Transaction* t = new Transaction;
  delete t;
}

TEST(TransactionTest, SameAccountThrows) {
  NiceMock<MockAccount> a(1, 1000);
  NiceMock<MockAccount> b(1, 1000);
  NiceMock<MockTransaction> tr;
  EXPECT_THROW(tr.Make(a, b, 200), std::logic_error);
}

TEST(TransactionTest, NegativeSumThrows) {
  NiceMock<MockAccount> a(1, 1000);
  NiceMock<MockAccount> b(2, 1000);
  NiceMock<MockTransaction> tr;
  EXPECT_THROW(tr.Make(a, b, -1), std::invalid_argument);
}

TEST(TransactionTest, TooSmallSumThrows) {
  NiceMock<MockAccount> a(1, 1000);
  NiceMock<MockAccount> b(2, 1000);
  NiceMock<MockTransaction> tr;
  EXPECT_THROW(tr.Make(a, b, 50), std::logic_error);
}

TEST(TransactionTest, BigFeeReturnsFalse) {
  NiceMock<MockAccount> a(1, 1000);
  NiceMock<MockAccount> b(2, 1000);
  NiceMock<MockTransaction> tr;
  tr.set_fee(60);
  EXPECT_CALL(a, Lock()).Times(0);
  EXPECT_CALL(b, Lock()).Times(0);
  EXPECT_CALL(tr, SaveToDataBase(_, _, _)).Times(0);
  EXPECT_FALSE(tr.Make(a, b, 100));
}

TEST(TransactionTest, SuccessfulTransfer) {
  MockAccount from(1, 1000);
  MockAccount to(2, 1000);
  MockTransaction tr;

  EXPECT_CALL(from, Lock()).Times(1);
  EXPECT_CALL(to, Lock()).Times(1);
  EXPECT_CALL(from, Unlock()).Times(1);
  EXPECT_CALL(to, Unlock()).Times(1);
  EXPECT_CALL(to, ChangeBalance(200)).Times(1);
  EXPECT_CALL(to, GetBalance()).WillOnce(Return(2000));
  EXPECT_CALL(to, ChangeBalance(-201)).Times(1);
  EXPECT_CALL(tr, SaveToDataBase(_, _, 200)).Times(1);

  EXPECT_TRUE(tr.Make(from, to, 200));
}

TEST(TransactionTest, RollbackOnFailedDebit) {
  MockAccount from(1, 1000);
  MockAccount to(2, 50);
  MockTransaction tr;

  EXPECT_CALL(from, Lock()).Times(1);
  EXPECT_CALL(to, Lock()).Times(1);
  EXPECT_CALL(from, Unlock()).Times(1);
  EXPECT_CALL(to, Unlock()).Times(1);
  EXPECT_CALL(to, ChangeBalance(200)).Times(1);
  EXPECT_CALL(to, GetBalance()).WillOnce(Return(100));
  EXPECT_CALL(to, ChangeBalance(-200)).Times(1);
  EXPECT_CALL(tr, SaveToDataBase(_, _, 200)).Times(1);

  EXPECT_FALSE(tr.Make(from, to, 200));
}

// Реальный SaveToDataBase нужен для покрытия — поэтому здесь Transaction не мокается.
TEST(TransactionTest, RealSaveToDataBaseCoverage) {
  NiceMock<MockAccount> from(1, 1000);
  NiceMock<MockAccount> to(2, 1000);
  Transaction tr;

  EXPECT_CALL(from, GetBalance()).WillRepeatedly(Return(800));
  EXPECT_CALL(to, GetBalance()).WillRepeatedly(Return(2000));
  EXPECT_CALL(to, ChangeBalance(_)).Times(AnyNumber());

  testing::internal::CaptureStdout();
  EXPECT_TRUE(tr.Make(from, to, 200));
  std::string captured = testing::internal::GetCapturedStdout();
  EXPECT_NE(captured.find("send to"), std::string::npos);
  EXPECT_NE(captured.find("Balance"), std::string::npos);
}
