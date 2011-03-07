/*
 * map.template.hh
 *
 *  Created on: Feb 27, 2011
 *      Author: max
 */

#include "first.hh"

#include "assert.hh"     // for ff_assert macro */
#include "map.hh"        // for ft_map<T> */
#include "vector.hh"     // for ft_vector<T> */

FT_NAMESPACE_BEGIN

#define ff_map_assert ff_assert

// construct empty ft_map
template<typename T>
ft_map<T>::ft_map() : super_type() { }


// duplicate a ft_map, i.e. initialize this ft_map as a copy of other.
template<typename T>
ft_map<T>::ft_map(const ft_map<T> & other) : super_type(other)
{ }

// copy ft_map, i.e. set this ft_map contents as a copy of other's contents.
template<typename T>
const ft_map<T> & ft_map<T>::operator=(const ft_map<T> & other)
{
    super_type::operator=(other);
    return * this;
}

// destroy ft_map
template<typename T>
ft_map<T>::~ft_map()
{ }



/** compare two key+value extents and find relative position */
template<typename T>
ft_extent_relation ft_map<T>::compare(const_iterator pos1,
                                      const_iterator pos2)
{
    return compare(pos1->first, pos1->second, pos2->first, pos2->second);
}

/** compare two key+value extents and find relative position */
template<typename T>
ft_extent_relation ft_map<T>::compare(const_iterator pos1,
                                      const key_type & key2, const mapped_type & value2)
{
    return compare(pos1->first, pos1->second, key2, value2);
}

/** compare two key+value extents and find relative position */
template<typename T>
ft_extent_relation ft_map<T>::compare(const key_type & key1, const mapped_type & value1,
                                      const key_type & key2, const mapped_type & value2)
{
    T physical1 = key1.fm_physical, logical1 = value1.fm_logical, length1 = value1.fm_length;
    T physical2 = key2.fm_physical, logical2 = value2.fm_logical, length2 = value2.fm_length;
    ft_extent_relation rel;

    if (physical1 < physical2) {
        if (physical1 + length1 == physical2 && logical1 + length1 == logical2)
            rel = FC_EXTENT_TOUCH_BEFORE;
        else if (physical1 + length1 <= physical2)
            rel = FC_EXTENT_BEFORE;
        else
            rel = FC_EXTENT_INTERSECT;
    } else if (physical1 == physical2) {
        rel = FC_EXTENT_INTERSECT;
    } else /* physical1 > physical2 */ {
        if (physical2 + length2 == physical1 && logical2 + length2 == logical1)
            rel = FC_EXTENT_TOUCH_BEFORE;
        else if (physical2 + length2 <= physical1)
            rel = FC_EXTENT_BEFORE;
        else
            rel = FC_EXTENT_INTERSECT;
    }
    return rel;
}


/**
 * merge extent (which can belong to this ft_map) into specified position.
 * the two extents MUST exactly touch!
 * i.e. their ft_extent_relation MUST be either FC_EXTENT_TOUCH_BEFORE or FC_EXTENT_TOUCH_AFTER
 *
 * return iterator to merged position.
 *
 * WARNING: this is an internal method and should ONLY be invoked by merge(),
 *          as it does not handle chains of merges, as merge() does instead.
 *          Again: call merge(), not this method.
 */
template<typename T>
typename ft_map<T>::iterator ft_map<T>::merge0(iterator pos1, const key_type & key2, const mapped_type & value2)
{
    ft_extent_relation rel = compare(pos1, key2, value2);
    mapped_type & value1 = pos1->second;
    T length = value1.fm_length + value2.fm_length;

    if (rel == FC_EXTENT_TOUCH_BEFORE) {
        // modify extent in-place
        value1.fm_length = length;

    } else if (rel == FC_EXTENT_TOUCH_AFTER) {
        /*
         * we cannot modify std::map keys in-place!
         * so we need to erase pos1 and reinsert it with updated key
         *
         * implementation:
         * go one place forward before erasing, in worst case we will land into end()
         * which is still a valid iterator (ok, it cannot be dereferenced)
         * then erase original position,
         * finally reinsert merged extent, giving hint as where to insert it
         */
        iterator tmp = pos1;
        ++pos1;
        super_type::erase(tmp);

        mapped_type value_to_insert = { value2.fm_logical, length };

        pos1 = super_type::insert(pos1, std::make_pair(key2, value_to_insert));

    } else {
        /* must not happen! trigger an assertion failure. */
        ff_map_assert(rel == FC_EXTENT_TOUCH_BEFORE || rel == FC_EXTENT_TOUCH_AFTER);
    }
    return pos1;
}

