

import std.core;
import std.regex;
import std.filesystem;
import std.memory;
import std.threading;

//#include <experimental/coroutine>

import VVE;

#include "VEDefine.h"
#include "VEHash.h"

#include "tables.h"
#include "MT.h"

using namespace vve;


int main()
{

    //tables::test();

    MT::test();


    return 0;
}
