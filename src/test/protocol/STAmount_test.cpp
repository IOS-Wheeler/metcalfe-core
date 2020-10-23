//------------------------------------------------------------------------------
/*
    This file is part of mtchaind: https://github.com/MTChain/MTChain-core
    Copyright (c) 2017, 2018 MTChain Alliance.

    Permission to use, copy, modify, and/or distribute this software for any

*/
//==============================================================================

#include <BeastConfig.h>
#include <mtchain/basics/Log.h>
#include <mtchain/basics/random.h>
#include <mtchain/protocol/STAmount.h>
#include <mtchain/beast/unit_test.h>

namespace mtchain {

class STAmount_test : public beast::unit_test::suite
{
public:
    static STAmount serializeAndDeserialize (STAmount const& s)
    {
        Serializer ser;
        s.add (ser);

        SerialIter sit (ser.slice());
        return STAmount(sit, sfGeneric);
    }

    //--------------------------------------------------------------------------
    STAmount roundSelf (STAmount const& amount)
    {
        if (amount.native ())
            return amount;

        std::uint64_t mantissa = amount.mantissa ();
        std::uint64_t valueDigits = mantissa % 1000000000;

        if (valueDigits == 1)
        {
            mantissa--;

            if (mantissa < STAmount::cMinValue)
                return { amount.issue (), mantissa, amount.exponent (),
                    amount.negative () };

            return { amount.issue (), mantissa, amount.exponent (),
                amount.native(), amount.negative (), STAmount::unchecked {} };
        }

        if (valueDigits == 999999999)
        {
            mantissa++;

            if (mantissa > STAmount::cMaxValue)
                return { amount.issue (), mantissa, amount.exponent (),
                    amount.negative () };

            return { amount.issue (), mantissa, amount.exponent (),
                amount.native(), amount.negative (), STAmount::unchecked {} };
        }

        return amount;
    }

    void roundTest (int n, int d, int m)
    {
        // check STAmount rounding
        STAmount num (noIssue(), n);
        STAmount den (noIssue(), d);
        STAmount mul (noIssue(), m);
        STAmount quot = divide (n, d, noIssue());
        STAmount res = roundSelf (multiply (quot, mul, noIssue()));

        BEAST_EXPECT(! res.native ());

        STAmount cmp (noIssue(), (n * m) / d);

        BEAST_EXPECT(! cmp.native ());

        BEAST_EXPECT(cmp.issue().currency == res.issue().currency);

        if (res != cmp)
        {
            log <<
                "(" << num.getText () << "/" << den.getText () <<
                ") X " << mul.getText () << " = " << res.getText () <<
                " not " << cmp.getText ();
            fail ("Rounding");
            return;
        }
    }

    void mulTest (int a, int b)
    {
        STAmount aa (noIssue(), a);
        STAmount bb (noIssue(), b);
        STAmount prod1 (multiply (aa, bb, noIssue()));

        BEAST_EXPECT(! prod1.native ());

        STAmount prod2 (noIssue(), static_cast<std::uint64_t> (a) * static_cast<std::uint64_t> (b));

        if (prod1 != prod2)
        {
            log <<
                "nn(" << aa.getFullText () << " * " << bb.getFullText () <<
                ") = " << prod1.getFullText () << " not " << prod2.getFullText ();
            fail ("Multiplication result is not exact");
        }
    }

    //--------------------------------------------------------------------------

    void testSetValue (
        std::string const& value, Issue const& issue, bool success = true)
    {
        try
        {
            STAmount const amount = amountFromString (issue, value);
            BEAST_EXPECT(amount.getText () == value);
        }
        catch (std::exception const&)
        {
            BEAST_EXPECT(!success);
        }
    }

