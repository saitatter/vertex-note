#include <gtest/gtest.h>

#include "util/raii/CLibrariesSPtr.h"

namespace {
class TestData {
public:
    TestData() { nbData++; };
    ~TestData() { nbData--; };

    int value = 2;

    int ref_count = 1;

    static int nbData;
};
int TestData::nbData = 0;

class TestDataHandler {
public:
    static TestData* ref(TestData* p) {
        p->ref_count++;
        return p;
    }
    static void unref(TestData* p) {
        p->ref_count--;
        if (p->ref_count == 0) {
            delete (p);
        }
    }
    constexpr static auto adopt = [](TestData* p) { return p; };
};

};  // namespace

using TestRAII = vn::util::CLibrariesSPtr<TestData, TestDataHandler>;

TEST(UtilsRAII, testCLibbrairiesSPtr_Constructors) {
    {
        TestRAII t;  // Default constructor
        EXPECT_EQ(t.get(), nullptr);
        EXPECT_FALSE(t);
        EXPECT_EQ(TestData::nbData, 0);

        TestData* p = nullptr;
        TestRAII t1(p = new TestData(), vn::util::adopt);  // Adoption
        EXPECT_EQ(t1.get(), p);
        EXPECT_EQ(TestData::nbData, 1);
        EXPECT_EQ(p->ref_count, 1);

        TestRAII t2(std::move(t1));  // move constructor
        EXPECT_EQ(t1.get(), nullptr);
        EXPECT_EQ(t2.get(), p);
        EXPECT_EQ(p->ref_count, 1);
        EXPECT_EQ(TestData::nbData, 1);

        TestRAII t3(t2);  // copy constructor
        EXPECT_EQ(t2.get(), p);
        EXPECT_EQ(t3.get(), p);
        EXPECT_EQ(p->ref_count, 2);
        EXPECT_EQ(TestData::nbData, 1);

        TestRAII t4(t2.get(), vn::util::ref);  // ref
        EXPECT_EQ(t4.get(), p);
        EXPECT_EQ(p->ref_count, 3);
        EXPECT_EQ(TestData::nbData, 1);
    }
    EXPECT_EQ(TestData::nbData, 0);
}

TEST(UtilsRAII, testCLibbrairiesSPtr_MoveAssignments) {
    {
        TestData* p = nullptr;
        TestData* q = nullptr;
        TestRAII t1(p = new TestData(), vn::util::adopt);
        TestRAII t2(q = new TestData(), vn::util::adopt);
        EXPECT_EQ(TestData::nbData, 2);
        EXPECT_EQ(t1.get(), p);
        EXPECT_EQ(t2.get(), q);
        EXPECT_EQ(p->ref_count, 1);
        EXPECT_EQ(q->ref_count, 1);
        t1->value = 4;
        t2->value = 14;

        std::swap(t1, t2);
        EXPECT_EQ(t1->value, 14);
        EXPECT_EQ(t2->value, 4);
        EXPECT_EQ(TestData::nbData, 2);
        EXPECT_EQ(t1.get(), q);
        EXPECT_EQ(t2.get(), p);
        EXPECT_EQ(p->ref_count, 1);
        EXPECT_EQ(q->ref_count, 1);

        t1 = std::move(t2);  // move assignment - non empty to non empty - different data
        q = nullptr;
        EXPECT_EQ(TestData::nbData, 1);
        EXPECT_EQ(p->ref_count, 1);
        EXPECT_EQ(t1.get(), p);
        EXPECT_EQ(t1->value, 4);
        EXPECT_EQ(t2.get(), nullptr);
        EXPECT_TRUE(t1);
        EXPECT_FALSE(t2);

        TestRAII t3(p, vn::util::ref);
        EXPECT_EQ(p->ref_count, 2);
        t1 = std::move(t3);  // move assignment - non empty to non empty - same data
        EXPECT_EQ(TestData::nbData, 1);
        EXPECT_EQ(p->ref_count, 1);
        EXPECT_EQ(t1.get(), p);
        EXPECT_EQ(t1->value, 4);
        EXPECT_EQ(t2.get(), nullptr);
        EXPECT_TRUE(t1);
        EXPECT_FALSE(t2);

        t2 = std::move(t1);  // move assignment - non empty to empty
        EXPECT_EQ(TestData::nbData, 1);
        EXPECT_EQ(p->ref_count, 1);
        EXPECT_EQ(t1.get(), nullptr);
        EXPECT_EQ(t2->value, 4);
        EXPECT_EQ(t2.get(), p);

        t2 = std::move(t1);  // move assignment - empty to non-empty
        p = nullptr;
        EXPECT_EQ(TestData::nbData, 0);
        EXPECT_EQ(t1.get(), nullptr);
        EXPECT_EQ(t2.get(), nullptr);

        t2 = std::move(t1);  // move assignment - empty to empty
        EXPECT_EQ(TestData::nbData, 0);
        EXPECT_EQ(t1.get(), nullptr);
        EXPECT_EQ(t2.get(), nullptr);
    }
    EXPECT_EQ(TestData::nbData, 0);
}

