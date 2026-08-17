#pragma once
#ifndef CATA_SRC_POINT_FLOAT_H
#define CATA_SRC_POINT_FLOAT_H

#include <cmath>
#include <climits>
#include <functional>
#include <iosfwd>
#include <string>

#include "coordinates.h"
#include "point.h"

/**
 * Continuous counterparts of @ref point and @ref tripoint.
 *
 * The RT fork stores creature positions as continuous coordinates. These types
 * are the raw storage; the strongly typed wrappers (tripoint_abs_ms_f and
 * friends in coords_fwd.h) add the origin/scale checking that the integer
 * coordinate system already provides.
 *
 * Conventions
 * -----------
 * Tile @c n covers the half-open interval <tt>[n, n+1)</tt>, so the tile a
 * position belongs to is <tt>floor(pos)</tt> and the centre of tile @c n is
 * <tt>n + 0.5</tt>. Truncation must never be used for this: @c static_cast<int>
 * rounds toward zero, which puts -0.5 in tile 0 instead of tile -1.
 *
 * Why not rl_vec2d / rl_vec3d
 * ---------------------------
 * Those exist but are 32-bit @c float. Absolute map-square coordinates run to
 * six figures and beyond, where a float's 24-bit mantissa gives a step of about
 * 1/16 of a tile - visibly quantised movement, which is exactly what this fork
 * is removing. Hence @c double.
 *
 * Why z stays integral
 * --------------------
 * CDDA z-levels are discrete floors, not a continuous axis. Making z continuous
 * would mean real 3D physics and a rewrite of the map. Level changes stay
 * discrete events.
 */

struct point_f {
    static constexpr int dimension = 2;

    static const point_f zero;
    static const point_f min;
    static const point_f max;
    static const point_f invalid;

    double x = 0.0;
    double y = 0.0;

    constexpr point_f() = default;
    constexpr point_f( double X, double Y ) : x( X ), y( Y ) {}
    explicit constexpr point_f( const point &p ) : x( p.x ), y( p.y ) {}

    inline bool is_invalid() const {
        return *this == invalid;
    }

    constexpr point_f operator+( const point_f &rhs ) const {
        return point_f( x + rhs.x, y + rhs.y );
    }
    constexpr point_f operator-( const point_f &rhs ) const {
        return point_f( x - rhs.x, y - rhs.y );
    }
    constexpr point_f operator-() const {
        return point_f( -x, -y );
    }
    constexpr point_f operator*( const double rhs ) const {
        return point_f( x * rhs, y * rhs );
    }
    friend constexpr point_f operator*( double lhs, const point_f &rhs ) {
        return rhs * lhs;
    }
    constexpr point_f operator/( const double rhs ) const {
        return point_f( x / rhs, y / rhs );
    }
    point_f &operator+=( const point_f &rhs ) {
        x += rhs.x;
        y += rhs.y;
        return *this;
    }
    point_f &operator-=( const point_f &rhs ) {
        x -= rhs.x;
        y -= rhs.y;
        return *this;
    }
    point_f &operator*=( const double rhs ) {
        x *= rhs;
        y *= rhs;
        return *this;
    }

    /** Dummy implementation to match @ref point, so generic code can call it. */
    constexpr point_f raw() const {
        return *this;
    }

    point_f abs() const {
        return point_f( std::abs( x ), std::abs( y ) );
    }

    double magnitude() const {
        return std::sqrt( x * x + y * y );
    }
    double magnitude_squared() const {
        return x * x + y * y;
    }

    std::string to_string() const;
    std::string to_string_writable() const;

    friend std::ostream &operator<<( std::ostream &, const point_f & );

    friend inline constexpr bool operator==( const point_f &a, const point_f &b ) {
        return a.x == b.x && a.y == b.y;
    }
    friend inline constexpr bool operator!=( const point_f &a, const point_f &b ) {
        return !( a == b );
    }
    friend inline bool operator<( const point_f &a, const point_f &b ) {
        return a.x != b.x ? a.x < b.x : a.y < b.y;
    }
};

struct tripoint_f {
    static constexpr int dimension = 3;

    /** The 2D companion, used by the coord_point templates. */
    using subpoint_type = point_f;

