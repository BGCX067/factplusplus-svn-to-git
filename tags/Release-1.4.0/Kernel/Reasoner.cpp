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

#include "Reasoner.h"
#include "logging.h"
#include "eFPPTimeout.h"

// comment the line out for printing tree info before save and after restoring
//#define __DEBUG_SAVE_RESTORE
// comment the line out for flushing LL after dumping significant piece of info
//#define __DEBUG_FLUSH_LL
// uncomment the following line to debug cascaded caching
//#define TMP_CACHE_DEBUG

#ifdef USE_REASONING_STATISTICS
AccumulatedStatistic* AccumulatedStatistic::root = NULL;
#endif

DlSatTester :: DlSatTester ( TBox& tbox, const ifOptionSet* Options )
	: tBox(tbox)
	, DLHeap(tbox.DLHeap)
	, Manager(64)
	, CGraph(1,this)
	, DTReasoner(tbox.DLHeap)
	, GCIs(tbox.GCIs)
	, testTimeout(0)
	, bContext(NULL)
	, tryLevel(InitBranchingLevelValue)
	, nonDetShift(0)
	, curNode(NULL)
	, dagSize(0)
{
	// init local options
	readConfig ( Options );

	// in presence of fairness constraints use ancestor blocking
	if ( tBox.hasFC() && useAnywhereBlocking )
	{
		useAnywhereBlocking = false;
		if ( LLM.isWritable(llAlways) )
			LL << "Fairness constraints: set useAnywhereBlocking = false\n";
	}

	// init static part of CTree
	CGraph.initContext ( useLazyBlocking, useAnywhereBlocking );
	// init datatype reasoner
	tBox.getDataTypeCenter().initDataTypeReasoner(DTReasoner);
	// init set of reflexive roles
	tbox.getORM()->fillReflexiveRoles(ReflexiveRoles);
	GCIs.setReflexive(!ReflexiveRoles.empty());
	// init blocking statistics
	clearBlockingStat();

	resetSessionFlags ();
}

