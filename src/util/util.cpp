#include <chrono>
#include <cmath>

#include <map>
#include <string>
#include <sstream>

#include <boost/uuid/uuid.hpp>
#include <boost/uuid/uuid_generators.hpp>
#include <boost/uuid/uuid_io.hpp>
#include <openssl/sha.h>

#include "gen-cpp2/RelevancedProtocol_types.h"

using namespace std;
using relevanced::thrift_protocol::Language;

namespace relevanced {
namespace util {

string getUuid() {
  boost::uuids::uuid uuid = boost::uuids::random_generator()();
  ostringstream oss;
  oss << uuid;
  return oss.str();
}

bool isOnlyAscii(const string &text) {
  for (size_t i = 0; i < text.size(); i++) {
    unsigned char c = text.at(i);
    if (c > 127) {
      return false;
    }
  }
  return true;
}

int64_t getChronoEpochTime() {
  chrono::system_clock::time_point now = chrono::system_clock::now();
  auto secs = chrono::duration_cast<chrono::seconds>(now.time_since_epoch());
  return secs.count();
}

string sha1(const string &input) {
  size_t hashLen = 20;
  unsigned char hashBuff[hashLen];
  SHA1((const unsigned char *) input.c_str(), input.size(), hashBuff);
  ostringstream output;
  for (size_t i = 0; i < hashLen; i++) {
    unsigned char c = hashBuff[i];
    int current = c;
    output << std::hex << current;
  }
  return output.str();
}

const char* countryCodeOfThriftLanguage(Language lang) {
  switch (lang) {
    case Language::DE : return "de";
    case Language::EN : return "en";
    case Language::ES : return "es";
    case Language::FR : return "fr";
    case Language::IT : return "it";
    case Language::RU : return "ru";
    case Language::OTHER : return "OTHER";
    default           : return "UNKNOWN";
  }
}

} // util
} // relevanced
