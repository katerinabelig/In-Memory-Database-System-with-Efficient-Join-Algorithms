#include <cassert>
#include <iostream>
#include <utility>
#include <sstream>
#include "Parser.hpp"
//---------------------------------------------------------------------------
using namespace std;
//---------------------------------------------------------------------------
static void splitString(string& line,vector<unsigned>& result,const char delimiter)
  // Split a line into numbers
{
  stringstream ss(line);
  string token;
  while (getline(ss,token,delimiter)) {
    result.push_back(stoul(token));
  }
}
//---------------------------------------------------------------------------
static void splitString(string& line,vector<string>& result,const char delimiter)
  // Parse a line into strings
{
  stringstream ss(line);
  string token;
  while (getline(ss,token,delimiter)) {
    result.push_back(token);
  }
}
//---------------------------------------------------------------------------
static void splitPredicates(string& line,vector<string>& result)
  // Split a line into predicate strings
{
  // Determine predicate type
  for (auto cT : comparisonTypes) {
    if (line.find(cT)!=string::npos) {
      splitString(line,result,cT);
      break;
    }
  }
}
//---------------------------------------------------------------------------
void QueryInfo::parseRelationIds(string& rawRelations)
  // Parse a string of relation ids
{
  splitString(rawRelations,relationIds,' ');
}
//---------------------------------------------------------------------------
static SelectInfo parseRelColPair(string& raw)
{
  vector<unsigned> ids;
  splitString(raw,ids,'.');
  return SelectInfo(0,ids[0],ids[1]);
}
//---------------------------------------------------------------------------
inline static bool isConstant(string& raw) { return raw.find('.')==string::npos; }
//---------------------------------------------------------------------------
void QueryInfo::parsePredicate(string& rawPredicate)
  // Parse a single predicate: join "r1Id.col1Id=r2Id.col2Id" or "r1Id.col1Id=constant" filter
{
  vector<string> relCols;
  splitPredicates(rawPredicate,relCols);
  assert(relCols.size()==2);
  assert(!isConstant(relCols[0])&&"left side of a predicate is always a SelectInfo");
  auto leftSelect=parseRelColPair(relCols[0]);
  if (isConstant(relCols[1])) {
    uint64_t constant=stoul(relCols[1]);
    char compType=rawPredicate[relCols[0].size()];
    filters.emplace_back(leftSelect,constant,FilterInfo::Comparison(compType));
  } else {
    predicates.emplace_back(leftSelect,parseRelColPair(relCols[1]));
  }
}
//---------------------------------------------------------------------------
void QueryInfo::parsePredicates(string& text)
  // Parse predicates
{
  vector<string> predicateStrings;
  splitString(text,predicateStrings,'&');
  for (auto& rawPredicate : predicateStrings) {
    parsePredicate(rawPredicate);
  }
}
//---------------------------------------------------------------------------
void QueryInfo::parseSelections(string& rawSelections)
 // Parse selections
{
  vector<string> selectionStrings;
  splitString(rawSelections,selectionStrings,' ');
  for (auto& rawSelect : selectionStrings) {
    selections.emplace_back(parseRelColPair(rawSelect));
  }
}
//---------------------------------------------------------------------------
static void resolveIds(vector<unsigned>& relationIds,SelectInfo& selectInfo)
  // Resolve relation id
{
  selectInfo.relId=relationIds[selectInfo.binding];
}
//---------------------------------------------------------------------------
void QueryInfo::resolveRelationIds()
  // Resolve relation ids
{
  // Selections
  for (auto& sInfo : selections) {
    resolveIds(relationIds,sInfo);
  }
  // Predicates
  for (auto& pInfo : predicates) {
    resolveIds(relationIds,pInfo.left);
    resolveIds(relationIds,pInfo.right);
  }
  // Filters
  for (auto& fInfo : filters) {
    resolveIds(relationIds,fInfo.filterColumn);
  }
}
//---------------------------------------------------------------------------
void QueryInfo::parseQuery(string& rawQuery)
  // Parse query [RELATIONS]|[PREDICATES]|[SELECTS]
{
  clear();
  vector<string> queryParts;
  splitString(rawQuery,queryParts,'|');
  assert(queryParts.size()==3);
  parseRelationIds(queryParts[0]);
  parsePredicates(queryParts[1]);
  parseSelections(queryParts[2]);
  resolveRelationIds();
}
//---------------------------------------------------------------------------
void QueryInfo::clear()
  // Reset query info
{
  relationIds.clear();
  predicates.clear();
  filters.clear();
  selections.clear();
}
//---------------------------------------------------------------------------
string SelectInfo::dumpText()
  // Dump text format
{
  return to_string(binding)+"."+to_string(colId);
}
//---------------------------------------------------------------------------
string FilterInfo::dumpText()
  // Dump text format
{
  return filterColumn.dumpText()+static_cast<char>(comparison)+to_string(constant);
}
//---------------------------------------------------------------------------
string PredicateInfo::dumpText()
  // Dump text format
{
  return left.dumpText()+'='+right.dumpText();
}
//---------------------------------------------------------------------------
QueryInfo::QueryInfo(string rawQuery) { parseQuery(rawQuery); }