    void testSetValue ()
    {
        {
            testcase ("set value (native)");

            Issue const m (mIssue ());

            // fractional M (i.e. drops)
            testSetValue ("1", m);
            testSetValue ("22", m);
            testSetValue ("333", m);
            testSetValue ("4444", m);
            testSetValue ("55555", m);
            testSetValue ("666666", m);

            // 1 M up to 100 billion, in powers of 10 (in drops)
            testSetValue ("1000000", m);
            testSetValue ("10000000", m);
            testSetValue ("100000000", m);
            testSetValue ("1000000000", m);
            testSetValue ("10000000000", m);
            testSetValue ("100000000000", m);
            testSetValue ("1000000000000", m);
            testSetValue ("10000000000000", m);
            testSetValue ("100000000000000", m);
            testSetValue ("1000000000000000", m);
            testSetValue ("10000000000000000", m);
            testSetValue ("100000000000000000", m);

            // Invalid native values:
            testSetValue ("1.1", m, false);
            testSetValue ("100000000000000001", m, false);
            testSetValue ("1000000000000000000", m, false);
        }

        {
            testcase ("set value (iou)");

            Issue const usd (Currency (0x5553440000000000), AccountID (0x4985601));

            testSetValue ("1", usd);
            testSetValue ("10", usd);
            testSetValue ("100", usd);
            testSetValue ("1000", usd);
            testSetValue ("10000", usd);
            testSetValue ("100000", usd);
            testSetValue ("1000000", usd);
            testSetValue ("10000000", usd);
            testSetValue ("100000000", usd);
            testSetValue ("1000000000", usd);
            testSetValue ("10000000000", usd);

            testSetValue ("1234567.1", usd);
            testSetValue ("1234567.12", usd);
            testSetValue ("1234567.123", usd);
            testSetValue ("1234567.1234", usd);
            testSetValue ("1234567.12345", usd);
            testSetValue ("1234567.123456", usd);
            testSetValue ("1234567.1234567", usd);
            testSetValue ("1234567.12345678", usd);
            testSetValue ("1234567.123456789", usd);
        }
    }

    //--------------------------------------------------------------------------

