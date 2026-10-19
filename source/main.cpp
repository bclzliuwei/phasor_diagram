#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>

#include <algorithm>
#include <array>
#include <cmath>
#include <cwchar>
#include <cwctype>
#include <iomanip>
#include <sstream>
#include <string>

namespace
{

constexpr wchar_t kMainClass[] = L"PhasorDiagramMainWindow";
constexpr wchar_t kPlotClass[] = L"PhasorDiagramPlotWindow";
constexpr int kPhaseCount = 3;
constexpr int kMagnitudeBaseId = 100;
constexpr int kAngleBaseId = 110;
constexpr int kPhaseBaseId = 120;
constexpr int kRotationInputId = 130;
constexpr int kCyclesInputId = 131;
constexpr int kBalanceButtonId = 9;
constexpr int kWaveformCheckboxId = 10;
constexpr int kCyclesLabelId = 11;
constexpr int kCursorReadoutId = 12;
constexpr int kCursorCount = 2;
constexpr int kCursorHitPixels = 6;

const std::array<const wchar_t*, kPhaseCount> kPhaseNames = {L"A", L"B", L"C"};
const std::array<COLORREF, kPhaseCount> kPhaseColors = {
    RGB(255, 0, 0),
    RGB(230, 110, 35),
    RGB(0, 0, 255),
};
const std::array<COLORREF, kCursorCount> kCursorColors = {
    RGB(0, 140, 70),
    RGB(150, 0, 170),
};

struct Phasor
{
  double magnitude = 0.0;
  double angleDegrees = 0.0;
};

struct AppState
{
  std::array<HWND, kPhaseCount> magnitudeInputs{};
  std::array<HWND, kPhaseCount> angleInputs{};
  HWND rotationInput = nullptr;
  HWND cyclesInput = nullptr;
  HWND status = nullptr;
  HWND cursorReadout = nullptr;
  HWND plot = nullptr;
  HFONT titleFont = nullptr;
  std::array<Phasor, kPhaseCount> phasors{};
  double rotationDegrees = 0.0;
  int waveformCycles = 2;
  bool inputsValid = true;
  bool showWaveforms = false;
  std::array<double, kCursorCount> cursorAngles = {90.0, 210.0};
  int activeCursor = 0;
  int draggingCursor = -1;
};

void UpdateCursorReadout(const AppState& state)
{
  if (state.cursorReadout == nullptr)
  {
    return;
  }
  std::wostringstream text;
  text << std::fixed << std::setprecision(1);
  for (int index = 0; index < kCursorCount; ++index)
  {
    text << (index == state.activeCursor ? L"> " : L"   ") << L"Line " << index + 1 << L": "
         << state.cursorAngles[index] << L" deg\n";
  }
  text << L"   \u0394 (Line 2 - Line 1): " << state.cursorAngles[1] - state.cursorAngles[0]
       << L" deg";
  SetWindowTextW(state.cursorReadout, text.str().c_str());
}

void SetCursorAngle(AppState& state, int index, double angle)
{
  state.cursorAngles[index] = std::clamp(angle, 0.0, state.waveformCycles * 360.0);
  UpdateCursorReadout(state);
  InvalidateRect(state.plot, nullptr, FALSE);
}

HFONT CreateUiFont(int height, int weight)
{
  return CreateFontW(height, 0, 0, 0, weight, FALSE, FALSE, FALSE, DEFAULT_CHARSET,
                     OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY,
                     DEFAULT_PITCH | FF_DONTCARE, L"Segoe UI");
}

HWND CreateLabel(HWND parent, const wchar_t* text, int controlId, DWORD style = SS_LEFT)
{
  return CreateWindowExW(0, L"STATIC", text, WS_CHILD | WS_VISIBLE | style, 0, 0, 0, 0, parent,
                         reinterpret_cast<HMENU>(static_cast<INT_PTR>(controlId)),
                         GetModuleHandleW(nullptr), nullptr);
}

HWND CreateInput(HWND parent, int controlId, const wchar_t* text = L"0")
{
  return CreateWindowExW(WS_EX_CLIENTEDGE, L"EDIT", text,
                         WS_CHILD | WS_VISIBLE | WS_TABSTOP | ES_AUTOHSCROLL | ES_RIGHT, 0, 0, 0, 0,
                         parent, reinterpret_cast<HMENU>(static_cast<INT_PTR>(controlId)),
                         GetModuleHandleW(nullptr), nullptr);
}

HWND CreateButton(HWND parent, int controlId, const wchar_t* text)
{
  return CreateWindowExW(0, L"BUTTON", text, WS_CHILD | WS_VISIBLE | WS_TABSTOP | BS_PUSHBUTTON, 0,
                         0, 0, 0, parent, reinterpret_cast<HMENU>(static_cast<INT_PTR>(controlId)),
                         GetModuleHandleW(nullptr), nullptr);
}

void SetControlFont(HWND control, HFONT font)
{
  SendMessageW(control, WM_SETFONT, reinterpret_cast<WPARAM>(font), TRUE);
}

bool ReadNumber(HWND input, double& value)
{
  wchar_t text[128]{};
  GetWindowTextW(input, text, static_cast<int>(std::size(text)));

  wchar_t* end = nullptr;
  const double parsed = std::wcstod(text, &end);
  if (text == end || !std::isfinite(parsed))
  {
    return false;
  }

  while (*end != L'\0' && std::iswspace(*end))
  {
    ++end;
  }
  if (*end != L'\0')
  {
    return false;
  }

  value = parsed;
  return true;
}

void UpdateInputs(AppState& state)
{
  std::array<Phasor, kPhaseCount> updated{};
  for (int index = 0; index < kPhaseCount; ++index)
  {
    if (!ReadNumber(state.magnitudeInputs[index], updated[index].magnitude) ||
        !ReadNumber(state.angleInputs[index], updated[index].angleDegrees))
    {
      state.inputsValid = false;
      SetWindowTextW(state.status, L"Enter a valid number in every input field.");
      InvalidateRect(state.plot, nullptr, TRUE);
      return;
    }
    if (updated[index].magnitude < 0.0 || updated[index].magnitude > 1.0e12)
    {
      state.inputsValid = false;
      SetWindowTextW(state.status, L"Magnitudes must be between 0 and 1,000,000,000,000.");
      InvalidateRect(state.plot, nullptr, TRUE);
      return;
    }
    if (std::abs(updated[index].angleDegrees) > 360000.0)
    {
      state.inputsValid = false;
      SetWindowTextW(state.status, L"Angles must be between -360,000 and 360,000 degrees.");
      InvalidateRect(state.plot, nullptr, TRUE);
      return;
    }
  }

  double rotationDegrees = 0.0;
  if (!ReadNumber(state.rotationInput, rotationDegrees))
  {
    state.inputsValid = false;
    SetWindowTextW(state.status, L"Enter a valid diagram rotation.");
    InvalidateRect(state.plot, nullptr, TRUE);
    return;
  }
  if (std::abs(rotationDegrees) > 360000.0)
  {
    state.inputsValid = false;
    SetWindowTextW(state.status, L"Diagram rotation must be between -360,000 and 360,000 degrees.");
    InvalidateRect(state.plot, nullptr, TRUE);
    return;
  }

  double waveformCycles = state.waveformCycles;
  if (state.showWaveforms &&
      (!ReadNumber(state.cyclesInput, waveformCycles) || waveformCycles < 1.0 ||
       waveformCycles > 100.0 || std::floor(waveformCycles) != waveformCycles))
  {
    state.inputsValid = false;
    SetWindowTextW(state.status, L"Cycles must be a whole number from 1 to 100.");
    InvalidateRect(state.plot, nullptr, TRUE);
    return;
  }

  state.phasors = updated;
  state.rotationDegrees = rotationDegrees;
  state.waveformCycles = static_cast<int>(waveformCycles);
  for (double& angle : state.cursorAngles)
  {
    angle = std::clamp(angle, 0.0, state.waveformCycles * 360.0);
  }
  UpdateCursorReadout(state);
  state.inputsValid = true;
  const bool allZero = std::all_of(updated.begin(), updated.end(),
                                   [](const Phasor& phasor) { return phasor.magnitude == 0.0; });
  SetWindowTextW(state.status, allZero ? L"Enter a non-zero magnitude to display the phases."
                                       : L"Plot updates as the phase values change.");
  InvalidateRect(state.plot, nullptr, TRUE);
}

void ApplyBalancedPhases(AppState& state)
{
  UpdateInputs(state);
  if (!state.inputsValid)
  {
    return;
  }

  const double referenceMagnitude = state.phasors[0].magnitude;
  const double referenceAngle = state.phasors[0].angleDegrees;
  const std::array<double, 2> balancedAngles = {
      std::remainder(referenceAngle - 120.0, 360.0),
      std::remainder(referenceAngle + 120.0, 360.0),
  };

  const auto setNumber = [](HWND input, double value) {
    std::wostringstream text;
    text << std::setprecision(12) << std::defaultfloat << value;
    SetWindowTextW(input, text.str().c_str());
  };

  for (int index = 1; index < kPhaseCount; ++index)
  {
    setNumber(state.magnitudeInputs[index], referenceMagnitude);
    setNumber(state.angleInputs[index], balancedAngles[index - 1]);
  }

  UpdateInputs(state);
  if (state.inputsValid && referenceMagnitude > 0.0)
  {
    SetWindowTextW(state.status, L"Balanced positive ABC sequence applied from phase A.");
  }
}

double NiceScaleMaximum(double maximum)
{
  if (maximum <= 0.0)
  {
    return 1.0;
  }

  const double magnitude = std::pow(10.0, std::floor(std::log10(maximum)));
  const double normalized = maximum / magnitude;
  const double nice = normalized <= 1.0   ? 1.0
                      : normalized <= 2.0 ? 2.0
                      : normalized <= 5.0 ? 5.0
                                          : 10.0;
  return nice * magnitude;
}

void DrawText(HDC dc, const std::wstring& text, int x, int y, COLORREF color)
{
  SetTextColor(dc, color);
  TextOutW(dc, x, y, text.c_str(), static_cast<int>(text.size()));
}

void DrawArrow(HDC dc, const POINT& center, const POINT& tip, COLORREF color)
{
  const int saved = SaveDC(dc);
  HPEN pen = CreatePen(PS_SOLID, 3, color);
  HGDIOBJ oldPen = SelectObject(dc, pen);

  MoveToEx(dc, center.x, center.y, nullptr);
  LineTo(dc, tip.x, tip.y);

  const double direction =
      std::atan2(static_cast<double>(tip.y - center.y), static_cast<double>(tip.x - center.x));
  constexpr double arrowLength = 11.0;
  constexpr double spread = 0.48;
  for (const double offset : {-spread, spread})
  {
    const int baseX = static_cast<int>(tip.x - arrowLength * std::cos(direction + offset));
    const int baseY = static_cast<int>(tip.y - arrowLength * std::sin(direction + offset));
    MoveToEx(dc, tip.x, tip.y, nullptr);
    LineTo(dc, baseX, baseY);
  }

  SelectObject(dc, oldPen);
  DeleteObject(pen);
  RestoreDC(dc, saved);
}

void PaintPhasors(HWND window, HDC dc, const AppState& state)
{
  RECT bounds{};
  GetClientRect(window, &bounds);

  const int width = bounds.right - bounds.left;
  const int height = bounds.bottom - bounds.top;
  const int centerX = width / 2;
  const int centerY = height / 2;
  const int radius = std::max(1, std::min(width - 100, height - 90) / 2);
  const double maximum = state.inputsValid
                             ? std::max({state.phasors[0].magnitude, state.phasors[1].magnitude,
                                         state.phasors[2].magnitude})
                             : 0.0;
  const double scaleMaximum = NiceScaleMaximum(maximum);

  const int saved = SaveDC(dc);
  HPEN gridPen = CreatePen(PS_SOLID, 1, RGB(222, 228, 230));
  HPEN axisPen = CreatePen(PS_SOLID, 1, RGB(139, 151, 156));
  HGDIOBJ oldPen = SelectObject(dc, gridPen);

  for (int ring = 1; ring <= 4; ++ring)
  {
    const int ringRadius = radius * ring / 4;
    Ellipse(dc, centerX - ringRadius, centerY - ringRadius, centerX + ringRadius,
            centerY + ringRadius);

    std::wostringstream tick;
    tick << std::setprecision(4) << std::defaultfloat << scaleMaximum * ring / 4.0;
    DrawText(dc, tick.str(), std::max(2, centerX - 35), centerY - ringRadius - 18,
             RGB(89, 102, 108));
  }

  SelectObject(dc, axisPen);
  MoveToEx(dc, centerX - radius, centerY, nullptr);
  LineTo(dc, centerX + radius, centerY);
  MoveToEx(dc, centerX, centerY - radius, nullptr);
  LineTo(dc, centerX, centerY + radius);
  DrawText(dc, L"Real", centerX + radius + 6, centerY - 9, RGB(89, 102, 108));
  DrawText(dc, L"Imaginary", centerX + 5, centerY - radius - 20, RGB(89, 102, 108));

  SelectObject(dc, oldPen);
  DeleteObject(gridPen);
  DeleteObject(axisPen);
  RestoreDC(dc, saved);

  if (!state.inputsValid)
  {
    return;
  }

  for (int index = 0; index < kPhaseCount; ++index)
  {
    const Phasor& phasor = state.phasors[index];
    if (phasor.magnitude <= 0.0)
    {
      continue;
    }

    const double displayedAngle =
        std::remainder(phasor.angleDegrees + state.rotationDegrees, 360.0);
    const double radians = displayedAngle * 3.14159265358979323846 / 180.0;
    const double length = phasor.magnitude / scaleMaximum * radius;
    const POINT tip{
        centerX + static_cast<LONG>(std::cos(radians) * length),
        centerY - static_cast<LONG>(std::sin(radians) * length),
    };
    const POINT center{centerX, centerY};
    DrawArrow(dc, center, tip, kPhaseColors[index]);

    HBRUSH brush = CreateSolidBrush(kPhaseColors[index]);
    HGDIOBJ oldBrush = SelectObject(dc, brush);
    const int oldMode = SetROP2(dc, R2_COPYPEN);
    Ellipse(dc, tip.x - 3, tip.y - 3, tip.x + 4, tip.y + 4);
    SetROP2(dc, oldMode);
    SelectObject(dc, oldBrush);
    DeleteObject(brush);

    std::wostringstream label;
    label << kPhaseNames[index] << L"  " << std::setprecision(4) << std::defaultfloat
          << phasor.magnitude << L" @ " << displayedAngle << L" deg";
    const int labelX = tip.x >= centerX ? tip.x + 10 : tip.x - 125;
    const int labelY = tip.y >= centerY ? tip.y + 4 : tip.y - 18;
    DrawText(dc, label.str(), labelX, labelY, kPhaseColors[index]);
  }
}

bool GetWaveformArea(HWND window, int& left, int& right, int& top, int& bottom)
{
  RECT bounds{};
  GetClientRect(window, &bounds);
  left = 80;
  right = bounds.right - 24;
  top = 54;
  bottom = bounds.bottom - 58;
  return right > left && bottom > top;
}

int AngleToX(double angle, int left, int right, int angleSpan)
{
  return left + static_cast<int>(std::lround(angle * (right - left) / angleSpan));
}

double XToAngle(int positionX, int left, int right, int angleSpan)
{
  const double angle = static_cast<double>(angleSpan) * (positionX - left) / (right - left);
  return std::round(angle * 10.0) / 10.0;
}

void PaintWaveforms(HWND window, HDC dc, const AppState& state)
{
  int left = 0;
  int right = 0;
  int top = 0;
  int bottom = 0;
  if (!GetWaveformArea(window, left, right, top, bottom))
  {
    return;
  }

  const int centerY = (top + bottom) / 2;
  const double radius = (bottom - top) / 2.0;
  const int angleSpan = state.waveformCycles * 360;
  const double maximum = state.inputsValid
                             ? std::max({state.phasors[0].magnitude, state.phasors[1].magnitude,
                                         state.phasors[2].magnitude})
                             : 0.0;
  const double scaleMaximum = NiceScaleMaximum(maximum);
  const COLORREF labelColor = RGB(89, 102, 108);
  DrawText(dc, L"Amplitude", 6, 12, labelColor);
  for (int index = 0; index < kPhaseCount; ++index)
  {
    DrawText(dc, std::wstring(L"Phase ") + kPhaseNames[index],
             left + (right - left) * index / kPhaseCount, 12, kPhaseColors[index]);
  }

  const int saved = SaveDC(dc);
  HPEN gridPen = CreatePen(PS_SOLID, 1, RGB(222, 228, 230));
  HPEN axisPen = CreatePen(PS_SOLID, 1, RGB(139, 151, 156));
  HGDIOBJ oldPen = SelectObject(dc, gridPen);
  for (int tick = -2; tick <= 2; ++tick)
  {
    const int positionY = centerY - static_cast<int>(radius * tick / 2.0);
    MoveToEx(dc, left, positionY, nullptr);
    LineTo(dc, right, positionY);
    std::wostringstream label;
    label << std::setprecision(4) << std::defaultfloat << scaleMaximum * tick / 2.0;
    DrawText(dc, label.str(), 6, positionY - 8, labelColor);
  }
  for (int tick = 0; tick <= 6; ++tick)
  {
    const int positionX = left + (right - left) * tick / 6;
    MoveToEx(dc, positionX, top, nullptr);
    LineTo(dc, positionX, bottom);
    const std::wstring label = std::to_wstring(tick * angleSpan / 6);
    SIZE textSize{};
    GetTextExtentPoint32W(dc, label.c_str(), static_cast<int>(label.size()), &textSize);
    DrawText(dc, label, positionX - textSize.cx / 2, bottom + 8, labelColor);
  }
  SelectObject(dc, axisPen);
  MoveToEx(dc, left, centerY, nullptr);
  LineTo(dc, right, centerY);
  MoveToEx(dc, left, top, nullptr);
  LineTo(dc, left, bottom);
  const std::wstring axisLabel = L"Electrical angle (deg)";
  SIZE axisLabelSize{};
  GetTextExtentPoint32W(dc, axisLabel.c_str(), static_cast<int>(axisLabel.size()), &axisLabelSize);
  DrawText(dc, axisLabel, left + (right - left - axisLabelSize.cx) / 2, bottom + 32, labelColor);
  SelectObject(dc, oldPen);
  DeleteObject(gridPen);
  DeleteObject(axisPen);

  if (state.inputsValid)
  {
    IntersectClipRect(dc, left, top, right + 1, bottom + 1);
    for (int index = 0; index < kPhaseCount; ++index)
    {
      const Phasor& phasor = state.phasors[index];
      const double phaseAngle = std::remainder(phasor.angleDegrees + state.rotationDegrees, 360.0);
      HPEN phasePen = CreatePen(PS_SOLID, 2, kPhaseColors[index]);
      SelectObject(dc, phasePen);
      for (int positionX = left; positionX <= right; ++positionX)
      {
        const double cycleAngle =
            static_cast<double>(angleSpan) * (positionX - left) / (right - left);
        const double radians = (cycleAngle + phaseAngle) * 3.14159265358979323846 / 180.0;
        const int positionY = centerY - static_cast<int>(phasor.magnitude / scaleMaximum * radius *
                                                         std::cos(radians));
        if (positionX == left)
        {
          MoveToEx(dc, positionX, positionY, nullptr);
        }
        else
        {
          LineTo(dc, positionX, positionY);
        }
      }
      SelectObject(dc, oldPen);
      DeleteObject(phasePen);
    }
  }
  RestoreDC(dc, saved);

  int previousLabelRight = -1;
  for (int index = 0; index < kCursorCount; ++index)
  {
    const int positionX = AngleToX(state.cursorAngles[index], left, right, angleSpan);
    const bool active = index == state.activeCursor;
    HPEN cursorPen = CreatePen(active ? PS_SOLID : PS_DASH, active ? 2 : 1, kCursorColors[index]);
    HGDIOBJ previousPen = SelectObject(dc, cursorPen);
    MoveToEx(dc, positionX, top, nullptr);
    LineTo(dc, positionX, bottom + 1);
    SelectObject(dc, previousPen);
    DeleteObject(cursorPen);

    std::wostringstream label;
    label << L"L" << index + 1 << L" " << std::fixed << std::setprecision(1)
          << state.cursorAngles[index];
    const std::wstring text = label.str();
    SIZE textSize{};
    GetTextExtentPoint32W(dc, text.c_str(), static_cast<int>(text.size()), &textSize);
    const int labelWidth = static_cast<int>(textSize.cx);
    const int labelX = std::clamp(positionX - labelWidth / 2, 0, std::max(0, right - labelWidth));
    // Stagger the second label upward when the two labels would overlap.
    const bool overlaps = previousLabelRight >= 0 && labelX < previousLabelRight + 4;
    DrawText(dc, text, labelX, top - 20 - (overlaps ? 16 : 0), kCursorColors[index]);
    previousLabelRight = labelX + labelWidth;
  }
}

void PaintPlot(HWND window, HDC dc, const AppState& state)
{
  RECT bounds{};
  GetClientRect(window, &bounds);
  FillRect(dc, &bounds, static_cast<HBRUSH>(GetStockObject(WHITE_BRUSH)));
  SetBkMode(dc, TRANSPARENT);
  if (state.showWaveforms)
  {
    PaintWaveforms(window, dc, state);
  }
  else
  {
    PaintPhasors(window, dc, state);
  }
}

LRESULT CALLBACK PlotWindowProc(HWND window, UINT message, WPARAM wParam, LPARAM lParam)
{
  switch (message)
  {
    case WM_ERASEBKGND:
      return 1;
    case WM_SIZE:
      InvalidateRect(window, nullptr, TRUE);
      return 0;
    case WM_PAINT: {
      PAINTSTRUCT paint{};
      HDC dc = BeginPaint(window, &paint);
      auto* state =
          reinterpret_cast<AppState*>(GetWindowLongPtrW(GetParent(window), GWLP_USERDATA));
      if (state != nullptr)
      {
        // Double-buffer to avoid flicker while dragging cursors.
        RECT bounds{};
        GetClientRect(window, &bounds);
        HDC memoryDc = CreateCompatibleDC(dc);
        HBITMAP bitmap =
            CreateCompatibleBitmap(dc, std::max(1L, bounds.right), std::max(1L, bounds.bottom));
        HGDIOBJ oldBitmap = SelectObject(memoryDc, bitmap);
        PaintPlot(window, memoryDc, *state);
        BitBlt(dc, 0, 0, bounds.right, bounds.bottom, memoryDc, 0, 0, SRCCOPY);
        SelectObject(memoryDc, oldBitmap);
        DeleteObject(bitmap);
        DeleteDC(memoryDc);
      }
      EndPaint(window, &paint);
      return 0;
    }
    case WM_LBUTTONDOWN: {
      auto* state =
          reinterpret_cast<AppState*>(GetWindowLongPtrW(GetParent(window), GWLP_USERDATA));
      int left = 0;
      int right = 0;
      int top = 0;
      int bottom = 0;
      if (state == nullptr || !state->showWaveforms ||
          !GetWaveformArea(window, left, right, top, bottom))
      {
        break;
      }
      SetFocus(window);
      const int angleSpan = state->waveformCycles * 360;
      const int positionX = static_cast<short>(LOWORD(lParam));
      int nearest = 0;
      for (int index = 1; index < kCursorCount; ++index)
      {
        if (std::abs(AngleToX(state->cursorAngles[index], left, right, angleSpan) - positionX) <
            std::abs(AngleToX(state->cursorAngles[nearest], left, right, angleSpan) - positionX))
        {
          nearest = index;
        }
      }
      state->activeCursor = nearest;
      state->draggingCursor = nearest;
      SetCapture(window);
      SetCursorAngle(*state, nearest, XToAngle(positionX, left, right, angleSpan));
      return 0;
    }
    case WM_MOUSEMOVE: {
      auto* state =
          reinterpret_cast<AppState*>(GetWindowLongPtrW(GetParent(window), GWLP_USERDATA));
      int left = 0;
      int right = 0;
      int top = 0;
      int bottom = 0;
      if (state != nullptr && state->draggingCursor >= 0 &&
          GetWaveformArea(window, left, right, top, bottom))
      {
        SetCursorAngle(
            *state, state->draggingCursor,
            XToAngle(static_cast<short>(LOWORD(lParam)), left, right, state->waveformCycles * 360));
      }
      return 0;
    }
    case WM_LBUTTONUP: {
      auto* state =
          reinterpret_cast<AppState*>(GetWindowLongPtrW(GetParent(window), GWLP_USERDATA));
      if (state != nullptr && state->draggingCursor >= 0)
      {
        ReleaseCapture();
      }
      return 0;
    }
    case WM_CAPTURECHANGED: {
      auto* state =
          reinterpret_cast<AppState*>(GetWindowLongPtrW(GetParent(window), GWLP_USERDATA));
      if (state != nullptr)
      {
        state->draggingCursor = -1;
      }
      return 0;
    }
    case WM_SETCURSOR: {
      auto* state =
          reinterpret_cast<AppState*>(GetWindowLongPtrW(GetParent(window), GWLP_USERDATA));
      int left = 0;
      int right = 0;
      int top = 0;
      int bottom = 0;
      if (LOWORD(lParam) == HTCLIENT && state != nullptr && state->showWaveforms &&
          GetWaveformArea(window, left, right, top, bottom))
      {
        POINT point{};
        GetCursorPos(&point);
        ScreenToClient(window, &point);
        bool overCursor = state->draggingCursor >= 0;
        for (int index = 0; index < kCursorCount && !overCursor; ++index)
        {
          overCursor = point.y >= top && point.y <= bottom &&
                       std::abs(AngleToX(state->cursorAngles[index], left, right,
                                         state->waveformCycles * 360) -
                                point.x) <= kCursorHitPixels;
        }
        if (overCursor)
        {
          SetCursor(LoadCursorW(nullptr, IDC_SIZEWE));
          return TRUE;
        }
      }
      break;
    }
    case WM_KEYDOWN: {
      auto* state =
          reinterpret_cast<AppState*>(GetWindowLongPtrW(GetParent(window), GWLP_USERDATA));
      if (state == nullptr || !state->showWaveforms)
      {
        break;
      }
      const double step = (GetKeyState(VK_SHIFT) & 0x8000) != 0 ? 10.0 : 1.0;
      if (wParam == VK_LEFT || wParam == VK_RIGHT)
      {
        const int index = state->activeCursor;
        SetCursorAngle(*state, index,
                       state->cursorAngles[index] + (wParam == VK_LEFT ? -step : step));
        return 0;
      }
      if (wParam == VK_TAB)
      {
        state->activeCursor = (state->activeCursor + 1) % kCursorCount;
        UpdateCursorReadout(*state);
        InvalidateRect(window, nullptr, FALSE);
        return 0;
      }
      break;
    }
  }
  return DefWindowProcW(window, message, wParam, lParam);
}

void LayoutControls(HWND window, AppState& state)
{
  RECT client{};
  GetClientRect(window, &client);
  const int clientWidth = client.right;
  const int clientHeight = client.bottom;

  MoveWindow(GetDlgItem(window, 1), 24, 22, 500, 36, TRUE);
  MoveWindow(GetDlgItem(window, 2), 24, 60, 650, 22, TRUE);
  MoveWindow(GetDlgItem(window, 3), 24, 112, 300, 28, TRUE);
  MoveWindow(GetDlgItem(window, 4), 24, 156, 54, 24, TRUE);
  MoveWindow(GetDlgItem(window, 5), 91, 156, 108, 24, TRUE);
  MoveWindow(GetDlgItem(window, 6), 212, 156, 120, 24, TRUE);

  for (int index = 0; index < kPhaseCount; ++index)
  {
    const int y = 188 + index * 58;
    MoveWindow(GetDlgItem(window, kPhaseBaseId + index), 25, y, 52, 32, TRUE);
    MoveWindow(state.magnitudeInputs[index], 91, y, 108, 32, TRUE);
    MoveWindow(state.angleInputs[index], 212, y, 120, 32, TRUE);
  }

  MoveWindow(GetDlgItem(window, 8), 24, 372, 160, 22, TRUE);
  MoveWindow(state.rotationInput, 24, 398, 140, 32, TRUE);
  MoveWindow(GetDlgItem(window, kCyclesLabelId), 190, 372, 142, 22, TRUE);
  MoveWindow(state.cyclesInput, 190, 398, 142, 32, TRUE);
  MoveWindow(GetDlgItem(window, kBalanceButtonId), 24, 436, 150, 32, TRUE);
  MoveWindow(GetDlgItem(window, kWaveformCheckboxId), 190, 436, 142, 32, TRUE);
  MoveWindow(state.status, 24, 474, 310, 36, TRUE);
  MoveWindow(state.cursorReadout, 24, 516, 310, 60, TRUE);
  const int plotLeft = 360;
  const int plotTop = 100;
  MoveWindow(state.plot, plotLeft, plotTop, std::max(1, clientWidth - plotLeft - 24),
             std::max(1, clientHeight - plotTop - 24), TRUE);
}

LRESULT CALLBACK MainWindowProc(HWND window, UINT message, WPARAM wParam, LPARAM lParam)
{
  auto* state = reinterpret_cast<AppState*>(GetWindowLongPtrW(window, GWLP_USERDATA));
  switch (message)
  {
    case WM_NCCREATE: {
      const auto* create = reinterpret_cast<const CREATESTRUCTW*>(lParam);
      SetWindowLongPtrW(window, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(create->lpCreateParams));
      return DefWindowProcW(window, message, wParam, lParam);
    }
    case WM_CREATE: {
      state = reinterpret_cast<AppState*>(GetWindowLongPtrW(window, GWLP_USERDATA));
      HWND title = CreateLabel(window, L"THREE-PHASE PHASOR DIAGRAM", 1);
      SetControlFont(title, state->titleFont);
      CreateLabel(window, L"PHASOR VIEW", 2);
      CreateLabel(window, L"PHASE INPUTS", 3);
      CreateLabel(window, L"PHASE", 4);
      CreateLabel(window, L"MAGNITUDE", 5);
      CreateLabel(window, L"ANGLE (deg)", 6);

      const std::array<const wchar_t*, kPhaseCount> initialAngles = {L"0", L"-120", L"120"};
      for (int index = 0; index < kPhaseCount; ++index)
      {
        CreateLabel(window, kPhaseNames[index], kPhaseBaseId + index);
        state->magnitudeInputs[index] = CreateInput(window, kMagnitudeBaseId + index, L"100");
        state->angleInputs[index] = CreateInput(window, kAngleBaseId + index, initialAngles[index]);
      }
      CreateLabel(window, L"ROTATION (deg)", 8);
      state->rotationInput = CreateInput(window, kRotationInputId);
      HWND cyclesLabel = CreateLabel(window, L"WAVEFORM CYCLES", kCyclesLabelId);
      state->cyclesInput = CreateInput(window, kCyclesInputId, L"2");
      EnableWindow(cyclesLabel, FALSE);
      EnableWindow(state->cyclesInput, FALSE);
      HWND balanceButton = CreateButton(window, kBalanceButtonId, L"Balance Phases");
      HWND waveformCheckbox = CreateWindowExW(
          0, L"BUTTON", L"Waveforms", WS_CHILD | WS_VISIBLE | WS_TABSTOP | BS_AUTOCHECKBOX, 0, 0, 0,
          0, window, reinterpret_cast<HMENU>(static_cast<INT_PTR>(kWaveformCheckboxId)),
          GetModuleHandleW(nullptr), nullptr);
      state->status = CreateLabel(window, L"Enter phase values to display the vectors.", 7);
      state->cursorReadout = CreateLabel(window, L"", kCursorReadoutId);
      ShowWindow(state->cursorReadout, SW_HIDE);
      state->plot = CreateWindowExW(0, kPlotClass, L"", WS_CHILD | WS_VISIBLE, 0, 0, 0, 0, window,
                                    nullptr, GetModuleHandleW(nullptr), nullptr);

      for (int index = 0; index < kPhaseCount; ++index)
      {
        SetControlFont(state->magnitudeInputs[index],
                       static_cast<HFONT>(GetStockObject(DEFAULT_GUI_FONT)));
        SetControlFont(state->angleInputs[index],
                       static_cast<HFONT>(GetStockObject(DEFAULT_GUI_FONT)));
      }
      SetControlFont(state->rotationInput, static_cast<HFONT>(GetStockObject(DEFAULT_GUI_FONT)));
      SetControlFont(state->cyclesInput, static_cast<HFONT>(GetStockObject(DEFAULT_GUI_FONT)));
      SetControlFont(balanceButton, static_cast<HFONT>(GetStockObject(DEFAULT_GUI_FONT)));
      SetControlFont(waveformCheckbox, static_cast<HFONT>(GetStockObject(DEFAULT_GUI_FONT)));
      LayoutControls(window, *state);
      UpdateInputs(*state);
      return 0;
    }
    case WM_SIZE:
      if (state != nullptr && state->plot != nullptr)
      {
        LayoutControls(window, *state);
      }
      return 0;
    case WM_COMMAND:
      if (state != nullptr)
      {
        if (LOWORD(wParam) == kBalanceButtonId && HIWORD(wParam) == BN_CLICKED)
        {
          ApplyBalancedPhases(*state);
        }
        else if (LOWORD(wParam) == kWaveformCheckboxId && HIWORD(wParam) == BN_CLICKED)
        {
          state->showWaveforms =
              SendDlgItemMessageW(window, kWaveformCheckboxId, BM_GETCHECK, 0, 0) == BST_CHECKED;
          SetWindowTextW(GetDlgItem(window, 1), state->showWaveforms
                                                    ? L"THREE-PHASE WAVEFORMS"
                                                    : L"THREE-PHASE PHASOR DIAGRAM");
          SetWindowTextW(GetDlgItem(window, 2),
                         state->showWaveforms ? L"WAVEFORM VIEW" : L"PHASOR VIEW");
          EnableWindow(state->cyclesInput, state->showWaveforms);
          EnableWindow(GetDlgItem(window, kCyclesLabelId), state->showWaveforms);
          ShowWindow(state->cursorReadout, state->showWaveforms ? SW_SHOW : SW_HIDE);
          UpdateInputs(*state);
        }
        else if (HIWORD(wParam) == EN_CHANGE)
        {
          UpdateInputs(*state);
        }
      }
      return 0;
    case WM_CTLCOLORSTATIC: {
      HDC dc = reinterpret_cast<HDC>(wParam);
      const int controlId = GetDlgCtrlID(reinterpret_cast<HWND>(lParam));
      if (controlId >= kPhaseBaseId && controlId < kPhaseBaseId + kPhaseCount)
      {
        SetTextColor(dc, kPhaseColors[controlId - kPhaseBaseId]);
      }
      else if (controlId == 7 && state != nullptr && !state->inputsValid)
      {
        SetTextColor(dc, RGB(176, 48, 48));
      }
      else
      {
        SetTextColor(dc, RGB(75, 88, 95));
      }
      SetBkMode(dc, TRANSPARENT);
      return reinterpret_cast<LRESULT>(GetStockObject(WHITE_BRUSH));
    }
    case WM_GETMINMAXINFO: {
      auto* info = reinterpret_cast<MINMAXINFO*>(lParam);
      info->ptMinTrackSize.x = 820;
      info->ptMinTrackSize.y = 640;
      return 0;
    }
    case WM_DESTROY:
      if (state != nullptr && state->titleFont != nullptr)
      {
        DeleteObject(state->titleFont);
        state->titleFont = nullptr;
      }
      PostQuitMessage(0);
      return 0;
  }
  return DefWindowProcW(window, message, wParam, lParam);
}

bool RegisterWindowClasses(HINSTANCE instance)
{
  WNDCLASSEXW mainClass{};
  mainClass.cbSize = sizeof(mainClass);
  mainClass.lpfnWndProc = MainWindowProc;
  mainClass.hInstance = instance;
  mainClass.hCursor = LoadCursorW(nullptr, IDC_ARROW);
  mainClass.hbrBackground = static_cast<HBRUSH>(GetStockObject(WHITE_BRUSH));
  mainClass.lpszClassName = kMainClass;
  if (RegisterClassExW(&mainClass) == 0)
  {
    return false;
  }

  WNDCLASSEXW plotClass{};
  plotClass.cbSize = sizeof(plotClass);
  plotClass.lpfnWndProc = PlotWindowProc;
  plotClass.hInstance = instance;
  plotClass.hCursor = LoadCursorW(nullptr, IDC_ARROW);
  plotClass.hbrBackground = static_cast<HBRUSH>(GetStockObject(WHITE_BRUSH));
  plotClass.lpszClassName = kPlotClass;
  return RegisterClassExW(&plotClass) != 0;
}

}  // namespace