/**
 * merge together two extents (which must belong to this ft_map).
 * the two extents MUST exactly touch!
 * i.e. their ft_extent_relation MUST be either FC_EXTENT_TOUCH_BEFORE or FC_EXTENT_TOUCH_AFTER
 *
 * return iterator to merged position.
 *
 * WARNING: this is an internal method and should ONLY be invoked by merge(),
 *          as it does not handle chains of merges, as merge() does instead.
 *          Again: call merge(), not this method.
 *
 * this method exists because it is simpler to implement than
 * merge0(iterator, const key_type &, const mapped_type &),
 * as it does not need to work around the limitation that std::map keys
 * cannot be modified in-place
 */
template<typename T>
typename ft_map<T>::iterator ft_map<T>::merge0(iterator pos1, iterator pos2)
{
    ft_extent_relation rel = compare(pos1, pos2);
    mapped_type & value1 = pos1->second;
    mapped_type & value2 = pos2->second;

    T length = value1.fm_length + value2.fm_length;

    if (rel == FC_EXTENT_TOUCH_BEFORE) {
        // modify first extent in-place, erase second
        value1.fm_length = length;
        super_type::erase(pos2);
        return pos1;

    } else if (rel == FC_EXTENT_TOUCH_AFTER) {
        // modify second extent in-place, erase first
        value2.fm_length = length;
        super_type::erase(pos1);
        return pos2;

    } else {
        /* must not happen! trigger an assertion failure. */
        ff_map_assert(rel == FC_EXTENT_TOUCH_BEFORE || rel == FC_EXTENT_TOUCH_AFTER);
    }
    return pos1;
}


/**
 * merge extent (which must NOT belong to this ft_map) into specified ft_map position.
 * the two extents MUST exactly touch!
 * i.e. their ft_extent_relation MUST be either FC_EXTENT_TOUCH_BEFORE or FC_EXTENT_TOUCH_AFTER
 *
 * return iterator to merged position.
 *
 * this method automatically performs chain of merges if needed:
 * for example, if extent 2 is merged in a ft_map containing 0..1 and 3..5,
 * this method will first merge 0..1 with 2, obtaining 0..2,
 * then it will realize that result can be merged with 3..5
 * and it will also perform this second merging, obtaining 0..5
 */
template<typename T>
typename ft_map<T>::iterator ft_map<T>::merge(iterator pos1, const key_type & key2, const mapped_type & value2)
{
    ft_extent_relation rel = compare(pos1, key2, value2);

    if (rel == FC_EXTENT_TOUCH_BEFORE) {
        pos1 = merge0(pos1, key2, value2);

        /* check for further possible merges! */
        if (pos1 != begin()) {
            iterator prev = pos1;
            --prev;
            if (compare(prev, pos1) == FC_EXTENT_TOUCH_BEFORE)
                pos1 = merge0(prev, pos1);
        }
    } else if (rel == FC_EXTENT_TOUCH_AFTER) {
        pos1 = merge0(pos1, key2, value2);

        /* check for further possible merges! */
        iterator next = pos1;
        ++next;
        if (next != end()) {
            if (compare(pos1, next) == FC_EXTENT_TOUCH_BEFORE)
                pos1 = merge0(pos1, next);
        }
    } else {
        /* must not happen! trigger an assertion failure. */
        ff_assert(rel == FC_EXTENT_TOUCH_BEFORE || rel == FC_EXTENT_TOUCH_AFTER);
    }
    return pos1;
}


/**
 * returns the minimum physical and the maximum physical+length in this map.
 * if this map is empty, return {0,0}
 */
template<typename T>
void ft_map<T>::bounds(key_type & min_key, key_type & max_key) const
{
    const_iterator b = begin(), e = end();
    if (b != e) {
        min_key.fm_physical = b->first.fm_physical;
        --e;
        max_key.fm_physical = e->first.fm_physical + e->second.fm_length;
    } else
        min_key.fm_physical = max_key.fm_physical = 0;
}


