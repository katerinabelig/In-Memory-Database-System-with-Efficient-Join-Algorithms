#include "Joiner.hpp"

Joiner::Joiner(uint32_t size, uint32_t rowSize){
  this->size = size;
  this->rowSize = rowSize;
  //->usedRelations = rowSize;
  relations = new Relation*[size]{};
  for (int i =0; i < size; i++){
    relations[i] = NULL;
  }
}

void Joiner::AddRelation(const char* fileName)
// Loads a relation from disk
{
  for (int i = 0; i < size; i++)
  if (relations[i] == NULL){
    relations[i] = new Relation(fileName, i);
    return;
  }
}
//---------------------------------------------------------------------------
Relation& Joiner::GetRelation(unsigned relationId)
// Loads a relation from disk
{
  if (relationId >= size) {
    char error[256];
    sprintf(error, "* relation with id: %d does not exist", relationId);
    throw runtime_error(error);
  }
  return *relations[relationId];
}

RelColumn* Joiner::GetRelationCol(unsigned relationId, unsigned colId){
    Relation& rel = GetRelation(relationId);
    RelColumn* relColumn = new RelColumn(relationId, rel.size);
    //TEMPORARY FOR DEBUGGING!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!
    //for (int i = 0; i < rel.size; i++){
    
    for (int i = 0; i < rel.size; i++){
      //i = rand()%(rel.size);
      relColumn->tuples[i].key = i;
      relColumn->tuples[i].payload = rel.columns[colId][i];
    }
    return relColumn;
}

RelColumn* Joiner::GetUsedRelation(unsigned relationId, unsigned colId){
  int notNullRow = getFirstURrow();
  if (notNullRow == -1 || usedRelations->matchRows[notNullRow]->arr[relationId] || firstJoin == true == -1)
    return GetRelationCol(relationId, colId);

  Relation& rel = GetRelation(relationId);
  RelColumn* relColumn = new RelColumn(relationId, usedRelations->activeSize);
  //changed sth here!!!!!!!!!!!!!!!!!!!!!!!
  int relCol_cnt = 0;
  for (int i=0; i<usedRelations->size; i++){
    if (usedRelations->matchRows[i] == NULL)
      continue;
    uint32_t rowid = usedRelations->matchRows[i]->arr[relationId];
    relColumn->tuples[relCol_cnt].key = rowid;
    relColumn->tuples[relCol_cnt].payload = rel.columns[colId][rowid];
    relCol_cnt++;
  }
  return relColumn;
}

//-----------------------------------------------------------------------
string Joiner::Join(Query& query)
  // Executes a join query
{
  cout << "\n\n--- Join Queries Start---\n\n";
  usedRelations = new UsedRelations(100000, query.number_of_relations);

  for (int i = 0; i < query.number_of_predicates; i++){
    /// Priority index
    int idx = query.priority_predicates[i];
    //int idx = i; 
    //CASE 1: Join is with filter e.g 1.0 > 3000
    if(query.prdcts[idx]->number_after_operation){
      RelColumn* relR = GetUsedRelation(query.prdcts[idx]->relation_index_left, query.prdcts[idx]->column_left);
      updateURself_Filter(query.prdcts[idx]->relation_index_left, filterJoin(relR, query.prdcts[idx]->operation, query.prdcts[idx]->number));
      delete relR;
    }
    //CASE 2: Join is self join e.g 1.0 = 1.2
    else if (isSelfJoin(query.prdcts[idx]->relation_index_left, query.prdcts[idx]->relation_index_right)){
      RelColumn* relR = GetUsedRelation(query.prdcts[idx]->relation_index_left, query.prdcts[idx]->column_left);
      RelColumn* relS = GetUsedRelation(query.prdcts[idx]->relation_index_right, query.prdcts[idx]->column_right);
      updateURself_Filter(query.prdcts[idx]->relation_index_left, selfJoin(relR, relS));
      delete relR;
      delete relS;
    }
    //CASE 3: Join is between 2 relationships e.g 1.0 = 2.1
    else {
      RelColumn* relR = GetUsedRelation(query.prdcts[idx]->relation_index_left, query.prdcts[idx]->column_left);
      RelColumn* relS = GetUsedRelation(query.prdcts[idx]->relation_index_right, query.prdcts[idx]->column_right);
      PartitionedHashJoin* phj = new PartitionedHashJoin(relR, relS);
      updateUsedRelations(phj->Solve(),query.prdcts[idx]->relation_index_left, query.prdcts[idx]->relation_index_right);
      delete phj;
      delete relR;
      delete relS;
    }
    if (usedRelations == NULL) return "NULL";
    //PrintUsedRelations();
    cout << usedRelations->activeSize << endl;
  }
  //cout << usedRelations->activeSize << endl;

  //PrintUsedRelations();

  for (unsigned i=0; i<query.number_of_projections; ++i) {
    cout << Checksum(query.projections[i]->getRelationIndex(),query.projections[i]->getColumn()) << " ";
  }
  cout << "\n";
  delete usedRelations;
  return "";
}