int WINAPI wWinMain(HINSTANCE instance, HINSTANCE, PWSTR, int showCommand)
{
  if (!RegisterWindowClasses(instance))
  {
    MessageBoxW(nullptr, L"Unable to register the application window classes.", L"Phasor Diagram",
                MB_ICONERROR | MB_OK);
    return 1;
  }

  AppState state{};
  state.titleFont = CreateUiFont(24, FW_BOLD);
  HWND window =
      CreateWindowExW(0, kMainClass, L"Three-Phase Phasor Diagram", WS_OVERLAPPEDWINDOW,
                      CW_USEDEFAULT, CW_USEDEFAULT, 1120, 740, nullptr, nullptr, instance, &state);
  if (window == nullptr)
  {
    if (state.titleFont != nullptr)
    {
      DeleteObject(state.titleFont);
    }
    MessageBoxW(nullptr, L"Unable to create the application window.", L"Phasor Diagram",
                MB_ICONERROR | MB_OK);
    return 1;
  }

  ShowWindow(window, showCommand);
  UpdateWindow(window);

  MSG message{};
  while (GetMessageW(&message, nullptr, 0, 0) > 0)
  {
    TranslateMessage(&message);
    DispatchMessageW(&message);
  }
  return static_cast<int>(message.wParam);
}