template<typename T>
static FT_INLINE T ff_min2(T a, T b)
{
    return a < b ? a : b;
}

template<typename T>
static FT_INLINE T ff_max2(T a, T b)
{
    return a > b ? a : b;
}

/**
 * find the intersection (matching physical and logical) between the two specified extents,
 * insert it into 'result' and return true.
 * if no intersections, return false and 'result' will be unchanged
 */
template<typename T>
bool ft_map<T>::intersect(const value_type & extent1, const value_type & extent2)
{
    const key_type & key1 = extent1.first;
    const mapped_type & value1 = extent1.second;

    T physical1 = key1.fm_physical;
    T logical1 = value1.fm_logical;
    T end1 = value1.fm_length + physical1;

    const key_type & key2 = extent2.first;
    const mapped_type & value2 = extent2.second;

    T physical2 = key2.fm_physical;
    T logical2 = value2.fm_logical;
    T end2 = value2.fm_length + physical1;

    if (end1 > physical2 && physical1 < end2
            && physical1 - physical2 == logical1 - logical2)
    {
        key_type key = { ff_max2(physical1, physical2) };
        mapped_type value = { ff_max2(logical1, logical2), ff_min2(end1, end2) - key.fm_physical };
        this->insert(key, value);
        return true;
    }
    return false;
}

/**
 * find the intersections (matching physical and logical) between specified map and extent.
 * insert list of intersections into this and return true.
 * if no intersections, return false and this will be unchanged
 */
template<typename T>
bool ft_map<T>::intersect_all(const ft_map<T> & map, const value_type & extent)
{
    const key_type & key1 = extent.first;
    const_iterator pos = map.super_type::upper_bound(key1), begin = map.begin(), end = map.end();
    bool ret = false;

    if (pos != begin) {
        --pos;
        /* pos is now last extent starting before key */
        ret |= intersect(*pos, extent);
        ++pos;
    }
    for (; pos != end; ++pos) {
        if (intersect(*pos, extent))
            ret = true;
        else
            break;
    }
    return ret;
}


/**
 * find the intersections (matching physical and logical) between specified map1 and map2.
 * insert list of intersections into this map and return true.
 * if no intersections, return false and this map will be unchanged
 */
template<typename T>
bool ft_map<T>::intersect_all_all(const ft_map<T> & map1, const ft_map<T> & map2)
{
    ft_size size1 = map1.size(), size2 = map2.size();
    if (size1 == 0 || size2 == 0)
        return false;

    const ft_map<T> & map_iterate = size1 < size2 ? map1 : map2;
    const ft_map<T> & map_other   = size1 < size2 ? map2 : map1;

    key_type bound_lo, bound_hi;
    map_other.bounds(bound_lo, bound_hi);

    const_iterator iter = map_iterate.super_type::upper_bound(bound_lo), end = map_iterate.super_type::lower_bound(bound_hi);
    if (iter != map_iterate.begin())
        /* iter is now last position less than bound_lo */
        --iter;

    bool ret = false;

    for (; iter != end; ++iter)
        ret |= intersect_all(map_other, *iter);
    return ret;
}

/**
 * add a single extent to the ft_map,
 * merging with existing extents where possible,
 */
template<typename T>
typename ft_map<T>::iterator ft_map<T>::insert(const key_type & key, const mapped_type & value)
{
    /*
     * pos = "next" extent, i.e.
     * first extent greater than or equal to this key,
     * or end() if no such extent exists
     */
    iterator pos = super_type::lower_bound(key);
    ft_extent_relation rel;

    if (pos != end()) {
        // check if extent to be added intersects or touches "next" extent
        rel = compare(pos, key, value);
        if (rel == FC_EXTENT_TOUCH_BEFORE || rel == FC_EXTENT_TOUCH_AFTER)
            return merge(pos, key, value);
    }
    if (pos != begin()) {
        // check if extent to be added intersects or touches "previous" extent
        --pos;
        rel = compare(pos, key, value);
        if (rel == FC_EXTENT_TOUCH_BEFORE || rel == FC_EXTENT_TOUCH_AFTER)
            return merge(pos, key, value);
        ++pos;
    }
    // just insert the key/value pair
    return super_type::insert(pos, std::make_pair(key, value));
}


