#include "extRar/rarMgr.h"
#include "misc/bzlib/bzlib.h"
#include "misc/zlib/zlib.h"

#include <iostream>
#include <algorithm>
#include <iomanip>


using namespace std;
using namespace Minisat;


inline int id( Abc_Obj_t *pObj ) { return Abc_ObjId(pObj); }

RarMgr::~RarMgr()
{
	reset();
}

void
RarMgr::reset()
{

	if ( _s ) delete _s;

	for( auto p : _nodes)
	{
		if (p) delete p;
	}
	_nodes.clear();
	_var2Id.clear();
	_changed.clear();
	_disabled.clear();

	_netListOk = false;
	_domOk = false;

}

void
RarMgr::fromNtk( Abc_Ntk_t* pNtk )
{
	Abc_Obj_t* pNode;
	int i;
	Var v;

	// init
	reset();
	_nodes.resize( Abc_NtkObjNum(pNtk), 0 );
	_var2Id.reserve( Abc_NtkObjNum(pNtk) );
	_s = new MySolver;
	MySolver &s = *_s;

	for( i = 0; i < _nodes.size(); i++ )
	{
		v = s.createVar();
		_nodes[i] = new RarNode(i, v);
		_var2Id.push_back(v);
	}

	Var v0, v1, vc;
	bool c0, c1;


	// pi
	Abc_NtkForEachPi( pNtk, pNode, i )
	{
		_piList.push_back(id(pNode));
		node(pNode) -> setType(RarPi);
	}


	// po
	Abc_NtkForEachPo( pNtk, pNode, i )
	{
		RarNode &n = *node(pNode);
		n.setType(RarPo);
		_poList.push_back(id(pNode));

		v = n.getVar();
		v0 = node(Abc_ObjFanin0( pNode )) -> getVar();
		c0 = Abc_ObjFaninC0( pNode );

		vc = s.createVar();
		n.setEnable(vc);

		s.addCtrlEqCNF( vc, v, v0, c0 );

		node(pNode) -> addFanin( node(Abc_ObjFanin0(pNode)), c0 );
	}

	// aignode
	Abc_AigForEachAnd( pNtk, pNode, i )
	{
		RarNode &n = *node(pNode);
		n.setType(RarAnd);

		v = n.getVar();
		v0 = node(Abc_ObjFanin0( pNode )) -> getVar();
		v1 = node(Abc_ObjFanin1( pNode )) -> getVar();
		c0 = Abc_ObjFaninC0( pNode );
		c1 = Abc_ObjFaninC1( pNode );

		vc = s.createVar();
		n.setEnable(vc);

		s.addCtrlAigCNF( vc, v, v0, c0, v1, c1 );

		n.addFanin( node(Abc_ObjFanin0(pNode)), c0 );
		n.addFanin( node(Abc_ObjFanin1(pNode)), c1 );
	}

	// const 1
	pNode = Abc_AigConst1( pNtk );
	node(pNode)->setType(RarConst);
	_constId = Abc_ObjId(pNode);

	// enable
	restoreAssumps();

	genNetlist();
	_pNtk = pNtk;
}

void
RarMgr::findAllDom()
{
	// assume netlist is generated
	for( int i = _netList.size()-1; i >= 0; i-- )
	{
		node(_netList[i]) -> updateDoms();
	}
	_domOk = true;


	// print
	// for( auto id : _netList )
	// {
	// 	cout << id << ": ";
	// 	auto &doms = node(id) -> doms();
	// 	for ( auto did : doms )
	// 	{
	// 		cout << " " << did;
	// 	}
	// 	cout << endl;
	// }
}

void
RarMgr::genNetlist()
{
	_netList.clear();
	RarNode::incGFlag();
	for( auto id : _poList )
	{
		node(id) -> DFSFanin( _netList );
	}
	_netListOk = true;

	// update level by the way
	node(_constId) -> updateLevel();
	for( auto id : _netList )
	{
		node(id) -> updateLevel();
	}


	// print
	// cout << "netlist: " << endl;
	// for( auto id : _netList )
	// {
	// 	cout << id  << endl;
	// }
}

