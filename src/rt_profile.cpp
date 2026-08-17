#include "rt_profile.h"

#include <algorithm>
#include <cstdio>
#include <deque>
#include <iomanip>
#include <ios>
#include <locale>
#include <sstream>
#include <vector>

#if defined(_WIN32)
#include "platform_win.h"
#endif

namespace
{

using clock_type = std::chrono::steady_clock;

std::deque<rt_profile::counter> &counters()
{
    // Function-local so that counters registered by static initialisers in other
    // translation units cannot be created before the container is. A deque because
    // counters hand out their addresses and must not be moved by a reallocation.
    static std::deque<rt_profile::counter> instance;
    return instance;
}

clock_type::time_point interval_start;
std::chrono::duration<double> report_interval{ 0.0 };
unsigned long long turns = 0;
unsigned long long frames = 0;
FILE *log_file = nullptr;

#if defined(_WIN32)
bool console_open = false;

/**
 * The Windows build is a GUI application: it has no stdout to print to. Attach to
 * the console it was launched from if there is one, and open a new window if not,
 * so that starting the game from a shortcut still gets you a report.
 */
void open_console()
{
    if( console_open ) {
        return;
    }
    if( AttachConsole( ATTACH_PARENT_PROCESS ) == 0 && AllocConsole() == 0 ) {
        return;
    }
    FILE *reopened = nullptr;
    freopen_s( &reopened, "CONOUT$", "w", stdout );
    SetConsoleTitleA( "Cataclysm RT profile" );
    console_open = true;
}

/** Processor time this process has used, in seconds, or a negative number. */
double process_cpu_seconds()
{
    FILETIME creation;
    FILETIME exit;
    FILETIME kernel;
    FILETIME user;
    if( GetProcessTimes( GetCurrentProcess(), &creation, &exit, &kernel, &user ) == 0 ) {
        return -1.0;
    }
    const auto to_seconds = []( const FILETIME & ft ) {
        const unsigned long long ticks =
            ( static_cast<unsigned long long>( ft.dwHighDateTime ) << 32 ) | ft.dwLowDateTime;
        return static_cast<double>( ticks ) * 1e-7;  // 100 ns units
    };
    return to_seconds( kernel ) + to_seconds( user );
}
#else
void open_console() {}
double process_cpu_seconds()
{
    return -1.0;
}
#endif

double cpu_at_interval_start = -1.0;

void reset_interval()
{
    for( rt_profile::counter &c : counters() ) {
        c.calls = 0;
        c.total_ns = 0;
        c.child_ns = 0;
    }
    turns = 0;
    frames = 0;
    interval_start = clock_type::now();
    cpu_at_interval_start = process_cpu_seconds();
}

void write_out( const std::string &text )
{
    std::fputs( text.c_str(), stdout );
    std::fflush( stdout );
    if( log_file != nullptr ) {
        std::fputs( text.c_str(), log_file );
        std::fflush( log_file );
    }
}

} // namespace

