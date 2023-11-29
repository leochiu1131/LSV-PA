
#include <iostream>
#include <algorithm>
#include <iomanip>

#include "extRar/rarNode.h"
// #include "extRar/util.h"
#include "sat/bsat2/Solver.h"

using namespace std;

flag_t RarNode::_gFlag = 0;
flag_t RarNode::_gFlag2 = 0;

void
RarNode::removeFanout( RarNode *n )
{
	for( int i = 0; i < _fanout.size(); i++ )
	{
		if ( _fanout[i] == n )
		{
			_fanout[i] = _fanout[_fanout.size()-1];
			_fanout.pop_back();
		}
	}
}

void
RarNode::DFSFanin( vector<int> &idList )
{
	if ( getFlag() == 0 ) return;

	setFlag();

	for( auto fanin : _fanin )
	{
		fanin.node() -> DFSFanin( idList );
	}
	idList.push_back( _id );
	return;
}

/*
	used for computing levels
	use flag
*/
void
RarNode::DFSFanout( vector<int> &idList )
{
	if ( getFlag() == 0 ) return;

	setFlag();

	for( auto fanout : _fanout )
	{
		fanout.node() -> DFSFanout( idList );
	}
	idList.push_back( _id );
	return;
}
void
RarNode::DFSFanout2( vector<int> &idList )
{
	if ( getFlag2() == 0 ) return;

	setFlag2();

	for( auto fanout : _fanout )
	{
		fanout.node() -> DFSFanout2( idList );
	}
	idList.push_back( _id );
	return;
}
void
RarNode::DFSUnmarkFanin()
{
	if ( getFlag2() != 0 ) return;

	setFlag2(-1);

	for( auto fanin : _fanin )
	{
		fanin.node() -> DFSUnmarkFanin();
	}
	return;
}
/*
	used to filtered out fanout cones
	use flag
*/
void
RarNode::DFSMarkFanout()
{
	if ( getFlag() == 0 ) return;
	setFlag();
	for( auto fanout : _fanout ) fanout.node() -> DFSMarkFanout();
	return;
}
void
RarNode::DFSMarkFanin()
{
	if ( getFlag() == 0 ) return;
	setFlag();
	for( auto fanin : _fanin ) fanin.node() -> DFSMarkFanout();
	return;
}

/*
	< -1: none
	0: fanin0
	-1: fanin1
*/
void
RarNode::markSideInput( RarNode* from )
{
	// assume all nodes will be visited at most twice
	if ( getFlag() >= -1 ) 	// visited twice, no side input
	{
		setFlag(-2); 
		return;
	}

	// visit fanout cone
	for( auto e : _fanout )
	{
		e.node() -> markSideInput( this );
	}

	// check fanin
	if ( _fanin[0] == from ) setFlag(-1);
	else setFlag(0);
}

void
RarNode::updateDoms()
{
	if ( numFanout() == 0 ) 
	{
		_doms.clear();
		return;
	}

	_doms = fanout(0).node() -> doms();
	_doms.push_back( getFanoutId(0) );

	for( int i = 1; i < numFanout(); i++ )
	{
		vector<int> doms_new;
		vector<int> &doms_fanout = fanout(i).node() -> doms() ;
		for( auto id : _doms )
		{
			if ( 
				id == getFanoutId(i) ||
				find( doms_fanout.begin(), doms_fanout.end(), id) != doms_fanout.end() 
			)
			{
				doms_new.push_back(id);
			}
		}
		_doms = doms_new;
	}
}

void 
RarNode::updateLevel()
{
	_level = 0;
	for ( auto fanin : _fanin )
	{
		if ( fanin.node() -> getLevel() >= _level )
		{
			_level = fanin.node() -> getLevel() + 1;
		}
	}
}

void
RarNode::printNode()
{
	cout << "  " << setw(5) << left << _id;
	if ( _fanin.empty() ) 
	{
		cout << endl;
		return;
	}
	cout << " = ";
	for ( auto fanin : _fanin )
	{
		cout << (fanin.phase() ? "-" : " ")
			<< setw(5) << left << fanin.node() -> getId() 
			<< " ";
	}
	cout << endl;
}



void
RarNode::findNeighbor( vector<Minisat::Var> &nb, int level )
{
	if ( level < 2 )
	{
		if ( level == 1 && ( getFlag() == -2 || getFlag() == 0 ) ) return;

		for ( auto fanin : _fanin )
		{
			fanin.node() -> findNeighbor(nb, level+1);
		}
		for ( auto fanout : _fanout )
		{
			fanout.node() -> findNeighbor(nb, level+1);
		}

		if ( level == 1  ) 
		{
			if ( getFlag() < -2  ) setFlag( -2 );
			else setFlag(0);
		}
		else
		{
			setFlag(0);
		}
		
	}
	else
	{
		if ( getFlag() > -2 ) return;

		nb.push_back(_var);

		if ( getFlag() == -2 ) setFlag(0);
		else setFlag(-1);
	}
	
}