    void testNativeCurrency ()
    {
        testcase ("native currency");
        STAmount zeroSt, one (1), hundred (100);
        // VFALCO NOTE Why repeat "STAmount fail" so many times??
        unexpected (serializeAndDeserialize (zeroSt) != zeroSt, "STAmount fail");
        unexpected (serializeAndDeserialize (one) != one, "STAmount fail");
        unexpected (serializeAndDeserialize (hundred) != hundred, "STAmount fail");
        unexpected (!zeroSt.native (), "STAmount fail");
        unexpected (!hundred.native (), "STAmount fail");
        unexpected (zeroSt != zero, "STAmount fail");
        unexpected (one == zero, "STAmount fail");
        unexpected (hundred == zero, "STAmount fail");
        unexpected ((zeroSt < zeroSt), "STAmount fail");
        unexpected (! (zeroSt < one), "STAmount fail");
        unexpected (! (zeroSt < hundred), "STAmount fail");
        unexpected ((one < zeroSt), "STAmount fail");
        unexpected ((one < one), "STAmount fail");
        unexpected (! (one < hundred), "STAmount fail");
        unexpected ((hundred < zeroSt), "STAmount fail");
        unexpected ((hundred < one), "STAmount fail");
        unexpected ((hundred < hundred), "STAmount fail");
        unexpected ((zeroSt > zeroSt), "STAmount fail");
        unexpected ((zeroSt > one), "STAmount fail");
        unexpected ((zeroSt > hundred), "STAmount fail");
        unexpected (! (one > zeroSt), "STAmount fail");
        unexpected ((one > one), "STAmount fail");
        unexpected ((one > hundred), "STAmount fail");
        unexpected (! (hundred > zeroSt), "STAmount fail");
        unexpected (! (hundred > one), "STAmount fail");
        unexpected ((hundred > hundred), "STAmount fail");
        unexpected (! (zeroSt <= zeroSt), "STAmount fail");
        unexpected (! (zeroSt <= one), "STAmount fail");
        unexpected (! (zeroSt <= hundred), "STAmount fail");
        unexpected ((one <= zeroSt), "STAmount fail");
        unexpected (! (one <= one), "STAmount fail");
        unexpected (! (one <= hundred), "STAmount fail");
        unexpected ((hundred <= zeroSt), "STAmount fail");
        unexpected ((hundred <= one), "STAmount fail");
        unexpected (! (hundred <= hundred), "STAmount fail");
        unexpected (! (zeroSt >= zeroSt), "STAmount fail");
        unexpected ((zeroSt >= one), "STAmount fail");
        unexpected ((zeroSt >= hundred), "STAmount fail");
        unexpected (! (one >= zeroSt), "STAmount fail");
        unexpected (! (one >= one), "STAmount fail");
        unexpected ((one >= hundred), "STAmount fail");
        unexpected (! (hundred >= zeroSt), "STAmount fail");
        unexpected (! (hundred >= one), "STAmount fail");
        unexpected (! (hundred >= hundred), "STAmount fail");
        unexpected (! (zeroSt == zeroSt), "STAmount fail");
        unexpected ((zeroSt == one), "STAmount fail");
        unexpected ((zeroSt == hundred), "STAmount fail");
        unexpected ((one == zeroSt), "STAmount fail");
        unexpected (! (one == one), "STAmount fail");
        unexpected ((one == hundred), "STAmount fail");
        unexpected ((hundred == zeroSt), "STAmount fail");
        unexpected ((hundred == one), "STAmount fail");
        unexpected (! (hundred == hundred), "STAmount fail");
        unexpected ((zeroSt != zeroSt), "STAmount fail");
        unexpected (! (zeroSt != one), "STAmount fail");
        unexpected (! (zeroSt != hundred), "STAmount fail");
        unexpected (! (one != zeroSt), "STAmount fail");
        unexpected ((one != one), "STAmount fail");
        unexpected (! (one != hundred), "STAmount fail");
        unexpected (! (hundred != zeroSt), "STAmount fail");
        unexpected (! (hundred != one), "STAmount fail");
        unexpected ((hundred != hundred), "STAmount fail");
        unexpected (STAmount ().getText () != "0", "STAmount fail");
        unexpected (STAmount (31).getText () != "31", "STAmount fail");
        unexpected (STAmount (310).getText () != "310", "STAmount fail");
        unexpected (to_string (Currency ()) != "M", "cHC(M)");
        Currency c;
        unexpected (!to_currency (c, "USD"), "create USD currency");
        unexpected (to_string (c) != "USD", "check USD currency");

        const std::string cur = "015841551A748AD2C1F76FF6ECB0CCCD00000000";
        unexpected (!to_currency (c, cur), "create custom currency");
        unexpected (to_string (c) != cur, "check custom currency");
        unexpected (c != Currency (
            from_hex_text<Currency>(cur)), "check custom currency");
    }

    //--------------------------------------------------------------------------

