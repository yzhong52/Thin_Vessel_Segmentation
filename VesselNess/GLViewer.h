#pragma once

#include "Graph.h"
#include "Edge.h"
#include "DataTypes.h"

#include <vector>
using namespace std;

namespace GLViewer
{
	void MIP( unsigned char* data, int x, int y, int z, void (*draw_func)(void) = NULL );
}

