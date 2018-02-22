#include "gtest/gtest.h"
#include "IrkSocket.h"

int main(int argc, char** argv)
{
    irk::network_setup();
    testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
