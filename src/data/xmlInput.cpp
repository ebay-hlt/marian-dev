#pragma once
/*
 * xmlInput.cpp
 *
 *  Created on: Jun 5, 2018
 *      Author: pramathur
 *  What does this do?
 *  	Monotonically replace the placeholders in the output with the entities in the input
 *
 *  Most of the code copied from mosesdecoder:moses/XMLOption.cpp
 */

#include "common/logging.h"
#include "xmlInput.h"
#include "common/utils.h"
#include <cstring>

namespace marian {
namespace data {

std::string ParseXmlTagAttribute(const std::string& tag,const std::string& attributeName)
{
  std::string tagOpen = attributeName + "=\"";
  size_t contentsStart = tag.find(tagOpen);
  if (contentsStart == std::string::npos) return "";
  contentsStart += tagOpen.size();
  size_t contentsEnd = tag.find_first_of('"',contentsStart+1);
  if (contentsEnd == std::string::npos) {
	  LOG(info, "Malformed XML attribute: {}", tag);
    return "";
  }
  size_t possibleEnd;
  while (tag.at(contentsEnd-1) == '\\' && (possibleEnd = tag.find_first_of('"',contentsEnd+1)) != std::string::npos) {
    contentsEnd = possibleEnd;
  }
  return tag.substr(contentsStart,contentsEnd-contentsStart);
}

void Trim(std::string& s) {
  boost::trim_if(s, boost::is_any_of(" \t\n"));
}

/**
 * Remove "<" and ">" from XML tag
 *
 * \param str xml token to be stripped
 * \param lbrackStr xml tag's left bracket string, typically "<"
 * \param rbrackStr xml tag's right bracket string, typically ">"
 */
std::string TrimXml(const std::string& str, const std::string& lbrackStr, const std::string& rbrackStr)
{
  // too short to be xml token -> do nothing
  if (str.size() < lbrackStr.length()+rbrackStr.length() ) return str;

  // strip first and last character
  const char* temp = str.c_str();
  const char* lbr = lbrackStr.c_str();
  const char* rbr = rbrackStr.c_str();
  if (temp[0] == lbr[0]  && temp[str.size()-1] == rbr[0]) {
    return str.substr(lbrackStr.length(), str.size()-lbrackStr.length()-rbrackStr.length());
  }
  // not an xml token -> do nothing
  else {
    return str;
  }
}

/**
 * Check if the token is an XML tag, i.e. starts with "<"
 *
 * \param tag token to be checked
 * \param lbrackStr xml tag's left bracket string, typically "<"
 * \param rbrackStr xml tag's right bracket string, typically ">"
 */
bool isXmlTag(const std::string& tag, const std::string& lbrackStr, const std::string& rbrackStr)
{
  return (tag.substr(0,lbrackStr.length()) == lbrackStr &&
          (tag[lbrackStr.length()] == '/' ||
           (tag[lbrackStr.length()] >= 'a' && tag[lbrackStr.length()] <= 'z') ||
           (tag[lbrackStr.length()] >= 'A' && tag[lbrackStr.length()] <= 'Z')));
}

/**
 * Split up the input character string into tokens made up of
 * either XML tags or text.
 * example: this <b> is a </b> test .
 *       => (this ), (<b>), ( is a ), (</b>), ( test .)
 *
 * \param str input string
 * \param lbrackStr xml tag's left bracket string, typically "<"
 * \param rbrackStr xml tag's right bracket string, typically ">"
 */
std::vector<std::string> TokenizeXml(const std::string& str, const std::string& lbrackStr, const std::string& rbrackStr)
{
	std::string lbrack = lbrackStr; // = "<";
	std::string rbrack = rbrackStr; // = ">";
	std::vector<std::string> tokens; // vector of tokens to be returned
	std::string::size_type cpos = 0; // current position in string
	std::string::size_type lpos = 0; // left start of xml tag
	std::string::size_type rpos = 0; // right end of xml tag

  // walk thorugh the string (loop vver cpos)
  while (cpos != str.size()) {
    // find the next opening "<" of an xml tag
    lpos = str.find(lbrack, cpos);			// lpos = str.find_first_of(lbrack, cpos);
    if (lpos != std::string::npos) {
      // find the end of the xml tag
      rpos = str.find(rbrack, lpos+lbrackStr.length()-1);			// rpos = str.find_first_of(rbrack, lpos);
      // sanity check: there has to be closing ">"
      if (rpos == std::string::npos) {
        LOG(info, "ERROR: malformed XML: {} \n", str);
        return tokens;
      }
    } else { // no more tags found
      // add the rest as token
      tokens.push_back(str.substr(cpos));
      break;
    }

    // add stuff before xml tag as token, if there is any
    if (lpos - cpos > 0)
      tokens.push_back(str.substr(cpos, lpos - cpos));

    // add xml tag as token
    tokens.push_back(str.substr(lpos, rpos-lpos+rbrackStr.length()));
    cpos = rpos + rbrackStr.length();
  }
  return tokens;
}

/**
 * Process a sentence with xml annotation
 * Xml tags may specifiy additional/replacing translation options
 * and reordering constraints
 *
 * \param line in: sentence, out: sentence without the xml
 * \param res vector with translation options specified by xml
 * \param lbrackStr xml tag's left bracket string, typically "<"
 * \param rbrackStr xml tag's right bracket string, typically ">"
 */

bool XMLInput::ProcessAndStripXMLTags(std::string &line,
		EntityMap &placeholders) {
  //parse XML markup in translation line

  const std::string& lbrackStr = "<";
  const std::string& rbrackStr = ">";

  // no xml tag? we're done.
  if (line.find(lbrackStr) == std::string::npos) {
    return true;
  }

  // break up input into a vector of xml tags and text
  // example: (this), (<b>), (is a), (</b>), (test .)
  std::vector<std::string> xmlTokens = TokenizeXml(line, lbrackStr, rbrackStr);

  // we need to store opened tags, until they are closed
  // tags are stored as tripled (tagname, startpos, contents)
  typedef std::pair< std::string, std::pair< size_t, std::string > > OpenedTag;
  std::vector< OpenedTag > tagStack; // stack that contains active opened tags

  std::string cleanLine; // return string (text without xml)
  size_t wordPos = 0; // position in sentence (in terms of number of words)

  // loop through the tokens
  for (auto xmlToken : xmlTokens) {
    // not a xml tag, but regular text (may contain many words)
    if(!isXmlTag(xmlToken, lbrackStr, rbrackStr)) {
      // add a space at boundary, if necessary
      if (cleanLine.size()>0 &&
          cleanLine[cleanLine.size() - 1] != ' ' &&
          xmlToken[0] != ' ') {
        cleanLine += " ";
      }
      cleanLine += xmlToken; // add to output
      wordPos = Split(cleanLine, " ").size();
    }
    // process xml tag
    else {
      // *** get essential information about tag ***

      // strip extra boundary spaces and "<" and ">"
      std::string tag = TrimXml(xmlToken, lbrackStr, rbrackStr);
      Trim(tag);

      if (tag.size() == 0) {
        LOG(debug, "ERROR: empty tag name: {} \n" ,line);
        return false;
      }

      // check if unary (e.g., "<wall/>")
      bool isUnary = ( tag[tag.size() - 1] == '/' );
      if (isUnary){
	    LOG(debug, "Unary tags not supported yet {} \n", line);
	    return false;
      }

      // check if opening tag (e.g. "<a>", not "</a>")g
      bool isClosed = ( tag[0] == '/' );
      bool isOpen = !isClosed;

      if (isClosed)
        tag = tag.substr(1); // remove "/" at the beginning


      // find the tag name and contents
      std::string::size_type endOfName = tag.find_first_of(' ');
      std::string tagName = tag;
      std::string tagContent = "";
      if (endOfName != std::string::npos) {
        tagName = tag.substr(0,endOfName);
        tagContent = tag.substr(endOfName+1);
      }

      // *** process new tag ***

      if (isOpen) {
        // put the tag on the tag stack
        OpenedTag openedTag = make_pair( tagName, make_pair( wordPos, tagContent ) );
        tagStack.push_back( openedTag );
      }

      // *** process completed tag ***

      if (isClosed) {
        // pop last opened tag from stack;
        if (tagStack.size() == 0) {
          return false;
        }
        OpenedTag openedTag = tagStack.back();
        tagStack.pop_back();

        // tag names have to match
        if (openedTag.first != tagName) {
          return false;
        }

        // assemble remaining information about tag
        size_t startPos = openedTag.second.first;
        std::string tagContent = openedTag.second.second;
        size_t endPos = wordPos;

        // name-entity placeholder
        if (tagName == "ne") {
          if (startPos != (endPos - 1)) {
            return false;
          }
          std::string entity = ParseXmlTagAttribute(tagContent,"entity");
          placeholders.push_back(std::pair<size_t, std::string>(startPos, entity));
        }
      }
    }
  }
  if (tagStack.size() > 0) {
    return false;
  }

  // return de-xml'ed sentence in line
  line = cleanLine;
  return true;
}

}
}