    void testCustomCurrency ()
    {
        testcase ("custom currency");
        STAmount zeroSt (noIssue()), one (noIssue(), 1), hundred (noIssue(), 100);
        unexpected (serializeAndDeserialize (zeroSt) != zeroSt, "STAmount fail");
        unexpected (serializeAndDeserialize (one) != one, "STAmount fail");
        unexpected (serializeAndDeserialize (hundred) != hundred, "STAmount fail");
        unexpected (zeroSt.native (), "STAmount fail");
        unexpected (hundred.native (), "STAmount fail");
        unexpected (zeroSt != zero, "STAmount fail");
        unexpected (one == zero, "STAmount fail");
        unexpected (hundred == zero, "STAmount fail");
        unexpected ((zeroSt < zeroSt), "STAmount fail");
        unexpected (! (zeroSt < one), "STAmount fail");
        unexpected (! (zeroSt < hundred), "STAmount fail");
        unexpected ((one < zeroSt), "STAmount fail");
        unexpected ((one < one), "STAmount fail");
        unexpected (! (one < hundred), "STAmount fail");
        unexpected ((hundred < zeroSt), "STAmount fail");
        unexpected ((hundred < one), "STAmount fail");
        unexpected ((hundred < hundred), "STAmount fail");
        unexpected ((zeroSt > zeroSt), "STAmount fail");
        unexpected ((zeroSt > one), "STAmount fail");
        unexpected ((zeroSt > hundred), "STAmount fail");
        unexpected (! (one > zeroSt), "STAmount fail");
        unexpected ((one > one), "STAmount fail");
        unexpected ((one > hundred), "STAmount fail");
        unexpected (! (hundred > zeroSt), "STAmount fail");
        unexpected (! (hundred > one), "STAmount fail");
        unexpected ((hundred > hundred), "STAmount fail");
        unexpected (! (zeroSt <= zeroSt), "STAmount fail");
        unexpected (! (zeroSt <= one), "STAmount fail");
        unexpected (! (zeroSt <= hundred), "STAmount fail");
        unexpected ((one <= zeroSt), "STAmount fail");
        unexpected (! (one <= one), "STAmount fail");
        unexpected (! (one <= hundred), "STAmount fail");
        unexpected ((hundred <= zeroSt), "STAmount fail");
        unexpected ((hundred <= one), "STAmount fail");
        unexpected (! (hundred <= hundred), "STAmount fail");
        unexpected (! (zeroSt >= zeroSt), "STAmount fail");
        unexpected ((zeroSt >= one), "STAmount fail");
        unexpected ((zeroSt >= hundred), "STAmount fail");
        unexpected (! (one >= zeroSt), "STAmount fail");
        unexpected (! (one >= one), "STAmount fail");
        unexpected ((one >= hundred), "STAmount fail");
        unexpected (! (hundred >= zeroSt), "STAmount fail");
        unexpected (! (hundred >= one), "STAmount fail");
        unexpected (! (hundred >= hundred), "STAmount fail");
        unexpected (! (zeroSt == zeroSt), "STAmount fail");
        unexpected ((zeroSt == one), "STAmount fail");
        unexpected ((zeroSt == hundred), "STAmount fail");
        unexpected ((one == zeroSt), "STAmount fail");
        unexpected (! (one == one), "STAmount fail");
        unexpected ((one == hundred), "STAmount fail");
        unexpected ((hundred == zeroSt), "STAmount fail");
        unexpected ((hundred == one), "STAmount fail");
        unexpected (! (hundred == hundred), "STAmount fail");
        unexpected ((zeroSt != zeroSt), "STAmount fail");
        unexpected (! (zeroSt != one), "STAmount fail");
        unexpected (! (zeroSt != hundred), "STAmount fail");
        unexpected (! (one != zeroSt), "STAmount fail");
        unexpected ((one != one), "STAmount fail");
        unexpected (! (one != hundred), "STAmount fail");
        unexpected (! (hundred != zeroSt), "STAmount fail");
        unexpected (! (hundred != one), "STAmount fail");
        unexpected ((hundred != hundred), "STAmount fail");
        unexpected (STAmount (noIssue()).getText () != "0", "STAmount fail");
        unexpected (STAmount (noIssue(), 31).getText () != "31", "STAmount fail");
        unexpected (STAmount (noIssue(), 31, 1).getText () != "310", "STAmount fail");
        unexpected (STAmount (noIssue(), 31, -1).getText () != "3.1", "STAmount fail");
        unexpected (STAmount (noIssue(), 31, -2).getText () != "0.31", "STAmount fail");
        unexpected (multiply (STAmount (noIssue(), 20), STAmount (3), noIssue()).getText () != "60",
            "STAmount multiply fail 1");
        unexpected (multiply (STAmount (noIssue(), 20), STAmount (3), mIssue ()).getText () != "60",
            "STAmount multiply fail 2");
        unexpected (multiply (STAmount (20), STAmount (3), noIssue()).getText () != "60",
            "STAmount multiply fail 3");
        unexpected (multiply (STAmount (20), STAmount (3), mIssue ()).getText () != "60",
            "STAmount multiply fail 4");

        if (divide (STAmount (noIssue(), 60), STAmount (3), noIssue()).getText () != "20")
        {
            log << "60/3 = " <<
                divide (STAmount (noIssue(), 60),
                    STAmount (3), noIssue()).getText ();
            fail ("STAmount divide fail");
        }
        else
        {
            pass ();
        }

        unexpected (divide (STAmount (noIssue(), 60), STAmount (3), mIssue ()).getText () != "20",
            "STAmount divide fail");

        unexpected (divide (STAmount (noIssue(), 60), STAmount (noIssue(), 3), noIssue()).getText () != "20",
            "STAmount divide fail");

        unexpected (divide (STAmount (noIssue(), 60), STAmount (noIssue(), 3), mIssue ()).getText () != "20",
            "STAmount divide fail");

        STAmount a1 (noIssue(), 60), a2 (noIssue(), 10, -1);

        unexpected (divide (a2, a1, noIssue()) != amountFromQuality (getRate (a1, a2)),
            "STAmount setRate(getRate) fail");

        unexpected (divide (a1, a2, noIssue()) != amountFromQuality (getRate (a2, a1)),
            "STAmount setRate(getRate) fail");
    }

