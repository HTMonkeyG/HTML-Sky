#include <cmath>
#include <vector>
#include "imgui.h"

#include "htinternal.h"
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

typedef std::vector<ConsoleChar> ConsoleLine;
typedef std::vector<ConsoleLine> ConsoleText;

static ConsoleText gText;

static bool checkColorMark(ConsoleLine &charBuf) {
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
          line.insert(line.end(), charBuffer.begin(), charBuffer.end());
        }
        // Push parsed character.
        charBuffer.clear();
      }
    }
  }

  if (!charBuffer.empty()) {
    // Push unfinished character.
    line.insert(line.end(), charBuffer.begin(), charBuffer.end());
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
    line.push_back(ConsoleChar(str[i], color));

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
    , subStrLen = 0;
  u08 insideChar = 0;
  void (*fmt)(const char *, u64, ImU32);

  fmt = raw
    ? textFormatRaw
    : textFormat;

  for (u64 i = 0; i < length + 1; i++, subStrLen++) {
    char ch = str[i];

    if (!insideChar && (ch == '\n' || ch == '\0')) {
      fmt(pBegin, subStrLen, basicColor);
      pBegin = &str[i + 1];
      continue;
    }

    if (!insideChar)
      // We need utf8 per character awareness.
      insideChar = utf8CharLen(ch);
    if (insideChar)
      insideChar--;
  }
}

void HTiRenderConsoleTexts() {
  ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(0, 0));
  ImGui::BeginChild(
    "##ConsoleTexts",
    ImVec2(0, 0),
    0,
    ImGuiWindowFlags_NoMove | ImGuiWindowFlags_HorizontalScrollbar);

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
    lineBegin + (i32)floor((scrollY + contentSize.y) / lineHeight)
  ), 0);

  if (gText.empty()) {
    // Early return.
    ImGui::Dummy(ImVec2(0, 0));
    ImGui::EndChild();
    ImGui::PopStyleVar();
    return;
  }

  ImU32 prevColor;
  std::string lineBuffer;
  for (i32 i = lineBegin; i <= lineEnd; i++) {
    ConsoleLine &line = gText[i];
    ImVec2 lineStartScreenPos = ImVec2(cursorScreenPos.x, cursorScreenPos.y + i * lineHeight);
    ImVec2 textScreenPos = ImVec2(lineStartScreenPos.x + MARGIN_LEFT, lineStartScreenPos.y);
    ImVec2 textPosOffset(0, 0);

    for (u64 j = 0; j < line.size(); ) {
      ConsoleChar &data = line[j];
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
          lineBuffer.push_back(line[j++].chByte);
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
  ImGui::Dummy(ImVec2(totalWidth, gText.size() * lineHeight));

  ImGui::EndChild();
  ImGui::PopStyleVar();
}

void HTiAddConsoleLineV(
  bool raw,
  const char *fmt,
  va_list args
) {
  va_list argDup;
  char *buffer;
  size_t len;

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

  // Add our new line.
  textFormatInto(buffer, 0xFFFFFFFF, raw);

  ImGui::MemFree(buffer);
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
  gText.clear();
}