void
RarMgr::rarAll()
{

	// Note: _flag2 is used as the index of optlist
	// therefore the val can be greater than _gFlag2
	// may need to increase more in the second run
	// (or reset every _flag2 & _gFlag2)
	RarNode::incGFlag2();

	// for each node, find alt wire/gate
	for( int i = _netList.size() - 1 ; i >= 0 ; i-- )
	// for( int i = 0 ; i < _netList.size() ; i++ )
	{
		rar(i);
	}

}
/*
	rar for each node
*/
void
RarMgr::rar( int nodeId )
{
	RarNode &n = *node(nodeId);
	if ( n.isPo() ) return;					// const / pi / po
	if ( n.numFanout() == 0 ) return;

	cout << "rar node " << nodeId << endl;

	// check how many fanout cannot be removed currently
	int fanoutFlag2 = n.fanout(0).node() -> getFlag2();
	for ( int i = 1; i < n.numFanout(); i++  )
	{
		if ( n.fanout(i).node() -> getFlag2() != fanoutFlag2 ) 
		{
			fanoutFlag2 = -1;
			break;
		}
	}
	if ( fanoutFlag2 >= 0 ) // it's dominator can be removed
	{
		if ( !n.isPi() )
		{
			_optList[fanoutFlag2].gain += 1;
			n.setFlag2(fanoutFlag2);
			cout << "Node " << nodeId << " is a removable node B (flag: " << fanoutFlag2 << ")" << endl;
		}
		return;
	}

	// check effort
	int effort = 0;
	if ( n.numFanout() == 1 && n.isAnd() )
	{
		effort = 1;		// a node can be removed, try to find an alt wire
		if ( 
			n.fanin(0).node() -> numFanout() == 1 ||
		 	n.fanin(1).node() -> numFanout() == 1 
		) effort = 2;	// some nodes can be removed, try to find an alt gate
	}

	bool all = true;
	bool removable;
	for( int i = 0; i < n.numFanout(); i++ )
	{
		if ( n.fanout(i).node() -> getFlag2() >= 0 ) continue;
		removable = rar( nodeId, i, effort );
		if ( removable )
		{
		}
		all = all && removable;
	}

	// if all can be removed, then this node is assume to be removable
	if ( all ) 	// all fanout wire can be removed
	{
		if ( effort > 1 ) 
		{
			n.setFlag2( _optList.size() - 1 );
			cout << "Node " << nodeId << " is a removable node A" << endl;
		}
		else
		{
			cout << "Node " << nodeId << "'s all fanout is removable" << endl;
		}
	}
}

