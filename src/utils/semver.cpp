#include <cstdlib>
#include <climits>
#include <cctype>
#include <sstream>
#include "htinternal.hpp"

// ----------------------------------------------------------------------------
// [SECTION] Internal utilities (anonymous namespace)
// ----------------------------------------------------------------------------

struct SemverCondition;

// AND set.
typedef std::vector<SemverCondition> ConditionSet;
// OR set.
typedef std::vector<ConditionSet> RangeConditions;

// Represents a single comparison atom (e.g., >=1.2.3).
struct SemverCondition {
  enum Op {
    // "=" or omitted.
    OP_EQ,
    OP_LT,
    OP_LTE,
    OP_GT,
    OP_GTE
  };

  static SemverCondition parseSingleCondition(
    const std::string &condStr,
    bool loose,
    bool includePrerelease);
  
  static RangeConditions parseRangeString(
    const std::string &range,
    bool loose,
    bool includePrerelease);

  static void expandRangePart(
    const std::string &part,
    bool loose,
    bool includePrerelease,
    std::vector<ConditionSet> &outSets);

  SemverCondition()
    : op(OP_EQ)
    , major(0)
    , minor(0)
    , patch(0)
    , explicitPrerelease(false)
  { }

  SemverCondition(
    const HTiSemVer &ver
  )
    : op(OP_EQ)
    , major(ver.major)
    , minor(ver.minor)
    , patch(ver.patch)
    , prerelease({})
    , build({})
    , explicitPrerelease(false)
  { }

  bool matches(
    const HTiSemVer &ver,
    bool includePrereleaseGlobal
  ) const {
    // Prerelease matching rules:
    // If the version has a prerelease tag, and global includePrerelease is false,
    // and this condition does not itself contain a prerelease tag, then the
    // version may only match if the condition shares the same major.minor.patch
    // and the condition also has a prerelease tag.
    bool verHasPre = !ver.getPrerelease().empty();
    if (verHasPre && !includePrereleaseGlobal && !explicitPrerelease) {
      if (major != ver.getMajor() || minor != ver.getMinor() || patch != ver.getPatch())
        return false;
      if (prerelease.empty())
        return false;
    }

    HTiSemVer condVer(major, minor, patch, prerelease, build);
    int cmp = ver.compare(condVer);
    switch (op) {
      case OP_EQ:  return cmp == 0;
      case OP_LT:  return cmp < 0;
      case OP_LTE: return cmp <= 0;
      case OP_GT:  return cmp > 0;
      case OP_GTE: return cmp >= 0;
    }
    return false;
  }

  Op op;
  int major;
  int minor;
  int patch;
  std::vector<std::string> prerelease;
  std::vector<std::string> build;
  // True if this condition contains a prerelease tag.
  bool explicitPrerelease;
};

namespace {

std::string trim(const std::string &s) {
  std::size_t start = 0;
  while (start < s.size() && std::isspace(static_cast<unsigned char>(s[start])))
    ++start;
  std::size_t end = s.size();
  while (end > start && std::isspace(static_cast<unsigned char>(s[end - 1])))
    --end;
  return s.substr(start, end - start);
}

bool isDigit(char c) {
  return c >= '0' && c <= '9';
}

bool isAlpha(char c) {
  return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z');
}

bool isNumericStr(const std::string &s) {
  if (s.empty())
    return false;
  for (char c : s)
    if (!isDigit(c))
      return false;
  return true;
}

int compareIds(
  const std::string &a,
  const std::string &b
) {
  bool aNum = isNumericStr(a);
  bool bNum = isNumericStr(b);
  if (aNum && bNum) {
    i64 na = std::strtoll(a.c_str(), nullptr, 10);
    i64 nb = std::strtoll(b.c_str(), nullptr, 10);
    if (na < nb) return -1;
    if (na > nb) return 1;
    return 0;
  } else if (aNum) {
    // Numeric identifiers are smaller than non-numeric.
    return -1; 
  } else if (bNum) {
    return 1;
  } else {
    if (a < b) return -1;
    if (a > b) return 1;
    return 0;
  }
}

std::vector<std::string> split(
  const std::string &s,
  char delim
) {
  std::vector<std::string> result;
  std::size_t start = 0, end;
  while ((end = s.find(delim, start)) != std::string::npos) {
    result.push_back(s.substr(start, end - start));
    start = end + 1;
  }
  if (start < s.size())
    result.push_back(s.substr(start));
  return result;
}

} // anonymous namespace

// ----------------------------------------------------------------------------
// [SECTION] HTiSemVer implementation
// ----------------------------------------------------------------------------

HTiSemVer::HTiSemVer()
  : major(0)
  , minor(0)
  , patch(0) { }

