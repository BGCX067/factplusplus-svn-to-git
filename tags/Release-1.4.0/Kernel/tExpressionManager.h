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

#ifndef TEXPRESSIONMANAGER_H
#define TEXPRESSIONMANAGER_H

#include "tDLExpression.h"
#include "tNameSet.h"
#include "tNAryQueue.h"
#include "tDataTypeManager.h"

inline bool
isUniversalRole ( const TDLObjectRoleExpression* R )
{
	return dynamic_cast<const TDLObjectRoleTop*>(R) != NULL;
}

inline bool
isUniversalRole ( const TDLDataRoleExpression* R )
{
	return dynamic_cast<const TDLDataRoleTop*>(R) != NULL;
}

inline TDLDataTypeName* getBasicDataType ( TDLDataTypeExpression* type )
{
	TDLDataTypeName* ret = dynamic_cast<TDLDataTypeName*>(type);
	if ( ret == NULL )
	{
		TDLDataTypeRestriction* hostType = dynamic_cast<TDLDataTypeRestriction*>(type);
		fpp_assert ( hostType != NULL );
		ret = const_cast<TDLDataTypeName*>(hostType->getExpr());
	}
	return ret;
}

/// manager to work with all DL expressions in the kernel
class TExpressionManager
{
protected:	// members
		/// nameset for concepts
	TNameSet<TDLConceptName> NS_C;
		/// nameset for individuals
	TNameSet<TDLIndividualName> NS_I;
		/// nameset for object roles
	TNameSet<TDLObjectRoleName> NS_OR;
		/// nameset for data roles
	TNameSet<TDLDataRoleName> NS_DR;
		/// nameset for data types
	TDataTypeManager NS_DT;

		/// n-ary queue for arguments
	TNAryQueue<const TDLExpression> ArgQueue;

		/// TOP concept
	TDLConceptTop* CTop;
		/// BOTTOM concept
	TDLConceptBottom* CBottom;
		/// TOP object role
	TDLObjectRoleTop* ORTop;
		/// BOTTOM object role
	TDLObjectRoleBottom* ORBottom;
		/// TOP data role
	TDLDataRoleTop* DRTop;
		/// BOTTOM data role
	TDLDataRoleBottom* DRBottom;
		/// TOP data element
	TDLDataTop* DTop;
		/// BOTTOM data element
	TDLDataBottom* DBottom;

		/// record all the references
	std::vector<TDLExpression*> RefRecorder;

protected:	// methods
		/// record the reference; @return the argument
	template<class T>
	T* record ( T* arg ) { RefRecorder.push_back(arg); return arg; }

public:		// interface
		/// empty c'tor
	TExpressionManager ( void );
		/// d'tor
	~TExpressionManager ( void );

		/// clear the ontology
	void clear ( void );

	// argument lists

		/// opens new argument list
	void newArgList ( void ) { ArgQueue.openArgList(); }
		/// add argument ARG to the current argument list
	void addArg ( const TDLExpression* arg ) { ArgQueue.addArg(arg); }
		/// get the latest argument list
	const std::vector<const TDLExpression*>& getArgList ( void ) { return ArgQueue.getLastArgList(); }

	// create expressions methods

	// concepts

		/// get TOP concept
	TDLConceptExpression* Top ( void ) const { return CTop; }
		/// get BOTTOM concept
	TDLConceptExpression* Bottom ( void ) const { return CBottom; }
		/// get named concept
	TDLConceptExpression* Concept ( const std::string& name ) { return NS_C.insert(name); }
		/// get negation of a concept C
	TDLConceptExpression* Not ( const TDLConceptExpression* C ) { return record(new TDLConceptNot(C)); }
		/// get an n-ary conjunction expression; take the arguments from the last argument list
	TDLConceptExpression* And ( void ) { return record(new TDLConceptAnd(getArgList())); }
		/// get an n-ary disjunction expression; take the arguments from the last argument list
	TDLConceptExpression* Or ( void ) { return record(new TDLConceptOr(getArgList())); }
		/// get an n-ary one-of expression; take the arguments from the last argument list
	TDLConceptExpression* OneOf ( void ) { return record(new TDLConceptOneOf(getArgList())); }
		/// @return concept {I} for the individual I
	TDLConceptExpression* OneOf ( const TDLIndividualExpression* I ) { newArgList(); addArg(I); return OneOf(); }