/* 
	rar for each wire
	return true if it can be removed
*/
bool
RarMgr::rar( int nodeId, int fanoutId, int effort )
{
	RarNode &n = *node(nodeId);

	// TODO: find dominators here
	// currently all doms are found before rar
	// but need to specifies for each wire
	vector<int> doms = n.fanout(fanoutId).node() -> doms();
	doms.push_back(n.getFanoutId(fanoutId));


	vector<RarNode*> sDoms;
	vector<RarEdge> sides;
	findSideInputs( nodeId, sDoms, sides, fanoutId );

	// print side inputs
	cout << "Node " << nodeId << "'s side inputs:" << endl;
	for( int i = 0; i < sDoms.size(); i++ )
	{
		cout << "\t" << sDoms[i]->getId() << " " << sides[i].phase() << " " << sides[i].node() -> getId() << endl;
	}

	// === main ===

	// redundancy check
	MySolver &s = *_s;
	vector<Lit> imp_tmp, imp;
	if ( !redundancyCheck( nodeId, sDoms, sides, &imp_tmp, n.fanout(fanoutId).phase() ) )
	{
		// the node itself is already redundant
		cout << "Node " << nodeId << "'s fanout " << fanoutId << " is redundant " << endl;
		_optList.emplace_back( nodeId, n.getFanoutId(fanoutId), effort == 0 ? 1 : 2 );
		cout << nodeId << " " << n.getFanoutId(fanoutId) << " is added to opt list" << endl;
		return true;
	}


	if ( effort == 0 ) 
	{
		// s.backTrackTo(0);
		// return false;
	}

	// copy imp
	// set counter example val of each dom
	// remove imp in the fanout of target wire
	RarNode::incGFlag();
	n.DFSMarkFanout();
	for( auto lit : imp_tmp )
	{
		if ( node(_var2Id[var(lit)]) -> getFlag() != 0 ) imp.push_back(lit);
	}

	// TODO: how to find the real cost of using a certain node
	//			maybe just remove the nodes in ffa from imp

	// for each dominator with side input
	// TODO: this also applies to doms with no side inputs
	vector<Lit> imp3;
	for( int level = sDoms.size() - 1 ; level >= 0; level -- )
	{
		cout << "Current decision level: " << s.getDecisionLevel() << endl;

		// may need to backtrack or restore
		// if it backtrack to level < i, than it's redundant from the begining?
		s.backTrackTo(level);

		RarNode &d = *sDoms[level];
		if ( s.value(d.getVar()) != l_Undef ) continue;

		// mark fanout cone (which cannot be selected as the fanin of alt wire)
		RarNode::incGFlag();
		d.DFSMarkFanout();

		// todo: return redundant dominator
		assert( s.makeDecision( d.getVar(), 1 ) );

		for ( auto lit : imp )
		{
			if ( _var2Id[var(lit)] == sDoms[level]->getId() ) continue;
			// TODO
			// if lit is in the fanout cone of d, continue;
			if ( node( _var2Id[var(lit)] ) -> getFlag() == 0 ) continue;

			if ( s.value(lit) == l_True )
			{
				continue;
			}
			else if ( s.value(lit) == l_False )
			{
				// inverse imp, alt wire found
				cout << "Alt wire found (inverse imp)" << endl;
				_optList.emplace_back( nodeId, n.getFanoutId(fanoutId), effort == 0 ? 1 : 2, RarEdge(&d), RarEdge( node(_var2Id[var(lit)]), !sign(lit) ) );
				cout << nodeId << " " << n.getFanoutId(fanoutId) << " is added to opt list" << endl;
				cout << "Alt wire: " << _var2Id[var(lit)] << " " << d.getId() << endl;

				// todo: keep finding
				s.backTrackTo(0);
				return true;
			}
			else
			{
				// unknown, make decision
				if ( !s.makeDecision( lit ) )
				{
					// conflict, alt wire found
					cout << "Alt wire found (conflict)" << endl;
					_optList.emplace_back( nodeId, n.getFanoutId(fanoutId), effort == 0 ? 1 : 2, RarEdge(&d), RarEdge( node(_var2Id[var(lit)]), !sign(lit) ) );
					cout << nodeId << " " << n.getFanoutId(fanoutId) << " is added to opt list" << endl;
					cout << "Alt wire: " << _var2Id[var(lit)] << " " << d.getId() << endl;

					// TODO:
					// keep finding
					// handle new imp from conflict
					s.backTrackTo(0);
					return true;
				}
				s.backTrackTo(level+1);
				// TODO:
				// rar gate
			}
		}
	}

	s.backTrackTo(0);
	return false;

}
void 
RarMgr::makeAltWire( RarNode *t, RarNode *d, RarNode *s )
{
}

