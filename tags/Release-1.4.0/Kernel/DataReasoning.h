/* This file is part of the FaCT++ DL reasoner
Copyright (C) 2005-2009 by Dmitry Tsarkov

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


#ifndef _DATAREASONING_H
#define _DATAREASONING_H


#include <map>
#include <vector>

#include "tDataEntry.h"
#include "DataTypeComparator.h"
#include "ConceptWithDep.h"
#include "dlDag.h"
#include "logging.h"

class DataTypeAppearance
{
public:		// typedefs
	typedef std::pair<const TDataEntry*, DepSet> DepDTE;

protected:	// classes
		/// data interval with dep-sets
	class DepInterval
	{
	protected:	// members
			/// interval itself
		TDataInterval Constraints;
			/// dep-set for the min border of the interval
		DepSet minDep;
			/// dep-set for the max border of the interval
		DepSet maxDep;
	public:		// interface
			/// empty c'tor
		DepInterval ( void ) {}
			/// empty d'tor
		~DepInterval ( void ) {}

			/// update MIN border of an TYPE's interval with VALUE wrt EXCL
		bool update ( bool min, bool excl, const ComparableDT& value, const DepSet& dep )
		{
			if ( !Constraints.update ( min, excl, value ) )
				return false;
			if ( min )
				minDep = dep;
			else
				maxDep = dep;
			return true;
		}
			/// correct MIN and MAX operands of a type
		bool checkMinMaxClash ( DepSet& dep ) const;
			/// check if the interval is consistent wrt given type
		bool consistent ( const ComparableDT& type, DepSet& dep ) const
		{
			if ( Constraints.consistent(type) )
				return true;
			if ( Constraints.hasMin() )
				dep += minDep;
			if ( Constraints.hasMax() )
				dep += maxDep;
			return false;
		}
			/// clear the interval
		void clear ( void ) { Constraints.clear(); }
	}; // DepInterval

		/// datatype restriction is a set of intervals
	typedef std::vector<DepInterval> DTConstraint;
	typedef DTConstraint::iterator iterator;
	typedef DTConstraint::const_iterator const_iterator;

public:		// members
		/// positive type appearance
	DepDTE PType;
		/// negative type appearance
	DepDTE NType;

protected:	// members
		/// interval of possible values
	DTConstraint Constraints;
		/// accumulated dep-set
	DepSet accDep;
		/// dep-set for the clash
	DepSet& clashDep;

	// local values for the updating

		/// local value for the min/max flag
	bool localMin;
		/// local value for the incl/excl flag
	bool localExcl;
		/// local value for the added value
	ComparableDT localValue;
		/// local dep-set for the update
	DepSet localDep;

protected:	// methods
		/// set clash dep-set to DEP, report with given REASON; @return true to simplify callers
	bool reportClash ( const DepSet& dep, const char* reason )
	{
		if ( LLM.isWritable(llCDAction) )	// level of logging
			LL << " DT-" << reason;	// inform about clash...

		clashDep = dep;
		return true;
	}
		/// set the local parameters for updating
	void setLocal ( bool min, bool excl, const ComparableDT& value, const DepSet& dep )
	{
		localMin = min;
		localExcl = excl;
		localValue = value;
		localDep = dep;
	}
		/// update and add a single interval I to the constraints. @return true iff clash occurs
	bool addUpdatedInterval ( DepInterval i )
	{
		if ( !i.consistent ( localValue, localDep ) )	// types are mismatch
			return reportClash ( localDep, "C-IT" );
		if ( !i.update ( localMin, localExcl, localValue, localDep ) )
			Constraints.push_back(i);
		if ( !hasPType() || !i.checkMinMaxClash(accDep) )
			Constraints.push_back(i);
		return false;
	}
		/// update and add all the intervals from the given range. @return true iff clash occurs
	bool addIntervals ( iterator begin, iterator end )
	{
		for ( ; begin != end; ++begin )
			if ( addUpdatedInterval(*begin) )
				return true;
		return false;
	}
		/// add interval INT positively to the DTA
	bool addPosInterval ( const TDataInterval& Int, const DepSet& dep );
		/// add interval INT negatively to the DTA
	bool addNegInterval ( const TDataInterval& Int, const DepSet& dep );

public:		// methods
		/// empty c'tor
	DataTypeAppearance ( DepSet& dep ) : clashDep(dep) {}
		/// empty d'tor
	~DataTypeAppearance ( void ) {}

		/// clear the appearance flags
	void clear ( void )
	{
		PType = NType =
			std::make_pair(static_cast<const TDataEntry*>(NULL),DepSet());
		Constraints.clear();
		Constraints.push_back(DepInterval());
		accDep.clear();
	}

	// presence interface

		/// check if type is present positively in the node
	bool hasPType ( void ) const { return PType.first != NULL; }
		/// check if type is present negatively in the node
	bool hasNType ( void ) const { return NType.first != NULL; }
		/// set the precense of the PType
	void setPType ( const DepDTE& type )
	{
		if ( !hasPType() )
			PType = type;
	}
		/// set the precense of the PType
	void setPType ( const TDataEntry* value, const DepSet& dep )
	{
		if ( !hasPType() )
			PType = std::make_pair(value->getType(),dep);
	}

	// complex methods

		/// add restrictions [POS]INT to intervals
	bool addInterval ( bool pos, const TDataInterval& Int, const DepSet& dep )
	{
		if ( LLM.isWritable(llCDAction) )	// level of logging
			LL << ' ' << (pos ? '+' : '-') << Int;
		return pos ? addPosInterval ( Int, dep ) : addNegInterval ( Int, dep );
	}

		/// @return true iff PType and NType leads to clash
	bool checkPNTypeClash ( void )
	{
		if ( hasNType() )
			return reportClash ( PType.second+NType.second, "TNT" );

		return false;
	}
}; // DataTypeAppearance

class DataTypeReasoner
{
protected:	// types
		/// vector of data types
	typedef std::vector<DataTypeAppearance*> DTAVector;
		/// map from positive BPs (DT pNames) to corresponding data type
	typedef std::map<const TDataEntry*,size_t> TypeMap;
		/// complex DTE type copied from DTA
	typedef DataTypeAppearance::DepDTE DepDTE;

protected:	// members
		/// vector of a types
	DTAVector Types;
		/// map Type.pName->Type appearance
	TypeMap Map;
		/// external DAG
	const DLDag& DLHeap;
		/// dep-set for the clash for *all* the types
	DepSet clashDep;

protected:	// methods
		/// process data value
	bool processDataValue ( bool pos, const TDataEntry* c, const DepSet& dep )
	{
		DataTypeAppearance* type = getDTAbyValue(c);

		if (pos)
			type->setPType(DepDTE(c,dep));

		// create interval [c,c]
		TDataInterval constraints;
		constraints.updateMin ( /*excl=*/false, c->getComp() );
		constraints.updateMax ( /*excl=*/false, c->getComp() );
		return type->addInterval ( pos, constraints, dep );
	}
		/// process data expr
	bool processDataExpr ( bool pos, const TDataEntry* c, const DepSet& dep )
	{
		const TDataInterval& constraints = *c->getFacet();
		if ( constraints.empty() )
			return false;
		DataTypeAppearance* type = getDTAbyValue(c);
		if (pos)
			type->setPType(DepDTE(c,dep));
		return type->addInterval ( pos, constraints, dep );
	}

		/// get data entry structure by a BP
	const TDataEntry* getDataEntry ( BipolarPointer p ) const
		{ return static_cast<const TDataEntry*>(DLHeap[p].getConcept()); }
		/// get TDE with a dep-set by a CWD
	DepDTE getDTE ( BipolarPointer p, const DepSet& dep ) const
		{ return DepDTE(getDataEntry(p),dep); }

	// get access to proper DataTypeAppearance

		/// get DTA by given data-type pointer
	DataTypeAppearance* getDTAbyType ( const TDataEntry* dataType )
	{
#	ifdef ENABLE_CHECKING
		fpp_assert ( Map.find(dataType) != Map.end() );
#	endif
		return Types[Map[dataType]];
	}
		/// get DTA by given data-value pointer
	DataTypeAppearance* getDTAbyValue ( const TDataEntry* dataValue )
	{
#	ifdef ENABLE_CHECKING
		fpp_assert ( !dataValue->isBasicDataType() );
#	endif
		return getDTAbyType(dataValue->getType());
	}

public:		// interface
		/// c'tor: save DAG
	DataTypeReasoner ( const DLDag& dag ) : DLHeap(dag) {}
		/// empty d'tor
	~DataTypeReasoner ( void )
	{
		for ( DTAVector::iterator p = Types.begin(); p < Types.end(); ++p )
			delete *p;
	}

	// managing DTR

		/// add data type to the reasoner
	void registerDataType ( const TDataEntry* p )
	{
		Map[p] = Types.size();
		Types.push_back(new DataTypeAppearance(clashDep));
	}
		/// prepare types for the reasoning
	void clear ( void )
	{
		for ( DTAVector::iterator p = Types.begin(), p_end = Types.end(); p < p_end; ++p )
			(*p)->clear();
	}

	// filling structures and getting answers

		/// add data entry to the DTAVector; @return true iff data-data clash was found
	bool addDataEntry ( BipolarPointer p, const DepSet& dep );
		/// @return true iff data inconsistency was found in a structure; ClashSet would be set approprietry
	bool checkClash ( void );
		/// get clash-set
	const DepSet& getClashSet ( void ) const { return clashDep; }
}; // DataTypeReasoner


#endif
