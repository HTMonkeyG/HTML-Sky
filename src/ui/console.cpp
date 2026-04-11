#include <cmath>
#include <vector>
#include <shared_mutex>
#include "imgui.h"

#include "htinternal.hpp"
#include "htaliases.h"

// Color code marker.
#define CONSOLE_MAX_LINE 2048
#define LINE_SPACING 2.0f
#define MARGIN_LEFT 1.0f
#define TAB_SIZE 2

struct ConsoleChar {
  char chByte;
  ImU32 color;

  // Only for create object easily.
  ConsoleChar(char ch, ImU32 c): chByte(ch), color(c) { }
};
struct ConsoleLine {
  std::vector<ConsoleChar> s;
  i32 repeatCount;

  ConsoleLine(): repeatCount(1) { }
};
typedef std::vector<ConsoleLine> ConsoleText;

static std::shared_mutex gMutex;

static ConsoleText gText;
static bool gScrollEnd = false;
static f32 gLastTextHeight = 0;

static struct {
  char *line;
  bool raw;
} gLastLine = {0};

static bool checkColorMark(std::vector<ConsoleChar> &charBuf) {
  // The byte sequence of '§'.
  return (
    charBuf.size() == 2
    && charBuf[0].chByte == (char)0xC2
    && charBuf[1].chByte == (char)0xA7
  );
}

// We assume that the char is a standalone character (<128) or a leading byte
// of an UTF-8 code sequence (non-10xxxxxx code)
static u08 utf8CharLen(char c) {
  if ((c & 0xFE) == 0xFC)
    return 6;
  if ((c & 0xFC) == 0xF8)
    return 5;
  if ((c & 0xF8) == 0xF0)
    return 4;
  else if ((c & 0xF0) == 0xE0)
    return 3;
  else if ((c & 0xE0) == 0xC0)
    return 2;
  return 1;
}

static ImU32 colorIndex(char index) {
  switch(index) {
    case '0':
      return 0xFF000000;
    case '1':
      return 0xFFAA0000;
    case '2':
      return 0xFF00AA00;
    case '3':
      return 0xFFAAAA00;
    case '4':
      return 0xFF0000AA;
    case '5':
      return 0xFFAA00AA;
    case '6':
      return 0xFF00AAFF;
    case '7':
      return 0xFFAAAAAA;
    case '8':
      return 0xFF555555;
    case '9':
      return 0xFFFF5555;
    case 'a':
      return 0xFF55FF55;
    case 'b':
      return 0xFFFFFF55;
    case 'c':
      return 0xFF5555FF;
    case 'd':
      return 0xFFFF55FF;
    case 'e':
      return 0xFF55FFFF;
    case 'f':
    default:
      return 0xFFFFFFFF;
  }
}

// Format texts with color excape sequence.
static void textFormat(
  const char *str,
  u64 length,
  ImU32 basicColor
) {
  ConsoleLine line;
  ImU32 color = basicColor;
  u08 insideChar = 0
    , insideEscape = 0
    , insideIgnore = 0;
  std::vector<ConsoleChar> charBuffer;

  for (u64 i = 0; i < length; i++) {
    char ch = str[i];

    if (insideEscape == 1) {
      // First character after entering the escape sequence.
      if (ch == 'r') {
        // Reset color to basic color.
        color = basicColor;
        insideEscape = 0;
        // Skip (ignore) the character 'r'.
        continue;
      } else if (ch == '#') {
        // TODO: Implement RGBA color parsing.
        insideIgnore = 7;
        insideEscape = 2;
      } else {
        // Ignore the first trailing character.
        color = colorIndex(ch);
        insideIgnore = utf8CharLen(ch);
      }
    }

    if (insideEscape && insideIgnore) {
      // Ignore a byte sequence.
      insideIgnore--;
      if (!insideIgnore)
        insideEscape = 0;
      // Skip current byte.
      continue;
    }

    if (!insideChar)
      // Normal character.
      insideChar = utf8CharLen(ch);

    if (insideChar) {
      // Push a byte.
      charBuffer.push_back(ConsoleChar(ch, color));
      insideChar--;
      if (!insideChar) {
        if (checkColorMark(charBuffer))
          // Enter the color code escape sequence and skip the character '§'.
          insideEscape = 1;
        else {
          line.s.insert(line.s.end(), charBuffer.begin(), charBuffer.end());
        }
        // Push parsed character.
        charBuffer.clear();
      }
    }
  }

  if (!charBuffer.empty()) {
    // Push unfinished character.
    line.s.insert(line.s.end(), charBuffer.begin(), charBuffer.end());
    charBuffer.clear();
  }

  gText.push_back(line);
}

