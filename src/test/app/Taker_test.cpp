//------------------------------------------------------------------------------
/*
    This file is part of mtchaind: https://github.com/MTChain/MTChain-core
    Copyright (c) 2017, 2018 MTChain Alliance.

    Permission to use, copy, modify, and/or distribute this software for any

*/
//==============================================================================

#include <mtchain/app/tx/impl/Taker.h>
#include <mtchain/beast/unit_test.h>
#include <mtchain/beast/core/LexicalCast.h>
#include <type_traits>

namespace mtchain {

class Taker_test : public beast::unit_test::suite
{
    static bool const Buy = false;
    static bool const Sell = true;

    class TestTaker
        : public BasicTaker
    {
        STAmount funds_;
        STAmount cross_funds;

    public:
        TestTaker (
                CrossType cross_type,
                Amounts const& amount,
                Quality const& quality,
                STAmount const& funds,
                std::uint32_t flags,
                Rate const& rate_in,
                Rate const& rate_out)
            : BasicTaker (
                cross_type,
                AccountID(0x4701),
                amount,
                quality,
                flags,
                rate_in,
                rate_out)
            , funds_ (funds)
        {
        }

        void
        set_funds (STAmount const& funds)
        {
            cross_funds = funds;
        }

        STAmount
        get_funds (AccountID const& owner, STAmount const& funds) const
        {
            if (owner == account ())
                return funds_;

            return cross_funds;
        }

        Amounts
        cross (Amounts offer, Quality quality)
        {
            if (reject (quality))
                return Amounts (offer.in.zeroed (), offer.out.zeroed ());

            // we need to emulate "unfunded offers" behavior
            if (get_funds (AccountID (0x4702), offer.out) == zero)
                return Amounts (offer.in.zeroed (), offer.out.zeroed ());

            if (done ())
                return Amounts (offer.in.zeroed (), offer.out.zeroed ());

            auto order = Offer();
            auto result = do_cross (order);//offer, quality, AccountID (0x4702));

            funds_ -= result.order.in;

            return result.order;
        }

        std::pair<Amounts, Amounts>
        cross (Amounts offer1, Quality quality1, Amounts offer2, Quality quality2)
        {
            /* check if composed quality should be rejected */
            Quality const quality (composed_quality (
                quality1, quality2));

            if (reject (quality))
                return std::make_pair(
                    Amounts { offer1.in.zeroed (), offer1.out.zeroed () },
                    Amounts { offer2.in.zeroed (), offer2.out.zeroed () });

            if (done ())
                return std::make_pair(
                    Amounts { offer1.in.zeroed (), offer1.out.zeroed () },
                    Amounts { offer2.in.zeroed (), offer2.out.zeroed () });

            auto order1 = Offer();
            auto order2 = Offer();
            auto result = do_cross (order1, order2);
                // offer1, quality1, AccountID (0x4703),
                // offer2, quality2, AccountID (0x4704));

            return std::make_pair (result.first.order, result.second.order);
        }
    };

private:
    Issue const& usd () const
    {
        static Issue const issue (
            Currency (0x5553440000000000), AccountID (0x4985601));
        return issue;
    }

    Issue const& eur () const
    {
        static Issue const issue (
            Currency (0x4555520000000000), AccountID (0x4985602));
        return issue;
    }

    Issue const& m () const
    {
        static Issue const issue (
            mCurrency (), mAccount ());
        return issue;
    }

    STAmount parse_amount (std::string const& amount, Issue const& issue)
    {
        return amountFromString (issue, amount);
    }

    Amounts parse_amounts (
        std::string const& amount_in, Issue const& issue_in,
        std::string const& amount_out, Issue const& issue_out)
    {
        STAmount const in (parse_amount (amount_in, issue_in));
        STAmount const out (parse_amount (amount_out, issue_out));

        return { in, out };
    }

    struct cross_attempt_offer
    {
        cross_attempt_offer (std::string const& in_,
            std::string const &out_)
            : in (in_)
            , out (out_)
        {
        }

        std::string in;
        std::string out;
    };

private:
    std::string
    format_amount (STAmount const& amount)
    {
        std::string txt = amount.getText ();
        txt += "/";
        txt += to_string (amount.issue().currency);
        return txt;
    }

