
#include "extRar/rarMgr.h"
#include "misc/bzlib/bzlib.h"
#include "misc/zlib/zlib.h"

#include <iostream>
#include <algorithm>


using namespace std;
using namespace Minisat;

void
RarMgr::nar()
{
	int count = 0;

	// single iteration

	for( int k = 0; k < 1; k++ )
	{
		genNetlist();
		findAllDom();
		RarNode::incGFlag2();
		for( int i = _netList.size()-1; i >= 0; i-- )
		// for( int i = 0; i < _netList.size(); i++ )
		{
			int id = _netList[i];
			if ( _nodes[id] == 0 ) continue;
			if ( nar(id) ) count ++;
		}

		cout << "NAR: " << count << " modifications." << endl;
	}

	// printAig();
}
bool 
RarMgr::nar( int nodeId )
{
	RarNode &n = *node(nodeId);
	if ( n.numFanout() == 0 ) return false;
	if ( n.isConst() || n.isPi() ) return false;  

	n.setFlag2();
	
	// find dom
	MySolver &s = *_s;

	// find side inputs
	vector<RarNode*> sDoms;
	vector<RarEdge> sides;
	findSideInputs( nodeId, sDoms, sides );

	// print side inputs
	// cout << "Node " << nodeId << "'s side inputs:" << endl;
	// for( int i = 0; i < sDoms.size(); i++ )
	// {
	// 	cout << "\t" << sDoms[i]->getId() << " " << sides[i].phase() << " " << sides[i].node() -> getId() << endl;
	// }

	// propagation constraint
	if ( !propConstraint(sDoms, sides, nullptr ) )
	{
		// replace n with 1
		narReplace( nodeId, _constId, true );
		return true;
	}

	// redundancy test of s-a-1 (val=0)
	vector<Lit> imp;
	if ( !s.makeDecision( n.getVar(), 0, &imp ) )
	{
		// replace n with 1
		narReplace( nodeId, _constId, true );
		return true;
	}

	if ( _flag_learn)
	{
		if ( !recursiveLearn( nodeId, 0, imp, sDoms, sides ) )
		{
			// replace n with 1
			narReplace( nodeId, _constId, true );
			return true;
		}
	}

	// redundancy test of s-a-0
	vector<Lit> imp2;
	int level = s.getDecisionLevel();
	s.backTrackTo(level-1);
	if ( !s.makeDecision( n.getVar(), 1, &imp2 ) )
	{
		// replace n with const 0
		narReplace( nodeId, _constId, false );
		return true;
	}

	if ( _flag_learn )
	{
		if ( !recursiveLearn( nodeId, 1, imp2, sDoms, sides ) )
		{
			// replace n with const 0
			narReplace( nodeId, _constId, false );
			return true;
		}
	}


	// remove n's fanout from imp
	RarNode::incGFlag();
	n.DFSMarkFanout();
	vector<Lit> imp_sa1;
	vector<Lit> imp_sa0;
	for( Lit l : imp ) if ( node( _var2Id[var(l)]) -> getFlag() != 0 &&  !node(_var2Id[var(l)]) -> isPo() ) imp_sa1.push_back(l);
	for( Lit l : imp2 ) if ( node( _var2Id[var(l)]) -> getFlag() != 0 && !node(_var2Id[var(l)]) -> isPo() ) imp_sa0.push_back(l);

	// mark fanout free are
	// RarNode::incGFlag();
	// vector<int> faninCone;
	// n.DFSFanin( faninCone );
	// bool setFlag;
	// for ( int i = faninCone.size()-1; i >= 0; i-- ) 
	// {
	// 	RarNode &t = *node(faninCone[i]);
	// 	setFlag = true;
	// 	for ( int j = 0; j < t.numFanout(); j++ )
	// 	{
	// 		if ( t.fanout(j).node() -> getFlag() != 0 ) setFlag = false;
	// 	}
	// 	if ( setFlag ) t.setFlag();
	// }


	// for comparison
	// sa1: output compl
	bool isFound = false;
	bool isGate = false;
	bool sa = false;
	Lit lit0, lit1, litTemp0, litTemp1;

	// prevent removable fanin node from being selected in alt gate
	bool remove0 = n.fanin(0).node()->numFanout()==1 && !n.fanin(0).node()->isPi();
	bool remove1 = n.fanin(1).node()->numFanout()==1 && !n.fanin(1).node()->isPi();
	bool doAddition = remove0 || remove1;
	bool both = remove0 && remove1;
	Var var0 = n.fanin(0).node() -> getVar();
	Var var1 = n.fanin(1).node() -> getVar();


	// check different value nodes
	level = s.getDecisionLevel();
	for ( auto lit : imp_sa1 )
	{
		assert( level == s.getDecisionLevel() );

		if ( s.value(lit) == l_True ) continue;
		else if ( s.value(lit) == l_False )
		{
			// alt node
			if ( !isFound || isGate || altLessThan(lit,lit0) )
			{
				isFound = true; isGate = false; sa=1;
				lit0 = lit;
			}

		}
		else
		{
			// alt node
			if ( !s.makeDecision( lit , nullptr ) )
			{
				if ( s.getDecisionLevel() < level )
				{
					if ( !propConstraint( sDoms, sides, nullptr ) || 
						!s.makeDecision(node(nodeId)->getVar(), 1, nullptr ) 
					)
					{
						// replace n with const 0
						cout << "Mid conflict const replacement (sa0) " << endl;
						narReplace( nodeId, _constId, false );
						return true;
					}
				}
				assert( level = s.getDecisionLevel() );

				// replace node
				if ( !isFound || isGate || altLessThan(lit,lit0) )
				{
					isFound = true; isGate = false; sa=1;
					lit0 = lit;
				}
			}

			// find altGates
			else if ( doAddition && ( !isFound || isGate ) && (var(lit) != var0 && var(lit) != var1 ) )
			{
				for( auto l : imp_sa1 )
				{
					if ( s.value(l) == l_False)
					{
						if ( (var(l) != var0 && var(l) != var1 ) || both )
						{
							litTemp0 = lit; litTemp1 = l;
							if ( altLessThan( litTemp1, litTemp0 ) ) swap(litTemp0, litTemp1);
							if ( !isFound || ( isGate && altLessThan(litTemp1, lit1) ) )
							{
								isFound = true; isGate = true; sa=1;
								lit0 = litTemp0;
								lit1 = litTemp1;
							}
						}
					}
				}
			}
			s.backTrackTo(level);
		}
	}

	// s-a-1
	s.backTrackTo(level-1);

	if ( !s.makeDecision( n.getVar(), 0, nullptr ) )
	{
		// replace n with 1
		narReplace( nodeId, _constId, true );
		return true;
	}

	for ( auto lit : imp_sa0 )
	{
		assert( level == s.getDecisionLevel() );

		if ( s.value(lit) == l_True ) continue;
		else if ( s.value(lit) == l_False )
		{
			// alt node
			if ( !isFound || isGate || altLessThan(lit,lit0) )
			{
				isFound = true; isGate = false; sa=0;
				lit0 = lit;
			}
		}
		else
		{
			if ( !s.makeDecision( lit , nullptr ) )
			{

				if ( s.getDecisionLevel() < level  )
				{
					if ( !propConstraint( sDoms, sides, nullptr ) || 
						!s.makeDecision(node(nodeId)->getVar(), 0, nullptr ) 
					)
					{
						// replace n with const 1
						cout << "Mid conflict const replacement (sa1)" << endl;
						narReplace( nodeId, _constId, true );
						return true;
					}
				}

				assert(s.getDecisionLevel() == level);

				// alt node
				if ( !isFound || isGate || altLessThan(lit,lit0) )
				{
					isFound = true; isGate = false; sa=0;
					lit0 = lit;
				}
			}
			// find altGates
			else if ( doAddition && ( !isFound || isGate ) && (var(lit) != var0 && var(lit) != var1 ) )
			{
				for( auto l : imp_sa0 )
				{
					if ( s.value(l) == l_False )
					{
						if ( (var(l) != var0 && var(l) != var1 ) || both ) 
						{
							litTemp0 = lit; litTemp1 = l;
							if ( altLessThan( litTemp1, litTemp0 ) ) swap(litTemp0, litTemp1);
							if ( !isFound || ( isGate && altLessThan(litTemp1, lit1) ) )
							{
								isFound = true; isGate = true; sa=0;
								lit0 = litTemp0;
								lit1 = litTemp1;
							}
						}
					}
				}
			}
			s.backTrackTo(level);
		}
	}


	// 2-way rar
	bool is2Way = false;
	int domId = -1;

	// if ( !isFound )
	// {
	// 	s.backTrackTo(level-1);

	// 	const vector<int> doms = n.doms();
	// 	int idx_sDoms = sDoms.size();
	// 	int idx_doms = doms.size() - 1;

	// 	while( idx_doms >= 0 )
	// 	{
	// 		RarNode *d = node(doms[idx_doms]);
	// 		idx_doms--;

	// 		if ( idx_sDoms == 0 || sDoms[idx_sDoms-1] != d )
	// 		{
	// 			s.backTrackTo(idx_sDoms);
	// 		}
	// 		else
	// 		{
	// 			idx_sDoms--;
	// 			s.backTrackTo(idx_sDoms);
	// 		}

	// 		level = s.getDecisionLevel() + 1;

	// 		assert( s.makeDecision( d->getVar(), 1, nullptr ) );
	// 		// TODO: redundant dominator

	// 		// current level: idx_sDoms+1;

	// 		for ( auto lit : imp_sa1 )
	// 		{
	// 			assert( level == s.getDecisionLevel() );

	// 			if ( s.value(lit) == l_True ) continue;
	// 			else if ( s.value(lit) == l_False )
	// 			{
	// 				// alt node
	// 				if ( !isFound || isGate || altLessThan(lit,lit0) )
	// 				{
	// 					cout << "2way found" << endl;
	// 					isFound = true; isGate = false; sa=1;
	// 					lit0 = lit;
	// 					domId = d->getId();
	// 					is2Way = true;
	// 				}
	// 			}
	// 			else
	// 			{
	// 				// alt node
	// 				if ( !s.makeDecision( lit , nullptr ) )
	// 				{
	// 					if ( s.getDecisionLevel() < level )
	// 					{
	// 						if ( !propConstraint( sDoms, sides, nullptr, level-1 ) || 
	// 							!s.makeDecision(d->getVar(), 1, nullptr ) 
	// 						)
	// 						{
	// 							// replace n with const 0
	// 							cout << "Mid conflict const replacement (sa1) " << endl;
	// 							narReplace( nodeId, _constId, false );
	// 							return true;
	// 						}
	// 					}
	// 					assert( level = s.getDecisionLevel() );

	// 					// replace node
	// 					cout << "2way found" << endl;
	// 					if ( !isFound || isGate || altLessThan(lit,lit0) )
	// 					{
	// 						isFound = true; isGate = false; sa=1;
	// 						lit0 = lit;
	// 						domId = d->getId();
	// 						is2Way = true;
	// 					}
	// 				}

	// 				// find altGates
	// 				// else if ( doAddition && ( !isFound || isGate ) && (var(lit) != var0 && var(lit) != var1 ) )
	// 				// {
	// 				// 	for( auto l : imp_sa1 )
	// 				// 	{
	// 				// 		if ( s.value(l) == l_False && ( (var(l) != var0 && var(l) != var1 ) || both )  )
	// 				// 		{
	// 				// 			litTemp0 = lit; litTemp1 = l;
	// 				// 			if ( altLessThan( litTemp1, litTemp0 ) ) swap(litTemp0, litTemp1);
	// 				// 			if ( !isFound || ( isGate && altLessThan(litTemp1, lit1) ) )
	// 				// 			{
	// 				// 				isFound = true; isGate = true; sa=1;
	// 				// 				lit0 = litTemp0;
	// 				// 				lit1 = litTemp1;
	// 				// 			}
	// 				// 		}
	// 				// 	}
	// 				// }
	// 				s.backTrackTo(level);
	// 			}
	// 		}
	// 		for ( auto lit : imp_sa0 )
	// 		{
	// 			assert( level == s.getDecisionLevel() );

	// 			if ( s.value(lit) == l_True ) continue;
	// 			else if ( s.value(lit) == l_False )
	// 			{
	// 				// alt node
	// 				cout << "2way found" << endl;
	// 				if ( !isFound || isGate || altLessThan(lit,lit0) )
	// 				{
	// 					isFound = true; isGate = false; sa=0;
	// 					lit0 = lit;
	// 					domId = d->getId();
	// 					is2Way = true;
	// 				}
	// 			}
	// 			else
	// 			{
	// 				// alt node
	// 				if ( !s.makeDecision( lit , nullptr ) )
	// 				{
	// 					if ( s.getDecisionLevel() < level )
	// 					{
	// 						if ( !propConstraint( sDoms, sides, nullptr, level-1 ) || 
	// 							!s.makeDecision(d->getVar(), 1, nullptr ) 
	// 						)
	// 						{
	// 							// replace n with const 1
	// 							cout << "Mid conflict const replacement (sa0) " << endl;
	// 							narReplace( nodeId, _constId, true );
	// 							return true;
	// 						}
	// 					}
	// 					assert( level = s.getDecisionLevel() );

	// 					// replace node
	// 					cout << "2way found" << endl;
	// 					if ( !isFound || isGate || altLessThan(lit,lit0) )
	// 					{
	// 						isFound = true; isGate = false; sa=0;
	// 						lit0 = lit;
	// 						domId = d->getId();
	// 						is2Way = true;
	// 					}
	// 				}

	// 				// find altGates
	// 				// else if ( doAddition && ( !isFound || isGate ) && (var(lit) != var0 && var(lit) != var1 ) )
	// 				// {
	// 				// 	for( auto l : imp_sa1 )
	// 				// 	{
	// 				// 		if ( s.value(l) == l_False && ( (var(l) != var0 && var(l) != var1 ) || both )  )
	// 				// 		{
	// 				// 			litTemp0 = lit; litTemp1 = l;
	// 				// 			if ( altLessThan( litTemp1, litTemp0 ) ) swap(litTemp0, litTemp1);
	// 				// 			if ( !isFound || ( isGate && altLessThan(litTemp1, lit1) ) )
	// 				// 			{
	// 				// 				isFound = true; isGate = true; sa=1;
	// 				// 				lit0 = litTemp0;
	// 				// 				lit1 = litTemp1;
	// 				// 			}
	// 				// 		}
	// 				// 	}
	// 				// }
	// 				s.backTrackTo(level);
	// 			}
	// 		}

	// 	}

	// }


	s.backTrackTo(0);

	// select by level heuristic
	if ( isFound )
	{
		if ( is2Way )
		{
			cout << "2Way: " << endl;
			narReplace( nodeId, _constId, sa );
			narAddEdge( domId, _var2Id[var(lit0)], !sign(lit0) );
		}
		else if ( isGate )
		{
			int newId = createAnd( _var2Id[var(lit0)], sign(lit0), _var2Id[var(lit1)], sign(lit1) );
			narReplace( nodeId, newId, sa );
		}
		else
		{
			narReplace( nodeId, _var2Id[var(lit0)], sign(lit0) != sa );
		}
	}

	// no replace node found
	return isFound;
}