uint64_t Joiner::Checksum(unsigned relationId, unsigned colId){
  uint64_t sum = 0;
  Relation& rel = GetRelation(relationId);
  for (int i=0; i<usedRelations->size; i++){
    if (usedRelations->matchRows[i] == NULL) continue;
    uint32_t idx = usedRelations->matchRows[i]->arr[relationId];
    if (idx == -1) return 0;
    sum += rel.columns[colId][idx];
  }
  return sum;
}
//-----------------------------------------------------------------------
void Joiner::updateUsedRelations(Matches* matches, int relRid, int relSid){
  if (matches == NULL || matches->activeSize == 0){
    clearUsedRelations();
    return;
  }
  if (firstJoin){ /// First Join
    firstJoin = false;
    cout << "[UpdateUR] First Join" << endl;
    updateURFirst(matches, relRid, relSid);
    return;
  }
  int i = getFirstURrow();
  // CASE 2.1: Only one of the Relations has been joined before, the Relation R.
  // Or both
  if (usedRelations->matchRows[i]->arr[relRid] != -1){
    cout << "[UpdateUR] Left or Both" << endl;
    updateURonlyR(matches, relRid, relSid);
    return;
  }
  // CASE 2.2: Only one of the Relations has been joined before, the Relation S.
  if (usedRelations->matchRows[i]->arr[relSid] != -1){
    cout << "[UpdateUR] Right" << endl;
    updateURonlyS(matches, relSid, relRid);
    return;
  }
}

void Joiner::updateURFirst(Matches* matches, int relRid, int relSid){
  uint32_t i;
  for (i=0; i<matches->activeSize; i++){
    usedRelations->matchRows[i] = new MatchRow(usedRelations->rowSize);
    usedRelations->matchRows[i]->arr[relRid] = matches->tuples[i]->key;
    usedRelations->matchRows[i]->arr[relSid] = matches->tuples[i]->payload;
  }
  usedRelations->activeSize = i;
}

void Joiner::updateURonlyR(Matches* matches, int relUR, int relNew){
  int deletions = 0;
  for (uint32_t i=0; i<usedRelations->size; i++){ /// For each entry from usedRelations
    if (usedRelations->matchRows[i] == NULL) continue;

    bool del = true;
    uint32_t rowid = usedRelations->matchRows[i]->arr[relUR];
    for (uint32_t j=0; j<matches->activeSize; j++){ /// Check if exists in new match table
      if (rowid == matches->tuples[j]->key){
        //cout << rowid<<":"<<matches->tuples[j]->payload<<endl;
        usedRelations->matchRows[i]->arr[relNew] = matches->tuples[j]->payload;
        del = false;
        break;
      }
    }
    if (del){
      deletions ++;
      delete usedRelations->matchRows[i];
      usedRelations->matchRows[i] = NULL;
      usedRelations->activeSize--;
    }
  }
}