    void
    attempt (
        bool sell,
        std::string name,
        Quality taker_quality,
        cross_attempt_offer const offer,
        std::string const funds,
        Quality cross_quality,
        cross_attempt_offer const cross,
        std::string const cross_funds,
        cross_attempt_offer const flow,
        Issue const& issue_in,
        Issue const& issue_out,
        Rate rate_in = parityRate,
        Rate rate_out = parityRate)
    {
        Amounts taker_offer (parse_amounts (
            offer.in, issue_in,
            offer.out, issue_out));

        Amounts cross_offer (parse_amounts (
            cross.in, issue_in,
            cross.out, issue_out));

        CrossType cross_type;

        if (isM (issue_out))
            cross_type = CrossType::IouToM;
        else if (isM (issue_in))
            cross_type = CrossType::MToIou;
        else
            cross_type = CrossType::IouToIou;

        // FIXME: We are always invoking the IOU-to-IOU taker. We should select
        // the correct type dynamically.
        TestTaker taker (cross_type, taker_offer, taker_quality,
            parse_amount (funds, issue_in), sell ? tfSell : 0,
            rate_in, rate_out);

        taker.set_funds (parse_amount (cross_funds, issue_out));

        auto result = taker.cross (cross_offer, cross_quality);

        Amounts const expected (parse_amounts (
            flow.in, issue_in,
            flow.out, issue_out));

        BEAST_EXPECT(expected == result);

        if (expected != result)
        {
            log <<
                "Expected: " << format_amount (expected.in) <<
                " : " << format_amount (expected.out) << '\n' <<
                "  Actual: " << format_amount (result.in) <<
                " : " << format_amount (result.out) << std::endl;
        }
    }

    Quality get_quality(std::string in, std::string out)
    {
        return Quality (parse_amounts (in, m(), out, m ()));
    }

public:
    // Notation for clamp scenario descriptions:
    //
    // IN:OUT (with the last in the list being limiting factor)
    //  N  = Nothing
    //  T  = Taker Offer Balance
    //  A  = Taker Account Balance
    //  B  = Owner Account Balance
    //
    // (s) = sell semantics: taker wants unlimited output
    // (b) = buy semantics: taker wants a limited amount out

