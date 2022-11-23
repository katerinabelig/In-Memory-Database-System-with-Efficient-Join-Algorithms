#include "Joiner.hpp"
#include <cassert>
#include <iostream>
#include <string>
#include <unordered_map>
#include <utility>
#include <set>
#include <sstream>
#include <vector>
#include "Parser.hpp"
#include "../../partition-phase/PartitionedHashJoin.h"
#include "../../partition-phase/Structures.h"

//---------------------------------------------------------------------------
using namespace std;
//---------------------------------------------------------------------------
void Joiner::addRelation(const char* fileName)
// Loads a relation from disk
{
  relations.emplace_back(fileName);
}
//---------------------------------------------------------------------------
Relation& Joiner::getRelation(unsigned relationId)
// Loads a relation from disk
{
  if (relationId >= relations.size()) {
    char error[256];
    sprintf(error, "* relation with id: %d does not exist", relationId);
    throw runtime_error(error);
  }
  return relations[relationId];
}
//---------------------------------------------------------------------------
unique_ptr<Operator> Joiner::addScan(set<unsigned>& usedRelations,SelectInfo& info,QueryInfo& query)
  // Add scan to query
{
  usedRelations.emplace(info.binding);
  vector<FilterInfo> filters;
  for (auto& f : query.filters) {
    if (f.filterColumn.binding==info.binding) {
      filters.emplace_back(f);
    }
  }
  return filters.size()?make_unique<FilterScan>(getRelation(info.relId),filters):make_unique<Scan>(getRelation(info.relId),info.binding);
}
//---------------------------------------------------------------------------
enum QueryGraphProvides {  Left, Right, Both, None };
//---------------------------------------------------------------------------
static QueryGraphProvides analyzeInputOfJoin(set<unsigned>& usedRelations,SelectInfo& leftInfo,SelectInfo& rightInfo)
  // Analyzes inputs of join
{
  bool usedLeft=usedRelations.count(leftInfo.binding);
  bool usedRight=usedRelations.count(rightInfo.binding);

  if (usedLeft^usedRight)
    return usedLeft?QueryGraphProvides::Left:QueryGraphProvides::Right;
  if (usedLeft&&usedRight)
    return QueryGraphProvides::Both;
  return QueryGraphProvides::None;
}
//---------------------------------------------------------------------------
string Joiner::joinTest(QueryInfo& query)
  // Executes a join query
{ //"3 0 1|0.2=1.0&0.1=2.0&0.2>3000|1.2 0.1";
  int relId = query.predicates[0].left.relId;
  int colId = query.predicates[0].left.colId;
  relId = query.predicates[0].right.binding;
  colId = query.predicates[0].right.colId;

  //unique_ptr<Operator>&& right = query.predicates[0].right;
  //unique_ptr<Operator> root=make_unique<PartitionedHashJoin>();

}
//---------------------------------------------------------------------------
string Joiner::join(QueryInfo& query)
  // Executes a join query
{
  set<unsigned> usedRelations;

  auto& firstJoin=query.predicates[0];
  auto left=addScan(usedRelations,firstJoin.left,query);
  auto right=addScan(usedRelations,firstJoin.right,query);
  unique_ptr<Operator> root=make_unique<Join>(move(left),move(right),firstJoin);

  for (unsigned i=1;i<query.predicates.size();++i) {
    auto& pInfo=query.predicates[i];
    auto& leftInfo=pInfo.left; auto& rightInfo=pInfo.right;
    unique_ptr<Operator> left, right;

    switch(analyzeInputOfJoin(usedRelations,leftInfo,rightInfo)) {
      case QueryGraphProvides::Left:
        left=move(root);
        right=addScan(usedRelations,rightInfo,query);
        root=make_unique<Join>(move(left),move(right),pInfo);
        break;
      case QueryGraphProvides::Right:
        left=addScan(usedRelations,leftInfo,query);
        right=move(root);
        root=make_unique<Join>(move(left),move(right),pInfo);
        break;
      case QueryGraphProvides::Both:
        // All relations of this join are already used somewhere else in the query.
        // Thus, we have either a cycle in our join graph or more than one join predicate per join.
        root=make_unique<SelfJoin>(move(root),pInfo);
        break;
      case QueryGraphProvides::None:
        // Process this predicate later when we can connect it to the other joins
        // We never have cross products
        query.predicates.push_back(pInfo);
        break;
    };
  }
  Checksum checkSum(move(root),query.selections);
  checkSum.run();

  stringstream out;
  auto& results=checkSum.checkSums;
  for (unsigned i=0;i<results.size();++i) {
    out << (checkSum.resultSize==0?"NULL":to_string(results[i]));
    if (i<results.size()-1)
      out << " ";
  }
  out << "\n";
  return out.str();
}
