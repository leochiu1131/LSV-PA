#pragma once

#include "base/io/ioAbc.h"
#include "base/abc/abc.h"
#include "sat/bsat2/Solver.h"
#include "extRar/util.h"

#include <vector>

using namespace std;

#define NEG 0x1

typedef long long flag_t;

enum RarType
{
	RarPi,
	RarPo,
	RarAnd,
	RarConst
};


class RarNode;

class RarEdge
{
public:

	RarEdge() { _node = 0; }
	RarEdge( RarNode *n, bool c = false ): _node( (size_t) n + c ) {}

	RarEdge( const RarEdge &e ): _node(e._node) {}
	RarEdge &operator=( const RarEdge &e ) { _node = e._node; return *this; }

	RarNode *node() const { return (RarNode*)(_node & (~NEG)); }
	bool phase() const { return _node & NEG; }
	bool operator==( const RarEdge &e ) const { return node() == e.node(); }
	bool operator==( const RarNode *pNode ) const { return node() == pNode; }
	bool isNull() const { return _node == 0; }
	void set( RarNode *n, bool c ) { _node = (size_t)n + c; }

private:

	size_t	_node;
};


class RarNode
{

public:

	// constructor
	RarNode( int id, Minisat::Var v ): 
	_id(id), _var(v), _enable(-1), 
	_level(0), _flag(0), _flag2(0) {}

	// graph
	/*
		add fanout at the same time
	*/
	void addFanin( RarNode* n, bool c ) 
	{
		_fanin.emplace_back( n, c );
		n -> _fanout.emplace_back( this, c );
	}
	/*
		doesn't handle fanin
	*/
	void addFanout( RarNode* n, bool c ) { _fanout.emplace_back( n, c ); }
	void removeFanout( RarNode* n );
	void removeFanin( int idx ) { myRemove<RarEdge>( _fanin, idx ); }
	void clearFanout() { _fanout.clear(); }

	int numFanin() const { return _fanin.size(); };
	int numFanout() const { return _fanout.size(); };
	RarEdge &fanin( int i ) { assert(i < _fanin.size()); return _fanin[i]; }
	RarEdge &fanout( int i ) { assert(i<_fanout.size()); return _fanout[i]; }
	vector<int> &doms() { return _doms; }
	void updateDoms();

	// traverse
	void DFSFanin( vector<int> &idList );
	void DFSFanout( vector<int> &idList );
	void DFSFanout2( vector<int> &idList );
	void DFSUnmarkFanin();
	void DFSMarkFanout();
	void DFSMarkFanin();
	void markSideInput( RarNode * from );
	void findNeighbor( vector<Minisat::Var> &nb, int level = 0 );

	// set
	void setVar( Minisat::Var v ) { _var = v; }
	void setId( int id ) { _id = id; }
	void setType( RarType t ) { _type = t; }
	void setEnable( Minisat::Var v ) { _enable = v; }
	void setLevel( int lv ) { _level = lv; }
	void updateLevel();

	// access
	Minisat::Var getVar() const { return _var; }
	int getId() const { return _id; }
	int getFanoutId( int i ) const { return _fanout[i].node() -> getId(); }
	RarType getType() const { return _type; }
	Minisat::Var getEnable() const { return _enable; } 
	int getLevel() const { return _level; }

	// check
	// TODO: change this if the circuit structure is going to change
	bool isPo() const { return _type == RarPo; }
	bool isAnd() const { return _type == RarAnd;}
	bool isPi() const { return _type == RarPi; }
	bool isConst() const { return _type == RarConst;}


	// flag
	static void resetGFlag() { _gFlag = _gFlag2 = 0; }
	static void incGFlag( int i = 1 ) 
	{ flag_t old = _gFlag; _gFlag += i; assert(_gFlag > old); }
	static void incGFlag2( int i = 1 )
	{ flag_t old = _gFlag2; _gFlag2 += i; assert(_gFlag2 > old); }
	flag_t getFlag() const { return _flag - _gFlag; }
	void setFlag( flag_t i = 0 ) { _flag = _gFlag + i; }
	flag_t getFlag2() const { return _flag2 - _gFlag2; }
	void setFlag2( flag_t i = 0 ) { _flag2 = _gFlag2 + i; }

	// print
	void printNode();

private: 

	int				_id;
	Minisat::Var	_var;
	/* 	
		not used for pi and const
		TODO: chang enable to lit
	*/
	Minisat::Var 	_enable;
	RarType 		_type;
	int 			_level;

	//flag
	flag_t				_flag;
	static flag_t		_gFlag;
	flag_t				_flag2;
	static flag_t		_gFlag2;

	// graph
	vector<RarEdge>	_fanin;
	vector<RarEdge>	_fanout;

	/*
		absolute dominators
		_dom[0] is closest to PO
	*/
	vector<int>		_doms;


};