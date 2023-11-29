#pragma once

#include <vector>

template<class T>
void myRemove( std::vector<T> &vec, int idx )
{
	vec[idx] = vec[vec.size()-1];
	vec.pop_back();
}

template<class T>
void myFindAndRemove( std::vector<T> &vec, T val )
{
	int i = 0;
	for( ; i < vec.size(); i++  )
	{
		if ( vec[i] == val ) break;
	}
	assert( i < vec.size() );
	vec[i] = vec[vec.size()-1];
	vec.pop_back();
}

template<class T>
int myFind( std::vector<T> &vec, T val )
{
	int i = 0;
	for( ; i < vec.size(); i++  )
	{
		if ( vec[i] == val ) break;
	}
	if ( i == vec.size() ) return -1;
	else return i;
}