		/// get self-reference restriction of an object role R
	TDLConceptExpression* SelfReference ( const TDLObjectRoleExpression* R ) { return record(new TDLConceptObjectSelf(R)); }
		/// get value restriction wrt an object role R and an individual I
	TDLConceptExpression* Value ( const TDLObjectRoleExpression* R, const TDLIndividualExpression* I )
		{ return record(new TDLConceptObjectValue(R,I)); }
		/// get existential restriction wrt an object role R and a concept C
	TDLConceptExpression* Exists ( const TDLObjectRoleExpression* R, const TDLConceptExpression* C )
		{ return record(new TDLConceptObjectExists(R,C)); }
		/// get universal restriction wrt an object role R and a concept C
	TDLConceptExpression* Forall ( const TDLObjectRoleExpression* R, const TDLConceptExpression* C )
		{ return record(new TDLConceptObjectForall(R,C)); }
		/// get min cardinality restriction wrt number N, an object role R and a concept C
	TDLConceptExpression* MinCardinality ( unsigned int n, const TDLObjectRoleExpression* R, const TDLConceptExpression* C )
		{ return record(new TDLConceptObjectMinCardinality(n,R,C)); }
		/// get max cardinality restriction wrt number N, an object role R and a concept C
	TDLConceptExpression* MaxCardinality ( unsigned int n, const TDLObjectRoleExpression* R, const TDLConceptExpression* C )
		{ return record(new TDLConceptObjectMaxCardinality(n,R,C)); }
		/// get exact cardinality restriction wrt number N, an object role R and a concept C
	TDLConceptExpression* Cardinality ( unsigned int n, const TDLObjectRoleExpression* R, const TDLConceptExpression* C )
		{ return record(new TDLConceptObjectExactCardinality(n,R,C)); }

		/// get value restriction wrt a data role R and a data value V
	TDLConceptExpression* Value ( const TDLDataRoleExpression* R, const TDLDataValue* V )
		{ return record(new TDLConceptDataValue(R,V)); }
		/// get existential restriction wrt a data role R and a data expression E
	TDLConceptExpression* Exists ( const TDLDataRoleExpression* R, const TDLDataExpression* E )
		{ return record(new TDLConceptDataExists(R,E)); }
		/// get universal restriction wrt a data role R and a data expression E
	TDLConceptExpression* Forall ( const TDLDataRoleExpression* R, const TDLDataExpression* E )
		{ return record(new TDLConceptDataForall(R,E)); }
		/// get min cardinality restriction wrt number N, a data role R and a data expression E
	TDLConceptExpression* MinCardinality ( unsigned int n, const TDLDataRoleExpression* R, const TDLDataExpression* E )
		{ return record(new TDLConceptDataMinCardinality(n,R,E)); }
		/// get max cardinality restriction wrt number N, a data role R and a data expression E
	TDLConceptExpression* MaxCardinality ( unsigned int n, const TDLDataRoleExpression* R, const TDLDataExpression* E )
		{ return record(new TDLConceptDataMaxCardinality(n,R,E)); }
		/// get exact cardinality restriction wrt number N, a data role R and a data expression E
	TDLConceptExpression* Cardinality ( unsigned int n, const TDLDataRoleExpression* R, const TDLDataExpression* E )
		{ return record(new TDLConceptDataExactCardinality(n,R,E)); }

	// individuals

		/// get named individual
	TDLIndividualExpression* Individual ( const std::string& name ) { return NS_I.insert(name); }

	// object roles

		/// get TOP object role
	TDLObjectRoleExpression* ObjectRoleTop ( void ) const { return ORTop; }
		/// get BOTTOM object role
	TDLObjectRoleExpression* ObjectRoleBottom ( void ) const { return ORBottom; }
		/// get named object role
	TDLObjectRoleExpression* ObjectRole ( const std::string& name ) { return NS_OR.insert(name); }
		/// get an inverse of a given object role expression R
	TDLObjectRoleExpression* Inverse ( const TDLObjectRoleExpression* R ) { return record(new TDLObjectRoleInverse(R)); }
		/// get a role chain corresponding to R1 o ... o Rn; take the arguments from the last argument list
	TDLObjectRoleComplexExpression* Compose ( void ) { return record(new TDLObjectRoleChain(getArgList())); }
		/// get a expression corresponding to R projected from C
	TDLObjectRoleComplexExpression* ProjectFrom ( const TDLObjectRoleExpression* R, const TDLConceptExpression* C )
		{ return record(new TDLObjectRoleProjectionFrom(R,C)); }
		/// get a expression corresponding to R projected into C
	TDLObjectRoleComplexExpression* ProjectInto ( const TDLObjectRoleExpression* R, const TDLConceptExpression* C )
		{ return record(new TDLObjectRoleProjectionInto(R,C)); }