    // NIKB TODO: Augment TestTaker so currencies and rates can be specified
    //            once without need for repetition.
    void
    test_m_to_iou ()
    {
        testcase ("M Quantization: input");

        Quality q1 = get_quality ("1", "1");

        //                             TAKER                    OWNER
        //                     QUAL    OFFER     FUNDS  QUAL    OFFER     FUNDS     EXPECTED
        //                                        M                      USD
        attempt (Sell, "N:N",   q1, { "2", "2" },  "2",  q1, { "2", "2" }, "2",   { "2", "2" },   m(), usd());
        attempt (Sell, "N:B",   q1, { "2", "2" },  "2",  q1, { "2", "2" }, "1.8", { "1", "1.8" }, m(), usd());
        attempt (Buy,  "N:T",   q1, { "1", "1" },  "2",  q1, { "2", "2" }, "2",   { "1", "1" },   m(), usd());
        attempt (Buy,  "N:BT",  q1, { "1", "1" },  "2",  q1, { "2", "2" }, "1.8", { "1", "1" },   m(), usd());
        attempt (Buy,  "N:TB",  q1, { "1", "1" },  "2",  q1, { "2", "2" }, "0.8", { "0", "0.8" }, m(), usd());

        attempt (Sell, "T:N",   q1, { "1", "1" },  "2",  q1, { "2", "2" }, "2",   { "1", "1" },   m(), usd());
        attempt (Sell, "T:B",   q1, { "1", "1" },  "2",  q1, { "2", "2" }, "1.8", { "1", "1.8" }, m(), usd());
        attempt (Buy,  "T:T",   q1, { "1", "1" },  "2",  q1, { "2", "2" }, "2",   { "1", "1" },   m(), usd());
        attempt (Buy,  "T:BT",  q1, { "1", "1" },  "2",  q1, { "2", "2" }, "1.8", { "1", "1" },   m(), usd());
        attempt (Buy,  "T:TB",  q1, { "1", "1" },  "2",  q1, { "2", "2" }, "0.8", { "0", "0.8" }, m(), usd());

        attempt (Sell, "A:N",   q1, { "2", "2" },  "1",  q1, { "2", "2" }, "2",   { "1", "1" },   m(), usd());
        attempt (Sell, "A:B",   q1, { "2", "2" },  "1",  q1, { "2", "2" }, "1.8", { "1", "1.8" }, m(), usd());
        attempt (Buy,  "A:T",   q1, { "2", "2" },  "1",  q1, { "3", "3" }, "3",   { "1", "1" },   m(), usd());
        attempt (Buy,  "A:BT",  q1, { "2", "2" },  "1",  q1, { "3", "3" }, "2.4", { "1", "1" },   m(), usd());
        attempt (Buy,  "A:TB",  q1, { "2", "2" },  "1",  q1, { "3", "3" }, "0.8", { "0", "0.8" }, m(), usd());

        attempt (Sell, "TA:N",  q1, { "2", "2" },  "1",  q1, { "2", "2" }, "2",   { "1", "1" },   m(), usd());
        attempt (Sell, "TA:B",  q1, { "2", "2" },  "1",  q1, { "3", "3" }, "1.8", { "1", "1.8" }, m(), usd());
        attempt (Buy,  "TA:T",  q1, { "2", "2" },  "1",  q1, { "3", "3" }, "3",   { "1", "1" },   m(), usd());
        attempt (Buy,  "TA:BT", q1, { "2", "2" },  "1",  q1, { "3", "3" }, "1.8", { "1", "1.8" }, m(), usd());
        attempt (Buy,  "TA:TB", q1, { "2", "2" },  "1",  q1, { "3", "3" }, "1.8", { "1", "1.8" }, m(), usd());

        attempt (Sell, "AT:N",  q1, { "2", "2" },  "1",  q1, { "3", "3" }, "3",   { "1", "1" },   m(), usd());
        attempt (Sell, "AT:B",  q1, { "2", "2" },  "1",  q1, { "3", "3" }, "1.8", { "1", "1.8" }, m(), usd());
        attempt (Buy,  "AT:T",  q1, { "2", "2" },  "1",  q1, { "3", "3" }, "3",   { "1", "1" },   m(), usd());
        attempt (Buy,  "AT:BT", q1, { "2", "2" },  "1",  q1, { "3", "3" }, "1.8", { "1", "1.8" }, m(), usd());
        attempt (Buy,  "AT:TB", q1, { "2", "2" },  "1",  q1, { "3", "3" }, "0.8", { "0", "0.8" }, m(), usd());
    }