void
RarMgr::opt()
{
	LessThan_OptEntry cmp;
	if ( ! _optList.empty() )
	{
		sort( _optList.begin(), _optList.end(), cmp );

		cout << "# Tar: "  << _optList.size() << endl;
		cout << "Max Gain: " << _optList[0].gain << endl;

		cout << "Opt list: " << endl;
		for ( auto o : _optList )
		{
			cout << " " << o.gain << "   "  << o.id_a << " " << o.id_b << endl;
		}

		vector<RarEdge> &alt = _optList[0].alt;

		if ( alt.size() == 2 )
		{
			int from = alt[1].node() -> getId();
			int to = alt[0].node() -> getId();

			bool c = alt[1].phase();

			if ( to == _optList[0].id_b && node(to) -> fanin(1) == node(_optList[0].id_a) )
			{
				_optList[0].id_b = _nodes.size();
			}
			else
			{
				cout << "nuh" << endl;
			}
			addEdge( from, to, c );
			
		}
		else if ( alt.size() == 3 )
		{

		}
		removeEdge( _optList[0].id_a, _optList[0].id_b );

		cout << "done" << endl;
		sweepFloating();
	}
}
void
RarMgr::removeEdge( int id_a, int id_b )
{
	_domOk = false;

	cout << "Removing Edge: " << id_a << " " << id_b << endl;
	RarNode *pA = node(id_a);
	RarNode *pB = node(id_b);
	RarNode *pS;
	bool cS;

	if ( pB -> fanin(0) == pA ) 
	{
		pS = pB -> fanin(1).node();
		cS = pB -> fanin(1).phase();
	}
	else
	{
		pS = pB -> fanin(0).node();
		cS = pB -> fanin(0).phase();
		assert( pB->fanin(1) == pA );
	}

	cout << "Saved fanin: " << pS -> getId() << endl;

	RarNode *pT;
	bool cT, cNew;
	int faninIdx;
	for ( int i = 0; i < pB -> numFanout(); i++ )
	{
		pT = pB->fanout(i).node();
		cT = pB->fanout(i).phase();
		faninIdx = pT -> fanin(0) == pB ? 0 : 1;
		assert( pT -> fanin(faninIdx) == pB );

		cNew = cS ? !cT : cT;
		
		pS -> addFanout( pT, cNew );
		pT -> fanin(faninIdx).set( pS, cNew );
	}
	pB -> clearFanout();
	_fltList.push_back(pB->getId());
}
void
RarMgr::addEdge( int id_a, int id_b, bool c )
{
	_netListOk = false;
	_domOk = false;

	assert( ! node(id_a) -> isConst() );
	assert( ! node(id_a) -> isPo() );
	assert( ! node(id_b) -> isPo() );
	assert( node(id_b) -> isAnd() );
	// TODO: handle po and const

	cout << "Adding Edge: " << id_a << " " << id_b << " " << c << endl;

	// find rewire fanin of b
	RarNode &a = *node(id_a);
	RarNode &b = *node(id_b);
	RarNode &f = *b.fanin(1).node();
	bool c_fanin = b.fanin(1).phase();

	// create new aig node
	RarNode *newNode = new RarNode( _nodes.size(), _s -> createVar() );
	_nodes.push_back( newNode );
	newNode->setType( RarAnd );

	// add fanin
	newNode -> addFanin( &a, c );
	newNode -> addFanin( &f, c_fanin );

	// handle f' fanout
	f.removeFanout( &b );
	// TODO: handle the alt and tar wire
	// 			of f -> b

	// handle b's fanin
	b.removeFanin(1);
	b.addFanin( newNode, 0 );

}

void
RarMgr::sweepFloating()
{
	// cout << "Sweeping floating nodes." << endl;

	int count = 0;
	for( int i = 0; i < _fltList.size(); i++ )
	{
		RarNode *n = node(_fltList[i]);
		if ( !n -> isAnd() ) continue;

		assert( n->numFanout() == 0);

		n -> fanin(0).node() -> removeFanout(n);
		if ( n -> fanin(0).node() -> numFanout() == 0 ) _fltList.push_back(n -> fanin(0).node() -> getId() );
		else _domUpdateList.push_back( n -> fanin(0).node() -> getId() );


		n -> fanin(1).node() -> removeFanout(n);
		if ( n -> fanin(1).node() -> numFanout() == 0 ) _fltList.push_back(n -> fanin(1).node() -> getId() );
		else _domUpdateList.push_back( n -> fanin(1).node() -> getId() );
		count ++;

		_disabled.push_back( mkLit( n->getEnable(), true ) );

		delete n;
		_nodes[_fltList[i]] = 0;
	}

	_fltList.clear();
	// cout << "Sweep " << count << " floating nodes." << endl;
}


bool
RarMgr::redundancyCheck( int nodeId, vector<RarNode*> &doms, vector<RarEdge> &sides, vector<Lit> *imp, bool val )
{
	MySolver &s = *_s;

	// dominators side input
	if ( !propConstraint( doms, sides, imp) ) return false;

	if ( !s.makeDecision( node(nodeId)->getVar(), val, imp ) )
	{
		// conflict
		s.backTrackTo(0);
		return false;
	}


	return true;
}
bool
RarMgr::propConstraint( vector<RarNode*> &doms, vector<RarEdge> &sides, vector<Lit> *imp, int toLevel )
{
	MySolver &s = *_s;

	int n;
	if ( toLevel == -1 ) n = doms.size();
	else n = toLevel;

	// dominators side input
	for( int i = s.getDecisionLevel(); i < doms.size(); i++  )
	{
		if ( !s.makeDecision( sides[i].node() -> getVar(), !sides[i].phase(), imp ) )
		{
			// conflict
			s.backTrackTo(0);
			return false;
		}
	}

	return true;
}