HTiSemVer::HTiSemVer(
  int major,
  int minor,
  int patch,
  const std::vector<std::string> &prerelease,
  const std::vector<std::string> &build)
  : major(major)
  , minor(minor)
  , patch(patch)
  , prerelease(prerelease)
  , build(build)
{ }

bool HTiSemVer::read(const std::string &version) {
  return parse(version, false);
}

bool HTiSemVer::parse(const std::string &input, bool loose) {
  std::string s = trim(input);
  if (s.empty())
    return false;

  // Remove optional leading 'v' or '='
  if (s[0] == 'v' || s[0] == 'V' || s[0] == '=') {
    s = s.substr(1);
    if (s.empty())
      return false;
  }

  std::size_t pos = 0;

  // Parse major
  while (pos < s.size() && isDigit(s[pos]))
    ++pos;
  if (pos == 0)
    return false;
  major = std::stoi(s.substr(0, pos));
  if (pos >= s.size() || s[pos] != '.')
    return false;
  ++pos; // skip '.'

  // Parse minor
  std::size_t start = pos;
  while (pos < s.size() && isDigit(s[pos]))
    ++pos;
  if (pos == start)
    return false;
  minor = std::stoi(s.substr(start, pos - start));
  if (pos >= s.size() || s[pos] != '.')
    return false;
  ++pos; // skip '.'

  // Parse patch
  start = pos;
  while (pos < s.size() && isDigit(s[pos]))
    ++pos;
  if (pos == start)
    return false;
  patch = std::stoi(s.substr(start, pos - start));

  prerelease.clear();
  build.clear();

  if (pos >= s.size())
    return true;

  // Parse prerelease identifiers
  if (s[pos] == '-') {
    ++pos;
    start = pos;
    while (pos < s.size() && s[pos] != '+')
      ++pos;
    std::string preStr = s.substr(start, pos - start);
    if (preStr.empty())
      return false;

    std::size_t ppos = 0;
    while (ppos < preStr.size()) {
      std::size_t next = preStr.find('.', ppos);
      std::string part = preStr.substr(
        ppos,
        (next == std::string::npos) ? std::string::npos : next - ppos);
      if (part.empty())
        return false;
      // Validate allowed characters
      for (char c : part) {
        if (!isDigit(c) && !isAlpha(c) && c != '-')
          return false;
      }
      // Numeric identifiers must not have leading zeros (except "0")
      if (isNumericStr(part) && part.size() > 1 && part[0] == '0')
        return false;
      prerelease.push_back(part);
      if (next == std::string::npos)
        break;
      ppos = next + 1;
    }
  }

  // Parse build metadata
  if (pos < s.size() && s[pos] == '+') {
    ++pos;
    std::string buildStr = s.substr(pos);
    if (buildStr.empty())
      return false;
    std::size_t ppos = 0;
    while (ppos < buildStr.size()) {
      std::size_t next = buildStr.find('.', ppos);
      std::string part = buildStr.substr(
        ppos,
        (next == std::string::npos) ? std::string::npos : next - ppos);
      if (part.empty())
        return false;
      for (char c : part) {
        if (!isDigit(c) && !isAlpha(c) && c != '-')
          return false;
      }
      build.push_back(part);
      if (next == std::string::npos)
        break;
      ppos = next + 1;
    }
  }

  return true;
}

std::string HTiSemVer::write() const {
  std::string result =
    std::to_string(major) + "." +
    std::to_string(minor) + "." +
    std::to_string(patch);

  if (!prerelease.empty()) {
    result += "-";
    for (std::size_t i = 0; i < prerelease.size(); ++i) {
      if (i > 0)
        result += ".";
      result += prerelease[i];
    }
  }

  if (!build.empty()) {
    result += "+";
    for (std::size_t i = 0; i < build.size(); ++i) {
      if (i > 0)
        result += ".";
      result += build[i];
    }
  }

  return result;
}

int HTiSemVer::compare(const HTiSemVer &other) const {
  if (major != other.major)
    return major > other.major ? 1 : -1;
  if (minor != other.minor)
    return minor > other.minor ? 1 : -1;
  if (patch != other.patch)
    return patch > other.patch ? 1 : -1;

  bool hasPre = !prerelease.empty();
  bool otherHasPre = !other.prerelease.empty();
  if (hasPre != otherHasPre)
    return hasPre ? -1 : 1;

  std::size_t minSize = std::min(prerelease.size(), other.prerelease.size());
  for (std::size_t i = 0; i < minSize; ++i) {
    int cmp = compareIds(prerelease[i], other.prerelease[i]);
    if (cmp != 0)
      return cmp;
  }

  if (prerelease.size() != other.prerelease.size())
    return prerelease.size() > other.prerelease.size() ? 1 : -1;

  // Build metadata does not affect precedence
  return 0;
}

bool HTiSemVer::operator==(const HTiSemVer &other) const {
  return compare(other) == 0;
}

