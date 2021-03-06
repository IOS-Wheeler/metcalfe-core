//------------------------------------------------------------------------------
/*
    This file is part of mtchaind: https://github.com/MTChain/MTChain-core
    Copyright (c) 2012-2017 MTChain Alliance.

    Permission to use, copy, modify, and/or distribute this software for any

*/
//==============================================================================

#include <BeastConfig.h>
#include <test/jtx.h>
#include <test/jtx/Env.h>
#include <mtchain/beast/unit_test.h>
#include <mtchain/protocol/JsonFields.h>
#include <mtchain/protocol/SField.h>
#include <boost/scope_exit.hpp>
#include <boost/filesystem.hpp>
#include <boost/algorithm/string.hpp>
#include <fstream>

namespace mtchain {

class LedgerLoad_test : public beast::unit_test::suite
{
    Json::Value jvLedger_;
    Json::Value jvHashes_;
    boost::filesystem::path ledgerFile_;
    boost::filesystem::path dbPath_;

    auto ledgerConfig(std::string const& ledger, Config::StartUpType type)
    {
        assert(! dbPath_.empty());
        auto p = std::make_unique<Config>();
        test::setupConfigForUnitTests(*p);
        p->START_LEDGER = ledger;
        p->START_UP = type;
        p->legacy("database_path", dbPath_.string());
        return p;
    }

    // setup for test cases
    void
    setupLedger()
    {
        using namespace test::jtx;

        boost::system::error_code ec;
        // create a temporary path to write ledger files in
        dbPath_  = boost::filesystem::temp_directory_path(ec);
        if(! BEAST_EXPECTS(!ec, ec.message()))
            return;
        dbPath_ /= boost::filesystem::unique_path("%%%%-%%%%-%%%%-%%%%", ec);
        if(! BEAST_EXPECTS(!ec, ec.message()))
            return;
        boost::filesystem::create_directories(dbPath_, ec);
        if(! BEAST_EXPECTS(!ec, ec.message()))
            return;

        ledgerFile_ = dbPath_ / "ledgerdata.json";

        Env env {*this};
        Account prev;

        for(auto i = 0; i < 20; ++i)
        {
            Account acct {"A" + std::to_string(i)};
            env.fund(M(10000), acct);
            env.close();
            if(i > 0)
            {
                env.trust(acct["USD"](1000), prev);
                env(pay(acct, prev, acct["USD"](5)));
            }
            env(offer(acct, M(100), acct["USD"](1)));
            env.close();
            prev = std::move(acct);
        }

        jvLedger_ = env.rpc ("ledger", "current", "full") [jss::result];
        BEAST_EXPECT(jvLedger_[jss::ledger][jss::accountState].size() == 101);

        for(auto const& it : jvLedger_[jss::ledger][jss::accountState])
        {
            if(it[sfLedgerEntryType.fieldName] == "LedgerHashes")
            {
                jvHashes_ = it[sfHashes.fieldName];
            }
        }
        BEAST_EXPECT(jvHashes_.size() == 41);

        //write this ledger data to a file.
        std::ofstream o (ledgerFile_.string(), std::ios::out | std::ios::trunc);
        o << to_string(jvLedger_);
        o.close();
    }

    void
    testLoad ()
    {
        testcase ("Load a saved ledger");
        using namespace test::jtx;

        // create a new env with the ledger file specified for startup
        Env env(*this, ledgerConfig(ledgerFile_.string(), Config::LOAD_FILE));
        auto jrb = env.rpc ( "ledger", "current", "full") [jss::result];
        BEAST_EXPECT(
            jvLedger_[jss::ledger][jss::accountState].size() ==
            jrb[jss::ledger][jss::accountState].size());
    }

    void
    testBadFiles ()
    {
        testcase ("Load ledger: Bad Files");
        using namespace test::jtx;
        using namespace boost::filesystem;

        // empty path
        except ([this]
        {
            Env env(*this, ledgerConfig("", Config::LOAD_FILE));
        });

        // file does not exist
        except ([this]
        {
            Env env(*this, ledgerConfig("badfile.json", Config::LOAD_FILE));
        });

        // make a corrupted version of the ledger file (last 10 bytes removed).
        boost::system::error_code ec;
        auto ledgerFileCorrupt = dbPath_ / "ledgerdata_bad.json";
        copy_file(
            ledgerFile_,
            ledgerFileCorrupt,
            copy_option::overwrite_if_exists,
            ec);
        if(! BEAST_EXPECTS(!ec, ec.message()))
            return;
        auto filesize = file_size(ledgerFileCorrupt, ec);
        if(! BEAST_EXPECTS(!ec, ec.message()))
            return;
        resize_file(ledgerFileCorrupt, filesize - 10, ec);
        if(! BEAST_EXPECTS(!ec, ec.message()))
            return;

        except ([this, &ledgerFileCorrupt]
        {
            Env env(*this, ledgerConfig(ledgerFileCorrupt.string(), Config::LOAD_FILE));
        });
    }

    void
    testLoadByHash ()
    {
        testcase ("Load by hash");
        using namespace test::jtx;

        // create a new env with the ledger hash specified for startup
        auto ledgerHash = to_string(jvHashes_[jvHashes_.size()-1]);
        boost::erase_all(ledgerHash, "\"");
        Env env(*this, ledgerConfig(ledgerHash, Config::LOAD));
        auto jrb = env.rpc ( "ledger", "current", "full") [jss::result];
        BEAST_EXPECT(jrb[jss::ledger][jss::accountState].size() == 97);
        BEAST_EXPECT(
            jrb[jss::ledger][jss::accountState].size() <=
            jvLedger_[jss::ledger][jss::accountState].size());
    }

    void
    testLoadLatest ()
    {
        testcase ("Load by keyword");
        using namespace test::jtx;

        // create a new env with the ledger "latest" specified for startup
        Env env(*this, ledgerConfig("latest", Config::LOAD));
        auto jrb = env.rpc ( "ledger", "current", "full") [jss::result];
        BEAST_EXPECT(
            jvLedger_[jss::ledger][jss::accountState].size() ==
            jrb[jss::ledger][jss::accountState].size());
    }

    void
    testLoadIndex ()
    {
        testcase ("Load by index");
        using namespace test::jtx;

        // create a new env with specific ledger index at startup
        Env env(*this, ledgerConfig("43", Config::LOAD));
        auto jrb = env.rpc ( "ledger", "current", "full") [jss::result];
        BEAST_EXPECT(
            jvLedger_[jss::ledger][jss::accountState].size() ==
            jrb[jss::ledger][jss::accountState].size());
    }

public:
    void run ()
    {
        setupLedger();
        BOOST_SCOPE_EXIT( this_ ) {
            boost::system::error_code ec;
            boost::filesystem::remove_all(this_->dbPath_, ec);
            this_->expect(!ec, ec.message(), __FILE__, __LINE__);
        } BOOST_SCOPE_EXIT_END

        // test cases
        testLoad ();
        testBadFiles ();
        testLoadByHash ();
        testLoadLatest ();
        testLoadIndex ();
    }
};

BEAST_DEFINE_TESTSUITE (LedgerLoad, app, mtchain);

}  //