void Joiner::updateURonlyS(Matches* matches, int relUR, int relNew){
  for (uint32_t i=0; i < usedRelations->size; i++){ /// For each entry from usedRelations
    if (usedRelations->matchRows[i] == NULL) continue;

    bool del = true;
    uint32_t rowid = usedRelations->matchRows[i]->arr[relUR];
    for (uint32_t j=0; j<matches->activeSize; j++){ /// Check if exists in new match table
      if (rowid == matches->tuples[j]->payload){
        usedRelations->matchRows[i]->arr[relNew] = matches->tuples[j]->key;
        del = false;
        break;
      }
    }
    if (del){
      delete usedRelations->matchRows[i];
      usedRelations->matchRows[i] = NULL;
      usedRelations->activeSize--;
    }
  }
}
//-----------------------------------------------------------------------
void Joiner::updateURself_Filter(int relId, SingleCol* sc){
  cout << "[UpdateUR] Self / Filter Join" << endl;
  if (sc->activeSize == 0){ //NO matches clear UR
    clearUsedRelations();
    return;
  }
  if (firstJoin){
    firstJoin = false;
    uint32_t i;
    for (i = 0; i < sc->activeSize; i++){
      usedRelations->matchRows[i] = new MatchRow(usedRelations->rowSize);
      usedRelations->matchRows[i]->arr[relId] = sc->arr[i];
    }
    usedRelations->activeSize = i;
    return;
  }
  for (uint32_t i=0; i < usedRelations->size; i++){
    if (usedRelations->matchRows[i] == NULL) continue;

    bool del = true;
    uint32_t rowid = usedRelations->matchRows[i]->arr[relId];
    for (uint32_t j=0; j < sc->activeSize; j++){ /// Check if exists in new match table
      if (rowid == sc->arr[j]){
        del = false;
        break;
      }
    }
    if (del){
      delete usedRelations->matchRows[i];
      usedRelations->matchRows[i] = NULL;
      usedRelations->activeSize--;
    }
  }
}
//-----------------------------------------------------------------------
SingleCol* Joiner::selfJoin(RelColumn* relR, RelColumn* relS){
  SingleCol* singleCol = new SingleCol(relR->num_tuples);
  for (uint32_t i=0; i<relR->num_tuples; i++){
    if (relR->tuples[i].payload == relS->tuples[i].payload){
      //cout << "relR->tuples[i].key: " << relR->tuples[i].key << " == " << relS->tuples[i].key << endl;
      singleCol->arr[singleCol->activeSize] = relR->tuples[i].key;
      singleCol->activeSize++;
    }
  }
  return singleCol;
}
//-----------------------------------------------------------------------
bool Joiner::isSelfJoin(unsigned int relR, unsigned int relS){
  return relR == relS;
}
int Joiner::getFirstURrow(){
  if (firstJoin) return -1;
  for (uint32_t i = 0; i < usedRelations->size; i++){
    if (usedRelations->matchRows[i] != NULL){
      return i;
    }
  }
  return -1;
}
//-----------------------------------------------------------------------
bool Joiner::isFilterJoin(char operation){
  return (operation != '=');
}
//-----------------------------------------------------------------------
SingleCol* Joiner::filterJoin(RelColumn* rel, char operation, int n){
  SingleCol* singleCol = new SingleCol(rel->num_tuples);
  if (operation == '>'){
    for (uint32_t i = 0; i < rel->num_tuples; i++){
      if (rel->tuples[i].payload > n){
        singleCol->arr[singleCol->activeSize] = rel->tuples[i].key;
        singleCol->activeSize++;
      }
    }
  }
  else if (operation == '<'){
    for (uint32_t i = 0; i < rel->num_tuples; i++){
      if (rel->tuples[i].payload < n){
        singleCol->arr[singleCol->activeSize] = rel->tuples[i].key;
        singleCol->activeSize++;
      }
    }
  }
  else {
    for (uint32_t i = 0; i < rel->num_tuples; i++){
      if (rel->tuples[i].payload == n){
        singleCol->arr[singleCol->activeSize] = rel->tuples[i].key;
        singleCol->activeSize++;
      }
    }

  }
  return singleCol;
}
//-----------------------------------------------------------------------
void Joiner::PrintUsedRelations(){
  cout << "\n--- Join Results in UR table---\n\n" << endl;
  for (int i=0; i<usedRelations->size; i++){
    if (usedRelations->matchRows[i] != nullptr) {
      for (int j = 0; j < usedRelations->matchRows[i]->size; j++) {
        cout << usedRelations->matchRows[i]->arr[j] << " ";
      }
      cout << endl;
    }
    // if (i == 10) {
    //   break;
    // }
  }
}
//-----------------------------------------------------------------------
void Joiner::clearUsedRelations(){
  delete usedRelations;
  usedRelations = NULL;
}
//-----------------------------------------------------------------------
Joiner::~Joiner(){
  for (int i = 0; i<size; i++){
    delete relations[i];
  }
  delete[] relations;
}
