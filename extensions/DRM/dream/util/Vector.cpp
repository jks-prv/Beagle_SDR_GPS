#include "Vector.h"

template<typename TData>
bool operator==(const CVector<TData>& lhs, const CVector<TData>& rhs)
{
    return lhs.data == rhs.data;
}