bool HTiSemVer::operator!=(const HTiSemVer &other) const {
  return compare(other) != 0;
}

bool HTiSemVer::operator<(const HTiSemVer &other) const {
  return compare(other) < 0;
}

bool HTiSemVer::operator<=(const HTiSemVer &other) const {
  return compare(other) <= 0;
}

bool HTiSemVer::operator>(const HTiSemVer &other) const {
  return compare(other) > 0;
}

bool HTiSemVer::operator>=(const HTiSemVer &other) const {
  return compare(other) >= 0;
}

// ----------------------------------------------------------------------------
// [SECTION] Static helper functions
// ----------------------------------------------------------------------------

bool HTiSemVer::valid(
  const std::string &v,
  bool loose
) {
  HTiSemVer tmp;
  return tmp.parse(v, loose);
}

std::string HTiSemVer::clean(
  const std::string &v,
  bool loose
) {
  HTiSemVer tmp;
  if (tmp.parse(v, loose))
    return tmp.write();
  return "";
}

HTiSemVer HTiSemVer::coerce(
  const std::string &v,
  bool includePrerelease,
  bool rtl
) {

  std::string s = v;
  std::size_t pos = 0;
  while (pos < s.size() && !isDigit(s[pos]))
    ++pos;
  if (pos == s.size())
    return HTiSemVer();

  std::size_t start = pos;
  while (pos < s.size() && (isDigit(s[pos]) || s[pos] == '.'))
    ++pos;
  std::string numPart = s.substr(start, pos - start);

  std::vector<std::string> parts = split(numPart, '.');
  while (parts.size() < 3)
    parts.push_back("0");
  if (parts.size() > 3)
    parts.resize(3);

  HTiSemVer result;
  result.major = std::stoi(parts[0]);
  result.minor = std::stoi(parts[1]);
  result.patch = std::stoi(parts[2]);

  // Simplified: prerelease extraction not implemented in this version.
  // If includePrerelease is true, a full implementation would parse tags.

  return result;
}

// ----------------------------------------------------------------------------
// [SECTION] Range parsing and matching internals
// ----------------------------------------------------------------------------

SemverCondition SemverCondition::parseSingleCondition(
  const std::string &condStr,
  bool loose,
  bool includePrerelease
) {
  std::string s = trim(condStr);
  SemverCondition cond;
  cond.explicitPrerelease = includePrerelease;

  // Detect operator
  std::size_t pos = 0;
  if (s.compare(0, 2, "<=") == 0) {
    cond.op = SemverCondition::OP_LTE;
    pos = 2;
  } else if (s.compare(0, 2, ">=") == 0) {
    cond.op = SemverCondition::OP_GTE;
    pos = 2;
  } else if (s[0] == '<') {
    cond.op = SemverCondition::OP_LT;
    pos = 1;
  } else if (s[0] == '>') {
    cond.op = SemverCondition::OP_GT;
    pos = 1;
  } else if (s[0] == '=') {
    cond.op = SemverCondition::OP_EQ;
    pos = 1;
  } else {
    cond.op = SemverCondition::OP_EQ;
  }

  std::string verStr = trim(s.substr(pos));
  HTiSemVer tmp;
  if (!tmp.parse(verStr, loose)) {
    // Return a condition that never matches
    return cond;
  }

  cond.major = tmp.major;
  cond.minor = tmp.minor;
  cond.patch = tmp.patch;
  cond.prerelease = tmp.prerelease;
  cond.build = tmp.build;
  cond.explicitPrerelease = includePrerelease || !cond.prerelease.empty();

  return cond;
}

