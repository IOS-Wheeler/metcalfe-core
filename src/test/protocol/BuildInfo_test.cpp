//------------------------------------------------------------------------------
/*
    This file is part of mtchaind: https://github.com/MTChain/MTChain-core
    Copyright (c) 2017, 2018 MTChain Alliance.

    Permission to use, copy, modify, and/or distribute this software for any

*/
//==============================================================================

#include <BeastConfig.h>
#include <mtchain/protocol/BuildInfo.h>
#include <mtchain/beast/core/SemanticVersion.h>
#include <mtchain/beast/unit_test.h>

namespace mtchain {

class BuildInfo_test : public beast::unit_test::suite
{
public:
    ProtocolVersion
    from_version (std::uint16_t major, std::uint16_t minor)
    {
        return ProtocolVersion (major, minor);
    }

    void testValues ()
    {
        testcase ("comparison");

        BEAST_EXPECT(from_version (1,2) == from_version (1,2));
        BEAST_EXPECT(from_version (3,4) >= from_version (3,4));
        BEAST_EXPECT(from_version (5,6) <= from_version (5,6));
        BEAST_EXPECT(from_version (7,8) >  from_version (6,7));
        BEAST_EXPECT(from_version (7,8) <  from_version (8,9));
        BEAST_EXPECT(from_version (65535,0) <  from_version (65535,65535));
        BEAST_EXPECT(from_version (65535,65535) >= from_version (65535,65535));
    }

    void testStringVersion ()
    {
        testcase ("string version");

        for (std::uint16_t major = 0; major < 8; major++)
        {
            for (std::uint16_t minor = 0; minor < 8; minor++)
            {
                BEAST_EXPECT(to_string (from_version (major, minor)) ==
                    std::to_string (major) + "." + std::to_string (minor));
            }
        }
    }

    void testVersionPacking ()
    {
        testcase ("version packing");

        BEAST_EXPECT(to_packed (from_version (0, 0)) == 0);
        BEAST_EXPECT(to_packed (from_version (0, 1)) == 1);
        BEAST_EXPECT(to_packed (from_version (0, 255)) == 255);
        BEAST_EXPECT(to_packed (from_version (0, 65535)) == 65535);

        BEAST_EXPECT(to_packed (from_version (1, 0)) == 65536);
        BEAST_EXPECT(to_packed (from_version (1, 1)) == 65537);
        BEAST_EXPECT(to_packed (from_version (1, 255)) == 65791);
        BEAST_EXPECT(to_packed (from_version (1, 65535)) == 131071);

        BEAST_EXPECT(to_packed (from_version (255, 0)) == 16711680);
        BEAST_EXPECT(to_packed (from_version (255, 1)) == 16711681);
        BEAST_EXPECT(to_packed (from_version (255, 255)) == 16711935);
        BEAST_EXPECT(to_packed (from_version (255, 65535)) == 16777215);

        BEAST_EXPECT(to_packed (from_version (65535, 0)) == 4294901760);
        BEAST_EXPECT(to_packed (from_version (65535, 1)) == 4294901761);
        BEAST_EXPECT(to_packed (from_version (65535, 255)) == 4294902015);
        BEAST_EXPECT(to_packed (from_version (65535, 65535)) == 4294967295);
    }

    void run ()
    {
        testValues ();
        testStringVersion ();
        testVersionPacking ();

        auto const current_protocol = BuildInfo::getCurrentProtocol ();
        auto const minimum_protocol = BuildInfo::getMinimumProtocol ();

        BEAST_EXPECT(current_protocol >= minimum_protocol);

        log <<
            "   MTChain Version: " << BuildInfo::getVersionString() << '\n' <<
            " Protocol Version: " << to_string (current_protocol) << std::endl;
    }
};

BEAST_DEFINE_TESTSUITE(BuildInfo,mtchain_data,mtchain);

} //