    static const tripoint_f zero;
    static const tripoint_f min;
    static const tripoint_f max;
    static const tripoint_f invalid;

    double x = 0.0;
    double y = 0.0;
    int z = 0;

    constexpr tripoint_f() = default;
    constexpr tripoint_f( double X, double Y, int Z ) : x( X ), y( Y ), z( Z ) {}
    constexpr tripoint_f( const point_f &p, int Z ) : x( p.x ), y( p.y ), z( Z ) {}
    explicit constexpr tripoint_f( const tripoint &p ) : x( p.x ), y( p.y ), z( p.z ) {}

    inline bool is_invalid() const {
        return *this == invalid;
    }

    constexpr point_f xy() const {
        return point_f( x, y );
    }

    /** Dummy implementation to match @ref tripoint, so generic code can call it. */
    constexpr tripoint_f raw() const {
        return *this;
    }

    constexpr tripoint_f operator+( const tripoint_f &rhs ) const {
        return tripoint_f( x + rhs.x, y + rhs.y, z + rhs.z );
    }
    constexpr tripoint_f operator-( const tripoint_f &rhs ) const {
        return tripoint_f( x - rhs.x, y - rhs.y, z - rhs.z );
    }
    constexpr tripoint_f operator-() const {
        return tripoint_f( -x, -y, -z );
    }
    constexpr tripoint_f operator+( const point_f &rhs ) const {
        return tripoint_f( x + rhs.x, y + rhs.y, z );
    }
    constexpr tripoint_f operator-( const point_f &rhs ) const {
        return tripoint_f( x - rhs.x, y - rhs.y, z );
    }
    tripoint_f &operator+=( const tripoint_f &rhs ) {
        x += rhs.x;
        y += rhs.y;
        z += rhs.z;
        return *this;
    }
    tripoint_f &operator-=( const tripoint_f &rhs ) {
        x -= rhs.x;
        y -= rhs.y;
        z -= rhs.z;
        return *this;
    }
    tripoint_f &operator+=( const point_f &rhs ) {
        x += rhs.x;
        y += rhs.y;
        return *this;
    }

    tripoint_f abs() const {
        return tripoint_f( std::abs( x ), std::abs( y ), std::abs( z ) );
    }

    std::string to_string() const;
    std::string to_string_writable() const;

    friend std::ostream &operator<<( std::ostream &, const tripoint_f & );

    friend inline constexpr bool operator==( const tripoint_f &a, const tripoint_f &b ) {
        return a.x == b.x && a.y == b.y && a.z == b.z;
    }
    friend inline constexpr bool operator!=( const tripoint_f &a, const tripoint_f &b ) {
        return !( a == b );
    }
    friend inline bool operator<( const tripoint_f &a, const tripoint_f &b ) {
        if( a.x != b.x ) {
            return a.x < b.x;
        }
        if( a.y != b.y ) {
            return a.y < b.y;
        }
        return a.z < b.z;
    }
};

/**
 * Which tile a continuous position occupies.
 *
 * Uses floor, not truncation: tile n covers [n, n+1), so -0.5 belongs to tile -1.
 */
inline point to_tile( const point_f &p )
{
    return point( static_cast<int>( std::floor( p.x ) ),
                  static_cast<int>( std::floor( p.y ) ) );
}

inline tripoint to_tile( const tripoint_f &p )
{
    return tripoint( static_cast<int>( std::floor( p.x ) ),
                     static_cast<int>( std::floor( p.y ) ),
                     p.z );
}

/** Position within the occupied tile, each component in [0, 1). */
inline point_f tile_fraction( const point_f &p )
{
    return point_f( p.x - std::floor( p.x ), p.y - std::floor( p.y ) );
}

inline point_f tile_fraction( const tripoint_f &p )
{
    return point_f( p.x - std::floor( p.x ), p.y - std::floor( p.y ) );
}

/** The continuous position of the centre of a tile. */
inline point_f tile_centre( const point &p )
{
    return point_f( p.x + 0.5, p.y + 0.5 );
}

inline tripoint_f tile_centre( const tripoint &p )
{
    return tripoint_f( p.x + 0.5, p.y + 0.5, p.z );
}