TEST(UtilsRAII, testCLibbrairiesSPtr_CopyAssignments) {
    {
        TestData* p = nullptr;
        TestData* q = nullptr;
        TestRAII t1(p = new TestData(), vn::util::adopt);
        TestRAII t2(q = new TestData(), vn::util::adopt);
        EXPECT_EQ(TestData::nbData, 2);
        EXPECT_EQ(t1.get(), p);
        EXPECT_EQ(t2.get(), q);
        EXPECT_EQ(p->ref_count, 1);
        EXPECT_EQ(q->ref_count, 1);
        t1->value = 4;
        t2->value = 14;

        t2 = t1;  // copy assignment - non empty to non empty - different data
        q = nullptr;
        EXPECT_EQ(TestData::nbData, 1);
        EXPECT_EQ(p->ref_count, 2);
        EXPECT_EQ(t1.get(), p);
        EXPECT_EQ(t2.get(), p);
        EXPECT_EQ(t2->value, 4);

        t2 = t1;  // copy assignment - non empty to non empty - same data
        EXPECT_EQ(TestData::nbData, 1);
        EXPECT_EQ(p->ref_count, 2);
        EXPECT_EQ(t1.get(), p);
        EXPECT_EQ(t2.get(), p);
        EXPECT_EQ(t2->value, 4);

        TestRAII t3;
        t2 = t3;  // copy assignment - empty to non-empty
        EXPECT_EQ(TestData::nbData, 1);
        EXPECT_EQ(p->ref_count, 1);  // t1
        EXPECT_EQ(t2.get(), nullptr);
        EXPECT_EQ(t3.get(), nullptr);

        t3 = t1;  // copy assignment - non empty to empty
        EXPECT_EQ(TestData::nbData, 1);
        EXPECT_EQ(p->ref_count, 2);  // t{1,3}
        EXPECT_EQ(t1.get(), p);
        EXPECT_EQ(t3->value, 4);
        EXPECT_EQ(t3.get(), p);

        TestRAII t4;
        t2 = t4;  // copy assignment - empty to empty
        EXPECT_EQ(TestData::nbData, 1);
        EXPECT_EQ(t2.get(), nullptr);
        EXPECT_EQ(t4.get(), nullptr);
    }
    EXPECT_EQ(TestData::nbData, 0);
}

TEST(UtilsRAII, testCLibbrairiesSPtr_Reset) {
    {
        TestData* p = nullptr;
        TestData* q = nullptr;
        TestRAII t1(p = new TestData(), vn::util::adopt);
        TestRAII t2;
        EXPECT_EQ(TestData::nbData, 1);
        EXPECT_EQ(t1.get(), p);
        EXPECT_EQ(t2.get(), nullptr);
        EXPECT_EQ(p->ref_count, 1);
        t1->value = 4;

        t2.reset(q = new TestData(), vn::util::adopt);  // Reset - non-nullptr to empty
        EXPECT_EQ(TestData::nbData, 2);
        EXPECT_EQ(t2.get(), q);
        EXPECT_TRUE(t2);
        EXPECT_EQ(q->ref_count, 1);

        t2.reset(q = new TestData(), vn::util::adopt);  // Reset - non-nullptr to non-empty
        EXPECT_EQ(t2.get(), q);
        EXPECT_EQ(TestData::nbData, 2);

        t1.reset();  // Reset - nullptr to non-empty
        p = nullptr;
        EXPECT_EQ(TestData::nbData, 1);
        EXPECT_EQ(t1.get(), nullptr);

        t1.reset();  // Reset - nullptr to empty
        EXPECT_EQ(TestData::nbData, 1);
        EXPECT_EQ(t1.get(), nullptr);
    }
    EXPECT_EQ(TestData::nbData, 0);
}

