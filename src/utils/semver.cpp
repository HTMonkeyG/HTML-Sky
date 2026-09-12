#include <cstdlib>
#include <climits>
#include <cctype>
#include <charconv>
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
  size_t start = 0;
  while (start < s.size() && std::isspace(static_cast<unsigned char>(s[start])))
    ++start;
  size_t end = s.size();
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

static bool parseInt(const std::string &s, int &out) {
  if (s.empty())
    return false;
  auto result = std::from_chars(s.data(), s.data() + s.size(), out, 10);
  return result.ec == std::errc{} && result.ptr == s.data() + s.size();
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
  size_t start = 0, end;
  while ((end = s.find(delim, start)) != std::string::npos) {
    result.push_back(s.substr(start, end - start));
    start = end + 1;
  }
  if (start < s.size())
    result.push_back(s.substr(start));
  return result;
}

// Represents a version where any component may be absent or a wildcard
// (x / X / *). Absent/wild components are stored as -1.
struct PartialVersion {
  int major; // -1 = wildcard / absent
  int minor; // -1 = wildcard / absent
  int patch; // -1 = wildcard / absent
  std::vector<std::string> prerelease;
  std::vector<std::string> build;

  PartialVersion() : major(-1), minor(-1), patch(-1) {}

  bool majorWild() const { return major < 0; }
  bool minorWild() const { return minor < 0; }
  bool patchWild() const { return patch < 0; }
};

// Parse a possibly partial or wildcard version string.
// Returns false only on a hard syntax error; missing/wild components are -1.
static bool parsePartialVersion(
  const std::string &input,
  bool /*loose*/,
  PartialVersion &out
) {
  out = PartialVersion();
  std::string s = trim(input);
  if (s.empty())
    // Fully wild.
    return true;

  size_t i = 0;

  // Strip optional leading 'v', 'V', or '='.
  if (i < s.size() && (s[i] == 'v' || s[i] == 'V' || s[i] == '='))
    i++;
  if (i >= s.size())
    // Just "v" -> fully wild.
    return true;

  // Parse major.
  if (s[i] == 'x' || s[i] == 'X' || s[i] == '*')
    // Wild major -> everything wild.
    return true;
  if (!isDigit(s[i]))
    return false;
  size_t start = i;
  while (i < s.size() && isDigit(s[i])) ++i;
  if (!parseInt(s.substr(start, i - start), out.major))
    return false;

  if (i >= s.size())
    // Only major given.
    return true;
  if (s[i] != '.')
    return false;
  // Consume '.'
  i++;

  // Parse minor.
  if (i < s.size() && (s[i] == 'x' || s[i] == 'X' || s[i] == '*'))
    // Wild minor (and therefore wild patch too).
    return true;
  if (i >= s.size() || !isDigit(s[i]))
    return false;
  start = i;
  while (i < s.size() && isDigit(s[i])) ++i;
  if (!parseInt(s.substr(start, i - start), out.minor))
    return false;

  if (i >= s.size())
    // Major.minor only.
    return true;
  if (s[i] != '.')
    return false;
  // Consume '.'
  i++;

  // Parse patch
  if (i < s.size() && (s[i] == 'x' || s[i] == 'X' || s[i] == '*')) {
    // Consume wildcard char.
    // Patch stays -1
    i++;
    return true;
  }
  if (i >= s.size() || !isDigit(s[i]))
    return false;
  start = i;
  while (i < s.size() && isDigit(s[i])) ++i;
  if (!parseInt(s.substr(start, i - start), out.patch))
    return false;

  // Optional prerelease.
  if (i < s.size() && s[i] == '-') {
    ++i;
    size_t preStart = i;
    while (i < s.size() && s[i] != '+') ++i;
    std::string preStr = s.substr(preStart, i - preStart);
    if (preStr.empty()) return false;
    size_t p = 0;
    while (p < preStr.size()) {
      size_t next = preStr.find('.', p);
      std::string part = preStr.substr(
        p, next == std::string::npos ? std::string::npos : next - p);
      if (part.empty()) return false;
      out.prerelease.push_back(part);
      if (next == std::string::npos) break;
      p = next + 1;
    }
  }

  // Optional build metadata.
  if (i < s.size() && s[i] == '+') {
    i++;

    std::string buildStr = s.substr(i);
    if (buildStr.empty())
      return false;

    size_t p = 0;
    while (p < buildStr.size()) {
      size_t next = buildStr.find('.', p);
      std::string part = buildStr.substr(
        p,
        next == std::string::npos ? std::string::npos : next - p);
      if (part.empty())
        return false;

      out.build.push_back(part);
      if (next == std::string::npos)
        break;
      p = next + 1;
    }
  }

  return true;
}

