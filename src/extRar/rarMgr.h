#pragma once 

#include "base/io/ioAbc.h"
#include "base/abc/abc.h"
#include "extRar/MySolver.h"
#include "extRar/rarNode.h"


#include <cassert>
#include <vector>


struct OptEntry
{
	OptEntry(): id_a(0), id_b(0), gain(0) {}
	OptEntry( int a, int b, int g ): id_a(a), id_b(b), gain(g) {}
	OptEntry( const OptEntry& e ): id_a(e.id_a), id_b(e.id_b), gain(e.gain), alt(e.alt) {}

	OptEntry( int a, int b, int g, RarEdge e0, RarEdge e1 ): id_a(a), id_b(b), gain(g) 
	{
		alt.push_back(e0);
		alt.push_back(e1);
	}

	OptEntry &operator=( const OptEntry& e )
	{
		id_a = e.id_a;
		id_b = e.id_b;
		gain = e.gain;
		alt = e.alt;
		return *this;
	}

	int id_a;
	int id_b;
	int gain;

	vector<RarEdge> alt;

	// TODO: level
};
struct LessThan_OptEntry
{
    inline bool operator() (const OptEntry& a, const OptEntry& b)
    {
        return ( a.gain > b.gain);
    }
};

class RarMgr
{

public:

	RarMgr(): _s(nullptr), _flag_learn(false) {}
	~RarMgr();

	void reset();
	void fromNtk( Abc_Ntk_t *pNtk );

	// helper
	void findAllDom();
	void genNetlist();
	void updateLevel();
	void updateDom();

	// set flags
	void set( bool flag_learn ) {_flag_learn = flag_learn; };

	// rar
	void rarAll();
	void rar( int nodeId );
	bool rar( int nodeId, int fanoutId, int effort );
	bool redundancyCheck( int nodeId, std::vector<RarNode*> &doms, std::vector<RarEdge> &sides, std::vector<Minisat::Lit> *imp, bool val );
	bool propConstraint( std::vector<RarNode*> &doms, std::vector<RarEdge> &sides, std::vector<Minisat::Lit> *imp, int toLevel = -1 );
	void makeAltWire( RarNode *t, RarNode *d, RarNode *s );


	// (fake) recursive learning
	bool recursiveLearn( int nodeId, bool val, vector<Minisat::Lit> &imp, vector<RarNode*> &sDoms, vector<RarEdge> &sides );


	// nar
	void nar();
	bool nar( int nodeId );
	void findDom( int nodeId );
	void findSideInputs( int nodeId, std::vector<RarNode*> &sDoms, std::vector<RarEdge> &sides, int fanoutId=-1 );
	void narReplace( int nodeId, int rId, bool phase, bool finish = true, RarNode* toNode = 0 );
	void narAddEdge( int toId, int fromId, bool phase );
	int createAnd( int id_a, bool c_a, int id_b, bool c_b );

	// return a < b
	bool altLessThan( Minisat::Lit a, Minisat::Lit b ); 

	// perform opt
	void opt();
	void removeEdge( int id_a, int id_b );
	void addEdge( int id_a, int id_b, bool c );
	void sweepFloating();
	/*
		for nodes in changed
		create new clause
		and update all assumps
	*/
	void updateChanged();
	void restoreAssumps();

	// aig
	Abc_Ntk_t* genAig( Abc_Ntk_t* pNtk );

	// access
	RarNode *node( int id ) { 
		assert(id >= 0);
		assert(id < _nodes.size());
		assert(_nodes[id] != 0);
		return _nodes[id]; 
	}
	RarNode *node( Abc_Obj_t *p ) { return node(Abc_ObjId(p)); }

	// print
	void printAig();


	// test
	void test();

private:

	Minisat::MySolver			*_s;
	Abc_Ntk_t					*_pNtk;

	// implies that 
	// netlist and doms are correct
	bool						_domOk;
	bool						_netListOk;

	vector<RarNode*>			_nodes;
	vector<int> 				_var2Id;
	vector<int>					_id2NewId;

	vector<int>					_piList;
	vector<int>					_poList;
	vector<int>					_netList;
	vector<int>					_fltList;
	vector<int>					_levelUpdateList;
	vector<int>					_domUpdateList;

	// rar
	vector<OptEntry>			_optList;


	// nar
	int 						_constId;
	vector<Minisat::Lit>		_disabled;
	vector<RarNode*>			_changed;
	vector<Minisat::Var>		_neighbors;


	// flags
	bool						_flag_learn;
};
