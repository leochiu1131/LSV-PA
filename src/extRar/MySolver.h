#pragma once

#include "sat/bsat2/Solver.h"

#include <string>
#include <vector>

std::string litStr(Minisat::Lit l);

namespace Minisat
{


class MySolver : public Solver 
{
public:

	MySolver(): _numVar(0), _rootLevel(0) {}


	// basic sat
	Var createVar() { newVar(); return _numVar++; }
	bool makeDecision( Var v, bool val, std::vector<Lit> *imp = 0 );
	bool makeDecision( Lit l, std::vector<Lit> *imp = 0, bool assump = false );
	void backTrackTo( int level );

	// get 
	int getNumVar() const { return _numVar; }
	int getDecisionLevel() const { return decisionLevel()-_rootLevel; };

	// clause
	void addAigCNF(Var vf, Var va, bool fa, Var vb, bool fb );
	void addEqCNF(Var vf, Var va, bool fa );
	void addCtrlAigCNF(Var vc, Var vf, Var va, bool fa, Var vb, bool fb );
	void addCtrlEqCNF(Var vc, Var vf, Var va, bool fa );
	void assertVar(Var v, bool val) { addClause(mkLit(v, !var)); }
	
	// assumps
	bool setAssumps( std::vector<Lit> &assumps);
	bool addAssumps( Lit l );
	void clearAssumps();

private:

	int _numVar;

	// level of assumptions
	int	_rootLevel;			

};

} // namespace minia