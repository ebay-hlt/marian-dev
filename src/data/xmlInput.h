#pragma once
/*
 * xmlInput.h
 *
 *  Created on: Jun 5, 2018
 *      Author: pramathur
 *  What does this do?
 *  	Monotonically replace the placeholders in the output with the entities in the input
 */

#include <vector>
#include <boost/algorithm/string.hpp>

namespace marian {
namespace data {

class XMLInput {

private:
  typedef std::vector< std::pair<size_t, std::string> > EntityMap;
  EntityMap placeholders_;
	

public:
	XMLInput(std::string& line){
	  /*
	   * parse this kind of a xmlInput
	   * <ne translation="$num" entity="100">$num</ne>
	   */
	  EntityMap plc;
	  bool parsed = ProcessAndStripXMLTags(line, plc);
	  if (parsed)
		placeholders_ = plc;
	}

	~XMLInput() {
		placeholders_.erase(placeholders_.begin(), placeholders_.end());
	}

	bool ProcessAndStripXMLTags(std::string &line, EntityMap& placeholders);
	const EntityMap getEntites() const { return placeholders_; }
	auto begin() const -> decltype(placeholders_.begin()) { return placeholders_.begin(); }
	auto end() const -> decltype(placeholders_.end()) { return placeholders_.end(); }

};
typedef std::shared_ptr<XMLInput> XMLInputPtr;

} /* namespace data */
} /* namespace marian */
