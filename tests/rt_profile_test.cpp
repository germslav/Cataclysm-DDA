#include <memory>
#include <string>

#include "cata_catch.h"
#include "rt_profile.h"

// The profiler is a development tool, so what is worth pinning is not its numbers
// - those are timings, and asserting on timings is how flaky tests are born - but
// its bookkeeping: that a scope is counted once, that a scope which has not
// returned yet still reports, and that the report survives being asked for in
// that state.

TEST_CASE( "rt_profile_counts_completed_scopes", "[rt][profile][nogame]" )
{
    rt_profile::counter *c = rt_profile::make_counter( "test: completed" );
    rt_profile::enabled = true;
    {
        const rt_profile::scope s( c );
    }
    {
        const rt_profile::scope s( c );
    }
    rt_profile::enabled = false;

    CHECK( c->calls == 2 );
    CHECK( c->total_ns >= 0 );
    // Nothing nested inside, so all of it is this scope's own time.
    CHECK( c->child_ns == 0 );

    // A scope entered while measurement is off must not be counted at all, or
    // switching the profiler on mid-turn would produce a half-measured interval.
    {
        const rt_profile::scope s( c );
    }
    CHECK( c->calls == 2 );
}

TEST_CASE( "rt_profile_reports_a_scope_that_has_not_returned", "[rt][profile][nogame]" )
{
    rt_profile::counter *c = rt_profile::make_counter( "test: still running" );
    rt_profile::enabled = true;
    // Held open across the report, which is the state the input wait is in every
    // time the profiler prints: time accumulating, no completed call yet.
    std::unique_ptr<rt_profile::scope> held = std::make_unique<rt_profile::scope>( c );

    const std::string first = rt_profile::report();
    CHECK( first.find( "test: still running" ) != std::string::npos );
    CHECK( c->calls == 0 );
    const long long charged = c->total_ns;

    // Settling must rebase the scope rather than leave it, so the same elapsed
    // time is not charged again by the next report or by the destructor.
    const std::string second = rt_profile::report();
    CHECK( second.find( "test: still running" ) != std::string::npos );
    CHECK( c->total_ns >= charged );

    held.reset();
    rt_profile::enabled = false;
    CHECK( c->calls == 1 );
}

TEST_CASE( "rt_profile_attributes_nested_time_to_the_child", "[rt][profile][nogame]" )
{
    rt_profile::counter *outer = rt_profile::make_counter( "test: outer" );
    rt_profile::counter *inner = rt_profile::make_counter( "test: inner" );
    rt_profile::enabled = true;
    {
        const rt_profile::scope o( outer );
        {
            const rt_profile::scope i( inner );
        }
    }
    rt_profile::enabled = false;

    // The outer scope's self time is what is left after the inner one, which is
    // what stops a report from being a list of the same milliseconds restated at
    // every level of nesting.
    CHECK( outer->child_ns == inner->total_ns );
    CHECK( outer->total_ns >= outer->child_ns );
}
