#include <algorithm>
#include <string>
#include <vector>

#include "cata_catch.h"
#include "coordinates.h"
#include "creature_tracker.h"
#include "debug.h"
#include "map.h"
#include "map_helpers.h"
#include "memory_fast.h"
#include "monster.h"
#include "point_float.h"
#include "type_id.h"

// The monsters are held in a spatial index rather than a tile -> monster map.
// The tile lookup it replaced is exercised by the whole suite already, since
// every creature_at call goes through it; what these tests cover is the part
// that map could not do - asking who is near a point - and the rule the swap
// deliberately left alone: one monster to a tile.

static const mtype_id pseudo_debug_mon( "pseudo_debug_mon" );

static bool contains( const std::vector<monster *> &found, const monster &mon )
{
    return std::find( found.begin(), found.end(), &mon ) != found.end();
}

TEST_CASE( "monsters_overlapping_spans_cells_and_measures_a_circle", "[rt][spatial]" )
{
    clear_map();
    map &here = get_map();
    creature_tracker &tracker = get_creature_tracker();

    // Spread wider than one cell of the index, so answering this needs more than
    // one bucket. Where the cell boundaries fall depends on the absolute origin of
    // the test map, which is the point: the query must not care.
    monster &west = spawn_test_monster( pseudo_debug_mon.str(), tripoint_bub_ms( 60, 60, 0 ) );
    monster &east = spawn_test_monster( pseudo_debug_mon.str(), tripoint_bub_ms( 68, 60, 0 ) );
    monster &south = spawn_test_monster( pseudo_debug_mon.str(), tripoint_bub_ms( 60, 75, 0 ) );
    monster &far_east = spawn_test_monster( pseudo_debug_mon.str(), tripoint_bub_ms( 70, 60, 0 ) );

    const tripoint_abs_ms_f centre = tile_centre( here.get_abs( tripoint_bub_ms( 64, 60, 0 ) ) );

    const std::vector<monster *> found = tracker.monsters_overlapping( centre, 5.0 );

    // Four tiles away on either side, in different cells from the centre.
    CHECK( contains( found, west ) );
    CHECK( contains( found, east ) );
    // Fifteen tiles south: nowhere near, and its cells are never visited.
    CHECK_FALSE( contains( found, south ) );
    // Six tiles east: outside the circle, but close enough that the cell it sits
    // in may well be one the query walks. That is what the exact test is for.
    CHECK_FALSE( contains( found, far_east ) );
}

TEST_CASE( "monsters_overlapping_measures_from_the_continuous_position", "[rt][spatial]" )
{
    clear_map();
    map &here = get_map();
    creature_tracker &tracker = get_creature_tracker();

    monster &mon = spawn_test_monster( pseudo_debug_mon.str(), tripoint_bub_ms( 60, 60, 0 ) );
    const tripoint_abs_ms_f centre = tile_centre( here.get_abs( tripoint_bub_ms( 64, 60, 0 ) ) );

    // Exactly four tiles away, centre to centre.
    CHECK( contains( tracker.monsters_overlapping( centre, 4.05 ), mon ) );

    // A step of four tenths of a tile west, which leaves the monster on the tile
    // it is filed under, takes it out of range. The index buckets by tile but
    // measures by position; were it measuring the tile, this would still be
    // exactly four tiles away and still in range.
    mon.setpos_f( here, tripoint_bub_ms_f( 60.1, 60.5, 0 ) );
    REQUIRE( mon.pos_bub() == tripoint_bub_ms( 60, 60, 0 ) );
    CHECK_FALSE( contains( tracker.monsters_overlapping( centre, 4.05 ), mon ) );
    // Still findable at its tile, which is the lookup the rest of the game uses.
    CHECK( tracker.creature_at<monster>( tripoint_bub_ms( 60, 60, 0 ) ) == &mon );
}

TEST_CASE( "a_tile_still_holds_only_one_monster", "[rt][spatial]" )
{
    clear_map();
    map &here = get_map();
    creature_tracker &tracker = get_creature_tracker();

    const tripoint_bub_ms taken( 60, 60, 0 );
    const monster &sitting = spawn_test_monster( pseudo_debug_mon.str(), taken );
    REQUIRE( tracker.creature_at<monster>( taken ) == &sitting );

    // The index would hold both of these perfectly happily - it is a bucket of
    // entries, not a slot per tile. The refusal comes from the tracker, which is
    // where the rule now lives on its own, ready to be lifted without touching
    // the storage underneath.
    const shared_ptr_fast<monster> intruder = make_shared_fast<monster>( pseudo_debug_mon );
    intruder->spawn( here.get_abs( taken ) );

    bool added = true;
    const std::string dmsg = capture_debugmsg_during( [&tracker, &intruder, &added]() {
        added = tracker.add( intruder );
    } );

    CHECK_FALSE( added );
    CHECK_THAT( dmsg, Catch::Contains( "already a monster" ) );
    CHECK( tracker.creature_at<monster>( taken ) == &sitting );
}