void
RarMgr::narReplace( int nodeId, int rId, bool phase, bool finish, RarNode* toNode )
{

	// TODO: try using equal to maintain leart clauses
	assert( !node(nodeId) -> isPo() );
	assert( !node(rId) -> isPo() );
	_s -> clearAssumps();

	// cout << "Node " << nodeId << " replaced by node " << rId  << " (" << phase << ") " << endl;

	RarNode* n = node(nodeId);
	RarNode* r = node(rId);
	int faninId;
	bool c;
	RarNode* out;

	if ( toNode != 0 )
	{
		for( int i = 0; i < n -> numFanout(); i++ )
		{
			if ( n -> fanout(i) == toNode )
			{
				out = n -> fanout(i).node();
			}
		}

		faninId = out -> fanin(0) == n ? 0 : 1;
		c = out -> fanin(faninId).phase();

		out -> removeFanin( faninId );
		out -> addFanin( r, c != phase );
		_changed.push_back( out );


		// need to update level
		_levelUpdateList.push_back( out -> getId() );

		n -> removeFanout( toNode );
		if ( n -> numFanout() == 0 ) 
		{
			_fltList.push_back( n->getId() );
		}

	}
	else if ( rId != _constId )
	{

		for( int i = 0; i < n -> numFanout(); i++ )
		{
			out = n -> fanout(i).node();
			faninId = out -> fanin(0) == n ? 0 : 1;
			c = out -> fanin(faninId).phase();

			out -> removeFanin( faninId );
			out -> addFanin( r, c != phase );
			// cout << out->getId() << " adding fanin " << rId  << endl;
			_changed.push_back( out );


			// need to update level
			_levelUpdateList.push_back( out -> getId() );
		}

		n -> clearFanout();
		_fltList.push_back( n->getId() );
	}
	else
	{
		int otherFanin;
		bool otherFaninPhase;

		for( int i = 0; i < n -> numFanout(); i++ )
		{

			out = n -> fanout(i).node();
			faninId = out -> fanin(0) == n ? 0 : 1;
			c = out -> fanin(faninId).phase();

			assert( !out->isPo() );
			// TODO: what if output is po

			if ( c != phase )
			{
				// this edge becomes 1, remove it
				otherFanin = faninId == 0 ? 1 : 0;
				otherFaninPhase = out -> fanin(otherFanin).phase();
				narReplace( out -> getId(), out -> fanin(otherFanin).node() -> getId(), otherFaninPhase, false );
			}
			else
			{
				cout << "Consecutive zero replacement!" << endl;
				// this edge becomes 0
				// the fanoout node becomes 0 too
				narReplace( out -> getId(), _constId, false, false );
			}

		}
	}

	if ( !r -> isConst() ) _domUpdateList.push_back( r -> getId() );

	if ( finish ) 
	{
		sweepFloating();
		updateChanged();
		updateLevel();
		updateDom();
	}

}