// Format texts with specified color.
static void textFormatRaw(
  const char *str,
  u64 length,
  ImU32 color
) {
  ConsoleLine line;

  for (u64 i = 0; i < length; i++)
    line.s.push_back(ConsoleChar(str[i], color));

  gText.push_back(line);
}

// Split input string at each '\n'.
static void textFormatInto(
  const char *str,
  ImU32 basicColor,
  bool raw
) {
  const char *pBegin = str;
  u64 length = strlen(str)
    , subStrLen = 1;
  u08 insideChar = 0;
  void (*fmt)(const char *, u64, ImU32);

  fmt = raw
    ? textFormatRaw
    : textFormat;

  for (u64 i = 0; i < length + 1; i++, subStrLen++) {
    char ch = str[i];

    if (!insideChar && (ch == '\n' || ch == '\0')) {
      fmt(pBegin, subStrLen - 1, basicColor);
      pBegin = &str[i + 1];
      subStrLen = 0;
      if (*pBegin == '\0')
        break;
      continue;
    }

    if (!insideChar)
      // We need utf8 per character awareness.
      insideChar = utf8CharLen(ch);
    if (insideChar)
      insideChar--;
  }
}

void HTiConsoleScrollEnd() {
  gScrollEnd = true;
}

void HTiRenderConsoleTexts() {
  std::shared_lock<std::shared_mutex> lock(gMutex);

  ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(0, 0));
  ImGui::BeginChild(
    "##ConsoleTexts",
    ImVec2(0, 0),
    0,
    ImGuiWindowFlags_HorizontalScrollbar);

  ImVec2 contentSize = ImGui::GetWindowContentRegionMax();
  ImDrawList *drawList = ImGui::GetWindowDrawList();

  f32 totalWidth = 0;
  f32 lineHeight = ImGui::CalcTextSize("#").y;
  f32 spaceWidth = ImGui::CalcTextSize(" ").x;
  lineHeight += LINE_SPACING;

  ImVec2 cursorScreenPos = ImGui::GetCursorScreenPos();
  f32 scrollY = ImGui::GetScrollY();

  // Visible line range.
  i32 lineBegin = (i32)floorf(scrollY / lineHeight);
  i32 lineEnd = std::max(std::min(
    (i32)(gText.size() - 1),
    lineBegin + (i32)floor((scrollY + contentSize.y) / lineHeight) + 1
  ), 0);

  if (gText.empty()) {
    // Early return.
    ImGui::Dummy(ImVec2(0, 0));
    ImGui::EndChild();
    ImGui::PopStyleVar();
    return;
  }

  ImU32 prevColor = 0xFFFFFFFF;
  std::string lineBuffer;
  for (i32 i = 0; i < (i32)gText.size(); i++) {
    ConsoleLine &line = gText[i];
    ImVec2 lineStartScreenPos = ImVec2(cursorScreenPos.x, cursorScreenPos.y + i * lineHeight)
      , textScreenPos = ImVec2(lineStartScreenPos.x + MARGIN_LEFT, lineStartScreenPos.y)
      , textPosOffset(0, 0);

    if (i < lineBegin || i > lineEnd) {
      // If the line is invisible, then just calculate its width.
      for (u64 j = 0; j < line.s.size(); )
        lineBuffer.push_back(line.s[j++].chByte);

      textPosOffset.x += ImGui::CalcTextSize(lineBuffer.c_str()).x;
      totalWidth = std::max(totalWidth, textPosOffset.x);
      lineBuffer.clear();
      continue;
    }

    if (line.repeatCount > 1) {
      // Show the repeat count.
      lineBuffer.resize(20);
      i32 len = sprintf(lineBuffer.data(), "(x%d) ", line.repeatCount);
      lineBuffer.resize(len);
    }

    for (u64 j = 0; j < line.s.size(); ) {
      ConsoleChar &data = line.s[j];
      ImU32 color = data.color;

      if ((color != prevColor || data.chByte == '\t' || data.chByte == ' ') && !lineBuffer.empty()) {
        // Submit drawing when color changes or invisible characters exist.
        const ImVec2 textPos(
          textScreenPos.x + textPosOffset.x,
          textScreenPos.y + textPosOffset.y);

        drawList->AddText(textPos, prevColor, lineBuffer.c_str());
        textPosOffset.x += ImGui::CalcTextSize(lineBuffer.c_str()).x;
        lineBuffer.clear();
      }
      // Record color of current text slice.
      prevColor = color;

      if (data.chByte == '\t') {
        textPosOffset.x = (1.0f + std::floor((1.0f + textPosOffset.x) / (TAB_SIZE * spaceWidth))) * (TAB_SIZE * spaceWidth);
        j++;
      } else if (data.chByte == ' ') {
        textPosOffset.x += spaceWidth;
        j++;
      } else {
        // Push a byte sequence representing a single character.
        u08 l = utf8CharLen(data.chByte);
        while (l-- > 0)
          lineBuffer.push_back(line.s[j++].chByte);
      }
    }

    if (!lineBuffer.empty()) {
      const ImVec2 textPos(
        textScreenPos.x + textPosOffset.x,
        textScreenPos.y + textPosOffset.y);

      drawList->AddText(textPos, prevColor, lineBuffer.c_str());
      textPosOffset.x += ImGui::CalcTextSize(lineBuffer.c_str()).x;
      lineBuffer.clear();
    }

    totalWidth = std::max(totalWidth, textPosOffset.x);
  }

  // We do not directly use the ImGui widgets to render texts, instead we use
  // drawlist to detach document stream. So we need to create this to enable
  // scroll bars.
  f32 textHeight = gText.size() * lineHeight;
  ImGui::Dummy(ImVec2(totalWidth, textHeight));

  if (gScrollEnd) {
    // ImGui::GetScrollMaxY() is calculated from the last frame's height, so
    // we need to add increased text heights in current frame.
    ImGui::SetScrollX(0);
    ImGui::SetScrollY(ImGui::GetScrollMaxY() + std::max(0.0f, textHeight - gLastTextHeight));
    gScrollEnd = false;
  }

  gLastTextHeight = textHeight;

  ImGui::EndChild();
  ImGui::PopStyleVar();
}