void
RarMgr::test()
{
	// MySolver &s = *_s;
	// vector<Lit> imp;


}

Abc_Ntk_t *
RarMgr::genAig(  Abc_Ntk_t *pNtk )
{

    Vec_Ptr_t * vNodes;
    Abc_Obj_t * pObj, * pNode0, * pNode1;
    Abc_Ntk_t * pNtkNew;
    int i;
	int nInputs = _piList.size();
	int nOutputs = _poList.size();
	int nAnds = 0;
	int uLit, uLit0, uLit1;
	int count;

	// map current id to new id
	_id2NewId.clear();
	_id2NewId.resize(_nodes.size(), 0 );
	genNetlist();

	for( auto id : _netList )
	{
		RarNode &n = *node(id);
		if ( !n.isAnd() ) continue;
		nAnds ++;
		_id2NewId[id] = nAnds + nInputs;
	}
	for( i = 0; i < _piList.size(); i++ )
	{
		_id2NewId[_piList[i]] = i+1;
	}

    // allocate the empty AIG
    pNtkNew = Abc_NtkAlloc( ABC_NTK_STRASH, ABC_FUNC_AIG, 1 );
    pNtkNew->pName = Extra_UtilStrsav( pNtk->pName );
    pNtkNew->pSpec = Extra_UtilStrsav( pNtk->pSpec );
    pNtkNew->nConstrs = pNtk->nConstrs;	// ??

    // prepare the array of nodes
    vNodes = Vec_PtrAlloc( 1 + nInputs + nAnds );
    Vec_PtrPush( vNodes, Abc_ObjNot( Abc_AigConst1(pNtkNew) ) );

    // create the PIs
    for ( i = 0; i < nInputs; i++ )
    {
        pObj = Abc_NtkCreatePi(pNtkNew);    
        Vec_PtrPush( vNodes, pObj );
    }
    // create the POs
    for ( i = 0; i < nOutputs; i++ )
    {
        pObj = Abc_NtkCreatePo(pNtkNew);   
    }

    // create the AND gates
	count = 0;
	for ( auto id : _netList )
	{
		RarNode &n = *node(id);
		if ( !n.isAnd() ) continue;

		uLit = _id2NewId[id] << 1;
		uLit0 = ( (_id2NewId[n.fanin(0).node()->getId()] << 1) + (int)n.fanin(0).phase() );
		uLit1 = ( (_id2NewId[n.fanin(1).node()->getId()] << 1) + (int)n.fanin(1).phase() );

		// cout << id << " " << (n.fanin(0).phase() ? "-" : " ") << n.fanin(0).node() -> getId() << " " << (n.fanin(1).phase() ? "-" : " ") << n.fanin(1).node()->getId() << endl;
        pNode0 = Abc_ObjNotCond( (Abc_Obj_t *)Vec_PtrEntry(vNodes, uLit0 >> 1), uLit0 & 1 );
        pNode1 = Abc_ObjNotCond( (Abc_Obj_t *)Vec_PtrEntry(vNodes, uLit1 >> 1), uLit1 & 1 );
		assert( Vec_PtrSize(vNodes) == count + 1 + nInputs );
        Vec_PtrPush( vNodes, Abc_AigAnd((Abc_Aig_t *)pNtkNew->pManFunc, pNode0, pNode1) );
		count ++;
	}

	// read the PO driver literals
	Abc_NtkForEachPo( pNtkNew, pObj, i )
	{
		RarNode &n = *node(_poList[i]);
		uLit0 = (_id2NewId[ n.fanin(0).node() -> getId() ] << 1) + (int)n.fanin(0).phase();
		pNode0 = Abc_ObjNotCond( (Abc_Obj_t *)Vec_PtrEntry(vNodes, uLit0 >> 1), (uLit0 & 1) );
		Abc_ObjAddFanin( pObj, pNode0 );
	}

	Abc_NtkShortNames( pNtkNew );

    // skipping the comments
    Vec_PtrFree( vNodes );

    // remove the extra nodes
    Abc_AigCleanup( (Abc_Aig_t *)pNtkNew->pManFunc );
	assert( Abc_NtkCheckRead( pNtkNew ) );

    return pNtkNew;
}
void
RarMgr::printAig()
{
	genNetlist();
	for( auto id : _netList )
	{
		node(id) -> printNode();
	}
}