void 
RarMgr::updateChanged()
{
	Var vc, v, v0, v1;
	bool c0, c1;
	for( auto n : _changed )
	{
		_disabled.push_back( mkLit( n -> getEnable(), 1 ) );
		vc = _s -> createVar();
		n -> setEnable(vc);
		if ( n -> isPo() )
		{
			v = n -> getVar();
			v0 = n -> fanin(0).node() -> getVar();
			c0 = n -> fanin(0).phase();

			_s -> addCtrlEqCNF( vc, v, v0, c0 );

		}
		else
		{
			assert( n->isAnd() );

			v = n -> getVar();
			v0 = n -> fanin(0).node() -> getVar();
			c0 = n -> fanin(0).phase();
			v1 = n -> fanin(1).node() -> getVar();
			c1 = n -> fanin(1).phase();

			_s -> addCtrlAigCNF( vc, v, v0, c0, v1, c1 );
		}
	}

	_changed.clear();
	restoreAssumps();
}
void
RarMgr::restoreAssumps()
{
	// enabled
	for( auto n : _nodes )
	{
		if ( n == 0 )  continue;
		if ( n -> isConst() || n -> isPi() ) continue;
		_s -> addAssumps( mkLit( n->getEnable(), false ) );
	}

	// disabled
	for( auto l : _disabled )
	{
		_s -> addAssumps( l );
	}

	// const
	_s -> addAssumps( mkLit( node(_constId) -> getVar(), true ) );
}

