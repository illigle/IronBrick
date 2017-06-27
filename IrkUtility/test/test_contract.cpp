#include <string>
#include "gtest/gtest.h"
#include "IrkContract.h"

using namespace irk;

static void my_contract_handler( const CViolationInfo& info )
{
    std::string msg = info.file;
    msg += "@";
    msg += std::to_string( info.line );
    msg += ": ";
    msg += info.expression;
    throw msg;
}

TEST( Contract, Contract )
{
    volatile int x = 0;
    irk_expect( x==0 );
    EXPECT_THROW( irk_ensure( x > 0 ), ContractViolation );
    x++;
    irk_ensure( x > 0 );

    auto oldhandler = set_violation_handler( my_contract_handler );
    EXPECT_THROW( irk_expect( x < 0 ), std::string );
    x = -10;
    irk_expect( x < 0 );

    EXPECT_EQ( &my_contract_handler, get_violation_handler() );
    set_violation_handler( oldhandler );
    x = -1;
    EXPECT_THROW( irk_expect( x > 0 ), ContractViolation );
}