/**
 * remove a part of an existing extent (or a whole existing extent)
 * from this ft_map, splitting the existing extent if needed.
 * throws an assertion failure if extent to remove is not part of existing extents.
 *
 * WARNING: does not support removing an extent that is part of TWO OR MORE existing extents.
 */
template<typename T>
void ft_map<T>::remove1(const value_type & extent)
{
    ff_assert(!empty());
    const key_type & key = extent.first;
    const mapped_type & value = extent.second;
    /*
     * pos = "next" extent, i.e. first extent greater than key to remove,
     * or end() if no such extent exists
     */
    iterator pos = super_type::upper_bound(key);
    ff_assert(pos != begin());
    /*
     * go back one place. pos will now be "prev",
     * i.e. the last extent lesser than or equal to key to remove
     */
    --pos;
    const key_type & last_key = pos->first;
    mapped_type & last_value_ = pos->second;

    T last_physical = last_key.fm_physical;
    T last_logical = last_value_.fm_logical;
    T last_length = last_value_.fm_length;

    T physical = key.fm_physical;
    T logical = value.fm_logical;
    T length = value.fm_length;

    ff_assert(last_physical <= physical);
    ff_assert(last_logical  <= logical);
    /* also logical to remove must match */
    ff_assert(physical - last_physical == logical - last_logical);
    /* last must finish together or after extent to remove */
    ff_assert(last_physical + last_length >= physical + length);

    /* let's consider extents start points */
    if (last_physical < physical) {
        /* first case:
         * "last" existing extent starts before extent to remove
         *    +------------
         *    | to_remove
         *  +-+------------
         *  | last extent
         *  +--------------
         */
        last_value_.fm_length = physical - last_physical;
    } else {
        /* second case:
         * "last" existing extent starts together with extent to remove
         *  +--------------
         *  | to_remove
         *  +--------------
         *  | last extent
         *  +--------------
         */
        super_type::erase(pos);
    }

    /* 2) let's consider extents end points */
    if (last_physical + last_length > physical + length) {
        /* if "last" existing extent ends after extent to remove
         * then we need to insert the remainder of last extent
         *  -----------+
         *   to_remove |
         *  -----------+-+
         *   last extent |
         *  -------------+
         */
        T new_physical = physical + length;
        T new_logical = logical + length;
        T new_length = last_physical + last_length - new_physical;
        insert0(new_physical, new_logical, new_length);
    } else {
        /* nothing to do */
    }
}


/**
 * remove a part of an existing extent (or one or more existing extents)
 * from this ft_map, splitting the existing extents if needed.
 */
template<typename T>
void ft_map<T>::remove(const value_type & extent)
{
    ft_map<T> intersect_list;
    intersect_list.intersect_all(*this, extent);
    const_iterator iter = intersect_list.begin(), end = intersect_list.end();
    for (; iter != end; ++iter)
        remove1(*iter);
}


/**
 * remove any (partial or full) intersection with existing extents from this ft_map,
 * splitting the existing extents if needed.
 */
template<typename T>
template<typename const_iter>
void ft_map<T>::remove_all(const_iter iter, const_iter end)
{
    for (; iter != end; ++iter)
        remove(*iter);
}



/**
 * remove any (partial or full) intersection with existing extents from this ft_map,
 * splitting the existing extents if needed.
 */
template<typename T>
void ft_map<T>::remove_all(const ft_map<T> & map)
{
    if (this == & map) {
        clear();
        return;
    }
    key_type bound_lo, bound_hi;
    bounds(bound_lo, bound_hi);
    const_iterator iter = map.super_type::upper_bound(bound_lo), end = map.super_type::lower_bound(bound_hi);
    if (iter != map.begin())
        --iter;
    remove_all(iter, end);
}

/**
 * add a single extent the ft_map
 *
 * WARNING: does not merge and does not check for merges
 */
template<typename T>
void ft_map<T>::insert0(T physical, T logical, T length)
{
    key_type key = { physical };
    mapped_type & value = (*this)[key];
    value.fm_logical = logical;
    value.fm_length = length;
}

/**
 * insert a single extent the ft_map, hinting that insertion is at map end
 *
 * WARNING: does not merge and does not check for merges
 */