void
RarMgr::findDom( int nodeId )
{

	// TODO: only compute for changed nodes
	// using flag2
	RarNode &n = *node(nodeId);
	vector<int> idList;

	RarNode::incGFlag();
	n.DFSFanout2( idList );
	for( auto id: idList )
	{
		node(id) -> updateDoms();
	}
}
void
RarMgr::findSideInputs( int nodeId, std::vector<RarNode*> &sDoms, std::vector<RarEdge> &sides, int fanoutId )
{

	RarNode &n = *node(nodeId);
	RarNode::incGFlag(2);

	assert( node(nodeId) -> numFanin() > 0 );

	if ( fanoutId != -1 )
	{
		n.fanout(fanoutId).node() -> markSideInput( &n );
	}
	else n.markSideInput( n.fanin(0).node() );


	// store dominator and side inputs
	// TODO: let PO be a dominator with no side input?
	// 			of just check different value of current node (wire)?
	vector<int> doms = (fanoutId == -1) ? n.doms() : n.fanout(fanoutId).node() -> doms();

	for( auto id : doms )
	{
		RarNode &d = *node(id);
		if ( !d.isPo() && d.getFlag() >= -1 )
		{
			sDoms.push_back(&d);
			sides.push_back(d.fanin(-d.getFlag()));
		}
	}

}


