#pragma once
// BISCUIT NODE and JCMK Node: the whole device becomes a scan-only wardriving
// node for a Biscuit Manager or a JustCallMeKoko dual-band core. While a node
// runs it owns both radios; detection, alerts, logging and the other tools stop
// until it is turned off. See docs/NODE-MODES.md.
#include <Arduino.h>
#include "node_link.h"

enum class NodeKind : uint8_t {None=0,Biscuit=1,Jcmk=2};

// Menu and screen names, exactly as they appear under Tools > Modes.
constexpr const char* NodeBiscuitName="BISCUIT NODE";
constexpr const char* NodeJcmkName="JCMK Node";
inline const char* nodeName(NodeKind kind){return kind==NodeKind::Biscuit?NodeBiscuitName:kind==NodeKind::Jcmk?NodeJcmkName:"";}

// True from the start of a node session until the normal radar has its radios back.
bool nodeBusy();
NodeKind nodeKind();
// Starts (or switches to) a node. On failure returns false and leaves a short,
// screen-ready reason in nodeError(). The choice is remembered across restarts.
bool nodeStart(NodeKind kind);
// Ends the node, gives the radios back to the radar and forgets the choice.
void nodeStop();
const char* nodeError();
// Live link state. Valid while nodeBusy(); read on the application task only.
const dragon::node::Status& nodeStatus();
uint32_t nodeElapsedMs();
bool nodeKeyed();        // JCMK Node with an ESP-NOW key set
bool nodeBleReports();   // JCMK Node also reports Bluetooth devices

// setup(): resumes a node that was running when the device lost power.
void nodeBegin();
// USB: node status|biscuit|jcmk|stop|key|bind|identity|ble.
bool nodeCommand(const String& command);
// Commands that still answer while a node runs.
bool nodeAllowsCommand(const String& command);

// Radio side, in radio.cpp.
bool radioNodeBegin(const dragon::node::Config& config,const uint8_t* pmk,const uint8_t* lmk,char* why,size_t whyLength);
void radioNodeEnd();
bool radioNodeOwned();
const dragon::node::Status& radioNodeStatus();
void radioNodeMac(uint8_t* mac);
uint32_t radioNodeReceiveDrops();