template<typename T>
void ft_map<T>::append0(T physical, T logical, T length)
{
    key_type key = { physical };
    mapped_type value = { logical, length };
    value_type extent(key, value);

    super_type::insert(end(), extent);
}



/**
 * insert the whole other vector into this map,
 * shifting extents by effective_block_size_log2,
 * and hinting that insertion is at map end.
 * optimized assuming that 'other' is sorted by physical.
 *
 * WARNING: does not merge and does not check for merges
 * WARNING: does not check for overflows
 */
template<typename T>
void ft_map<T>::append0_shift(const ft_vector<ft_uoff> & other, ft_uoff effective_block_size_log2)
{
    typename ft_vector<ft_uoff>::const_iterator iter = other.begin(), end = other.end();
    for (; iter != end; ++iter) {
        const ft_extent<ft_uoff> & extent = * iter;
        append0(extent.physical() >> effective_block_size_log2,
                extent.logical()  >> effective_block_size_log2,
                extent.length()   >> effective_block_size_log2);
    }
}


/**
 * makes the physical complement of 'other' vector,
 * i.e. calculates the physical extents NOT used in 'other' vector,
 * shifts them by effective_block_size_log2,
 * and inserts it in this map.
 *
 * since the file(s) contained in such complementary extents are not known,
 * all calculated extents will have ->logical == ->physical.
 *
 * WARNING: 'other' must be already sorted by physical!
 * WARNING: does not merge and does not check for merges
 * WARNING: does not check for overflows
 */
template<typename T>
void ft_map<T>::complement0_physical_shift(const ft_vector<ft_uoff> & other,
                                  ft_uoff effective_block_size_log2, ft_uoff device_length)
{
    T physical, last;
    ft_size i, n = other.size();

    if (empty())
        last = 0;
    else {
        const value_type & back = *--this->end();
        last = back.first.fm_physical + back.second.fm_length;
    }
    /* loop on 'other' extents */
    for (i = 0; i < n; i++) {
        physical = other[i].physical() >> effective_block_size_log2;

        if (physical == last) {
            /* nothing to do */
        } else if (physical > last) {
            /* add "hole" with fm_logical == fm_physical */
            append0(last, last, physical - last);
        } else {
            /* oops.. some programmer really screwed up */
            ff_assert("internal error! somebody programmed a call to ft_map<T>::complement0_physical_shift() with an argument not sorted by ->physical() !" == 0);
        }

        last = physical + (other[i].length() >> effective_block_size_log2);
    }
    device_length >>= effective_block_size_log2;
    if (last < device_length) {
        /* add last "hole" with fm_logical == fm_physical */
        append0(last, last, device_length - last);
    }
}


/**
 * makes the logical complement of 'other' vector,
 * i.e. calculates the logical extents NOT used in 'other' vector,
 * shifts them by effective_block_size_log2,
 * and inserts it in this map.
 *
 * since the file(s) contained in such complementary extents are not known,
 * all calculated extents will have ->logical == ->physical.
 *
 * WARNING: 'other' must be already sorted by physical!
 * WARNING: does not merge and does not check for merges
 * WARNING: does not check for overflows
 */
template<typename T>
void ft_map<T>::complement0_logical_shift(const ft_vector<ft_uoff> & other, ft_uoff effective_block_size_log2, ft_uoff device_length)
{
    T logical, last;
    ft_size i, n = other.size();

    if (empty())
        last = 0;
    else {
        const mapped_type & back = (--this->end())->second;
        last = back.fm_logical + back.fm_length;
    }
    /* loop on 'other' extents */
    for (i = 0; i < n; i++) {
        logical = other[i].logical() >> effective_block_size_log2;

        if (logical == last) {
            /* nothing to do */
        } else if (logical > last) {
            /* add "hole" with fm_logical == fm_logical */
            append0(last, last, logical - last);
        } else {
            /* oops.. some programmer really screwed up */
            ff_assert("internal error! somebody programmed a call to ft_map<T>::complement0_logical_shift() with an argument not sorted by ->logical() !" == 0);
        }

        last = logical + (other[i].length() >> effective_block_size_log2);
    }
    device_length >>= effective_block_size_log2;
    if (last < device_length) {
        /* add last "hole" with fm_logical == fm_logical */
        append0(last, last, device_length - last);
    }
}

FT_NAMESPACE_END