/**
 * Strongly typed continuous coordinates.
 *
 * The integer coordinate system is already parameterised over the point type
 * (see coords_fwd.h), so the continuous variants slot into the same
 * origin/scale checking rather than bypassing it with a bare vector. Keeping
 * abs and bub apart matters more here than for integers: kinematics, collision
 * and combat all consume these, and mixing the two origins is the classic
 * coordinate bug this type system exists to prevent.
 *
 * Declared here rather than in coords_fwd.h so that only code that actually
 * needs continuous positions pays for the include.
 */
using point_abs_ms_f = coords::coord_point<point_f, coords::origin::abs, coords::ms>;
using point_bub_ms_f = coords::coord_point<point_f, coords::origin::reality_bubble, coords::ms>;
using tripoint_abs_ms_f = coords::coord_point<tripoint_f, coords::origin::abs, coords::ms>;
using tripoint_bub_ms_f =
    coords::coord_point<tripoint_f, coords::origin::reality_bubble, coords::ms>;

/** @copydoc to_tile(const tripoint_f &) */
template<coords::origin Origin>
inline coords::coord_point_ob<tripoint, Origin, coords::ms>
to_tile( const coords::coord_point_ob<tripoint_f, Origin, coords::ms> &p )
{
    return coords::coord_point_ob<tripoint, Origin, coords::ms>( to_tile( p.raw() ) );
}

/** @copydoc tile_centre(const tripoint &) */
template<coords::origin Origin>
inline coords::coord_point_ob<tripoint_f, Origin, coords::ms>
tile_centre( const coords::coord_point_ob<tripoint, Origin, coords::ms> &p )
{
    return coords::coord_point_ob<tripoint_f, Origin, coords::ms>( tile_centre( p.raw() ) );
}

/** @copydoc tile_fraction(const tripoint_f &) */
template<coords::origin Origin>
inline point_f tile_fraction( const coords::coord_point_ob<tripoint_f, Origin, coords::ms> &p )
{
    return tile_fraction( p.raw() );
}

/**
 * Offset of a continuous position from the centre of the tile it occupies,
 * each component in [-0.5, 0.5). This, not the raw fraction, is what a renderer
 * wants: a creature standing in the middle of its tile yields zero offset and
 * draws exactly where the integer-only renderer would put it.
 */
inline point_f offset_from_tile_centre( const tripoint_f &p )
{
    const point_f frac = tile_fraction( p );
    return point_f( frac.x - 0.5, frac.y - 0.5 );
}

template<coords::origin Origin>
inline point_f offset_from_tile_centre(
    const coords::coord_point_ob<tripoint_f, Origin, coords::ms> &p )
{
    return offset_from_tile_centre( p.raw() );
}

inline constexpr const point_f point_f::zero{};
inline constexpr const point_f point_f::min = { -HUGE_VAL, -HUGE_VAL };
inline constexpr const point_f point_f::max = { HUGE_VAL, HUGE_VAL };
inline constexpr const point_f point_f::invalid = point_f::min;

inline constexpr const tripoint_f tripoint_f::zero{};
inline constexpr const tripoint_f tripoint_f::min = { -HUGE_VAL, -HUGE_VAL, INT_MIN };
inline constexpr const tripoint_f tripoint_f::max = { HUGE_VAL, HUGE_VAL, INT_MAX };
inline constexpr const tripoint_f tripoint_f::invalid = tripoint_f::min;

namespace std
{
template <>
struct hash<point_f> {
    std::size_t operator()( const point_f &k ) const noexcept {
        const std::size_t hx = std::hash<double> {}( k.x );
        const std::size_t hy = std::hash<double> {}( k.y );
        return hx ^ ( hy << 1 );
    }
};

template <>
struct hash<tripoint_f> {
    std::size_t operator()( const tripoint_f &k ) const noexcept {
        const std::size_t hx = std::hash<double> {}( k.x );
        const std::size_t hy = std::hash<double> {}( k.y );
        const std::size_t hz = std::hash<int> {}( k.z );
        return hx ^ ( hy << 1 ) ^ ( hz << 2 );
    }
};
} // namespace std

#endif // CATA_SRC_POINT_FLOAT_H
