#include "point_float.h"

#include <locale>
#include <ostream>
#include <sstream>
#include <string>

std::ostream &operator<<( std::ostream &os, const point_f &pos )
{
    return os << "(" << pos.x << "," << pos.y << ")";
}

std::ostream &operator<<( std::ostream &os, const tripoint_f &pos )
{
    return os << "(" << pos.x << "," << pos.y << "," << pos.z << ")";
}

std::string point_f::to_string() const
{
    std::ostringstream os;
    os.imbue( std::locale::classic() );
    os << *this;
    return os.str();
}

std::string point_f::to_string_writable() const
{
    // Matches point::to_string_writable: the readable form is already
    // locale-independent, so there is nothing to vary.
    return to_string();
}

std::string tripoint_f::to_string() const
{
    std::ostringstream os;
    os.imbue( std::locale::classic() );
    os << *this;
    return os.str();
}

std::string tripoint_f::to_string_writable() const
{
    return to_string();
}

// Force instantiation of the members the continuous coordinate wrappers are
// expected to provide. Without a use site the templates stay uninstantiated and
// a mismatch would only surface later, in whichever file first touched them.
namespace
{
void instantiation_check()
{
    const tripoint_abs_ms_f abs_pos( 3.25, -4.75, 2 );
    const tripoint_bub_ms_f bub_pos( 0.5, 0.5, 0 );

    static_cast<void>( abs_pos.x() );
    static_cast<void>( abs_pos.y() );
    static_cast<void>( abs_pos.z() );
    static_cast<void>( abs_pos.raw() );
    static_cast<void>( abs_pos == abs_pos );
    static_cast<void>( bub_pos.raw() );
    static_cast<void>( to_tile( abs_pos.raw() ) );
    static_cast<void>( tile_fraction( abs_pos.raw() ) );
}
} // namespace
