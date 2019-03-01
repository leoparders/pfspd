#include <gtest/gtest.h>
#include <numeric>
#include <vector>
#include "test_functions.h"


#define OK  0
#define FAILED 1
#define MBYTE   1024*1024
TestFunction test_func;
TEST(PFSPD, standardFileWrite)
{
    //int status = write_file("test.pfspd",(long long)(4000)*MBYTE, (long long)(1024*2)*MBYTE);
    test_func.FileWrite();
    EXPECT_EQ(test_func.IsTeskOk(), true);
}

TEST(PFSPD, standardFileRead)
{
    //int status = write_file("test.pfspd",(long long)(4000)*MBYTE, (long long)(1024*2)*MBYTE);
    test_func.FileRead();
    EXPECT_EQ(test_func.IsTeskOk(), true);
}
int main(int argc, char *argv[])
{
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}