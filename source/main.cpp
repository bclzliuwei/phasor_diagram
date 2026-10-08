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

namespace {

constexpr wchar_t kMainClass[] = L"PhasorDiagramMainWindow";
constexpr wchar_t kPlotClass[] = L"PhasorDiagramPlotWindow";
constexpr int kPhaseCount = 3;
constexpr int kMagnitudeBaseId = 100;
constexpr int kAngleBaseId = 110;
constexpr int kPhaseBaseId = 120;
constexpr int kRotationInputId = 130;
constexpr int kBalanceButtonId = 9;

const std::array<const wchar_t*, kPhaseCount> kPhaseNames = {L"A", L"B", L"C"};
const std::array<COLORREF, kPhaseCount> kPhaseColors = {
    RGB(255, 0, 0),
    RGB(230, 110, 35),
    RGB(0, 0, 255),
};

struct Phasor {
    double magnitude = 0.0;
    double angleDegrees = 0.0;
};

struct AppState {
    std::array<HWND, kPhaseCount> magnitudeInputs{};
    std::array<HWND, kPhaseCount> angleInputs{};
    HWND rotationInput = nullptr;
    HWND status = nullptr;
    HWND plot = nullptr;
    HFONT titleFont = nullptr;
    std::array<Phasor, kPhaseCount> phasors{};
    double rotationDegrees = 0.0;
    bool inputsValid = true;
};

HFONT CreateUiFont(int height, int weight)
{
    return CreateFontW(
        height, 0, 0, 0, weight, FALSE, FALSE, FALSE, DEFAULT_CHARSET,
        OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY,
        DEFAULT_PITCH | FF_DONTCARE, L"Segoe UI");
}

HWND CreateLabel(HWND parent, const wchar_t* text, int controlId, DWORD style = SS_LEFT)
{
    return CreateWindowExW(
        0, L"STATIC", text, WS_CHILD | WS_VISIBLE | style,
        0, 0, 0, 0, parent, reinterpret_cast<HMENU>(static_cast<INT_PTR>(controlId)),
        GetModuleHandleW(nullptr), nullptr);
}

HWND CreateInput(HWND parent, int controlId, const wchar_t* text = L"0")
{
    return CreateWindowExW(
    WS_EX_CLIENTEDGE, L"EDIT", text, WS_CHILD | WS_VISIBLE | WS_TABSTOP | ES_AUTOHSCROLL | ES_RIGHT,
        0, 0, 0, 0, parent, reinterpret_cast<HMENU>(static_cast<INT_PTR>(controlId)),
        GetModuleHandleW(nullptr), nullptr);
}