    //--------------------------------------------------------------------------

    void testArithmetic ()
    {
        testcase ("arithmetic");

        // Test currency multiplication and division operations such as
        // convertToDisplayAmount, convertToInternalAmount, getRate, getClaimed, and getNeeded

        unexpected (getRate (STAmount (1), STAmount (10)) != (((100ull - 14) << (64 - 8)) | 1000000000000000ull),
            "STAmount getRate fail 1");

        unexpected (getRate (STAmount (10), STAmount (1)) != (((100ull - 16) << (64 - 8)) | 1000000000000000ull),
            "STAmount getRate fail 2");

        unexpected (getRate (STAmount (noIssue(), 1), STAmount (noIssue(), 10)) != (((100ull - 14) << (64 - 8)) | 1000000000000000ull),
            "STAmount getRate fail 3");

        unexpected (getRate (STAmount (noIssue(), 10), STAmount (noIssue(), 1)) != (((100ull - 16) << (64 - 8)) | 1000000000000000ull),
            "STAmount getRate fail 4");

        unexpected (getRate (STAmount (noIssue(), 1), STAmount (10)) != (((100ull - 14) << (64 - 8)) | 1000000000000000ull),
            "STAmount getRate fail 5");

        unexpected (getRate (STAmount (noIssue(), 10), STAmount (1)) != (((100ull - 16) << (64 - 8)) | 1000000000000000ull),
            "STAmount getRate fail 6");

        unexpected (getRate (STAmount (1), STAmount (noIssue(), 10)) != (((100ull - 14) << (64 - 8)) | 1000000000000000ull),
            "STAmount getRate fail 7");

        unexpected (getRate (STAmount (10), STAmount (noIssue(), 1)) != (((100ull - 16) << (64 - 8)) | 1000000000000000ull),
            "STAmount getRate fail 8");

        roundTest (1, 3, 3);
        roundTest (2, 3, 9);
        roundTest (1, 7, 21);
        roundTest (1, 2, 4);
        roundTest (3, 9, 18);
        roundTest (7, 11, 44);

        for (int i = 0; i <= 100000; ++i)
        {
            mulTest (
                rand_int(10000000),
                rand_int(10000000));
        }
    }

    //--------------------------------------------------------------------------

    void testUnderflow ()
    {
        testcase ("underflow");

        STAmount bigNative (STAmount::cMaxNative / 2);
        STAmount bigValue (noIssue(),
            (STAmount::cMinValue + STAmount::cMaxValue) / 2,
            STAmount::cMaxOffset - 1);
        STAmount smallValue (noIssue(),
            (STAmount::cMinValue + STAmount::cMaxValue) / 2,
            STAmount::cMinOffset + 1);
        STAmount zeroSt (noIssue(), 0);

        STAmount smallXsmall = multiply (smallValue, smallValue, noIssue());

        BEAST_EXPECT(smallXsmall == zero);

        STAmount bigDsmall = divide (smallValue, bigValue, noIssue());

        BEAST_EXPECT(bigDsmall == zero);

        BEAST_EXPECT(bigDsmall == zero);

        bigDsmall = divide (smallValue, bigValue, mIssue ());

        BEAST_EXPECT(bigDsmall == zero);

        bigDsmall = divide (smallValue, bigNative, mIssue ());

        BEAST_EXPECT(bigDsmall == zero);

        // very bad offer
        std::uint64_t r = getRate (smallValue, bigValue);

        BEAST_EXPECT(r == 0);

        // very good offer
        r = getRate (bigValue, smallValue);

        BEAST_EXPECT(r == 0);
    }

