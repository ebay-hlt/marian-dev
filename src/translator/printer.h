#pragma once

#include <vector>
#include "common/utils.h"
#include "data/vocab.h"
#include "translator/history.h"

namespace marian {

inline void ReplaceXMLEntities(std::string& sent, marian::data::XMLInputPtr xmlPtr, bool reverse){
	std::vector<std::string> fields;
	Split(sent, fields, " ");
	std::vector<std::string> vecStr;
	std::vector<size_t> eraseMe;
	for (auto entities: xmlPtr->getEntites()){
		// we can use pos here to determine the aligned source word
		// and then replace that trg word with the entity but not right now
		// size_t pos = entities.first;
		vecStr.push_back(entities.second);
	}
	for (size_t pos = 0; pos < fields.size(); pos++){
		// replace the placeholders with their entities
		// only if there exists an entity in the vector
		// and the token is of the form $[a-z]+
		if (fields[pos].size() > 2 && fields[pos][0] == '$' && isalpha(fields[pos][1])){
			if (vecStr.size() > 0){
			  fields[pos] = vecStr.front();
			  vecStr.erase(vecStr.begin());
			} else{
			  fields[pos] = "";
			  eraseMe.push_back(pos);
			}
		}
	}
	sent = Join(fields, " ", false);
	return;
}

template <class OStream>
void Printer(Ptr<Config> options,
             Ptr<Vocab> vocab,
             Ptr<History> history,
             OStream& best1,
             OStream& bestn) {
  bool reverse = options->get<bool>("right-left");

  if(options->has("n-best") && options->get<bool>("n-best")) {
    const auto& nbl = history->NBest(options->get<size_t>("beam-size"));

    for(size_t i = 0; i < nbl.size(); ++i) {
      const auto& result = nbl[i];
      const auto& words = std::get<0>(result);
      const auto& hypo = std::get<1>(result);

      float realCost = std::get<2>(result);
      std::string translation = Join((*vocab)(words), " ", reverse);
      if (options->has("using-placeholders")){
    	  data::XMLInputPtr xmlPtr = history->getXMLInput();
    	  ReplaceXMLEntities(translation, xmlPtr, reverse);
      }

      bestn << history->GetLineNum() << " ||| " << translation << " |||";

      if(hypo->GetCostBreakdown().empty()) {
        bestn << " F0=" << hypo->GetCost();
      } else {
        for(size_t j = 0; j < hypo->GetCostBreakdown().size(); ++j) {
          bestn << " F" << j << "= " << hypo->GetCostBreakdown()[j];
        }
      }

      bestn << " ||| " << realCost;

      if(i < nbl.size() - 1)
        bestn << std::endl;
      else
        bestn << std::flush;
    }
  }

  auto bestTranslation = history->Top();

  std::string translation
      = Join((*vocab)(std::get<0>(bestTranslation)), " ", reverse);
  if (options->has("using-placeholders")){
    data::XMLInputPtr xmlPtr = history->getXMLInput();
    ReplaceXMLEntities(translation, xmlPtr, reverse);
  }
  best1 << translation << std::flush;
}

}