// Build a GTE condition from a partial version, filling missing parts with 0.
static SemverCondition makeGTEfromPartial(
  const PartialVersion &pv,
  bool inclPre
) {
  SemverCondition c;
  c.op = SemverCondition::OP_GTE;
  c.major = pv.majorWild() ? 0 : pv.major;
  c.minor = pv.minorWild() ? 0 : pv.minor;
  c.patch = pv.patchWild() ? 0 : pv.patch;
  c.prerelease = pv.prerelease;
  c.build = pv.build;
  c.explicitPrerelease = inclPre || !c.prerelease.empty();
  return c;
}

// Build a LT condition with the prerelease sentinel "-0".
//
// This sentinel is the standard upper-bound marker used by X-ranges, tilde,
// caret and partial-version comparators.  Any real prerelease identifier
// ("alpha", "beta", "rc", etc.) is an alphanumeric identifier and therefore
// sorts *above* the pure-numeric identifier "0", so the boundary
// LT (major, minor, patch, prerelease=["0"]) correctly excludes every
// prerelease of that version tuple while still excluding the release itself.
static SemverCondition makeLTSentinel(
  int major,
  int minor,
  int patch
) {
  SemverCondition c;
  c.op = SemverCondition::OP_LT;
  c.major = major;
  c.minor = minor;
  c.patch = patch;
  c.prerelease = {"0"};
  c.explicitPrerelease = true;
  return c;
}

} // anonymous namespace

// ----------------------------------------------------------------------------
// [SECTION] HTiSemVer implementation
// ----------------------------------------------------------------------------