	// data roles

		/// get TOP data role
	TDLDataRoleExpression* DataRoleTop ( void ) const { return DRTop; }
		/// get BOTTOM data role
	TDLDataRoleExpression* DataRoleBottom ( void ) const { return DRBottom; }
		/// get named data role
	TDLDataRoleExpression* DataRole ( const std::string& name ) { return NS_DR.insert(name); }

	// data expressions

		/// get TOP data element
	TDLDataExpression* DataTop ( void ) const { return DTop; }
		/// get BOTTOM data element
	TDLDataExpression* DataBottom ( void ) const { return DBottom; }

		/// get named data type
	TDLDataTypeName* DataType ( const std::string& name ) { return NS_DT.insert(name); }
		/// get basic string data type
	TDLDataTypeName* getStrDataType ( void ) { return DataType(TDataTypeManager::getStrTypeName()); }
		/// get basic integer data type
	TDLDataTypeName* getIntDataType ( void ) { return DataType(TDataTypeManager::getIntTypeName()); }
		/// get basic floating point data type
	TDLDataTypeName* getRealDataType ( void ) { return DataType(TDataTypeManager::getRealTypeName()); }
		/// get basic boolean data type
	TDLDataTypeName* getBoolDataType ( void ) { return DataType(TDataTypeManager::getBoolTypeName()); }

		/// get basic boolean data type
	TDLDataTypeRestriction* RestrictedType ( TDLDataTypeExpression* type, const TDLFacetExpression* facet )
	{
		TDLDataTypeRestriction* ret = dynamic_cast<TDLDataTypeRestriction*>(type);
		if ( ret == NULL )
		{	// get a type and build an appropriate restriction of it
			TDLDataTypeName* hostType = dynamic_cast<TDLDataTypeName*>(type);
			fpp_assert ( hostType != NULL );
			ret = record(new TDLDataTypeRestriction(hostType));
		}
		ret->add(facet);
		return ret;
	}

		/// get data value with given VALUE and TYPE;
		// FIXME!! now change the type to the basic type of the given one
		// That is, value of a type positiveInteger will be of a type Integer
	const TDLDataValue* DataValue ( const std::string& value, TDLDataTypeExpression* type ) { return getBasicDataType(type)->getValue(value); }
		/// get negation of a data expression E
	TDLDataExpression* DataNot ( const TDLDataExpression* E ) { return record(new TDLDataNot(E)); }
		/// get an n-ary data conjunction expression; take the arguments from the last argument list
	TDLDataExpression* DataAnd ( void ) { return record(new TDLDataAnd(getArgList())); }
		/// get an n-ary data disjunction expression; take the arguments from the last argument list
	TDLDataExpression* DataOr ( void ) { return record(new TDLDataOr(getArgList())); }
		/// get an n-ary data one-of expression; take the arguments from the last argument list
	TDLDataExpression* DataOneOf ( void ) { return record(new TDLDataOneOf(getArgList())); }

		/// get minInclusive facet with a given VALUE
	const TDLFacetExpression* FacetMinInclusive ( const TDLDataValue* V ) { return record(new TDLFacetMinInclusive(V)); }
		/// get minExclusive facet with a given VALUE
	const TDLFacetExpression* FacetMinExclusive ( const TDLDataValue* V ) { return record(new TDLFacetMinExclusive(V)); }
		/// get maxInclusive facet with a given VALUE
	const TDLFacetExpression* FacetMaxInclusive ( const TDLDataValue* V ) { return record(new TDLFacetMaxInclusive(V)); }
		/// get maxExclusive facet with a given VALUE
	const TDLFacetExpression* FacetMaxExclusive ( const TDLDataValue* V ) { return record(new TDLFacetMaxExclusive(V)); }

}; // TExpressionManager

#endif
