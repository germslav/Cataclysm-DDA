#pragma once
#ifndef CATA_SRC_MONSTER_SPATIAL_INDEX_H
#define CATA_SRC_MONSTER_SPATIAL_INDEX_H

#include <cstddef>
#include <unordered_map>
#include <vector>

#include "coordinates.h"
#include "memory_fast.h"
#include "point.h"
#include "point_float.h"

class monster;

/**
 * Where the monsters are, indexed by position.
 *
 * RT fork: replaces the plain tile -> monster map the tracker used to hold. The
 * tile lookup it has to keep serving is only one of the questions a continuous
 * simulation asks; the others - who is within a radius of this point, who might
 * this projectile hit, who is close enough to matter this tick - are answered by
 * scanning a neighbourhood, and a map keyed by exact tile can only answer them by
 * probing every tile in it one at a time.
 *
 * Monsters are therefore filed into cells of @ref cell_size tiles square, one
 * bucket per cell per z-level. A tile lookup hashes the cell and scans its
 * bucket; a radius query visits the few cells the circle covers and tests the
 * candidates exactly.
 *
 * The one-monster-per-tile rule is not this class's business. It is a rule about
 * what the game permits, enforced by creature_tracker before it calls @ref
 * insert, and this index would hold two monsters on one tile perfectly happily
 * the day that rule goes.
 *
 * Positions are filed, not observed: an entry stays where it was put until
 * something moves it, exactly as the map's keys did. A monster whose position
 * changed without the tracker being told is findable at the tile it was filed
 * under and not at the one it occupies - the same staleness the tracker has
 * always had, and the same recovery, @ref erase_anywhere.
 */
class monster_spatial_index
{
    public:
        /**
         * Tiles per cell, per axis. Small enough that a bucket stays a handful of
         * entries in a crowd, large enough that a collision-sized query touches
         * four cells rather than dozens. A starting point to be tuned against
         * measurements, not a number with anything behind it yet.
         */
        static constexpr int cell_shift = 2;
        static constexpr int cell_size = 1 << cell_shift;

        /**
         * The monster filed at this tile, or nullptr. Says nothing about whether
         * it is alive or a hallucination; that is the caller's question.
         */
        const shared_ptr_fast<monster> *find( const tripoint_abs_ms &pos ) const;

        /** Files a monster at a tile, replacing whatever was filed there. */
        void insert( const tripoint_abs_ms &pos, const shared_ptr_fast<monster> &mon );

        /** Unfiles whatever is at this tile. */
        void erase( const tripoint_abs_ms &pos );

        /**
         * Unfiles this monster if it is the one filed at this tile.
         * @return whether it was.
         */
        bool erase( const tripoint_abs_ms &pos, const monster &critter );

        /**
         * Unfiles this monster from wherever it happens to be filed, scanning the
         * whole index. The recovery path for an entry that went stale.
         * @return whether it was found.
         */
        bool erase_anywhere( const monster &critter );

        void clear();

        /** Number of filed monsters. */
        size_t size() const;

        /**
         * The monsters within @p radius of @p centre, on its z-level.
         *
         * Distance is measured between continuous positions - the monster's own
         * @c pos_abs_f, not the tile it is filed under - so this is a real circle,
         * not a square of tiles. Creature size does not enter into it yet: this
         * asks which monsters are near a point, not which ones a body of a given
         * radius overlaps.
         *
         * The order is unspecified. It is reproducible for a given sequence of
         * insertions and removals, which is what the determinism requirement
         * needs, but it is not sorted by distance or by anything else.
         */
        std::vector<monster *> overlapping( const tripoint_abs_ms_f &centre, double radius ) const;

    private:
        struct entry {
            /** The tile this monster is filed under, which is its cell. */
            tripoint_abs_ms pos;
            shared_ptr_fast<monster> mon;
        };

        static tripoint cell_of( const tripoint_abs_ms &pos );

        std::unordered_map<tripoint, std::vector<entry>> cells_;
        size_t size_ = 0;
};

#endif // CATA_SRC_MONSTER_SPATIAL_INDEX_H
