/* This file is part of the FaCT++ DL reasoner
Copyright (C) 2010 by Dmitry Tsarkov

This library is free software; you can redistribute it and/or
modify it under the terms of the GNU Lesser General Public
License as published by the Free Software Foundation; either
version 2.1 of the License, or (at your option) any later version.

This library is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
Lesser General Public License for more details.

You should have received a copy of the GNU Lesser General Public
License along with this library; if not, write to the Free Software
Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
*/

#ifndef THEADTAILCACHE_H
#define THEADTAILCACHE_H

/// Template class for the cache element. Assumes that new elements of a HEADTYPE
/// are constructed using a single argument of a TAILTYPE. Uniqueness of a tails
/// leads to the uniqueness of a constructed object
template<class HeadType, class TailType>
class THeadTailCache
{
protected:	// types
		/// auxiliary map
	typedef std::map<const TailType*,HeadType*> CacheMap;
		/// map RW iterator
	typedef typename CacheMap::iterator iterator;
		/// map RO iterator
	typedef typename CacheMap::const_iterator const_iterator;

protected:	// members
		/// map tail into an object head(tail)
	CacheMap Map;

protected:	// methods
		/// the way to create an object by a given tail
	virtual HeadType* build ( TailType* ) = 0;

public:		// interface
		/// empty c'tor
	THeadTailCache ( void ) {}
		/// d'tor: delete all cached elements
	virtual ~THeadTailCache ( void )
	{
		// don't delete tails as they are referenced outside
		for ( iterator p = Map.begin(), p_end = Map.end(); p != p_end; ++p )
			delete p->second;
	}

		/// get an object corresponding to Head.Tail
	HeadType* get ( TailType* tail )
	{
		// try to find cached dep-set
		const_iterator p = Map.find(tail);
		if ( p != Map.end() )
			return p->second;

		// no cached entry -- create a new one and cache it
		HeadType* concat = build(tail);
		Map[tail] = concat;
		return concat;
	}
}; // THeadTailCache


#endif