int
RarMgr::createAnd( int id_a, bool c_a, int id_b, bool c_b )
{
	_s -> clearAssumps();

	// cout << "Create And " << (c_a ? "-" :" ") << id_a << " " << (c_b ? "-" : " ") << id_b << endl;

	Var v = _s -> createVar();
	RarNode *newNode = new RarNode( _nodes.size(), v );
	newNode -> setType( RarAnd );
	newNode -> addFanin(node(id_a), c_a);
	newNode -> addFanin(node(id_b), c_b);
	newNode -> updateLevel();
	_nodes.push_back(newNode);

	// add clause

	Var vc, v0, v1;
	v0 = node(id_a) -> getVar();
	v1 = node(id_b) -> getVar();
	vc = _s -> createVar();
	newNode -> setEnable(vc);

	_s -> addCtrlAigCNF( vc, v, v0, c_a, v1, c_b );
	
	// update _var2Id
	if ( _var2Id.size() < v )
	{
		_var2Id.resize( v+1, -1 );
	}
	_var2Id[v] = newNode->getId();


	// update dom for the whole fanin cone
	_domUpdateList.push_back( newNode -> getId() );


	return newNode->getId();
}

bool 
RarMgr::altLessThan( Lit a, Lit b )
{
	RarNode &na = *node(_var2Id[var(a)]);
	RarNode &nb = *node(_var2Id[var(b)]);

	if ( ( na.getFlag2() == 0 ) != ( na.getFlag2() == 0 ) )
	{
		return ( na.getFlag2() == 0 );
	}
	else
	{
		if ( na.numFanout() == nb.numFanout() ) return na.getLevel() < nb.getLevel();
		else return na.numFanout() > nb.numFanout();

		// if ( na.getLevel() == nb.getLevel() ) return na.numFanout() > nb.numFanout();
		// else return na.getLevel() < nb.getLevel();
	}
	

}