HTiSemVer::HTiSemVer(): major(0), minor(0), patch(0) { }

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

  size_t pos = 0;

  // Parse major.
  while (pos < s.size() && isDigit(s[pos]))
    ++pos;
  if (pos == 0)
    return false;
  if (!parseInt(s.substr(0, pos), major))
    return false;
  if (pos >= s.size() || s[pos] != '.')
    return false;
  // Skip '.'
  pos++;

  // Parse minor.
  size_t start = pos;
  while (pos < s.size() && isDigit(s[pos]))
    pos++;
  if (pos == start)
    return false;
  if (!parseInt(s.substr(start, pos - start), minor))
    return false;
  if (pos >= s.size() || s[pos] != '.')
    return false;
  // Skip '.'
  pos++;

  // Parse patch.
  start = pos;
  while (pos < s.size() && isDigit(s[pos]))
    ++pos;
  if (pos == start)
    return false;
  if (!parseInt(s.substr(start, pos - start), patch))
    return false;

  prerelease.clear();
  build.clear();

  if (pos >= s.size())
    return true;

  if (s[pos] != '-' && s[pos] != '+')
    return false;

  // Parse prerelease identifiers.
  if (s[pos] == '-') {
    ++pos;
    start = pos;
    while (pos < s.size() && s[pos] != '+')
      ++pos;
    std::string preStr = s.substr(start, pos - start);
    if (preStr.empty())
      return false;

    size_t ppos = 0;
    while (ppos < preStr.size()) {
      size_t next = preStr.find('.', ppos);
      std::string part = preStr.substr(
        ppos,
        (next == std::string::npos) ? std::string::npos : next - ppos);
      if (part.empty())
        return false;

      // Validate allowed characters.
      for (char c : part) {
        if (!isDigit(c) && !isAlpha(c) && c != '-')
          return false;
      }

      // Numeric identifiers must not have leading zeros (except "0").
      if (isNumericStr(part) && part.size() > 1 && part[0] == '0')
        return false;
      prerelease.push_back(part);
      if (next == std::string::npos)
        break;
      ppos = next + 1;
    }
  }

  // Parse build metadata.
  if (pos < s.size() && s[pos] == '+') {
    pos++;
    std::string buildStr = s.substr(pos);
    if (buildStr.empty())
      return false;

    size_t ppos = 0;
    while (ppos < buildStr.size()) {
      size_t next = buildStr.find('.', ppos);
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

  if (pos != s.size())
    return false;

  return true;
}

std::string HTiSemVer::write() const {
  std::string result =
    std::to_string(major) + "." +
    std::to_string(minor) + "." +
    std::to_string(patch);

  if (!prerelease.empty()) {
    result += "-";
    for (size_t i = 0; i < prerelease.size(); ++i) {
      if (i > 0)
        result += ".";
      result += prerelease[i];
    }
  }

  if (!build.empty()) {
    result += "+";
    for (size_t i = 0; i < build.size(); ++i) {
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

  size_t minSize = std::min(prerelease.size(), other.prerelease.size());
  for (size_t i = 0; i < minSize; ++i) {
    int cmp = compareIds(prerelease[i], other.prerelease[i]);
    if (cmp != 0)
      return cmp;
  }

  if (prerelease.size() != other.prerelease.size())
    return prerelease.size() > other.prerelease.size() ? 1 : -1;

  // Build metadata does not affect precedence.
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

bool HTiSemVer::valid(
  const std::string &v,
  bool loose
) {
  HTiSemVer tmp;
  return tmp.parse(v, loose);
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
  size_t pos = 0;
  if (s.compare(0, 2, "<=") == 0) {
    cond.op = SemverCondition::OP_LTE;
    pos = 2;
  } else if (s.compare(0, 2, ">=") == 0) {
    cond.op = SemverCondition::OP_GTE;
    pos = 2;
  } else if (!s.empty() && s[0] == '<') {
    cond.op = SemverCondition::OP_LT;
    pos = 1;
  } else if (!s.empty() && s[0] == '>') {
    cond.op = SemverCondition::OP_GT;
    pos = 1;
  } else if (!s.empty() && s[0] == '=') {
    cond.op = SemverCondition::OP_EQ;
    pos = 1;
  } else {
    cond.op = SemverCondition::OP_EQ;
  }

  PartialVersion pv;
  if (!parsePartialVersion(trim(s.substr(pos)), loose, pv) || pv.patchWild()) {
    // Partial or unparseable version: return a no-match sentinel.
    return cond;
  }

  cond.major = pv.major;
  cond.minor = pv.minor;
  cond.patch = pv.patch;
  cond.prerelease = pv.prerelease;
  cond.build = pv.build;
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

  // Logical OR ("||").
  // Recurse on each side.  We scan left-to-right so nested || still works.
  size_t orPos = s.find("||");
  if (orPos != std::string::npos) {
    expandRangePart(s.substr(0, orPos),  loose, includePrerelease, outSets);
    expandRangePart(s.substr(orPos + 2), loose, includePrerelease, outSets);
    return;
  }

  // Hyphen range "left - right".
  // "1.2.3 - 2.3.4"  ->  >=1.2.3  <=2.3.4
  // "1.2   - 2.3.4"  ->  >=1.2.0  <=2.3.4   (left partial: fill with 0)
  // "1.2.3 - 2.3"    ->  >=1.2.3  <2.4.0-0  (right partial major.minor)
  // "1.2.3 - 2"      ->  >=1.2.3  <3.0.0-0  (right partial major only)
  size_t hyphen = s.find(" - ");
  if (hyphen != std::string::npos) {
    std::string leftStr  = trim(s.substr(0, hyphen));
    std::string rightStr = trim(s.substr(hyphen + 3));

    PartialVersion pvL, pvR;
    bool okL = parsePartialVersion(leftStr,  loose, pvL);
    bool okR = parsePartialVersion(rightStr, loose, pvR);

    ConditionSet cs;

    // Left side: always GTE; fill any wildcards with 0
    if (okL && !pvL.majorWild())
      cs.push_back(makeGTEfromPartial(pvL, includePrerelease));

    // Right side:
    if (okR && !pvR.majorWild()) {
      if (!pvR.patchWild()) {
        // Fully-specified right side -> <=right
        SemverCondition hi;
        hi.op    = OP_LTE;
        hi.major = pvR.major;
        hi.minor = pvR.minor;
        hi.patch = pvR.patch;
        hi.prerelease = pvR.prerelease;
        hi.build      = pvR.build;
        hi.explicitPrerelease = includePrerelease || !hi.prerelease.empty();
        cs.push_back(hi);
      } else if (!pvR.minorWild()) {
        // major.minor only -> <major.(minor+1).0-0
        cs.push_back(makeLTSentinel(pvR.major, pvR.minor + 1, 0));
      } else {
        // major only -> <(major+1).0.0-0
        cs.push_back(makeLTSentinel(pvR.major + 1, 0, 0));
      }
    }

    if (!cs.empty())
      outSets.push_back(cs);

    return;
  }

  // Whitespace-separated comparators (AND set).
  ConditionSet cs;
  std::istringstream iss(s);
  std::string token;

  while (iss >> token) {

    // Tilde range ~partial
    // ~M.N.P  ->  >=M.N.P  <M.(N+1).0-0
    // ~M.N    ->  >=M.N.0  <M.(N+1).0-0   (same as 1.2.x)
    // ~M      ->  >=M.0.0  <(M+1).0.0-0   (same as M.x)
    if (token[0] == '~') {
      PartialVersion pv;
      if (!parsePartialVersion(token.substr(1), loose, pv))
        continue;

      if (pv.majorWild()) {
        // ~* -> >=0.0.0
        SemverCondition lo;
        lo.op = OP_GTE;
        lo.major = 0; lo.minor = 0; lo.patch = 0;
        lo.explicitPrerelease = includePrerelease;
        cs.push_back(lo);
        continue;
      }

      // Lower bound: >=M.N.P (missing parts filled with 0)
      cs.push_back(makeGTEfromPartial(pv, includePrerelease));

      // Upper bound:
      if (pv.minorWild()) {
        // ~M -> <(M+1).0.0-0
        cs.push_back(makeLTSentinel(pv.major + 1, 0, 0));
      } else {
        // ~M.N or ~M.N.P -> <M.(N+1).0-0
        cs.push_back(makeLTSentinel(pv.major, pv.minor + 1, 0));
      }
      continue;
    }

    // Caret range ^partial
    // Locks the left-most non-zero, non-wild element.
    //
    // All specified:
    //   ^M.N.P (M>0)  ->  >=M.N.P  <(M+1).0.0-0
    //   ^0.N.P (N>0)  ->  >=0.N.P  <0.(N+1).0-0
    //   ^0.0.P        ->  >=0.0.P  <0.0.(P+1)-0
    //
    // Patch wild:
    //   ^M.N.x (M>0)  ->  >=M.N.0  <(M+1).0.0-0
    //   ^0.N.x        ->  >=0.N.0  <0.(N+1).0-0
    //   ^0.0.x / ^0.0 ->  >=0.0.0  <0.1.0-0
    //
    // Minor+patch wild:
    //   ^M.x / ^M     ->  >=M.0.0  <(M+1).0.0-0
    //   ^0.x / ^0     ->  >=0.0.0  <1.0.0-0
    if (token[0] == '^') {
      PartialVersion pv;
      if (!parsePartialVersion(token.substr(1), loose, pv))
        continue;

      if (pv.majorWild()) {
        // ^* -> >=0.0.0
        SemverCondition lo;
        lo.op = OP_GTE;
        lo.major = 0; lo.minor = 0; lo.patch = 0;
        lo.explicitPrerelease = includePrerelease;
        cs.push_back(lo);
        continue;
      }

      int maj = pv.major;
      int min = pv.minorWild() ? 0 : pv.minor;
      int pat = pv.patchWild() ? 0 : pv.patch;

      // Lower bound
      SemverCondition lo;
      lo.op    = OP_GTE;
      lo.major = maj; lo.minor = min; lo.patch = pat;
      lo.prerelease = pv.prerelease;
      lo.build      = pv.build;
      lo.explicitPrerelease = includePrerelease || !lo.prerelease.empty();
      cs.push_back(lo);

      // Upper bound
      if (pv.minorWild()) {
        // ^M.x or ^M
        cs.push_back(makeLTSentinel(maj + 1, 0, 0));
      } else if (pv.patchWild()) {
        // ^M.N.x
        if (maj > 0)
          cs.push_back(makeLTSentinel(maj + 1, 0, 0));
        else
          cs.push_back(makeLTSentinel(0, min + 1, 0));
      } else {
        // All parts specified
        if (maj > 0)
          cs.push_back(makeLTSentinel(maj + 1, 0, 0));
        else if (min > 0)
          cs.push_back(makeLTSentinel(0, min + 1, 0));
        else
          cs.push_back(makeLTSentinel(0, 0, pat + 1));
      }
      continue;
    }

    // Primitive comparator or bare version.
    // Detect optional leading operator.
    size_t pos = 0;
    Op op = OP_EQ;
    if (token.size() >= 2 && token.compare(0, 2, "<=") == 0) {
      op = OP_LTE; pos = 2;
    } else if (token.size() >= 2 && token.compare(0, 2, ">=") == 0) {
      op = OP_GTE; pos = 2;
    } else if (token[0] == '<') {
      op = OP_LT; pos = 1;
    } else if (token[0] == '>') {
      op = OP_GT; pos = 1;
    } else if (token[0] == '=') {
      op = OP_EQ; pos = 1;
    }

    PartialVersion pv;
    if (!parsePartialVersion(trim(token.substr(pos)), loose, pv))
      continue;

    // Bare wildcard ("*", "", "=*", etc.) -> >=0.0.0
    if (pv.majorWild()) {
      SemverCondition lo;
      lo.op = OP_GTE;
      lo.major = 0; lo.minor = 0; lo.patch = 0;
      lo.explicitPrerelease = includePrerelease;
      cs.push_back(lo);
      continue;
    }

    bool partial = pv.minorWild() || pv.patchWild();

    if (!partial) {
      // Full version: emit comparator literally
      SemverCondition cond;
      cond.op    = op;
      cond.major = pv.major; cond.minor = pv.minor; cond.patch = pv.patch;
      cond.prerelease = pv.prerelease;
      cond.build      = pv.build;
      cond.explicitPrerelease = includePrerelease || !cond.prerelease.empty();
      cs.push_back(cond);

    } else {
      // Partial version: desugar per operator.
      int maj = pv.major;
      int min = pv.minorWild() ? 0 : pv.minor;

      switch (op) {
        case OP_EQ: {
          // Treat bare partial as X-range.
          // "1"   -> >=1.0.0 <2.0.0-0
          // "1.2" -> >=1.2.0 <1.3.0-0
          cs.push_back(makeGTEfromPartial(pv, includePrerelease));
          if (pv.minorWild())
            cs.push_back(makeLTSentinel(maj + 1, 0, 0));
          else
            cs.push_back(makeLTSentinel(maj, min + 1, 0));
          break;
        }
        case OP_GT: {
          // ">M"   -> ">=(M+1).0.0"
          // ">M.N" -> ">=M.(N+1).0"
          SemverCondition lo;
          lo.op = OP_GTE;
          lo.prerelease = {};
          lo.explicitPrerelease = includePrerelease;
          if (pv.minorWild()) {
            lo.major = maj + 1; lo.minor = 0; lo.patch = 0;
          } else {
            lo.major = maj; lo.minor = min + 1; lo.patch = 0;
          }
          cs.push_back(lo);
          break;
        }
        case OP_GTE: {
          // ">=M" -> ">=M.0.0"  ">=M.N" -> ">=M.N.0"
          cs.push_back(makeGTEfromPartial(pv, includePrerelease));
          break;
        }
        case OP_LT: {
          // "<M"   -> "<M.0.0-0"
          // "<M.N" -> "<M.N.0-0"
          if (pv.minorWild())
            cs.push_back(makeLTSentinel(maj, 0, 0));
          else
            cs.push_back(makeLTSentinel(maj, min, 0));
          break;
        }
        case OP_LTE: {
          // "<=M"   -> "<(M+1).0.0-0"
          // "<=M.N" -> "<M.(N+1).0-0"
          if (pv.minorWild())
            cs.push_back(makeLTSentinel(maj + 1, 0, 0));
          else
            cs.push_back(makeLTSentinel(maj, min + 1, 0));
          break;
        }
      }
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
