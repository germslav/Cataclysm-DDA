#include <cmath>

#include "avatar.h"
#include "cata_catch.h"
#include "coordinates.h"
#include "creature.h"
#include "creature_tracker.h"
#include "map.h"
#include "map_helpers.h"
#include "monster.h"
#include "player_helpers.h"
#include "point.h"
#include "point_float.h"
#include "type_id.h"

// The RT fork stores creature positions continuously. Everything downstream -
// collision, combat geometry, rendering - assumes the continuous position and
// the integer position never disagree about which tile a creature is in.
//
// The continuous position is now the authority and the integer one is projected
// from it, so these tests cover both directions: that the projection lands on the
// right tile, and that the sub-tile part is storage rather than something the
// accessors round away.

static const mtype_id pseudo_debug_mon( "pseudo_debug_mon" );

TEST_CASE( "tile_membership_uses_floor_not_truncation", "[rt][coords][nogame]" )
{
    // Casting to int rounds toward zero, which puts -0.5 in tile 0. Tile n covers
    // [n, n+1), so -0.5 belongs to tile -1. This only ever goes wrong north and
    // west of the origin, which is exactly why it is worth a test.
    CHECK( to_tile( tripoint_f( 0.0, 0.0, 0 ) ) == tripoint( 0, 0, 0 ) );
    CHECK( to_tile( tripoint_f( 0.5, 0.9, 0 ) ) == tripoint( 0, 0, 0 ) );
    CHECK( to_tile( tripoint_f( 1.0, 1.0, 0 ) ) == tripoint( 1, 1, 0 ) );

    CHECK( to_tile( tripoint_f( -0.5, -0.5, 0 ) ) == tripoint( -1, -1, 0 ) );
    CHECK( to_tile( tripoint_f( -0.001, -0.001, 0 ) ) == tripoint( -1, -1, 0 ) );
    CHECK( to_tile( tripoint_f( -1.0, -1.0, 0 ) ) == tripoint( -1, -1, 0 ) );
    CHECK( to_tile( tripoint_f( -1.5, -2.5, 0 ) ) == tripoint( -2, -3, 0 ) );
}

TEST_CASE( "tile_fraction_is_always_in_unit_range", "[rt][coords][nogame]" )
{
    for( const double v : {
             -3.75, -1.5, -0.25, 0.0, 0.25, 1.5, 3.75, 1234.5, -1234.5
         } ) {
        const point_f frac = tile_fraction( tripoint_f( v, v, 0 ) );
        INFO( "v = " << v );
        CHECK( frac.x >= 0.0 );
        CHECK( frac.x < 1.0 );
        CHECK( frac.y >= 0.0 );
        CHECK( frac.y < 1.0 );
    }
}

TEST_CASE( "tile_centre_round_trips_through_to_tile", "[rt][coords][nogame]" )
{
    for( const tripoint &p : {
             tripoint( 0, 0, 0 ), tripoint( 7, 3, 1 ), tripoint( -1, -1, 0 ),
             tripoint( -40, 17, -2 ), tripoint( 131072, -65536, 3 )
         } ) {
        INFO( "p = " << p.to_string() );
        CHECK( to_tile( tile_centre( p ) ) == p );
        // A creature standing in the middle of its tile must draw exactly where
        // the integer-only renderer would put it.
        const point_f off = offset_from_tile_centre( tile_centre( p ) );
        CHECK( off.x == Approx( 0.0 ) );
        CHECK( off.y == Approx( 0.0 ) );
    }
}

TEST_CASE( "double_keeps_sub_tile_precision_at_world_scale", "[rt][coords][nogame]" )
{
    // The reason these types are double rather than reusing rl_vec3d, which is
    // 32-bit float: absolute map-square coordinates reach six figures and beyond,
    // where a float mantissa quantises to roughly 1/16 of a tile.
    const double far_away = 4'000'000.0;
    const tripoint_f p( far_away + 0.25, far_away + 0.75, 0 );
    CHECK( tile_fraction( p ).x == Approx( 0.25 ) );
    CHECK( tile_fraction( p ).y == Approx( 0.75 ) );
    CHECK( to_tile( p ) == tripoint( static_cast<int>( far_away ),
                                     static_cast<int>( far_away ), 0 ) );
}