void SemverCondition::expandRangePart(
  const std::string &part,
  bool loose,
  bool includePrerelease,
  std::vector<ConditionSet> &outSets
) {

  std::string s = trim(part);
  if (s.empty())
    return;

  // Handle logical OR (||).
  std::size_t orPos = s.find("||");
  if (orPos != std::string::npos) {
    expandRangePart(s.substr(0, orPos), loose, includePrerelease, outSets);
    expandRangePart(s.substr(orPos + 2), loose, includePrerelease, outSets);
    return;
  }

  // Handle hyphen ranges (e.g., "1.2.3 - 2.3.4").
  std::size_t hyphen = s.find(" - ");
  if (hyphen != std::string::npos) {
    std::string left = trim(s.substr(0, hyphen));
    std::string right = trim(s.substr(hyphen + 3));
    ConditionSet cs;

    HTiSemVer vLeft;
    if (vLeft.parse(left, loose)) {
      SemverCondition c;
      c.op = OP_GTE;
      c.major = vLeft.major;
      c.minor = vLeft.minor;
      c.patch = vLeft.patch;
      c.prerelease = vLeft.prerelease;
      c.build = vLeft.build;
      c.explicitPrerelease = includePrerelease || !vLeft.prerelease.empty();
      cs.push_back(c);
    }

    HTiSemVer vRight;
    if (vRight.parse(right, loose)) {
      SemverCondition c;
      c.op = OP_LTE;
      c.major = vRight.major;
      c.minor = vRight.minor;
      c.patch = vRight.patch;
      c.prerelease = vRight.prerelease;
      c.build = vRight.build;
      c.explicitPrerelease = includePrerelease || !vRight.prerelease.empty();
      cs.push_back(c);
    }

    if (!cs.empty())
      outSets.push_back(cs);

    return;
  }

  // Split by spaces to form an AND set.
  ConditionSet cs;
  std::istringstream iss(s);
  std::string token;
  while (iss >> token) {
    if (token[0] == '~') {
      // Tilde range: allows patch-level changes.
      std::string verStr = token.substr(1);
      HTiSemVer base;
      if (!base.parse(verStr, loose))
        continue;

      SemverCondition low = base;
      low.op = OP_GTE;
      low.prerelease = base.prerelease;
      low.explicitPrerelease = includePrerelease || !low.prerelease.empty();
      cs.push_back(low);

      SemverCondition high;
      high.op = OP_LT;
      if (base.minor != 0 || base.patch != 0) {
        high.major = base.major;
        high.minor = base.minor + 1;
        high.patch = 0;
      } else {
        high.major = base.major + 1;
        high.minor = 0;
        high.patch = 0;
      }
      high.explicitPrerelease = false;
      cs.push_back(high);
    } else if (token[0] == '^') {
      // Caret range: allows non-breaking changes.
      std::string verStr = token.substr(1);
      HTiSemVer base;
      if (!base.parse(verStr, loose))
        continue;

      SemverCondition low;
      low.op = OP_GTE;
      low.major = base.major;
      low.minor = base.minor;
      low.patch = base.patch;
      low.prerelease = base.prerelease;
      low.explicitPrerelease = includePrerelease || !low.prerelease.empty();
      cs.push_back(low);

      SemverCondition high;
      high.op = OP_LT;
      if (base.major > 0) {
        high.major = base.major + 1;
        high.minor = 0;
        high.patch = 0;
      } else if (base.minor > 0) {
        high.major = 0;
        high.minor = base.minor + 1;
        high.patch = 0;
      } else {
        high.major = 0;
        high.minor = 0;
        high.patch = base.patch + 1;
      }
      high.explicitPrerelease = false;
      cs.push_back(high);
    } else if (token.find_first_of("xX*") != std::string::npos) {
      // X-range (wildcard)
      std::string lower = token;
      for (char &c: lower)
        if (c == 'x' || c == 'X' || c == '*')
          c = '0';
      HTiSemVer base;
      if (!base.parse(lower, loose))
        continue;

      SemverCondition low;
      low.op = OP_GTE;
      low.major = base.major;
      low.minor = base.minor;
      low.patch = base.patch;
      low.prerelease = base.prerelease;
      low.explicitPrerelease = includePrerelease || !low.prerelease.empty();
      cs.push_back(low);

      SemverCondition high;
      high.op = OP_LT;
      if (token.find('.') == std::string::npos) {
        high.major = base.major + 1;
        high.minor = 0;
        high.patch = 0;
      } else if (token.find('.', token.find('.') + 1) == std::string::npos) {
        high.major = base.major;
        high.minor = base.minor + 1;
        high.patch = 0;
      } else {
        high.major = base.major;
        high.minor = base.minor;
        high.patch = base.patch + 1;
      }
      high.explicitPrerelease = false;
      cs.push_back(high);
    } else {
      // Plain comparator.
      SemverCondition cond = parseSingleCondition(token, loose, includePrerelease);
      cs.push_back(cond);
    }
  }

  if (!cs.empty())
    outSets.push_back(cs);
}

RangeConditions SemverCondition::parseRangeString(
  const std::string &range,
  bool loose,
  bool includePrerelease
) {
  RangeConditions result;
  expandRangePart(range, loose, includePrerelease, result);
  return result;
}

bool HTiSemVer::satisfies(
  const std::string &version,
  const std::string &range,
  bool loose,
  bool includePrerelease
) {
  HTiSemVer ver;
  if (!ver.read(version))
    return false;
  return satisfies(ver, range, loose, includePrerelease);
}

bool HTiSemVer::satisfies(
  const HTiSemVer &ver,
  const std::string &range,
  bool loose,
  bool includePrerelease
) {
  RangeConditions conds = SemverCondition::parseRangeString(
    range,
    loose,
    includePrerelease);
  if (conds.empty())
    return false;

  for (const auto &andSet: conds) {
    bool setOk = true;
    for (const auto &cond: andSet) {
      if (!cond.matches(ver, includePrerelease)) {
        setOk = false;
        break;
      }
    }
    if (setOk)
      return true;
  }

  return false;
}