void
RarMgr::narAddEdge( int toId, int fromId, bool phase ) 
{
	RarEdge &e = node(toId) -> fanin(0);
	int newId = createAnd( e.node()->getId(), e.phase(), node(fromId)->getId(), phase );
	narReplace( e.node()->getId(), newId, false, false, node(toId) );
}

/*
	return true if success
	return false if conflict is caused by fault test
*/
bool 
RarMgr::recursiveLearn( int nodeId, bool val, vector<Lit> &imp, vector<RarNode*> &sDoms, vector<RarEdge> &sides )
{
	RarNode::incGFlag(2);
	node(nodeId) -> DFSMarkFanout();
	RarNode::incGFlag(1);

	// flag < -2: none
	// flag = -2: checked 1
	// flag = -1: checked 2
	// flag =  0: checked 1 2

	// flag > -3 (&& flag % 2 == 0) checked 1
	
	_neighbors.clear();
	for ( auto l : imp )
	{
		RarNode &n =  *(node(_var2Id[var(l)]));
		n.findNeighbor( _neighbors );
	}

	// cout << "# imp: " << imp.size() << endl;
	// cout << "# NB: " << _neighbors.size() << endl; 

	// decision 
	Var vBase = node(nodeId) -> getVar();
	MySolver &s = *_s;
	int level = s.getDecisionLevel();
	int conflCnt = 0;
	int undefCnt = 0;


	// imp filter:
	// -1, 0: 0 checked
	// -2, 0: 1 checked
	RarNode::incGFlag(3);
	vector<Lit> imp_tmp;
	for( auto v : _neighbors )
	{
		assert( s.getDecisionLevel() == level );

		if ( s.value(v) == l_Undef )
		{
			RarNode &ni = *node(_var2Id[v]);

			// try 0
			imp_tmp.clear();
			if ( ni.getFlag() < -1 &&  !s.makeDecision(v, 0, &imp_tmp) )
			{
				conflCnt++;
				if ( s.getDecisionLevel() < level &&
					( !propConstraint( sDoms, sides, nullptr ) ||
				 	 !s.makeDecision( vBase, val, nullptr ) ) )
				{
					return false;
				}
				continue;
			}
			else
			{
				s.backTrackTo(level);
			}
			

			// imp filter
			for( auto l : imp_tmp )
			{
				RarNode &n = *node(_var2Id[ var(l) ]);
				if ( sign(l) )
				{
					if ( n.getFlag() < -2 ) n.setFlag(-1);
					else if ( n.getFlag() == -2 ) n.setFlag(0);
				}
				else
				{
					if ( n.getFlag() < -2 ) n.setFlag(-2);
					else if ( n.getFlag() == -1 ) n.setFlag(0);
				}
			}

			// try 1
			imp_tmp.clear();
			if ( ( ni.getFlag() < -2 || ni.getFlag() == -1) && !s.makeDecision(v, 1, &imp_tmp) )
			{
				conflCnt++;
				if ( s.getDecisionLevel() < level &&
					( !propConstraint( sDoms, sides, nullptr ) ||
				 	 !s.makeDecision( vBase, val, nullptr ) ) )
				{
					return false;
				}
			}
			s.backTrackTo(level);

			// imp filter
			for( auto l : imp_tmp )
			{
				RarNode &n = *node(_var2Id[ var(l) ]);
				if ( sign(l) )
				{
					if ( n.getFlag() < -2 ) n.setFlag(-1);
					else if ( n.getFlag() == -2 ) n.setFlag(0);
				}
				else
				{
					if ( n.getFlag() < -2 ) n.setFlag(-2);
					else if ( n.getFlag() == -1 ) n.setFlag(0);
				}
			}


		}
	}


	s.backTrackTo(level-1);
	int old = imp.size();
	imp.clear();
	assert( s.makeDecision( vBase, val, &imp ) );

	// cout << "# undef: " << undefCnt << endl;
	// cout << "# confl: " << conflCnt << endl;

	return true;

}

void
RarMgr::updateLevel()
{
	vector<int> idList;
	RarNode::incGFlag();

	for( auto id : _levelUpdateList )
	{
		if ( _nodes[id] != 0 ) node(id) -> DFSFanout( idList );
	}

	for( int i = idList.size() - 1; i >= 0; i -- )
	{
		node(idList[i]) -> updateLevel();
	}

	_levelUpdateList.clear();
}
void
RarMgr::updateDom()
{
	vector<int> idList;
	RarNode::incGFlag();

	for( auto id : _domUpdateList )
	{
		if ( _nodes[id] != 0 ) node(id) -> DFSFanin( idList );
	}

	for( int i = idList.size() - 1; i >= 0; i -- )
	{
		node(idList[i]) -> updateDoms();
	}

	_domUpdateList.clear();
}