TEST_CASE( "creature_continuous_position_agrees_with_tile_position", "[rt][coords]" )
{
    clear_map();
    avatar &u = get_avatar();
    map &here = get_map();

    for( const tripoint_bub_ms &p : {
             tripoint_bub_ms( 60, 60, 0 ), tripoint_bub_ms( 61, 60, 0 ),
             tripoint_bub_ms( 60, 61, 0 ), tripoint_bub_ms( 65, 68, 0 )
         } ) {
        u.setpos( here, p );
        INFO( "placed at " << p.to_string() );

        // The two representations must never disagree about the tile.
        CHECK( to_tile( u.pos_abs_f() ) == u.pos_abs() );
        CHECK( to_tile( u.pos_bub_f() ) == u.pos_bub() );

        // The integer setters name a tile, so they place the creature at its
        // centre: rendering offsets are zero and the picture is unchanged from
        // the tile-only renderer.
        const point_f off = offset_from_tile_centre( u.pos_abs_f() );
        CHECK( off.x == Approx( 0.0 ) );
        CHECK( off.y == Approx( 0.0 ) );
    }
}

TEST_CASE( "creature_keeps_a_sub_tile_position", "[rt][coords]" )
{
    clear_map();
    avatar &u = get_avatar();
    map &here = get_map();

    // The point of the flip: a position between tiles survives being stored and
    // read back, instead of being rounded to the tile the creature is nearest.
    const tripoint_bub_ms_f p( 60.25, 61.75, 0 );
    u.setpos_f( here, p );

    CHECK( u.pos_bub_f().x() == Approx( 60.25 ) );
    CHECK( u.pos_bub_f().y() == Approx( 61.75 ) );
    // Occupancy is floor(), so this is tile (60, 61) - not the nearest tile.
    CHECK( u.pos_bub() == tripoint_bub_ms( 60, 61, 0 ) );
    CHECK( u.pos_abs() == here.get_abs( tripoint_bub_ms( 60, 61, 0 ) ) );
    CHECK( to_tile( u.pos_abs_f() ) == u.pos_abs() );

    // What the renderer displaces the sprite by: a quarter tile west, a quarter
    // tile south of the tile centre.
    const point_f off = offset_from_tile_centre( u.pos_abs_f() );
    CHECK( off.x == Approx( -0.25 ) );
    CHECK( off.y == Approx( 0.25 ) );

    // Naming a tile still means the centre of that tile - the integer setters are
    // placement, not kinematics, and must not leave a stale fraction behind.
    u.setpos( here, tripoint_bub_ms( 60, 61, 0 ) );
    CHECK( u.pos_bub_f().x() == Approx( 60.5 ) );
    CHECK( u.pos_bub_f().y() == Approx( 61.5 ) );
}

TEST_CASE( "monster_continuous_position_agrees_with_tile_position", "[rt][coords]" )
{
    clear_map();
    map &here = get_map();

    const tripoint_bub_ms start( 62, 62, 0 );
    monster &mon = spawn_test_monster( pseudo_debug_mon.str(), start );

    CHECK( to_tile( mon.pos_abs_f() ) == mon.pos_abs() );
    CHECK( to_tile( mon.pos_bub_f() ) == mon.pos_bub() );

    const tripoint_bub_ms moved( 64, 63, 0 );
    mon.setpos( here, moved );
    CHECK( mon.pos_bub() == moved );
    CHECK( to_tile( mon.pos_abs_f() ) == mon.pos_abs() );
    CHECK( to_tile( mon.pos_bub_f() ) == mon.pos_bub() );
}

TEST_CASE( "sub_tile_movement_keeps_the_monster_where_the_tracker_expects_it", "[rt][coords]" )
{
    clear_map();
    map &here = get_map();
    creature_tracker &tracker = get_creature_tracker();

    const tripoint_bub_ms start( 62, 62, 0 );
    monster &mon = spawn_test_monster( pseudo_debug_mon.str(), start );

    // The tracker is keyed by tile, so a step that stays inside one tile must not
    // touch it - and must not silently drop the monster out of it either.
    mon.setpos_f( here, tripoint_bub_ms_f( 62.9, 62.1, 0 ) );
    CHECK( mon.pos_bub() == start );
    CHECK( tracker.creature_at<monster>( start ) == &mon );
    CHECK( mon.pos_bub_f().x() == Approx( 62.9 ) );

    // Crossing the boundary is an ordinary move as far as the tracker is
    // concerned, even though the creature travelled a fifth of a tile.
    const tripoint_bub_ms next( 63, 62, 0 );
    mon.setpos_f( here, tripoint_bub_ms_f( 63.1, 62.1, 0 ) );
    CHECK( mon.pos_bub() == next );
    CHECK( tracker.creature_at<monster>( next ) == &mon );
    CHECK( tracker.creature_at<monster>( start ) == nullptr );
}
