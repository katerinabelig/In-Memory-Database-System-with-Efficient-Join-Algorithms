#include <iostream>
#include "./Parsing/Joiner.hpp"
#include "./Parsing/Parser.hpp"

using namespace std;
//---------------------------------------------------------------------------
int main(int argc, char* argv[]) {

   Joiner joiner;
   string line;

   cout << ">>> Insert Relations:" << endl;

   const string workspace = "./workloads/small/";

   while (getline(cin, line)) {
      if (line == "Done") break;
      joiner.addRelation((workspace + line).c_str());
   }

   // Preparation phase (not timed)
   // Build histograms, indexes,...

   cout << ">>> Insert Queries:" << endl;

   QueryInfo i;
   line = "3 0 1|0.2=1.0&0.1=2.0&0.2>3000|1.2 0.1";
   i.parseQuery(line);
   joiner.join(i);
   // while (getline(cin, line)) {
   //    cout << line<<endl;
   //    if (line == "F") continue; // End of a batch
   //    i.parseQuery(line);
   //    //cout << joiner.join(i);
   // }
   //cout << i.predicates[0].left.relId<<endl;
   //cout << i.predicates[0].right.relId<<endl;
   cout << i.filters[0].filterColumn.relId<<endl;
   return 0;
}
