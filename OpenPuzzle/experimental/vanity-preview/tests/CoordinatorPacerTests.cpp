#include "CoordinatorPacer.hpp"
#include <cassert>
#include <chrono>
#include <iostream>
using namespace std::chrono_literals;
using openpuzzle::vanityPreview::CoordinatorPacer;
int main(){
 CoordinatorPacer p;auto t=CoordinatorPacer::Clock::time_point{};
 assert(p.due(0,t));p.acknowledged(0,t);
 assert(!p.due(0,t+20s));assert(p.due(10,t+1ms));p.acknowledged(10,t+1ms);
 assert(!p.due(20,t+1s));assert(!p.due(30,t+4s));assert(p.due(40,t+5001ms));
 assert(!p.confirmed(40));p.acknowledged(40,t+5001ms);assert(p.confirmed(40));
 assert(!p.due(40,t+30s));assert(p.due(50,t+5100ms,true));
 // An unacknowledged request never advances the pacer; a retry remains due.
 assert(p.due(50,t+20s));assert(p.due(50,t+20s));
 CoordinatorPacer resumed;assert(resumed.due(100,t,true));resumed.acknowledged(100,t);
 assert(!resumed.due(101,t+1s));assert(resumed.due(101,t+5s));
 std::cout<<"CoordinatorPacerTests passed: monotonic deadline, coalescing, mandatory flush/probe and acknowledgement gates.\n";
}
