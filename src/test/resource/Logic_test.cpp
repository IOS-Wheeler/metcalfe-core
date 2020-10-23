//------------------------------------------------------------------------------
/*
    This file is part of mtchaind: https://github.com/MTChain/MTChain-core
    Copyright (c) 2017, 2018 MTChain Alliance.

    Permission to use, copy, modify, and/or distribute this software for any

*/
//==============================================================================

#include <BeastConfig.h>
#include <mtchain/basics/chrono.h>
#include <mtchain/basics/random.h>
#include <mtchain/beast/unit_test.h>
#include <boost/utility/base_from_member.hpp>
#include <mtchain/resource/Consumer.h>
#include <mtchain/resource/impl/Entry.h>
#include <mtchain/resource/impl/Logic.h>



namespace mtchain {
namespace Resource {

class Manager_test : public beast::unit_test::suite
{
public:
    class TestLogic
        : private boost::base_from_member<TestStopwatch>
        , public Logic

    {
    private:
        using clock_type =
            boost::base_from_member<TestStopwatch>;

    public:
        explicit TestLogic (beast::Journal journal)
            : Logic (beast::insight::NullCollector::New(), member, journal)
        {
        }

        void advance ()
        {
            ++member;
        }

        TestStopwatch& clock ()
        {
            return member;
        }
    };

    //--------------------------------------------------------------------------

    void createGossip (Gossip& gossip)
    {
        int const v (10 + rand_int(9));
        int const n (10 + rand_int(9));
        gossip.items.reserve (n);
        for (int i = 0; i < n; ++i)
        {
            Gossip::Item item;
            item.balance = 100 + rand_int(499);
            item.address = beast::IP::Endpoint (
                beast::IP::AddressV4 (192, 0, 2, v + i));
            gossip.items.push_back (item);
        }
    }

    //--------------------------------------------------------------------------

    void testDrop (beast::Journal j)
    {
        testcase ("Warn/drop");

        TestLogic logic (j);

        Charge const fee (dropThreshold + 1);
        beast::IP::Endpoint const addr (
            beast::IP::Endpoint::from_string ("192.0.2.2"));

        {
            Consumer c (logic.newInboundEndpoint (addr));

            // Create load until we get a warning
            int n = 10000;

            while (--n >= 0)
            {
                if (n == 0)
                {
                    fail ("Loop count exceeded without warning");
                    return;
                }

                if (c.charge (fee) == warn)
                {
                    pass ();
                    break;
                }
                ++logic.clock ();
            }

            // Create load until we get dropped
            while (--n >= 0)
            {
                if (n == 0)
                {
                    fail ("Loop count exceeded without dropping");
                    return;
                }

                if (c.charge (fee) == drop)
                {
                    // Disconnect abusive Consumer
                    BEAST_EXPECT(c.disconnect ());
                    break;
                }
                ++logic.clock ();
            }
        }

        // Make sure the consumer is on the blacklist for a while.
        {
            Consumer c (logic.newInboundEndpoint (addr));
            logic.periodicActivity();
            if (c.disposition () != drop)
            {
                fail ("Dropped consumer not put on blacklist");
                return;
            }
        }

        // Makes sure the Consumer is eventually removed from blacklist
        bool readmitted = false;
        {
            // Give Consumer time to become readmitted.  Should never
            // exceed expiration time.
            std::size_t n (secondsUntilExpiration + 1);
            while (--n > 0)
            {
                ++logic.clock ();
                logic.periodicActivity();
                Consumer c (logic.newInboundEndpoint (addr));
                if (c.disposition () != drop)
                {
                    readmitted = true;
                    break;
                }
            }
        }
        if (readmitted == false)
        {
            fail ("Dropped Consumer left on blacklist too long");
            return;
        }
        pass();
    }

    void testImports (beast::Journal j)
    {
        testcase ("Imports");

        TestLogic logic (j);

        Gossip g[5];

        for (int i = 0; i < 5; ++i)
            createGossip (g[i]);

        for (int i = 0; i < 5; ++i)
            logic.importConsumers (std::to_string (i), g[i]);

        pass();
    }

    void testImport (beast::Journal j)
    {
        testcase ("Import");

        TestLogic logic (j);

        Gossip g;
        Gossip::Item item;
        item.balance = 100;
        item.address = beast::IP::Endpoint (
            beast::IP::AddressV4 (192, 0, 2, 1));
        g.items.push_back (item);

        logic.importConsumers ("g", g);

        pass();
    }

    void testCharges (beast::Journal j)
    {
        testcase ("Charge");

        TestLogic logic (j);

        {
            beast::IP::Endpoint address (beast::IP::Endpoint::from_string ("192.0.2.1"));
            Consumer c (logic.newInboundEndpoint (address));
            Charge fee (1000);
            JLOG(j.info()) <<
                "Charging " << c.to_string() << " " << fee << " per second";
            c.charge (fee);
            for (int i = 0; i < 128; ++i)
            {
                JLOG(j.info()) <<
                    "Time= " << logic.clock().now().time_since_epoch().count() <<
                    ", Balance = " << c.balance();
                logic.advance();
            }
        }

        {
            beast::IP::Endpoint address (beast::IP::Endpoint::from_string ("192.0.2.2"));
            Consumer c (logic.newInboundEndpoint (address));
            Charge fee (1000);
            JLOG(j.info()) <<
                "Charging " << c.to_string() << " " << fee << " per second";
            for (int i = 0; i < 128; ++i)
            {
                c.charge (fee);
                JLOG(j.info()) <<
                    "Time= " << logic.clock().now().time_since_epoch().count() <<
                    ", Balance = " << c.balance();
                logic.advance();
            }
        }

        pass();
    }

    void run()
    {
        beast::Journal j;

        testDrop (j);
        testCharges (j);
        testImports (j);
        testImport (j);
    }
};

BEAST_DEFINE_TESTSUITE(Manager,resource,mtchain);

}
}