    //--------------------------------------------------------------------------

    void testRounding ()
    {
        // VFALCO TODO There are no actual tests here, just printed output?
        //             Change this to actually do something.

#if 0
        beginTestCase ("rounding ");

        std::uint64_t value = 25000000000000000ull;
        int offset = -14;
        canonicalizeRound (false, value, offset, true);

        STAmount one (noIssue(), 1);
        STAmount two (noIssue(), 2);
        STAmount three (noIssue(), 3);

        STAmount oneThird1 = divRound (one, three, noIssue(), false);
        STAmount oneThird2 = divide (one, three, noIssue());
        STAmount oneThird3 = divRound (one, three, noIssue(), true);
        log << oneThird1;
        log << oneThird2;
        log << oneThird3;

        STAmount twoThird1 = divRound (two, three, noIssue(), false);
        STAmount twoThird2 = divide (two, three, noIssue());
        STAmount twoThird3 = divRound (two, three, noIssue(), true);
        log << twoThird1;
        log << twoThird2;
        log << twoThird3;

        STAmount oneA = mulRound (oneThird1, three, noIssue(), false);
        STAmount oneB = multiply (oneThird2, three, noIssue());
        STAmount oneC = mulRound (oneThird3, three, noIssue(), true);
        log << oneA;
        log << oneB;
        log << oneC;

        STAmount fourThirdsB = twoThird2 + twoThird2;
        log << fourThirdsA;
        log << fourThirdsB;
        log << fourThirdsC;

        STAmount dripTest1 = mulRound (twoThird2, two, mIssue (), false);
        STAmount dripTest2 = multiply (twoThird2, two, mIssue ());
        STAmount dripTest3 = mulRound (twoThird2, two, mIssue (), true);
        log << dripTest1;
        log << dripTest2;
        log << dripTest3;
#endif
    }

    void
    testConvertM ()
    {
        testcase ("STAmount to MAmount conversions");

        Issue const usd { Currency (0x5553440000000000), AccountID (0x4985601) };
        Issue const m { mIssue () };

        for (std::uint64_t drops = 100000000000000000; drops != 1; drops = drops / 10)
        {
            auto const t = amountFromString (m, std::to_string (drops));
            auto const s = t.m ();
            BEAST_EXPECT(s.drops() == drops);
            BEAST_EXPECT(t == STAmount (MAmount (drops)));
            BEAST_EXPECT(s == MAmount (drops));
        }

        try
        {
            auto const t = amountFromString (usd, "136500");
            fail (to_string (t.m ()));
        }
        catch (std::logic_error const&)
        {
            pass ();
        }
        catch (std::exception const&)
        {
            fail ("wrong exception");
        }
    }

    void
    testConvertIOU ()
    {
        testcase ("STAmount to IOUAmount conversions");

        Issue const usd { Currency (0x5553440000000000), AccountID (0x4985601) };
        Issue const m { mIssue () };

        for (std::uint64_t dollars = 10000000000; dollars != 1; dollars = dollars / 10)
        {
            auto const t = amountFromString (usd, std::to_string (dollars));
            auto const s = t.iou ();
            BEAST_EXPECT(t == STAmount (s, usd));
            BEAST_EXPECT(s.mantissa () == t.mantissa ());
            BEAST_EXPECT(s.exponent () == t.exponent ());
        }

        try
        {
            auto const t = amountFromString (m, "136500");
            fail (to_string (t.iou ()));
        }
        catch (std::logic_error const&)
        {
            pass ();
        }
        catch (std::exception const&)
        {
            fail ("wrong exception");
        }
    }

    //--------------------------------------------------------------------------

    void run ()
    {
        testSetValue ();
        testNativeCurrency ();
        testCustomCurrency ();
        testArithmetic ();
        testUnderflow ();
        testRounding ();
        testConvertM ();
        testConvertIOU ();
    }
};

BEAST_DEFINE_TESTSUITE(STAmount,mtchain_data,mtchain);

} //
