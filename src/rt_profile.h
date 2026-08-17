#pragma once
#ifndef CATA_SRC_RT_PROFILE_H
#define CATA_SRC_RT_PROFILE_H

#include <chrono>
#include <string>

/**
 * Where the time goes.
 *
 * RT fork: the continuous simulation has a frame budget, so "the game feels slow"
 * and "the machine is busy" have to become numbers before anything can be done
 * about them. This is the smallest thing that produces those numbers: named
 * scopes, accumulated per interval, printed as a table.
 *
 * It is instrumentation, not a sampling profiler. It says how long the phases of
 * the turn took and how much of the wall clock the process actually spent on the
 * CPU; it does not say which line inside a phase was to blame. That is the right
 * trade for a loop whose phases are already named and already the unit of work.
 *
 * Off by default and cheap when off: an enabled check and nothing else. Switch it
 * on with @c --rt-profile, which on Windows also opens a console window to print
 * into, since the game itself is built as a GUI application and has no stdout.
 *
 * Single-threaded, like the simulation it measures. The scope stack is a plain
 * global.
 */
namespace rt_profile
{

/** One named accumulator. Created once per instrumented scope, never destroyed. */
struct counter {
    const char *name = nullptr;
    unsigned long long calls = 0;
    /** Time inside this scope, including nested scopes. */
    long long total_ns = 0;
    /** Time inside nested scopes, so that self time is total - child. */
    long long child_ns = 0;
};

/** Whether measurement is on. Read directly on the hot path; do not make it a call. */
extern bool enabled;

/** Registers a counter. The name must outlive the program - use a literal. */
counter *make_counter( const char *name );

/**
 * Measures the enclosing block. Prefer the RT_PROFILE_SCOPE macro.
 *
 * A scope that starts while measurement is off stays off for its whole life, so
 * switching the profiler on mid-turn cannot produce a half-measured interval.
 */
class scope
{
    public:
        explicit scope( counter *c );
        ~scope();

        scope( const scope & ) = delete;
        scope &operator=( const scope & ) = delete;

        /**
         * Charges the time so far to every scope that is still open, and restarts
         * their clocks from now.
         *
         * Without this a report says nothing about a scope that has not returned
         * yet - and the interesting case, a game sitting in its input wait, is
         * precisely one long scope that spans every interval. Called by the
         * reporter, not by instrumented code.
         */
        static void settle_open();

    private:
        counter *c_;
        scope *parent_ = nullptr;
        std::chrono::steady_clock::time_point start_;

        static scope *innermost;
};

/**
 * Turns measurement on, opening the output console and starting the interval.
 * @param interval how often to print. Zero prints only on @ref shutdown.
 */
void enable( std::chrono::duration<double> interval );

/** Prints a final report and closes the output file. */
void shutdown();

/** Counts one world turn. */
void count_turn();
/** Counts one displayed frame. */
void count_frame();

/**
 * Prints the report if the interval has elapsed. Cheap enough to call from the
 * input polling loop, which is the only thing running when the game is idle -
 * and an idle game that is nonetheless busy is exactly what this is for.
 */
void tick();

/** The report as it would be printed, for tests and for the debug menu. */
std::string report();

} // namespace rt_profile

#define RT_PROFILE_CONCAT_INNER( a, b ) a##b
#define RT_PROFILE_CONCAT( a, b ) RT_PROFILE_CONCAT_INNER( a, b )

/** Measures the enclosing block under @p name, which must be a string literal. */
#define RT_PROFILE_SCOPE( name )                                                  \
    static rt_profile::counter *RT_PROFILE_CONCAT( rt_prof_counter_, __LINE__ ) = \
            rt_profile::make_counter( name );                                     \
    const rt_profile::scope RT_PROFILE_CONCAT( rt_prof_scope_, __LINE__ )(        \
            RT_PROFILE_CONCAT( rt_prof_counter_, __LINE__ ) )

#endif // CATA_SRC_RT_PROFILE_H