// load init values from config file
void DlSatTester :: readConfig ( const ifOptionSet* Options )
{
	fpp_assert ( Options != NULL );	// safety check

// define a macro for registering boolean option
#	define addBoolOption(name)				\
	name = Options->getBool ( #name );		\
	if ( LLM.isWritable(llAlways) )			\
		LL << "Init " #name " = " << name << "\n"
	addBoolOption(useSemanticBranching);
	addBoolOption(useBackjumping);
	addBoolOption(useLazyBlocking);
	addBoolOption(useAnywhereBlocking);
#undef addBoolOption
}

/// prerpare Nominal Reasoner to a new job
void
DlSatTester :: prepareReasoner ( void )
{
	if ( hasNominals() )
	{
		if ( LLM.isWritable(llSRState) )
			LL << "\nInitNominalReasoner:";

		restore(1);

		// check whether branching op is not a barrier...
		if ( dynamic_cast<BCBarrier*>(bContext) == NULL )
		{	// replace it with a barrier
			Stack.pop();
			createBCBarrier();
		}
		// save the barrier (also remember the entry to be produced)
		save();
		// free the memory used in the pools before
		Stack.clearPools();
	}
	else
	{
		CGraph.clear();
		Stack.clear();
		TODO.clear();
		pUsed.clear();
		nUsed.clear();

		curNode = NULL;
		bContext = NULL;
		tryLevel = InitBranchingLevelValue;
	}

	// clear last session information
	resetSessionFlags();
}

addConceptResult
DlSatTester :: checkAddedConcept ( const CWDArray& lab, BipolarPointer p, const DepSet& dep )
{
#ifdef ENABLE_CHECKING
	fpp_assert ( isCorrect(p) );	// sanity checking
	// constants are not allowed here
	fpp_assert ( p != bpTOP );
	fpp_assert ( p != bpBOTTOM );
#endif

	incStat(nLookups);
	incStat(nLookups);

	const BipolarPointer inv_p = inverse(p);

	for ( const_label_iterator i = lab.begin(), i_end = lab.end(); i < i_end; ++i )
	{
		if ( i->bp() == p )
			return acrExist;
		else if ( i->bp() == inv_p )
		{
			// create clashSet
			clashSet = dep + i->getDep();
			return acrClash;
		}
	}

	// we are able to insert a concept
	return acrDone;
}

bool
DlSatTester :: findConcept ( const CWDArray& lab, BipolarPointer p )
{
#ifdef ENABLE_CHECKING
	fpp_assert ( isCorrect(p) );	// sanity checking
	// constants are not allowed here
	fpp_assert ( p != bpTOP );
	fpp_assert ( p != bpBOTTOM );
#endif

	incStat(nLookups);

	return std::find ( lab.begin(), lab.end(), p ) != lab.end();
}

bool
DlSatTester :: findConcept ( const CWDArray& lab, BipolarPointer p, const DepSet& dep )
{
#ifdef ENABLE_CHECKING
	fpp_assert ( isCorrect(p) );	// sanity checking
	// constants are not allowed here
	fpp_assert ( p != bpTOP );
	fpp_assert ( p != bpBOTTOM );
#endif

	incStat(nLookups);

	for ( const_label_iterator i = lab.begin(), i_end = lab.end(); i < i_end; ++i )
	if ( i->bp() == p )
	{
		// create clashSet
		clashSet = dep + i->getDep();
		return true;
	}

	// we are able to insert a concept
	return false;
}

addConceptResult
DlSatTester :: tryAddConcept ( const CWDArray& lab, BipolarPointer c, const DepSet& dep )
{
	// check whether C or ~C can occurs in a node label
	bool canC = isUsed(c);
	bool canNegC = isUsed(inverse(c));

	// if either C or ~C is used already, it's not new in a label
	if ( canC )
	{
		if ( canNegC )	// both C and ~C can be in the label
			return checkAddedConcept(lab,c,dep);
		else			// C but not ~C can be in the label
			return findConcept(lab,c) ? acrExist : acrDone;
	}
	else
	{
		if ( canNegC )	// ~C but not C can be in the label
			return findConcept(lab,inverse(c),dep) ? acrClash : acrDone;
		else			// neither C nor ~C can be in the label
			return acrDone;
	}
}

tacticUsage DlSatTester :: addToDoEntry ( DlCompletionTree* n, BipolarPointer c, const DepSet& dep,
										  const char* reason )
{
	if ( c == bpTOP )	// simplest things first
		return utUnusable;
	if ( c == bpBOTTOM )
	{
		setClashSet(dep);
		if ( LLM.isWritable(llGTA) )
			logClash ( n, c, dep );
		return utClash;
	}

	const DLVertex& v = DLHeap[c];
	DagTag tag = v.Type();

	// collections shouldn't appear in the node labels
	if ( tag == dtCollection )
	{
		if ( isNegative(c) )	// nothing to do
			return utUnusable;
		// setup and run and()
		incStat(nTacticCalls);		// to balance nAndCalls later
		DlCompletionTree* oldNode = curNode;
		ConceptWDep oldConcept = curConcept;
		curNode = n;
		curConcept = ConceptWDep(c,dep);
		tacticUsage ret = commonTacticBodyAnd(v);
		curNode = oldNode;
		curConcept = oldConcept;
		return ret;
	}

	// try to add a concept to a node label
	switch ( tryAddConcept ( n->label().getLabel(tag), c, dep ) )
	{
	case acrClash:	// clash -- return
		if ( LLM.isWritable(llGTA) )
			logClash ( n, c, dep );
		return utClash;
	case acrExist:	// already exists -- nothing new
		return utUnusable;
	case acrDone:	// try was done
		return insertToDoEntry ( n, c, dep, tag, reason );
	default:	// safety check
		fpp_unreachable();
	}
}

/// insert P to the label of N; do necessary updates; may return Clash in case of data node N
tacticUsage DlSatTester :: insertToDoEntry ( DlCompletionTree* n, BipolarPointer c, const DepSet& dep,
											 DagTag tag, const char* reason = NULL )
{
	// we will need this tag for TODO entry anyway
	ConceptWDep p ( c, dep );

	// we will change current Node => save it if necessary
	updateLevel ( n, dep );
	CGraph.addConceptToNode ( n, p, tag );

	setUsed(c);

	if ( n->isCached() )
	{
		tacticUsage ret = correctCachedEntry(n);
		return ret == utUnusable ? utDone : ret;
	}

	// add new info in TODO list
	TODO.addEntry ( n, c, tag );

	if ( n->isDataNode() )	// data concept -- run data center for it
		return checkDataNode ? checkDataClash(n) : utUnusable;
	else if ( LLM.isWritable(llGTA) )	// inform about it
		logEntry ( n, c, dep, reason );

	return utDone;
}

//-----------------------------------------------------------------------------
//--		internal cache support
//-----------------------------------------------------------------------------

bool
DlSatTester :: canBeCached ( DlCompletionTree* node )
{
	DlCompletionTree::const_label_iterator p;
	bool shallow = true;
	unsigned int size = 0;

	// nominal nodes can not be cached
	if ( node->isNominalNode() )
		return false;

	incStat(nCacheTry);

	// check applicability of the caching
	for ( p = node->beginl_sc(); p != node->endl_sc(); ++p )
	{
		if ( DLHeap.getCache(p->bp()) == NULL )
		{
			incStat(nCacheFailedNoCache);
			if ( LLM.isWritable(llGTA) )
				LL << " cf(" << p->bp() << ")";
			return false;
		}

		shallow &= DLHeap.getCache(p->bp())->shallowCache();
		++size;
	}

	for ( p = node->beginl_cc(); p != node->endl_cc(); ++p )
	{
		if ( DLHeap.getCache(p->bp()) == NULL )
		{
			incStat(nCacheFailedNoCache);
			if ( LLM.isWritable(llGTA) )
				LL << " cf(" << p->bp() << ")";
			return false;
		}

		shallow &= DLHeap.getCache(p->bp())->shallowCache();
		++size;
	}

	// it's useless to cache shallow nodes
	if ( shallow && size != 0 )
	{
		incStat(nCacheFailedShallow);
		if ( LLM.isWritable(llGTA) )
			LL << " cf(s)";
		return false;
	}

	return true;
}

/// perform caching of the node (it is known that caching is possible)
modelCacheIan*
DlSatTester :: doCacheNode ( DlCompletionTree* node )
{
	DlCompletionTree::const_label_iterator p;

	// It's unsafe to have a cache that touchs nominal here; set flagNominals to prevent it
	modelCacheIan* cache = new modelCacheIan(true);
	DepSet dep;

	for ( p = node->beginl_sc(); p != node->endl_sc(); ++p )
	{
		dep.add(p->getDep());
		// try to merge cache of a node label element with accumulator
		switch ( cache->merge(DLHeap.getCache(p->bp())) )
		{
		case csValid:	// continue
			break;
		case csInvalid:	// clash: set the clash-set
			setClashSet(dep);
		// fall through
		default:		// caching of node fails
			return cache;
		}
	}

	for ( p = node->beginl_cc(); p != node->endl_cc(); ++p )
	{
		dep.add(p->getDep());
		// try to merge cache of a node label element with accumulator
		switch ( cache->merge(DLHeap.getCache(p->bp())) )
		{
		case csValid:	// continue
			break;
		case csInvalid:	// clash: set the clash-set
			setClashSet(dep);
		// fall through
		default:		// caching of node fails
			return cache;
		}
	}

	// all concepts in label are mergable; now try to add input arc
	if ( node->hasParent() )
	{
		modelCacheIan cachePar(false);
		cachePar.initRolesFromArcs(node);	// the only arc is parent

		cache->merge(&cachePar);
	}

	return cache;
}

tacticUsage
DlSatTester :: reportNodeCached ( modelCacheIan* cache, DlCompletionTree* node )
{
	enum modelCacheState status = cache->getState();
	delete cache;
	switch ( status )
	{
	case csValid:
		incStat(nCachedSat);
		if ( LLM.isWritable(llGTA) )
			LL << " cached(" << node->getId() << ")";
		return utDone;
	case csInvalid:
		incStat(nCachedUnsat);
		return utClash;
	case csFailed:
	case csUnknown:
		incStat(nCacheFailed);
		if ( LLM.isWritable(llGTA) )
			LL << " cf(c)";
		return utUnusable;
	default:
		fpp_unreachable();
	}
}

tacticUsage DlSatTester :: correctCachedEntry ( DlCompletionTree* n )
{
	fpp_assert ( n->isCached() );	// safety check

	// FIXME!! check if it is possible to leave node cached in more efficient way
	tacticUsage ret = tryCacheNode(n);

	// uncheck cached node status and add all elements in TODO list
	if ( ret == utUnusable )
		redoNodeLabel ( n, "ce" );

	return ret;
}

//-----------------------------------------------------------------------------
//--		internal datatype support
//-----------------------------------------------------------------------------

/// @return true iff given data node contains data contradiction
bool
DlSatTester :: hasDataClash ( const DlCompletionTree* Node )
{
	fpp_assert ( Node && Node->isDataNode() );	// safety check

	DTReasoner.clear();

	// data node may contain only "simple" concepts in there
	for ( DlCompletionTree::const_label_iterator r = Node->beginl_sc(); r != Node->endl_sc(); ++r )
		if ( DTReasoner.addDataEntry ( r->bp(), r->getDep() ) )	// clash found
			return true;

	return DTReasoner.checkClash();
}

bool DlSatTester :: runSat ( void )
{
	testTimer.Start();
	bool result = checkSatisfiability ();
	testTimer.Stop();

	if ( LLM.isWritable(llSatTime) )
		LL << "\nChecking time was " << testTimer << " seconds";

	testTimer.Reset();

	finaliseStatistic();

	if ( result )
		writeRoot(llRStat);

	return result;
}

void
DlSatTester :: finaliseStatistic ( void )
{
#ifdef USE_REASONING_STATISTICS
	// add the integer stat values
	nNodeSaves.set(CGraph.getNNodeSaves());
	nNodeRestores.set(CGraph.getNNodeRestores());

	// log statistics data
	if ( LLM.isWritable(llRStat) )
		logStatisticData ( LL, /*needLocal=*/true );

	// merge local statistics with the global one
	AccumulatedStatistic::accumulateAll();
#endif

	// clear global statistics
	CGraph.clearStatistics();
}

bool DlSatTester :: applyReflexiveRoles ( DlCompletionTree* node, const DepSet& dep )
{
	for ( RoleMaster::roleSet::const_iterator p = ReflexiveRoles.begin(); p != ReflexiveRoles.end(); ++p )
	{
		// create R-loop through the NODE
		DlCompletionTreeArc* pA = CGraph.addRoleLabel ( node, node, /*isPredEdge=*/false, *p, dep );
		if ( setupEdge ( pA, dep, 0 ) == utClash )
			return true;
	}

	// no clash found
	return false;
}

const modelCacheInterface* DlSatTester :: createCache ( BipolarPointer p )
{
	fpp_assert ( isValid(p) );	// safety check

	const modelCacheInterface* cache;

	// check if cache already calculated
	if ( (cache = DLHeap.getCache(p)) != NULL )
		return cache;

#ifdef TMP_CACHE_DEBUG
	std::cerr << "\nCCache for " << p << ":";
#endif
	prepareCascadedCache(p);

	// it may be a cycle and the cache for p is already calculated
	if ( (cache = DLHeap.getCache(p)) != NULL )
		return cache;

	// need to build cache
	cache = buildCache(p);
	DLHeap.setCache ( p, cache );
	return cache;
}

void
DlSatTester :: prepareCascadedCache ( BipolarPointer p )
{
	/// cycle found -- shall be processed without caching
	if ( inProcess.find(p) != inProcess.end() )
	{
#	ifdef TMP_CACHE_DEBUG
		std::cerr << " cycle with " << p << ";";
#	endif
		return;
	}

	const DLVertex& v = DLHeap[p];
	bool pos = isPositive(p);

	// check if a concept already cached
	if ( v.getCache(pos) != NULL )
		return;

	switch ( v.Type() )
	{
	case dtTop:
		break;

	case dtDataType:	// data things are checked by data inferer
	case dtDataValue:
	case dtDataExpr:
		break;

	case dtAnd:
	case dtCollection:
	{
		for ( DLVertex::const_iterator q = v.begin(), q_end = v.end(); q < q_end; ++q )
			prepareCascadedCache ( pos ? *q : inverse(*q) );
		break;
	}

	case dtPSingleton:
	case dtNSingleton:
	case dtNConcept:
	case dtPConcept:
		if ( isNegative(p) && isPNameTag(v.Type()) )
			return;
		inProcess.insert(p);
#	ifdef TMP_CACHE_DEBUG
		std::cerr << " expanding " << p << ";";
#	endif
		prepareCascadedCache ( pos ? v.getC() : inverse(v.getC()) );
		inProcess.erase(p);
		break;

	case dtForall:
	case dtLE:
	{
		const TRole* R = v.getRole();
		if ( R->isDataRole() )	// skip data-related stuff
			break;
		BipolarPointer x = pos ? v.getC() : inverse(v.getC());

		// build cache for C in \AR.C
		if ( x != bpTOP )
		{
			inProcess.insert(x);
#		ifdef TMP_CACHE_DEBUG
			std::cerr << " caching " << x << ";";
#		endif
			createCache(x);
			inProcess.erase(x);
		}

		// build cache for the Range(R) if necessary
		x = R->getBPRange();
		if ( x != bpTOP )
		{
			inProcess.insert(x);
#		ifdef TMP_CACHE_DEBUG
			std::cerr << " caching range(" << v.getRole()->getName() << ") = " << x << ";";
#		endif
			createCache(x);
			inProcess.erase(x);
		}

		break;
	}

	case dtIrr:
		break;

	default:
		fpp_unreachable();
	}
}

/// build cache for given DAG node using SAT tests; @return cache
const modelCacheInterface*
DlSatTester :: buildCache ( BipolarPointer p )
{
	if ( LLM.isWritable(llDagSat) )
	{
		LL << "\nChecking satisfiability of DAG entry " << p;
		tBox.PrintDagEntry(LL,p);
		LL << ":\n";
#	ifdef __DEBUG_FLUSH_LL
		LL.flush ();
#	endif
	}

#ifdef TMP_CACHE_DEBUG
	std::cerr << " building cache for " << p << "...";
#endif

	bool sat = runSat(p);

#ifdef TMP_CACHE_DEBUG
	std::cerr << " done";
#endif

	// unsat => P -> \bot
	if ( !sat && LLM.isWritable(llAlways) )
		LL << "\nDAG entry " << p << " is unsatisfiable\n";

	return buildCacheByCGraph(sat);
}

bool DlSatTester :: checkSatisfiability ( void )
{
	unsigned int loop = 0;
	for (;;)
	{
		if ( curNode == NULL )
		{
			if ( TODO.empty() )	// nothing more to do
			{	// make sure all blocked nodes are still blocked
				if ( LLM.isWritable(llGTA) )
				{
					logIndentation();
					LL << "[*ub:";
				}
				CGraph.retestCGBlockedStatus();
				if ( LLM.isWritable(llGTA) )
					LL << "] utDone";
				// clear blocker cache
				dBlocked = NULL;
				if ( TODO.empty() )
#				ifndef RKG_USE_FAIRNESS
					return true;
#				else
				{	// check fairness constraints
					if ( !tBox.hasFC() )
						return true;
					// reactive fairness: for every given FC, if it is violated, reject current model
					for ( TBox::ConceptVector::const_iterator p = tBox.Fairness.begin(), p_end = tBox.Fairness.end(); p < p_end; ++p )
						if ( CGraph.isFCViolated((*p)->pName) )
						{
							nFairnessViolations.inc();
							if ( straightforwardRestore() )	// no more branching alternatives
								return false;
							else
								break;
						}

					if ( TODO.empty() )
						return true;
				}
#				endif
			}

			const ToDoEntry* curTDE = TODO.getNextEntry ();
			fpp_assert ( curTDE != NULL );

			// setup current context
			curNode = curTDE->Node;
			curConcept = curNode->label().getConcept(curTDE->offset);
		}

		if ( ++loop == 5000 )
		{
			loop = 0;
			if ( tBox.isCancelled() )
				return false;
			if ( testTimeout && (float)testTimer > testTimeout )
				throw EFPPTimeout();
		}
		// here curNode/curConcept are set
		if ( commonTactic() == utClash )	// clash found
		{
			if ( tunedRestore() )	// the concept is unsatisfiable
				return false;
		}
		else
			curNode = NULL;
	}
}

/********************************************************************************
  *
  *  Save/Restore section
  *
  ******************************************************************************/

	/// restore local state from BContext
void DlSatTester :: restoreBC ( void )
{
	// restore reasoning context
	curNode = bContext->curNode;
	curConcept = bContext->curConcept;
	pUsed.resize(bContext->pUsedIndex);
	nUsed.resize(bContext->nUsedIndex);

	// update branch dep-set
	updateBranchDep();
	bContext->nextOption();
}

void DlSatTester :: save ( void )
{
	// save tree
	CGraph.save();

	// save ToDoList
	TODO.save();

	// increase tryLevel
	++tryLevel;
	Manager.ensureLevel(getCurLevel());

	// init BC
	clearBC();

	incStat(nStateSaves);

	if ( LLM.isWritable(llSRState) )
		LL << " ss(" << getCurLevel()-1 << ")";
#ifdef __DEBUG_SAVE_RESTORE
	writeRoot(llSRState);
#endif
}

void DlSatTester :: restore ( unsigned int newTryLevel )
{
	fpp_assert ( !Stack.empty () );
	fpp_assert ( newTryLevel > 0 );

	// skip all intermediate restorings
	setCurLevel(newTryLevel);

	// restore local
	bContext = Stack.top(getCurLevel());
	restoreBC();

	// restore tree
	CGraph.restore(getCurLevel());

	// restore TODO list
	TODO.restore(getCurLevel());

	incStat(nStateRestores);

	if ( LLM.isWritable(llSRState) )
		LL << " sr(" << getCurLevel() << ")";
#ifdef __DEBUG_SAVE_RESTORE
	writeRoot(llSRState);
#endif
}

/**
  * logging methods
  */

void DlSatTester :: logIndentation ( void ) const
{
	CHECK_LL_RETURN(llGTA);	// useless but safe
	LL << "\n";
	for ( register unsigned int i = getCurLevel(); --i; )
		LL << " ";
}
void DlSatTester :: logStartEntry ( void ) const
{
	CHECK_LL_RETURN(llGTA);	// useless but safe
	logIndentation();
	LL << "[*(";
	curNode->logNode ();
	LL << "," << curConcept << "){";
	if ( isNegative (curConcept.bp()) )
		LL << "~";
	LL << DLHeap[curConcept.bp()].getTagName() << "}:";
}

void DlSatTester :: logFinishEntry ( tacticUsage res ) const
{
	CHECK_LL_RETURN(llGTA);	// useless but safe

	LL << "] ";
	switch (res)
	{
	case utDone:
		LL << "utDone";
		break;
	case utUnusable:
		LL << "utUnusable";
		break;
	case utClash:
		LL << "utClash" << getClashSet();
		break;
	default:	// safety check
		fpp_unreachable();
	}
#ifdef __DEBUG_FLUSH_LL
	LL.flush ();
#endif
}

void DlSatTester :: logStatisticData ( std::ostream& o, bool needLocal ) const
{
#ifdef USE_REASONING_STATISTICS
	nTacticCalls.Print	( o, needLocal, "\nThere were made ", " tactic operations, of which:" );
	nIdCalls.Print		( o, needLocal, "\n    CN   operations: ", "" );
	nSingletonCalls.Print(o, needLocal, "\n           including ", " singleton ones" );
	nOrCalls.Print		( o, needLocal, "\n    OR   operations: ", "" );
	nAndCalls.Print		( o, needLocal, "\n    AND  operations: ", "" );
	nSomeCalls.Print	( o, needLocal, "\n    SOME operations: ", "" );
	nAllCalls.Print		( o, needLocal, "\n    ALL  operations: ", "" );
	nFuncCalls.Print	( o, needLocal, "\n    Func operations: ", "" );
	nLeCalls.Print		( o, needLocal, "\n    LE   operations: ", "" );
	nGeCalls.Print		( o, needLocal, "\n    GE   operations: ", "" );
	nUseless.Print		( o, needLocal, "\n    N/A  operations: ", "" );

	nNNCalls.Print		( o, needLocal, "\nThere were made ", " NN rule application" );
	nMergeCalls.Print	( o, needLocal, "\nThere were made ", " merging operations" );

	nSRuleAdd.Print		( o, needLocal, "\nThere were made ", " simple rule additions" );
	nSRuleFire.Print	( o, needLocal, "\n       of which ", " simple rules fired" );

	nStateSaves.Print		( o, needLocal, "\nThere were made ", " save(s) of global state" );
	nStateRestores.Print	( o, needLocal, "\nThere were made ", " restore(s) of global state" );
	nNodeSaves.Print		( o, needLocal, "\nThere were made ", " save(s) of tree state" );
	nNodeRestores.Print		( o, needLocal, "\nThere were made ", " restore(s) of tree state" );
	nLookups.Print			( o, needLocal, "\nThere were made ", " concept lookups" );
#ifdef RKG_USE_FAIRNESS
	nFairnessViolations.Print	( o, needLocal, "\nThere were ", " fairness constraints violation" );
#endif

	nCacheTry.Print				( o, needLocal, "\nThere were made ", " tries to cache completion tree node, of which:" );
	nCacheFailedNoCache.Print	( o, needLocal, "\n                ", " fails due to cache absence" );
	nCacheFailedShallow.Print	( o, needLocal, "\n                ", " fails due to shallow node" );
	nCacheFailed.Print			( o, needLocal, "\n                ", " fails due to cache merge failure" );
	nCachedSat.Print			( o, needLocal, "\n                ", " cached satisfiable nodes" );
	nCachedUnsat.Print			( o, needLocal, "\n                ", " cached unsatisfiable nodes" );
#endif

	if ( !needLocal )
		o << "\nThe maximal graph size is " << CGraph.maxSize() << " nodes";
}

float
DlSatTester :: printReasoningTime ( std::ostream& o ) const
{
	o << "\n     SAT takes " << satTimer << " seconds\n     SUB takes " << subTimer << " seconds";
	return satTimer + subTimer;
}