void HTiAddConsoleLineV(
  bool raw,
  const char *fmt,
  va_list args
) {
  std::unique_lock<std::shared_mutex> lock(gMutex);

  va_list argDup;
  char *buffer;
  size_t len;
  bool repeated = false;

  va_copy(argDup, args);

  len = vsnprintf(nullptr, 0, fmt, args);
  if (len <= 0)
    return;

  buffer = (char *)ImGui::MemAlloc(len + 1);
  if (!buffer)
    return;

  vsnprintf(buffer, len + 1, fmt, argDup);
  buffer[len] = 0;

  if (gText.size() >= CONSOLE_MAX_LINE)
    // Remove the earliest line if the maximum number of lines is reached.
    gText.erase(gText.begin());

  if (!gLastLine.line) {
    // If the console is empty, save current line as the last line.
    gLastLine.line = buffer;
    textFormatInto(buffer, 0xFFFFFFFF, raw);
  } else if (!strcmp(gLastLine.line, buffer) && gLastLine.raw == raw) {
    // Repeated line, free current buffer and increase repeat count.
    if (!gText.empty())
      gText.back().repeatCount++;
    ImGui::MemFree(buffer);
    repeated = true;
  } else {
    // Free the old line and add our new line.
    ImGui::MemFree(gLastLine.line);
    textFormatInto(buffer, 0xFFFFFFFF, raw);
    gLastLine.line = buffer;
  }

  if (!repeated)
    // Scroll to the end only when new line was actually added.
    HTiConsoleScrollEnd();
}

void HTiAddConsoleLine(
  bool raw,
  const char *fmt,
  ...
) {
  va_list args;

  va_start(args, fmt);
  HTiAddConsoleLineV(raw, fmt, args);
  va_end(args);
}

void HTiClearConsole() {
  std::unique_lock<std::shared_mutex> lock(gMutex);

  gText.clear();

  if (gLastLine.line)
    ImGui::MemFree(gLastLine.line);
  gLastLine.line = nullptr;

  HTiConsoleScrollEnd();
}