    HWND CreateButton(HWND parent, int controlId, const wchar_t* text)
    {
        return CreateWindowExW(
        0, L"BUTTON", text, WS_CHILD | WS_VISIBLE | WS_TABSTOP | BS_PUSHBUTTON,
        0, 0, 0, 0, parent, reinterpret_cast<HMENU>(static_cast<INT_PTR>(controlId)),
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
    if (text == end || !std::isfinite(parsed)) {
        return false;
    }

    while (*end != L'\0' && std::iswspace(*end)) {
        ++end;
    }
    if (*end != L'\0') {
        return false;
    }

    value = parsed;
    return true;
}

void UpdateInputs(AppState& state)
{
    std::array<Phasor, kPhaseCount> updated{};
    for (int index = 0; index < kPhaseCount; ++index) {
        if (!ReadNumber(state.magnitudeInputs[index], updated[index].magnitude) ||
            !ReadNumber(state.angleInputs[index], updated[index].angleDegrees)) {
            state.inputsValid = false;
            SetWindowTextW(state.status, L"Enter a valid number in every input field.");
            InvalidateRect(state.plot, nullptr, TRUE);
            return;
        }
        if (updated[index].magnitude < 0.0 || updated[index].magnitude > 1.0e12) {
            state.inputsValid = false;
            SetWindowTextW(state.status, L"Magnitudes must be between 0 and 1,000,000,000,000.");
            InvalidateRect(state.plot, nullptr, TRUE);
            return;
        }
        if (std::abs(updated[index].angleDegrees) > 360000.0) {
            state.inputsValid = false;
            SetWindowTextW(state.status, L"Angles must be between -360,000 and 360,000 degrees.");
            InvalidateRect(state.plot, nullptr, TRUE);
            return;
        }
    }

    double rotationDegrees = 0.0;
    if (!ReadNumber(state.rotationInput, rotationDegrees)) {
        state.inputsValid = false;
        SetWindowTextW(state.status, L"Enter a valid diagram rotation.");
        InvalidateRect(state.plot, nullptr, TRUE);
        return;
    }
    if (std::abs(rotationDegrees) > 360000.0) {
        state.inputsValid = false;
        SetWindowTextW(state.status, L"Diagram rotation must be between -360,000 and 360,000 degrees.");
        InvalidateRect(state.plot, nullptr, TRUE);
        return;
    }

    state.phasors = updated;
    state.rotationDegrees = rotationDegrees;
    state.inputsValid = true;
    const bool allZero = std::all_of(updated.begin(), updated.end(), [](const Phasor& phasor) {
        return phasor.magnitude == 0.0;
    });
    SetWindowTextW(state.status, allZero
        ? L"Enter a non-zero magnitude to display the vectors."
        : L"Plot updates as the phase values change.");
    InvalidateRect(state.plot, nullptr, TRUE);
}

void ApplyBalancedPhases(AppState& state)
{
    UpdateInputs(state);
    if (!state.inputsValid) {
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

    for (int index = 1; index < kPhaseCount; ++index) {
        setNumber(state.magnitudeInputs[index], referenceMagnitude);
        setNumber(state.angleInputs[index], balancedAngles[index - 1]);
    }

    UpdateInputs(state);
    if (state.inputsValid && referenceMagnitude > 0.0) {
        SetWindowTextW(state.status, L"Balanced positive ABC sequence applied from phase A.");
    }
}

double NiceScaleMaximum(double maximum)
{
    if (maximum <= 0.0) {
        return 1.0;
    }

    const double magnitude = std::pow(10.0, std::floor(std::log10(maximum)));
    const double normalized = maximum / magnitude;
    const double nice = normalized <= 1.0 ? 1.0 : normalized <= 2.0 ? 2.0 : normalized <= 5.0 ? 5.0 : 10.0;
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

    const double direction = std::atan2(
        static_cast<double>(tip.y - center.y),
        static_cast<double>(tip.x - center.x));
    constexpr double arrowLength = 11.0;
    constexpr double spread = 0.48;
    for (const double offset : {-spread, spread}) {
        const int baseX = static_cast<int>(tip.x - arrowLength * std::cos(direction + offset));
        const int baseY = static_cast<int>(tip.y - arrowLength * std::sin(direction + offset));
        MoveToEx(dc, tip.x, tip.y, nullptr);
        LineTo(dc, baseX, baseY);
    }

    SelectObject(dc, oldPen);
    DeleteObject(pen);
    RestoreDC(dc, saved);
}

void PaintPlot(HWND window, HDC dc, const AppState& state)
{
    RECT bounds{};
    GetClientRect(window, &bounds);
    FillRect(dc, &bounds, static_cast<HBRUSH>(GetStockObject(WHITE_BRUSH)));
    SetBkMode(dc, TRANSPARENT);

    const int width = bounds.right - bounds.left;
    const int height = bounds.bottom - bounds.top;
    const int centerX = width / 2;
    const int centerY = height / 2;
    const int radius = std::max(1, std::min(width - 100, height - 90) / 2);
    const double maximum = state.inputsValid
        ? std::max({state.phasors[0].magnitude, state.phasors[1].magnitude, state.phasors[2].magnitude})
        : 0.0;
    const double scaleMaximum = NiceScaleMaximum(maximum);

    const int saved = SaveDC(dc);
    HPEN gridPen = CreatePen(PS_SOLID, 1, RGB(222, 228, 230));
    HPEN axisPen = CreatePen(PS_SOLID, 1, RGB(139, 151, 156));
    HGDIOBJ oldPen = SelectObject(dc, gridPen);

    for (int ring = 1; ring <= 4; ++ring) {
        const int ringRadius = radius * ring / 4;
        Ellipse(dc, centerX - ringRadius, centerY - ringRadius, centerX + ringRadius, centerY + ringRadius);

        std::wostringstream tick;
        tick << std::setprecision(4) << std::defaultfloat << scaleMaximum * ring / 4.0;
        DrawText(dc, tick.str(), std::max(2, centerX - 35), centerY - ringRadius - 18, RGB(89, 102, 108));
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

    if (!state.inputsValid) {
        return;
    }

    for (int index = 0; index < kPhaseCount; ++index) {
        const Phasor& phasor = state.phasors[index];
        if (phasor.magnitude <= 0.0) {
            continue;
        }

        const double displayedAngle = std::remainder(phasor.angleDegrees + state.rotationDegrees, 360.0);
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

LRESULT CALLBACK PlotWindowProc(HWND window, UINT message, WPARAM wParam, LPARAM lParam)
{
    switch (message) {
    case WM_ERASEBKGND:
        return 1;
    case WM_SIZE:
        InvalidateRect(window, nullptr, TRUE);
        return 0;
    case WM_PAINT: {
        PAINTSTRUCT paint{};
        HDC dc = BeginPaint(window, &paint);
        auto* state = reinterpret_cast<AppState*>(GetWindowLongPtrW(GetParent(window), GWLP_USERDATA));
        if (state != nullptr) {
            PaintPlot(window, dc, *state);
        }
        EndPaint(window, &paint);
        return 0;
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

    for (int index = 0; index < kPhaseCount; ++index) {
        const int y = 188 + index * 58;
        MoveWindow(GetDlgItem(window, kPhaseBaseId + index), 25, y, 52, 32, TRUE);
        MoveWindow(state.magnitudeInputs[index], 91, y, 108, 32, TRUE);
        MoveWindow(state.angleInputs[index], 212, y, 120, 32, TRUE);
    }

    MoveWindow(GetDlgItem(window, 8), 24, 372, 260, 22, TRUE);
    MoveWindow(state.rotationInput, 24, 398, 140, 32, TRUE);
    MoveWindow(GetDlgItem(window, kBalanceButtonId), 24, 436, 150, 32, TRUE);
    MoveWindow(state.status, 24, 474, 310, 36, TRUE);
    const int plotLeft = 360;
    const int plotTop = 100;
    MoveWindow(state.plot, plotLeft, plotTop, std::max(1, clientWidth - plotLeft - 24),
        std::max(1, clientHeight - plotTop - 24), TRUE);
}

LRESULT CALLBACK MainWindowProc(HWND window, UINT message, WPARAM wParam, LPARAM lParam)
{
    auto* state = reinterpret_cast<AppState*>(GetWindowLongPtrW(window, GWLP_USERDATA));
    switch (message) {
    case WM_NCCREATE: {
        const auto* create = reinterpret_cast<const CREATESTRUCTW*>(lParam);
        SetWindowLongPtrW(window, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(create->lpCreateParams));
        return DefWindowProcW(window, message, wParam, lParam);
    }
    case WM_CREATE: {
        state = reinterpret_cast<AppState*>(GetWindowLongPtrW(window, GWLP_USERDATA));
        HWND title = CreateLabel(window, L"THREE-PHASE PHASOR DIAGRAM", 1);
        SetControlFont(title, state->titleFont);
        CreateLabel(window, L"Vector angle = phase angle + rotation (degrees, positive counterclockwise).", 2);
        CreateLabel(window, L"PHASE INPUTS", 3);
        CreateLabel(window, L"PHASE", 4);
        CreateLabel(window, L"MAGNITUDE", 5);
        CreateLabel(window, L"ANGLE (deg)", 6);

        const std::array<const wchar_t*, kPhaseCount> initialAngles = {L"0", L"-120", L"120"};
        for (int index = 0; index < kPhaseCount; ++index) {
            CreateLabel(window, kPhaseNames[index], kPhaseBaseId + index);
            state->magnitudeInputs[index] = CreateInput(window, kMagnitudeBaseId + index, L"100");
            state->angleInputs[index] = CreateInput(window, kAngleBaseId + index, initialAngles[index]);
        }
        CreateLabel(window, L"DIAGRAM ROTATION (deg)", 8);
        state->rotationInput = CreateInput(window, kRotationInputId);
        HWND balanceButton = CreateButton(window, kBalanceButtonId, L"Balance Phases");
        state->status = CreateLabel(window, L"Enter phase values to display the vectors.", 7);
        state->plot = CreateWindowExW(
            0, kPlotClass, L"", WS_CHILD | WS_VISIBLE,
            0, 0, 0, 0, window, nullptr, GetModuleHandleW(nullptr), nullptr);

        for (int index = 0; index < kPhaseCount; ++index) {
            SetControlFont(state->magnitudeInputs[index], static_cast<HFONT>(GetStockObject(DEFAULT_GUI_FONT)));
            SetControlFont(state->angleInputs[index], static_cast<HFONT>(GetStockObject(DEFAULT_GUI_FONT)));
        }
        SetControlFont(state->rotationInput, static_cast<HFONT>(GetStockObject(DEFAULT_GUI_FONT)));
        SetControlFont(balanceButton, static_cast<HFONT>(GetStockObject(DEFAULT_GUI_FONT)));
        LayoutControls(window, *state);
        UpdateInputs(*state);
        return 0;
    }
    case WM_SIZE:
        if (state != nullptr && state->plot != nullptr) {
            LayoutControls(window, *state);
        }
        return 0;
    case WM_COMMAND:
        if (state != nullptr) {
            if (LOWORD(wParam) == kBalanceButtonId && HIWORD(wParam) == BN_CLICKED) {
                ApplyBalancedPhases(*state);
            } else if (HIWORD(wParam) == EN_CHANGE) {
                UpdateInputs(*state);
            }
        }
        return 0;
    case WM_CTLCOLORSTATIC: {
        HDC dc = reinterpret_cast<HDC>(wParam);
        const int controlId = GetDlgCtrlID(reinterpret_cast<HWND>(lParam));
        if (controlId >= kPhaseBaseId && controlId < kPhaseBaseId + kPhaseCount) {
            SetTextColor(dc, kPhaseColors[controlId - kPhaseBaseId]);
        } else if (controlId == 7 && state != nullptr && !state->inputsValid) {
            SetTextColor(dc, RGB(176, 48, 48));
        } else {
            SetTextColor(dc, RGB(75, 88, 95));
        }
        SetBkMode(dc, TRANSPARENT);
        return reinterpret_cast<LRESULT>(GetStockObject(WHITE_BRUSH));
    }
    case WM_GETMINMAXINFO: {
        auto* info = reinterpret_cast<MINMAXINFO*>(lParam);
        info->ptMinTrackSize.x = 820;
        info->ptMinTrackSize.y = 560;
        return 0;
    }
    case WM_DESTROY:
        if (state != nullptr && state->titleFont != nullptr) {
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
    if (RegisterClassExW(&mainClass) == 0) {
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

} // namespace

int WINAPI wWinMain(HINSTANCE instance, HINSTANCE, PWSTR, int showCommand)
{
    if (!RegisterWindowClasses(instance)) {
        MessageBoxW(nullptr, L"Unable to register the application window classes.", L"Phasor Diagram", MB_ICONERROR | MB_OK);
        return 1;
    }

    AppState state{};
    state.titleFont = CreateUiFont(24, FW_BOLD);
    HWND window = CreateWindowExW(
        0, kMainClass, L"Three-Phase Phasor Diagram", WS_OVERLAPPEDWINDOW,
        CW_USEDEFAULT, CW_USEDEFAULT, 1120, 740, nullptr, nullptr, instance, &state);
    if (window == nullptr) {
        if (state.titleFont != nullptr) {
            DeleteObject(state.titleFont);
        }
        MessageBoxW(nullptr, L"Unable to create the application window.", L"Phasor Diagram", MB_ICONERROR | MB_OK);
        return 1;
    }

    ShowWindow(window, showCommand);
    UpdateWindow(window);

    MSG message{};
    while (GetMessageW(&message, nullptr, 0, 0) > 0) {
        TranslateMessage(&message);
        DispatchMessageW(&message);
    }
    return static_cast<int>(message.wParam);
}