    void
    test_iou_to_m ()
    {
        testcase ("M Quantization: output");

        Quality q1 = get_quality ("1", "1");

        //                             TAKER                     OWNER
        //                     QUAL    OFFER     FUNDS   QUAL    OFFER     FUNDS    EXPECTED
        //                                        USD                       M
        attempt (Sell, "N:N",   q1, { "3", "3" }, "3",   q1, { "3", "3" }, "3",  { "3",   "3" }, usd(), m());
        attempt (Sell, "N:B",   q1, { "3", "3" }, "3",   q1, { "3", "3" }, "2",  { "2",   "2" }, usd(), m());
        attempt (Buy,  "N:T",   q1, { "3", "3" }, "2.5", q1, { "5", "5" }, "5",  { "2.5", "2" }, usd(), m());
        attempt (Buy,  "N:BT",  q1, { "3", "3" }, "1.5", q1, { "5", "5" }, "4",  { "1.5", "1" }, usd(), m());
        attempt (Buy,  "N:TB",  q1, { "3", "3" }, "2.2", q1, { "5", "5" }, "1",  { "1",   "1" }, usd(), m());

        attempt (Sell, "T:N",   q1, { "1", "1" }, "2",   q1, { "2", "2" }, "2",  { "1",   "1" }, usd(), m());
        attempt (Sell, "T:B",   q1, { "2", "2" }, "2",   q1, { "3", "3" }, "1",  { "1",   "1" }, usd(), m());
        attempt (Buy,  "T:T",   q1, { "1", "1" }, "2",   q1, { "2", "2" }, "2",  { "1",   "1" }, usd(), m());
        attempt (Buy,  "T:BT",  q1, { "1", "1" }, "2",   q1, { "3", "3" }, "2",  { "1",   "1" }, usd(), m());
        attempt (Buy,  "T:TB",  q1, { "2", "2" }, "2",   q1, { "3", "3" }, "1",  { "1",   "1" }, usd(), m());

        attempt (Sell, "A:N",   q1, { "2", "2" }, "1.5", q1, { "2", "2" }, "2",  { "1.5", "1" }, usd(), m());
        attempt (Sell, "A:B",   q1, { "2", "2" }, "1.8", q1, { "3", "3" }, "2",  { "1.8", "1" }, usd(), m());
        attempt (Buy,  "A:T",   q1, { "2", "2" }, "1.2", q1, { "3", "3" }, "3",  { "1.2", "1" }, usd(), m());
        attempt (Buy,  "A:BT",  q1, { "2", "2" }, "1.5", q1, { "4", "4" }, "3",  { "1.5", "1" }, usd(), m());
        attempt (Buy,  "A:TB",  q1, { "2", "2" }, "1.5", q1, { "4", "4" }, "1",  { "1",   "1" }, usd(), m());

        attempt (Sell, "TA:N",  q1, { "2", "2" }, "1.5", q1, { "2", "2" }, "2",  { "1.5", "1" }, usd(), m());
        attempt (Sell, "TA:B",  q1, { "2", "2" }, "1.5", q1, { "3", "3" }, "1",  { "1",   "1" }, usd(), m());
        attempt (Buy,  "TA:T",  q1, { "2", "2" }, "1.5", q1, { "3", "3" }, "3",  { "1.5", "1" }, usd(), m());
        attempt (Buy,  "TA:BT", q1, { "2", "2" }, "1.8", q1, { "4", "4" }, "3",  { "1.8", "1" }, usd(), m());
        attempt (Buy,  "TA:TB", q1, { "2", "2" }, "1.2", q1, { "3", "3" }, "1",  { "1",   "1" }, usd(), m());

        attempt (Sell, "AT:N",  q1, { "2", "2" }, "2.5", q1, { "4", "4" }, "4",  { "2",   "2" }, usd(), m());
        attempt (Sell, "AT:B",  q1, { "2", "2" }, "2.5", q1, { "3", "3" }, "1",  { "1",   "1" }, usd(), m());
        attempt (Buy,  "AT:T",  q1, { "2", "2" }, "2.5", q1, { "3", "3" }, "3",  { "2",   "2" }, usd(), m());
        attempt (Buy,  "AT:BT", q1, { "2", "2" }, "2.5", q1, { "4", "4" }, "3",  { "2",   "2" }, usd(), m());
        attempt (Buy,  "AT:TB", q1, { "2", "2" }, "2.5", q1, { "3", "3" }, "1",  { "1",   "1" }, usd(), m());
    }

    void
    test_iou_to_iou ()
    {
        testcase ("IOU to IOU");

        Quality q1 = get_quality ("1", "1");

        // Highly exaggerated 50% transfer rate for the input and output:
        Rate const rate { parityRate.value + (parityRate.value / 2) };

        //                             TAKER                    OWNER
        //                     QUAL    OFFER     FUNDS  QUAL    OFFER     FUNDS     EXPECTED
        //                                        EUR                      USD
        attempt (Sell, "N:N",   q1, { "2", "2" },  "10",  q1, { "2", "2" }, "10",   { "2",                  "2" },                  eur(), usd(), rate, rate);
        attempt (Sell, "N:B",   q1, { "4", "4" },  "10",  q1, { "4", "4" },  "4",   { "2.666666666666666",  "2.666666666666666" },  eur(), usd(), rate, rate);
        attempt (Buy,  "N:T",   q1, { "1", "1" },  "10",  q1, { "2", "2" }, "10",   { "1",                  "1" },                  eur(), usd(), rate, rate);
        attempt (Buy,  "N:BT",  q1, { "2", "2" },  "10",  q1, { "6", "6" },  "5",   { "2",                  "2" },                  eur(), usd(), rate, rate);
        attempt (Buy,  "N:TB",  q1, { "2", "2" },   "2",  q1, { "6", "6" },  "1",   { "0.6666666666666667", "0.6666666666666667" }, eur(), usd(), rate, rate);
        attempt (Sell, "A:N",   q1, { "2", "2" },  "2.5", q1, { "2", "2" }, "10",   { "1.666666666666666",  "1.666666666666666" },  eur(), usd(), rate, rate);
    }

    void
    run()
    {
        test_m_to_iou ();
        test_iou_to_m ();
        test_iou_to_iou ();
    }
};

BEAST_DEFINE_TESTSUITE(Taker,tx,mtchain);

}