namespace rt_profile
{

bool enabled = false;

scope *scope::innermost = nullptr;

counter *make_counter( const char *const name )
{
    counters().push_back( counter{ name, 0, 0, 0 } );
    return &counters().back();
}

scope::scope( counter *const c ) : c_( enabled ? c : nullptr )
{
    if( c_ != nullptr ) {
        parent_ = innermost;
        innermost = this;
        start_ = clock_type::now();
    }
}

scope::~scope()
{
    if( c_ == nullptr ) {
        return;
    }
    const long long ns = std::chrono::duration_cast<std::chrono::nanoseconds>(
                             clock_type::now() - start_ ).count();
    c_->calls++;
    c_->total_ns += ns;
    innermost = parent_;
    if( parent_ != nullptr ) {
        // Attributed to the enclosing scope so that its self time excludes this
        // one: without it the outer phases just restate the sum of the inner ones.
        parent_->c_->child_ns += ns;
    }
}

void scope::settle_open()
{
    const clock_type::time_point now = clock_type::now();
    for( scope *s = innermost; s != nullptr; s = s->parent_ ) {
        const long long ns = std::chrono::duration_cast<std::chrono::nanoseconds>(
                                 now - s->start_ ).count();
        s->c_->total_ns += ns;
        if( s->parent_ != nullptr ) {
            s->parent_->c_->child_ns += ns;
        }
        // Rebased rather than left alone, so the destructor charges only what
        // happens after this point and no time is counted twice.
        s->start_ = now;
    }
}

void enable( const std::chrono::duration<double> interval )
{
    open_console();
    if( log_file == nullptr ) {
#if defined(_MSC_VER)
        fopen_s( &log_file, "rt-profile.log", "w" );
#else
        log_file = std::fopen( "rt-profile.log", "w" );
#endif
    }
    report_interval = interval;
    reset_interval();
    enabled = true;
    write_out( "rt-profile: measuring. Columns are for the interval, not the run.\n" );
}

void count_turn()
{
    turns++;
}

void count_frame()
{
    frames++;
}

std::string report()
{
    scope::settle_open();
    const double wall = std::chrono::duration<double>( clock_type::now() - interval_start ).count();
    const double cpu = process_cpu_seconds();
    const double cpu_used = cpu >= 0.0 && cpu_at_interval_start >= 0.0
                            ? cpu - cpu_at_interval_start : -1.0;

    std::ostringstream out;
    // The game runs under the player's locale, which would put decimal commas in
    // a table that is also a log file people grep and plot.
    out.imbue( std::locale::classic() );
    out << std::fixed << std::setprecision( 2 );
    out << "\n--- rt-profile  " << wall << " s  |  world "
        << ( wall > 0.0 ? turns / wall : 0.0 ) << " turns/s  |  "
        << std::setprecision( 1 ) << ( wall > 0.0 ? frames / wall : 0.0 ) << " fps";
    if( cpu_used >= 0.0 ) {
        // The number that answers "why is the machine busy": processor time used
        // divided by time passed. 100% is one core saturated.
        out << "  |  cpu " << std::setprecision( 0 )
            << ( wall > 0.0 ? 100.0 * cpu_used / wall : 0.0 ) << "% of one core";
    }
    out << " ---\n";

    std::vector<const counter *> sorted;
    for( const counter &c : counters() ) {
        // Time without calls is a scope that is still running - the input wait,
        // usually - and is the most interesting row there is when the game looks
        // idle but the machine does not.
        if( c.calls > 0 || c.total_ns > 0 ) {
            sorted.push_back( &c );
        }
    }
    // By self time: the question is which phase is spending the time, not which
    // phase contains the one that is.
    std::sort( sorted.begin(), sorted.end(), []( const counter * a, const counter * b ) {
        return ( a->total_ns - a->child_ns ) > ( b->total_ns - b->child_ns );
    } );

    out << std::left << std::setw( 32 ) << "scope" << std::right
        << std::setw( 8 ) << "calls" << std::setw( 11 ) << "total ms"
        << std::setw( 11 ) << "self ms" << std::setw( 9 ) << "% wall"
        << std::setw( 11 ) << "ms/call" << "\n";
    for( const counter *c : sorted ) {
        const double total_ms = c->total_ns / 1e6;
        const double self_ms = ( c->total_ns - c->child_ns ) / 1e6;
        out << std::left << std::setw( 32 ) << c->name << std::right
            << std::setw( 8 ) << c->calls
            << std::setprecision( 2 ) << std::setw( 11 ) << total_ms << std::setw( 11 ) << self_ms
            << std::setprecision( 1 ) << std::setw( 8 )
            << ( wall > 0.0 ? 100.0 * total_ms / ( wall * 1000.0 ) : 0.0 ) << "%";
        if( c->calls > 0 ) {
            out << std::setprecision( 3 ) << std::setw( 11 ) << total_ms / c->calls;
        } else {
            // Still inside its first call; there is no per-call figure yet.
            out << std::setw( 11 ) << "running";
        }
        out << "\n";
    }
    if( sorted.empty() ) {
        out << "(nothing measured this interval)\n";
    }
    return out.str();
}

void tick()
{
    if( !enabled || report_interval <= std::chrono::duration<double>::zero() ) {
        return;
    }
    if( clock_type::now() - interval_start < report_interval ) {
        return;
    }
    write_out( report() );
    reset_interval();
}

void shutdown()
{
    if( !enabled ) {
        return;
    }
    write_out( report() );
    enabled = false;
    if( log_file != nullptr ) {
        std::fclose( log_file );
        log_file = nullptr;
    }
}

} // namespace rt_profile
