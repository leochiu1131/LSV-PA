
#include "sat/bsat2/Solver.h"
#include "extRar/MySolver.h"

#include <iostream>
#include <string>

using namespace Minisat;
using namespace std;

// #define DEBUG

string 
litStr(Lit l) 
{ 
	return sign(l) ? "-" + to_string(var(l)) : " " + to_string(var(l)) ;
}

bool 
MySolver::makeDecision( Var v, bool val, vector<Lit> *imp )
{
	return makeDecision( mkLit( v, !val ), imp );
}
/*
	return true if sucess
	return false if conflict
*/
bool
MySolver::makeDecision( Lit l, std::vector<Lit> *imp, bool assump )
{
	assert(ok);
	int backtrack_level;
	bool hasConflict = false;

	// check if it's already assigned
	if ( value(l) != l_Undef )
	{
		if ( value(l) == l_True )
		{
			#ifdef DEBUG
			cout << "Decision " << var(l) << "=" << !sign(l) << " is trivial" << endl;
			#endif
			newDecisionLevel();
			return true;
		}
		else
		{
			#ifdef DEBUG
			cout << "Decision " << var(l) << "=" << !sign(l) << " is a direct conflict" << endl;
			#endif
			if ( assump ) analyzeFinal( ~l, conflict );
			return false;
		}
	}

	// make decision and propagate
	newDecisionLevel();
	uncheckedEnqueue(l);
	unsigned trail_before = trail.size();

	while( qhead < trail.size() )
	{
		CRef confl = propagate();
		if ( confl != CRef_Undef )
		{
			#ifdef DEBUG
			cout << "Decision " << var(l) << "=" << !sign(l) << " leads to a conflict" << endl;
			#endif
			
			// conflict
			if ( assump ) 
			{
				analyzeFinal( ~l, conflict );
				return false;
			}
			else 
			{

				hasConflict = true;
				vec<Lit>    learnt_clause;
				analyze(confl, learnt_clause, backtrack_level);
				cancelUntil(backtrack_level);
				// cout << "Cancel from " << current_level << " back to " << backtrack_level << endl;

				trail_before = trail.size();

				if (learnt_clause.size() == 1){
					// should not reach here
					// since no var should be assert;w
					assert(false);
					uncheckedEnqueue(learnt_clause[0]);
				}else{
					CRef cr = ca.alloc(learnt_clause, true);
					learnts.push(cr);
					attachClause(cr);
					// enqueue and keep propagating
					uncheckedEnqueue(learnt_clause[0], cr);

					// print learnt clause
					#ifdef DEBUG
					cout << "learnt: ";
					for( int i = 0; i < learnt_clause.size(); i++ )
					{
						cout << litStr(learnt_clause[i]) << " ";
					}
					cout << endl;
					#endif
				}
			}
		}
	}

	if ( imp )
	{
		// if no conflict, return new implications
		// if conflict: return new implications of backtracked level
		for( int i = trail_before; i < trail.size(); i++ )
		{
			imp -> push_back(trail[i]);
		}

	}

	#ifdef DEBUG
	// print imp
	cout << "Decision " << var(l) << "=" << !sign(l) << " implies ";
	for( int i = trail_before; i < trail.size(); i++ )
	{
		cout << " " << litStr(trail[i]);
	}
	cout << endl;
	#endif

	return !hasConflict;

}

void 
MySolver::backTrackTo( int level )
{
	// cout << "Backtrack: cancel until " << _rootLevel + level << endl;
	cancelUntil(_rootLevel + level);
}


void 
MySolver::addAigCNF(Var vf, Var va, bool ca, Var vb, bool cb )
{        
	assert( vf < _numVar );
	assert( va < _numVar );
	assert( vb < _numVar );
	vec<Lit> lits;
	Lit lf = mkLit( vf, 0 );
	Lit la = mkLit( va, ca );
	Lit lb = mkLit( vb, cb );
	lits.push(la); lits.push(~lf);
	addClause(lits); lits.clear();
	lits.push(lb); lits.push(~lf);
	addClause(lits); lits.clear();
	lits.push(~la); lits.push(~lb); lits.push(lf);
	addClause(lits); lits.clear();
}
void 
MySolver::addEqCNF(Var vf, Var va, bool fa) {
	vec<Lit> lits;
	Lit lf = mkLit(vf);
	Lit la = mkLit(va, fa);
	lits.push(la); lits.push(~lf);
	addClause(lits); lits.clear();
	lits.push(lf); lits.push(~la);
	addClause(lits); lits.clear();
}
void
MySolver::addCtrlAigCNF( Var vc, Var vf, Var va, bool ca, Var vb, bool cb )
{        
	assert( vf < _numVar );
	assert( va < _numVar );
	assert( vb < _numVar );
	vec<Lit> lits;
	Lit lc = mkLit( vc, 0 );
	Lit lf = mkLit( vf, 0 );
	Lit la = mkLit( va, ca );
	Lit lb = mkLit( vb, cb );
	lits.push(~lc); lits.push(la); lits.push(~lf);
	addClause(lits); lits.clear();
	lits.push(~lc); lits.push(lb); lits.push(~lf);
	addClause(lits); lits.clear();
	lits.push(~lc); lits.push(~la); lits.push(~lb); lits.push(lf);
	addClause(lits); lits.clear();
}
void 
MySolver::addCtrlEqCNF( Var vc, Var vf, Var va, bool fa) {
	vec<Lit> lits;
	Lit lc = mkLit(vc, 0);
	Lit lf = mkLit(vf);
	Lit la = mkLit(va, fa);
	lits.push(~lc); lits.push(la); lits.push(~lf);
	addClause(lits); lits.clear();
	lits.push(~lc); lits.push(lf); lits.push(~la);
	addClause(lits); lits.clear();
}
bool
MySolver::setAssumps( vector<Lit> &assumps )
{
	cancelUntil(0);
	// TODO: do we need the imp from assumps?

	for( auto l : assumps )
	{
		assumptions.push(l);
		if ( !makeDecision(l, nullptr, true) )
		{
			assert(false);
			return false;
		}
	}

    _rootLevel = decisionLevel();
	return true;
	
}

bool 
MySolver::addAssumps( Lit l )
{
	assert( decisionLevel() == _rootLevel );
	_rootLevel ++;
	assumptions.push(l);
	vector<Lit> imp;
	if ( !makeDecision(l, nullptr, true) )
	{
		assert(false);
		return false;
	}
	return true;
}
void
MySolver::clearAssumps()
{
	cancelUntil(0);
	assumptions.clear();
	_rootLevel = 0;
}