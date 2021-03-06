/* This file is part of the FaCT++ DL reasoner
Copyright (C) 2003-2010 by Dmitry Tsarkov

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

#include "DIGParserHandlers.h"

#include <xercesc/sax/AttributeList.hpp>

#include "strx.h"
#include "globaldef.h"

using namespace xercesc;

// ---------------------------------------------------------------------------
// Implementation of a parser in details
// ---------------------------------------------------------------------------

inline void writeConcept ( ostream& o, const char* name )
{
	o << "<catom name=\"" << name << "\"/>";
}
inline void writeRole ( ostream& o, const char* name )
{
	o << "<ratom name=\"" << name << "\"/>";
}
inline void writeIndividual ( ostream& o, const char* name )
{
	o << "<individual name=\"" << name << "\"/>";
}

// Actor for Concept hierarchy
class ConceptActor
{
protected:
	std::ostream& o;
	closedXMLEntry* syn;
	closedXMLEntry* pEntry;

		/// process single entry in a vertex label
	bool tryEntry ( const ClassifiableEntry* p )
	{
		// check the applicability
		if ( p->isSystem() || static_cast<const TConcept*>(p)->isSingleton() )
			return false;

		// set the context
		if ( syn == NULL )
			syn = new closedXMLEntry ( "synonyms", o );

		// print the concept
		o << "\n  ";
		const std::string name(p->getName());

		if ( p->getId () >= 0 )
			writeConcept ( o, name.c_str() );
		else if ( name == std::string("TOP") )
			simpleXMLEntry top ( "top", o );
		else if ( name == std::string("BOTTOM") )
			simpleXMLEntry bottom ( "bottom", o );
		else	// error
			return false;

		return true;
	}

public:
	ConceptActor ( std::ostream& oo, const char* id )
		: o(oo)
		, syn(NULL)
		, pEntry ( new closedXMLEntry ( "conceptSet", oo, id ) )
		{}
	~ConceptActor ( void ) { delete pEntry; }

	bool apply ( const TaxonomyVertex& v )
	{
		tryEntry(v.getPrimer());

		for ( TaxonomyVertex::syn_iterator p = v.begin_syn(), p_end=v.end_syn(); p != p_end; ++p )
			tryEntry(*p);

		if ( syn )
		{
			delete syn;
			syn = NULL;
			return true;
		}
		else
			return false;
	}
}; // ConceptActor

// Actor for Individual "hierarchy"
class IndividualActor
{
protected:
	std::ostream& o;
	closedXMLEntry* pEntry;

		/// process single entry in a vertex label
	bool tryEntry ( const ClassifiableEntry* p )
	{
		// check the applicability
		if ( p->isSystem() || !static_cast<const TConcept*>(p)->isSingleton() )
			return false;

		// print the concept
		o << "\n  <individual name=\"" << p->getName() << "\"/>";
		return true;
	}

public:
	IndividualActor ( std::ostream& oo, const char* id )
		: o(oo)
		, pEntry ( new closedXMLEntry ( "individualSet", oo, id ) )
		{}
	~IndividualActor ( void ) { delete pEntry; }

	bool apply ( const TaxonomyVertex& v )
	{
		bool ret = tryEntry(v.getPrimer());

		for ( TaxonomyVertex::syn_iterator p = v.begin_syn(), p_end=v.end_syn(); p != p_end; ++p )
			ret |= tryEntry(*p);

		return ret;
	}
}; // IndividualActor

// Actor for Role hierarchy for DIG 1.1
class RoleActor
{
protected:
	std::ostream& o;
	closedXMLEntry* syn;
	closedXMLEntry* pEntry;

		/// process single entry in a vertex label
	bool tryEntry ( const ClassifiableEntry* p )
	{
		// check the applicability: system and inverse roles are useless
		if ( p->isSystem() || p->getId() <= 0 )
			return false;

		// set the context
		if ( syn == NULL )
			syn = new closedXMLEntry ( "synonyms", o );

		// print the role
		o << "\n  <ratom name=\"" << p->getName() << "\"/>";
		return true;
	}

public:
	RoleActor ( std::ostream& oo, const char* id )
		: o(oo)
		, syn(NULL)
		, pEntry ( new closedXMLEntry ( "roleSet", oo, id ) )
		{}
	~RoleActor ( void ) { delete pEntry; }

	bool apply ( const TaxonomyVertex& v )
	{
		tryEntry(v.getPrimer());

		for ( TaxonomyVertex::syn_iterator p = v.begin_syn(), p_end=v.end_syn(); p != p_end; ++p )
			tryEntry(*p);

		if ( syn )
		{
			delete syn;
			syn = NULL;
			return true;
		}
		else
			return false;
	}
}; // RoleActor

/// start of ask element (allNames, satisfy)
void DIGParseHandlers :: startAsk ( DIGTag tag, AttributeList& attributes )
{
	fpp_assert ( tag >= dig_Ask_Begin && tag < dig_Ask_End );	// safety check

	// set up id of the ask
	const XMLCh* parm = attributes.getValue ( "id" );

	if ( parm == NULL )
		throwAttributeAbsence ( "id", tag );

	// create current id string
	curId = " id=\"";
	curId += StrX (parm).localForm ();
	curId += "\"";
}


/// end of ask element (allNames, satisfy)
void DIGParseHandlers :: endAsk ( DIGTag tag )
{
	fpp_assert ( tag >= dig_Ask_Begin && tag < dig_Ask_End );	// safety check

#define ERROR_400														\
	do {																\
		string Reason ( "General 'ask' error\"" );						\
		Reason += curId;												\
		*Reason.rbegin() = '\0';										\
		outError ( 400, Reason.c_str(), "undefined names in query" );	\
		return;															\
	} while(0)

#define ERROR_401											\
	do {													\
		string Reason ( "Unsupported 'ask' command\"" );	\
		Reason += curId;									\
		*Reason.rbegin() = '\0';							\
		outError ( 401, Reason.c_str(), "" );				\
	} while(0)

// FIXME!! make more detailed error notification in the future
#define ASK_QUERY(Action)		\
	do { try { Action; }		\
	catch ( EFaCTPlusPlus )		\
		{ fail = true; }		\
	} while(0)

	bool fail = false;

	switch (tag)
	{
	case digAllConceptNames:	// all
	{
		ConceptActor actor ( *o, curId.c_str() );
		ASK_QUERY(pKernel->getAllConcepts(actor));
		return;
	}
	case digAllRoleNames:
	{
		RoleActor actor ( *o, curId.c_str() );
		ASK_QUERY(pKernel->getAllORoles(actor) ; pKernel->getAllDRoles(actor));
		return;
	}
	case digAllIndividuals:
	{
		IndividualActor actor ( *o, curId.c_str() );
		ASK_QUERY(pKernel->getAllIndividuals(actor));
		return;
	}
	case digSatisfiable:	// sat
	case digSubsumes:
	case digInstance:		// the same as subsumes
	case digDisjointQuery:
	{
		bool ret = false;

		if ( workStack.empty() )
			throwArgumentAbsence ( "concept", tag );
		TConceptExpr* q = dynamic_cast<TConceptExpr*>(workStack.top());
		workStack.pop();

		if ( wasError )
			fail = true;
		else if ( tag == digSatisfiable )
			ASK_QUERY ( ret = pKernel->isSatisfiable(q) ) ;
		else
		{
			if ( workStack.empty() )
				throwArgumentAbsence ( "concept", tag );
			TExpr* p = workStack.top();
			workStack.pop();

			if ( tag == digSubsumes )
				ASK_QUERY ( ret = pKernel->isSubsumedBy ( q, dynamic_cast<TConceptExpr*>(p) ) );
			else if ( tag == digInstance )
				ASK_QUERY ( ret = pKernel->isInstance ( dynamic_cast<TIndividualExpr*>(p), q ) );
			else	// ( name == "disjoint" )
				ASK_QUERY ( ret = pKernel->isDisjoint ( dynamic_cast<TConceptExpr*>(p), q ) );
		}

		simpleXMLEntry ( fail?"error":(ret?"true":"false"), *o, curId.c_str() );

		return;
	}
	case digCParents:			// concept hierarchy
	case digCChildren:
	case digCAncestors:
	case digCDescendants:
	{
		if ( workStack.empty() )
			throwArgumentAbsence ( "concept", tag );

		TConceptExpr* p = dynamic_cast<TConceptExpr*>(workStack.top());
		workStack.pop();
		ConceptActor actor ( *o, curId.c_str() );

		// if were any errors during concept expression construction
		if ( wasError )
			fail = true;
		else if ( tag == digCParents )
			ASK_QUERY ( pKernel->getParents ( p, actor ) );
		else if ( tag == digCChildren )
			ASK_QUERY ( pKernel->getChildren ( p, actor ) );
		else if ( tag == digCAncestors )
			ASK_QUERY ( pKernel->getAncestors ( p, actor ) );
		else if ( tag == digCDescendants )
			ASK_QUERY ( pKernel->getDescendants ( p, actor ) );

		if ( fail )	// error
			ERROR_400;

		return;
	}
	case digTypes:
	{
		if ( workStack.empty() )
			throwArgumentAbsence ( "individual", tag );

		TIndividualExpr* p = dynamic_cast<TIndividualExpr*>(workStack.top());
		workStack.pop();
		ConceptActor actor ( *o, curId.c_str() );

		// if were any errors during concept expression construction
		if ( wasError )
			fail = true;
		else
			ASK_QUERY ( pKernel->getDirectTypes ( p, actor ) );

		if ( fail )	// error
			ERROR_400;

		return;
	}
	case digCEquivalents:
	{
		if ( workStack.empty() )
			throwArgumentAbsence ( "concept expression", tag );
		TConceptExpr* p = dynamic_cast<TConceptExpr*>(workStack.top());
		workStack.pop();
		ConceptActor actor ( *o, curId.c_str() );

		if ( wasError )
			fail = true;
		else
			ASK_QUERY ( pKernel->getEquivalents ( p, actor ) );

		if ( fail )	// error
			ERROR_400;

		return;
	}
	case digRParents:			// role hierarchy
	case digRChildren:
	case digRAncestors:
	case digRDescendants:
	{
		if ( workStack.empty() )
			throwArgumentAbsence ( "role", tag );

		TRoleExpr* p = dynamic_cast<TRoleExpr*>(workStack.top());
		workStack.pop();
		RoleActor actor ( *o, curId.c_str() );

		// if were any errors during concept expression construction
		if ( wasError )
			fail = true;
		else if ( tag == digRParents )
			ASK_QUERY ( pKernel->getRParents ( p, actor ) );
		else if ( tag == digRChildren )
			ASK_QUERY ( pKernel->getRChildren ( p, actor ) );
		else if ( tag == digRAncestors )
			ASK_QUERY ( pKernel->getRAncestors ( p, actor ) );
		else if ( tag == digRDescendants )
			ASK_QUERY ( pKernel->getRDescendants ( p, actor ) );

		if ( fail )	// error
			ERROR_400;

		return;
	}
	case digREquivalents:
	{
		if ( workStack.empty() )
			throwArgumentAbsence ( "role", tag );
		TRoleExpr* p = dynamic_cast<TRoleExpr*>(workStack.top());
		workStack.pop();
		RoleActor actor ( *o, curId.c_str() );

		if ( wasError )
			fail = true;
		else
			ASK_QUERY ( pKernel->getREquivalents ( p, actor ) );

		if ( fail )	// error
			ERROR_400;

		return;
	}

	case digInstances:			// individual queries
	{
		if ( workStack.empty() )
			throwArgumentAbsence ( "concept expression", tag );
		TConceptExpr* p = dynamic_cast<TConceptExpr*>(workStack.top());
		workStack.pop();
		IndividualActor actor ( *o, curId.c_str() );

		// to find instances just locate all descendants and remove non-nominals
		if ( wasError )
			fail = true;
		else
			ASK_QUERY ( pKernel->getInstances ( p, actor ) );

		if ( fail )
			ERROR_400;

		return;
	}
	case digRoleFillers:
	{
		if ( workStack.empty() )
			throwArgumentAbsence ( "role", tag );
		TORoleExpr* R = dynamic_cast<TORoleExpr*>(workStack.top());
		workStack.pop();
		if ( workStack.empty() )
			throwArgumentAbsence ( "individual", tag );
		TIndividualExpr* I = dynamic_cast<TIndividualExpr*>(workStack.top());
		workStack.pop();
		ReasoningKernel::IndividualSet Js;

		// if were any errors during concept expression construction
		if ( wasError )
			fail = true;
		else
			ASK_QUERY ( pKernel->getRoleFillers ( I, R, Js ) );

		if ( fail )
			ERROR_400;

		// output individual set
		closedXMLEntry x ( "individualSet", *o, curId.c_str() );
		for ( ReasoningKernel::IndividualSet::const_iterator
			  p = Js.begin(), p_end = Js.end(); p < p_end; ++p )
			writeIndividual ( *o, (*p)->getName() );

		return;
	}
	case digRelatedIndividuals:
	{
		if ( workStack.empty() )
			throwArgumentAbsence ( "role", tag );
		TORoleExpr* R = dynamic_cast<TORoleExpr*>(workStack.top());
		workStack.pop();
		ReasoningKernel::IndividualSet Is, Js;

		// if were any errors during concept expression construction
		if ( wasError )
			fail = true;
		else
			ASK_QUERY ( pKernel->getRelatedIndividuals ( R, Is, Js ) );

		if ( fail )
			ERROR_400;

		// output individual set
		closedXMLEntry x ( "individualPairSet", *o, curId.c_str() );
		for ( ReasoningKernel::IndividualSet::const_iterator
			  p = Is.begin(), p_end = Is.end(), q = Js.begin(); p < p_end; ++p, ++q )
		{
			*o << "\n  <individualPair>";
			writeIndividual ( *o, (*p)->getName() );
			writeIndividual ( *o, (*q)->getName() );
			*o << "</individualPair>";
		}

		return;
	}
	case digToldValue:
		 ERROR_401;		// unsupported
		 return;

	case digAllAbsorbedPrimitiveConceptDefinitions:	// get all PConcept's definitions
	{
		closedXMLEntry x ( "absorbedPrimitiveConceptDefinitions", *o, curId.c_str() );
		pKernel->absorbedPrimitiveConceptDefinitions(*o);
		return;
	}
	case digUnabsorbableGCI:
	{
		closedXMLEntry x ( "unabsorbed", *o, curId.c_str() );
		pKernel->unabsorbed(*o);
		return;
	}

	default:
		fpp_unreachable();	// safety check
	}

	*o << "\n";
#undef ERROR_400
#undef ERROR_401
}

void DIGParseHandlers :: outputSupportedLanguage ( void )
{
	// write down supported syntax
	closedXMLEntry supp ( "supports", *o );

	// language part
	{
		*o << "\n";
		closedXMLEntry lang ( "language", *o );

		simpleXMLEntry ( "top", *o );
		simpleXMLEntry ( "bottom", *o );
		simpleXMLEntry ( "catom", *o );

		simpleXMLEntry ( "and", *o );
		simpleXMLEntry ( "or", *o );
		simpleXMLEntry ( "not", *o );

		simpleXMLEntry ( "some", *o );
		simpleXMLEntry ( "all", *o );
		simpleXMLEntry ( "atmost", *o );
		simpleXMLEntry ( "atleast", *o );
		simpleXMLEntry ( "iset", *o );

		simpleXMLEntry ( "defined", *o );
		simpleXMLEntry ( "stringmin", *o );
		simpleXMLEntry ( "stringmax", *o );
		simpleXMLEntry ( "stringequals", *o );
		simpleXMLEntry ( "stringrange", *o );
		simpleXMLEntry ( "intmin", *o );
		simpleXMLEntry ( "intmax", *o );
		simpleXMLEntry ( "intequals", *o );
		simpleXMLEntry ( "intrange", *o );

		simpleXMLEntry ( "ratom", *o );
		simpleXMLEntry ( "feature", *o );
		simpleXMLEntry ( "inverse", *o );
		simpleXMLEntry ( "attribute", *o );
//		simpleXMLEntry ( "chain", *o );

		simpleXMLEntry ( "individual", *o );
	}

	// tell part
	{
		*o << "\n";
		closedXMLEntry tell ( "tell", *o );

		simpleXMLEntry ( "defconcept", *o );
		simpleXMLEntry ( "defrole", *o );
		simpleXMLEntry ( "deffeature", *o );
		simpleXMLEntry ( "defattribute", *o );
		simpleXMLEntry ( "defindividual", *o );

		simpleXMLEntry ( "impliesc", *o );
		simpleXMLEntry ( "equalc", *o );
		simpleXMLEntry ( "disjoint", *o );

		simpleXMLEntry ( "impliesr", *o );
		simpleXMLEntry ( "equalr", *o );
		simpleXMLEntry ( "domain", *o );
		simpleXMLEntry ( "range", *o );
		simpleXMLEntry ( "rangeint", *o );
		simpleXMLEntry ( "rangestring", *o );
		simpleXMLEntry ( "transitive", *o );
		simpleXMLEntry ( "functional", *o );

		simpleXMLEntry ( "instanceof", *o );
		simpleXMLEntry ( "related", *o );
		simpleXMLEntry ( "value", *o );
	}

	// ask part
	{
		*o << "\n";
		closedXMLEntry ask ( "ask", *o );

		simpleXMLEntry ( "allConceptNames", *o );
		simpleXMLEntry ( "allRoleNames", *o );
		simpleXMLEntry ( "allIndividuals", *o );

		simpleXMLEntry ( "satisfiable", *o );
		simpleXMLEntry ( "subsumes", *o );
		simpleXMLEntry ( "disjoint", *o );

		simpleXMLEntry ( "parents", *o );
		simpleXMLEntry ( "children", *o );
		simpleXMLEntry ( "ancestors", *o );
		simpleXMLEntry ( "descendants", *o );
		simpleXMLEntry ( "equivalents", *o );

		simpleXMLEntry ( "rparents", *o );
		simpleXMLEntry ( "rchildren", *o );
		simpleXMLEntry ( "rancestors", *o );
		simpleXMLEntry ( "rdescendants", *o );

		simpleXMLEntry ( "instances", *o );
		simpleXMLEntry ( "types", *o );
		simpleXMLEntry ( "instance", *o );
		simpleXMLEntry ( "roleFillers", *o );
		simpleXMLEntry ( "relatedIndividuals", *o );
//		simpleXMLEntry ( "toldValues", *o );
	}
	*o << "\n";
}
