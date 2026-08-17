#include "monster_spatial_index.h"

#include <utility>

#include "monster.h"

tripoint monster_spatial_index::cell_of( const tripoint_abs_ms &pos )
{
    // Floor division, not a shift. Absolute map-square coordinates go negative
    // west and north of the world origin, and rounding those toward zero would
    // fold the cells either side of each axis into one.
    return tripoint( divide_round_to_minus_infinity( pos.x(), cell_size ),
                     divide_round_to_minus_infinity( pos.y(), cell_size ),
                     pos.z() );
}

const shared_ptr_fast<monster> *monster_spatial_index::find( const tripoint_abs_ms &pos ) const
{
    const auto cell = cells_.find( cell_of( pos ) );
    if( cell == cells_.end() ) {
        return nullptr;
    }
    for( const entry &e : cell->second ) {
        if( e.pos == pos ) {
            return &e.mon;
        }
    }
    return nullptr;
}

void monster_spatial_index::insert( const tripoint_abs_ms &pos,
                                    const shared_ptr_fast<monster> &mon )
{
    std::vector<entry> &bucket = cells_[cell_of( pos )];
    for( entry &e : bucket ) {
        if( e.pos == pos ) {
            // Replacing rather than refusing: the map this index replaced assigned
            // through operator[], and the checks that make a tile refuse a second
            // monster live in creature_tracker, above this.
            e.mon = mon;
            return;
        }
    }
    bucket.push_back( entry{ pos, mon } );
    ++size_;
}

void monster_spatial_index::erase( const tripoint_abs_ms &pos )
{
    const auto cell = cells_.find( cell_of( pos ) );
    if( cell == cells_.end() ) {
        return;
    }
    std::vector<entry> &bucket = cell->second;
    for( auto it = bucket.begin(); it != bucket.end(); ++it ) {
        if( it->pos == pos ) {
            *it = std::move( bucket.back() );
            bucket.pop_back();
            --size_;
            if( bucket.empty() ) {
                cells_.erase( cell );
            }
            return;
        }
    }
}

bool monster_spatial_index::erase( const tripoint_abs_ms &pos, const monster &critter )
{
    const shared_ptr_fast<monster> *filed = find( pos );
    if( filed == nullptr || filed->get() != &critter ) {
        return false;
    }
    erase( pos );
    return true;
}

bool monster_spatial_index::erase_anywhere( const monster &critter )
{
    for( auto cell = cells_.begin(); cell != cells_.end(); ++cell ) {
        std::vector<entry> &bucket = cell->second;
        for( auto it = bucket.begin(); it != bucket.end(); ++it ) {
            if( it->mon.get() != &critter ) {
                continue;
            }
            *it = std::move( bucket.back() );
            bucket.pop_back();
            --size_;
            if( bucket.empty() ) {
                cells_.erase( cell );
            }
            return true;
        }
    }
    return false;
}

void monster_spatial_index::clear()
{
    cells_.clear();
    size_ = 0;
}

size_t monster_spatial_index::size() const
{
    return size_;
}

std::vector<monster *> monster_spatial_index::overlapping( const tripoint_abs_ms_f &centre,
        const double radius ) const
{
    std::vector<monster *> result;
    if( radius < 0.0 ) {
        return result;
    }

    // Broad phase: every cell the circle's bounding box touches. Narrow phase:
    // the exact distance, below, so the corners of those cells cost nothing but
    // a test each.
    const tripoint_abs_ms low = to_tile( tripoint_abs_ms_f( centre.x() - radius,
                                         centre.y() - radius, centre.z() ) );
    const tripoint_abs_ms high = to_tile( tripoint_abs_ms_f( centre.x() + radius,
                                          centre.y() + radius, centre.z() ) );
    const tripoint first = cell_of( low );
    const tripoint last = cell_of( high );

    const double radius_squared = radius * radius;
    for( int cy = first.y; cy <= last.y; ++cy ) {
        for( int cx = first.x; cx <= last.x; ++cx ) {
            const auto cell = cells_.find( tripoint( cx, cy, centre.z() ) );
            if( cell == cells_.end() ) {
                continue;
            }
            for( const entry &e : cell->second ) {
                const tripoint_abs_ms_f &pos = e.mon->pos_abs_f();
                const double dx = pos.x() - centre.x();
                const double dy = pos.y() - centre.y();
                if( dx * dx + dy * dy <= radius_squared ) {
                    result.push_back( e.mon.get() );
                }
            }
        }
    }
    return result;
}
