#define _USE_MATH_DEFINES
#define _CRT_SECURE_NO_WARNINGS
#include <windows.h>
#include <commdlg.h>
#include <gdiplus.h>
#include <vector>
#include <string>
#include <cmath>
#include <algorithm>
#include <sstream>

// Подключение библиотек
#pragma comment(lib, "gdiplus.lib")
#pragma comment(lib, "user32.lib")
#pragma comment(lib, "gdi32.lib")
#pragma comment(lib, "advapi32.lib")
#pragma comment(lib, "comdlg32.lib")

using namespace Gdiplus;
using namespace std;

// -------------------------------------------------------------------------
// 1. Константы и ID
// -------------------------------------------------------------------------
#define ID_TOOL_PEN       1001
#define ID_TOOL_LINE      1002
#define ID_TOOL_RECT      1003
#define ID_TOOL_ELLIPSE   1004
#define ID_TOOL_TRIANGLE  1005 
#define ID_TOOL_STAR      1006 
#define ID_TOOL_FUNC      1007
#define ID_TOOL_ERASER    1008
#define ID_TOOL_IMAGE     1009 

#define ID_ACTION_CLEAR   1101
#define ID_ACTION_SAVE    1102
#define ID_ACTION_OPEN    1103 
#define ID_ACTION_COLOR   1104
#define ID_ACTION_AUTORUN 1105

// Размеры ластика
#define ID_ERASER_XS      1201
#define ID_ERASER_S       1202
#define ID_ERASER_M       1203
#define ID_ERASER_L       1204
#define ID_ERASER_XL      1205

#define ID_BTN_OK         2001
#define ID_BTN_CANCEL     2002
#define ID_CHK_AXIS       2003
#define ID_CHK_CLIP       2004

const wchar_t* MUTEX_NAME = L"Global\\MyGDIPlusPaintMutex_MegaV6";
const wchar_t* REG_PATH = L"Software\\Microsoft\\Windows\\CurrentVersion\\Run";
const wchar_t* APP_NAME = L"MyGDIPlusPaint";

// -------------------------------------------------------------------------
// 2. Математический парсер
// -------------------------------------------------------------------------
class MathParser {
public:
    static double Evaluate(string expr, double x) {
        expr.erase(remove(expr.begin(), expr.end(), ' '), expr.end());
        size_t pos = 0;
        return ParseExpression(expr, pos, x);
    }
private:
    static double ParseExpression(const string& expr, size_t& pos, double x) {
        double left = ParseTerm(expr, pos, x);
        while (pos < expr.length()) {
            char op = expr[pos];
            if (op != '+' && op != '-') break;
            pos++;
            double right = ParseTerm(expr, pos, x);
            if (op == '+') left += right; else left -= right;
        }
        return left;
    }
    static double ParseTerm(const string& expr, size_t& pos, double x) {
        double left = ParsePower(expr, pos, x);
        while (pos < expr.length()) {
            char op = expr[pos];
            if (op != '*' && op != '/') break;
            pos++;
            double right = ParsePower(expr, pos, x);
            if (op == '*') left *= right; else if (right != 0) left /= right;
        }
        return left;
    }
    static double ParsePower(const string& expr, size_t& pos, double x) {
        double left = ParseFactor(expr, pos, x);
        while (pos < expr.length()) {
            if (expr[pos] == '^') {
                pos++;
                double right = ParseFactor(expr, pos, x);
                left = pow(left, right);
            }
            else {
                break;
            }
        }
        return left;
    }
    static double ParseFactor(const string& expr, size_t& pos, double x) {
        if (pos >= expr.length()) return 0;
        if (expr[pos] == '-') { pos++; return -ParseFactor(expr, pos, x); }
        if (expr[pos] == '(') {
            pos++;
            double val = ParseExpression(expr, pos, x);
            if (pos < expr.length() && expr[pos] == ')') pos++;
            return val;
        }
        if (isalpha(expr[pos])) {
            string func;
            while (pos < expr.length() && isalpha(expr[pos])) func += expr[pos++];
            if (func == "x") return x;
            if (func == "pi") return M_PI;
            if (func == "e") return M_E;
            if (pos < expr.length() && expr[pos] == '(') {
                pos++;
                double val = ParseExpression(expr, pos, x);
                if (pos < expr.length() && expr[pos] == ')') pos++;
                if (func == "sin") return sin(val);
                if (func == "cos") return cos(val);
                if (func == "tan") return tan(val);
                if (func == "sqrt") return sqrt(abs(val));
                if (func == "abs") return abs(val);
                if (func == "log") return log(val);
                if (func == "ln") return log(val);
            }
        }
        if (isdigit(expr[pos]) || expr[pos] == '.') {
            string numStr;
            while (pos < expr.length() && (isdigit(expr[pos]) || expr[pos] == '.')) numStr += expr[pos++];
            return stod(numStr);
        }
        return 0;
    }
};

// -------------------------------------------------------------------------
// 3. Классы фигур
// -------------------------------------------------------------------------
class Shape {
public:
    Color color;
    float width;
    Shape(Color c, float w) : color(c), width(w) {}
    virtual ~Shape() {}
    virtual void Draw(Graphics& g) = 0;
};

class PenShape : public Shape {
public:
    std::vector<PointF> points;
    PenShape(Color c, float w) : Shape(c, w) {}
    void AddPoint(PointF p) { points.push_back(p); }
    void Draw(Graphics& g) override {
        if (points.size() < 2) return;
        Pen pen(color, width);
        pen.SetStartCap(LineCapRound);
        pen.SetEndCap(LineCapRound);
        pen.SetLineJoin(LineJoinRound);
        g.DrawCurve(&pen, points.data(), (INT)points.size());
    }
};

class LineShape : public Shape {
public:
    PointF start, end;
    LineShape(PointF s, PointF e, Color c, float w) : Shape(c, w), start(s), end(e) {}
    void Draw(Graphics& g) override {
        Pen pen(color, width);
        g.DrawLine(&pen, start, end);
    }
};

class RectShape : public Shape {
public:
    RectF rect;
    RectShape(RectF r, Color c, float w) : Shape(c, w), rect(r) {}
    void Draw(Graphics& g) override {
        Pen pen(color, width);
        g.DrawRectangle(&pen, rect);
    }
};

class EllipseShape : public Shape {
public:
    RectF rect;
    EllipseShape(RectF r, Color c, float w) : Shape(c, w), rect(r) {}
    void Draw(Graphics& g) override {
        Pen pen(color, width);
        g.DrawEllipse(&pen, rect);
    }
};

class TriangleShape : public Shape {
public:
    RectF rect;
    TriangleShape(RectF r, Color c, float w) : Shape(c, w), rect(r) {}
    void Draw(Graphics& g) override {
        Pen pen(color, width);
        PointF p1(rect.X + rect.Width / 2, rect.Y);
        PointF p2(rect.X, rect.Y + rect.Height);
        PointF p3(rect.X + rect.Width, rect.Y + rect.Height);

        PointF points[] = { p1, p2, p3 };
        g.DrawPolygon(&pen, points, 3);
    }
};

class StarShape : public Shape {
public:
    RectF rect;
    StarShape(RectF r, Color c, float w) : Shape(c, w), rect(r) {}
    void Draw(Graphics& g) override {
        Pen pen(color, width);
        float cx = rect.X + rect.Width / 2;
        float cy = rect.Y + rect.Height / 2;
        float R = min(rect.Width, rect.Height) / 2;
        float r = R * 0.4f;

        PointF pnts[10];
        double angle = -M_PI / 2;
        double step = M_PI / 5;

        for (int i = 0; i < 10; i++) {
            float currR = (i % 2 == 0) ? R : r;
            pnts[i] = PointF(cx + (float)(cos(angle) * currR), cy + (float)(sin(angle) * currR));
            angle += step;
        }
        g.DrawPolygon(&pen, pnts, 10);
    }
};

class ImageShape : public Shape {
public:
    RectF rect;
    Bitmap* image;

    ImageShape(const WCHAR* filename, RectF r) : Shape(Color(0, 0, 0), 0), rect(r) {
        image = Bitmap::FromFile(filename);
    }

    ~ImageShape() {
        if (image) delete image;
    }

    void Draw(Graphics& g) override {
        if (image && image->GetLastStatus() == Ok) {
            g.DrawImage(image, rect);
        }
    }
};

class FunctionShape : public Shape {
public:
    string expression;
    double rangeStart, rangeEnd;
    PointF origin;
    bool drawAxes;
    bool clipToRange;

    FunctionShape(string expr, double start, double end, PointF org, Color c, float w, bool axes, bool clip)
        : Shape(c, w), expression(expr), rangeStart(start), rangeEnd(end), origin(org), drawAxes(axes), clipToRange(clip) {
    }

    void Draw(Graphics& g) override {
        Pen pen(color, width);

        if (drawAxes) {
            Pen axisPen(Color(200, 0, 0, 0), 1);
            Font font(L"Arial", 8);
            SolidBrush brush(Color(200, 0, 0, 0));

            g.DrawLine(&axisPen, PointF(origin.X - 100000, origin.Y), PointF(origin.X + 100000, origin.Y));
            g.DrawLine(&axisPen, PointF(origin.X, origin.Y - 100000), PointF(origin.X, origin.Y + 100000));

            double step = 50.0;
            for (double x = -3000; x <= 3000; x += step) {
                if (x == 0) continue;
                float screenX = origin.X + (float)x;
                g.DrawLine(&axisPen, PointF(screenX, origin.Y - 3), PointF(screenX, origin.Y + 3));
                wstring s = to_wstring((int)x);
                g.DrawString(s.c_str(), -1, &font, PointF(screenX - 10, origin.Y + 5), &brush);
            }
            for (double y = -3000; y <= 3000; y += step) {
                if (y == 0) continue;
                float screenY = origin.Y - (float)y;
                g.DrawLine(&axisPen, PointF(origin.X - 3, screenY), PointF(origin.X + 3, screenY));
                wstring s = to_wstring((int)y);
                g.DrawString(s.c_str(), -1, &font, PointF(origin.X + 5, screenY - 6), &brush);
            }
        }

        double start, end;
        if (clipToRange) {
            start = min(rangeStart, rangeEnd);
            end = max(rangeStart, rangeEnd);
            RectF clipRect((float)(origin.X + start), (float)(origin.Y - 100000), (float)(end - start), 200000.0f);
            g.SetClip(clipRect, CombineModeIntersect);
        }
        else {
            start = -50000.0; end = 50000.0;
        }

        double step = 0.05;

        std::vector<std::vector<PointF>> segments;
        std::vector<PointF> currentSegment;

        for (double xPix = start; xPix <= end; xPix += step) {
            double yVal = MathParser::Evaluate(expression, xPix);

            if (isnan(yVal) || isinf(yVal)) {
                if (!currentSegment.empty()) { segments.push_back(currentSegment); currentSegment.clear(); }
                continue;
            }

            float screenX = origin.X + (float)xPix;
            float screenY = origin.Y - (float)yVal;

            if (!currentSegment.empty()) {
                float lastY = currentSegment.back().Y;
                if (abs(screenY - lastY) > 2000.0f) {
                    segments.push_back(currentSegment);
                    currentSegment.clear();
                }
            }
            currentSegment.push_back(PointF(screenX, screenY));
        }
        if (!currentSegment.empty()) segments.push_back(currentSegment);

        for (const auto& seg : segments) {
            if (seg.size() > 1) {
                g.DrawLines(&pen, seg.data(), (INT)seg.size());
            }
        }

        if (clipToRange) g.ResetClip();
    }
};

// -------------------------------------------------------------------------
// 4. Глобальное состояние
// -------------------------------------------------------------------------
enum Tool { T_PEN, T_LINE, T_RECT, T_ELLIPSE, T_TRIANGLE, T_STAR, T_ERASER, T_FUNC_PREPARE, T_FUNC_PLACE, T_IMAGE_PLACE };

struct AppState {
    Tool currentTool = T_PEN;
    Color currentColor = Color(255, 0, 0, 0);
    float currentWidth = 2.0f;
    float eraserSize = 20.0f;
    int eraserMenuID = ID_ERASER_M;
    std::vector<Shape*> shapes;
    bool isDrawing = false;
    bool isPanning = false;

    PointF startPoint;
    PointF currentPoint;
    Point lastMousePos;

    string funcExpr;
    double funcStart = -300, funcEnd = 300;
    bool funcShowAxes = true;
    bool funcClip = true;

    wstring imagePath;

    float zoom = 1.0f;
    float offsetX = 0.0f;
    float offsetY = 0.0f;
} appState;

struct FuncParams {
    wchar_t expr[256];
    wchar_t rangeStart[20];
    wchar_t rangeEnd[20];
    BOOL showAxes;
    BOOL clipRange;
    BOOL resultOK;
} g_FuncParams;

// -------------------------------------------------------------------------
// 5. Вспомогательные функции
// -------------------------------------------------------------------------
int GetEncoderClsid(const WCHAR* format, CLSID* pClsid) {
    UINT  num = 0; UINT  size = 0;
    GetImageEncodersSize(&num, &size);
    if (size == 0) return -1;
    ImageCodecInfo* pImageCodecInfo = (ImageCodecInfo*)(malloc(size));
    if (pImageCodecInfo == NULL) return -1;
    GetImageEncoders(num, size, pImageCodecInfo);
    for (UINT j = 0; j < num; ++j) {
        if (wcscmp(pImageCodecInfo[j].MimeType, format) == 0) {
            *pClsid = pImageCodecInfo[j].Clsid;
            free(pImageCodecInfo);
            return j;
        }
    }
    free(pImageCodecInfo);
    return -1;
}

PointF ScreenToWorld(int sx, int sy) {
    return PointF((sx - appState.offsetX) / appState.zoom, (sy - appState.offsetY) / appState.zoom);
}

// Проверка, включен ли автозапуск
bool IsAutorunEnabled() {
    HKEY hKey;
    if (RegOpenKeyEx(HKEY_CURRENT_USER, REG_PATH, 0, KEY_READ, &hKey) == ERROR_SUCCESS) {
        if (RegQueryValueEx(hKey, APP_NAME, NULL, NULL, NULL, NULL) == ERROR_SUCCESS) {
            RegCloseKey(hKey);
            return true;
        }
        RegCloseKey(hKey);
    }
    return false;
}

// Переключение автозапуска
void SetAutorun(bool enable) {
    HKEY hKey;
    if (RegOpenKeyEx(HKEY_CURRENT_USER, REG_PATH, 0, KEY_SET_VALUE, &hKey) == ERROR_SUCCESS) {
        if (enable) {
            wchar_t path[MAX_PATH];
            GetModuleFileName(NULL, path, MAX_PATH);
            RegSetValueEx(hKey, APP_NAME, 0, REG_SZ, (BYTE*)path, (lstrlen(path) + 1) * sizeof(wchar_t));
        }
        else {
            RegDeleteValue(hKey, APP_NAME);
        }
        RegCloseKey(hKey);
    }
}

void SelectColor(HWND hWnd) {
    CHOOSECOLOR cc;
    static COLORREF acrCustClr[16];
    ZeroMemory(&cc, sizeof(cc));
    cc.lStructSize = sizeof(cc);
    cc.hwndOwner = hWnd;
    cc.lpCustColors = (LPDWORD)acrCustClr;
    cc.rgbResult = RGB(appState.currentColor.GetR(), appState.currentColor.GetG(), appState.currentColor.GetB());
    cc.Flags = CC_FULLOPEN | CC_RGBINIT;
    if (ChooseColor(&cc)) {
        appState.currentColor = Color(255, GetRValue(cc.rgbResult), GetGValue(cc.rgbResult), GetBValue(cc.rgbResult));
    }
}

// -------------------------------------------------------------------------
// 6. Диалог (Кнопка ОТМЕНА исправлена)
// -------------------------------------------------------------------------
LRESULT CALLBACK InputDialogProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    static HWND hEditExpr, hEditStart, hEditEnd, hChkAxis, hChkClip;
    switch (msg) {
    case WM_CREATE:
        CreateWindow(L"STATIC", L"f(x) =", WS_VISIBLE | WS_CHILD, 15, 20, 40, 20, hWnd, NULL, NULL, NULL);
        hEditExpr = CreateWindow(L"EDIT", L"x^2", WS_VISIBLE | WS_CHILD | WS_BORDER | ES_AUTOHSCROLL, 60, 18, 240, 22, hWnd, NULL, NULL, NULL);

        CreateWindow(L"STATIC", L"Диапазон X: от", WS_VISIBLE | WS_CHILD, 15, 55, 100, 20, hWnd, NULL, NULL, NULL);
        hEditStart = CreateWindow(L"EDIT", L"-300", WS_VISIBLE | WS_CHILD | WS_BORDER, 120, 52, 60, 22, hWnd, NULL, NULL, NULL);
        CreateWindow(L"STATIC", L"до", WS_VISIBLE | WS_CHILD, 190, 55, 20, 20, hWnd, NULL, NULL, NULL);
        hEditEnd = CreateWindow(L"EDIT", L"300", WS_VISIBLE | WS_CHILD | WS_BORDER, 215, 52, 60, 22, hWnd, NULL, NULL, NULL);

        hChkAxis = CreateWindow(L"BUTTON", L"Рисовать оси координат и сетку", WS_VISIBLE | WS_CHILD | BS_AUTOCHECKBOX, 15, 85, 300, 20, hWnd, (HMENU)ID_CHK_AXIS, NULL, NULL);
        CheckDlgButton(hWnd, ID_CHK_AXIS, BST_CHECKED);

        hChkClip = CreateWindow(L"BUTTON", L"Ограничить функцию диапазоном", WS_VISIBLE | WS_CHILD | BS_AUTOCHECKBOX, 15, 110, 300, 20, hWnd, (HMENU)ID_CHK_CLIP, NULL, NULL);
        CheckDlgButton(hWnd, ID_CHK_CLIP, BST_CHECKED);

        // Кнопка ОК
        CreateWindow(L"BUTTON", L"OK", WS_VISIBLE | WS_CHILD | BS_DEFPUSHBUTTON, 60, 150, 90, 30, hWnd, (HMENU)ID_BTN_OK, NULL, NULL);
        // Кнопка ОТМЕНА (ширина 100 пикселей)
        CreateWindow(L"BUTTON", L"Отмена", WS_VISIBLE | WS_CHILD, 170, 150, 100, 30, hWnd, (HMENU)ID_BTN_CANCEL, NULL, NULL);
        break;
    case WM_COMMAND:
        if (LOWORD(wParam) == ID_BTN_OK) {
            GetWindowText(hEditExpr, g_FuncParams.expr, 256);
            GetWindowText(hEditStart, g_FuncParams.rangeStart, 20);
            GetWindowText(hEditEnd, g_FuncParams.rangeEnd, 20);
            g_FuncParams.showAxes = IsDlgButtonChecked(hWnd, ID_CHK_AXIS);
            g_FuncParams.clipRange = IsDlgButtonChecked(hWnd, ID_CHK_CLIP);
            g_FuncParams.resultOK = TRUE;
            DestroyWindow(hWnd);
        }
        else if (LOWORD(wParam) == ID_BTN_CANCEL) {
            g_FuncParams.resultOK = FALSE;
            DestroyWindow(hWnd);
        }
        break;
    case WM_CLOSE:
        g_FuncParams.resultOK = FALSE;
        DestroyWindow(hWnd);
        break;
    default: return DefWindowProc(hWnd, msg, wParam, lParam);
    }
    return 0;
}

void ShowFuncDialog(HWND hParent) {
    WNDCLASS wc = { 0 };
    wc.lpfnWndProc = InputDialogProc;
    wc.hInstance = GetModuleHandle(NULL);
    wc.hbrBackground = (HBRUSH)(COLOR_WINDOW);
    wc.lpszClassName = L"FuncDlg";
    wc.hCursor = LoadCursor(NULL, IDC_ARROW);
    RegisterClass(&wc);

    g_FuncParams.resultOK = FALSE;

    HWND hDlg = CreateWindowEx(WS_EX_DLGMODALFRAME | WS_EX_TOPMOST, L"FuncDlg", L"Параметры функции",
        WS_VISIBLE | WS_POPUP | WS_CAPTION | WS_SYSMENU, 0, 0, 340, 240, hParent, NULL, GetModuleHandle(NULL), NULL);

    RECT rcP, rcD; GetWindowRect(hParent, &rcP); GetWindowRect(hDlg, &rcD);
    SetWindowPos(hDlg, NULL, rcP.left + (rcP.right - rcP.left) / 2 - (rcD.right - rcD.left) / 2,
        rcP.top + (rcP.bottom - rcP.top) / 2 - (rcD.bottom - rcD.top) / 2, 0, 0, SWP_NOSIZE | SWP_NOZORDER);

    EnableWindow(hParent, FALSE);
    MSG msg;
    bool done = false;
    while (!done && GetMessage(&msg, NULL, 0, 0)) {
        if (!IsWindow(hDlg)) {
            done = true;
            break;
        }
        if (msg.message == WM_KEYDOWN && msg.wParam == VK_RETURN) {
            SendMessage(hDlg, WM_COMMAND, ID_BTN_OK, 0);
        }
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }
    EnableWindow(hParent, TRUE);
    SetForegroundWindow(hParent);
}

// -------------------------------------------------------------------------
// 7. WndProc
// -------------------------------------------------------------------------
LRESULT CALLBACK WndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    static ULONG_PTR gdiToken;
    static GdiplusStartupInput gdiInput;
    static HDC hdcMem;
    static HBITMAP hbmMem, hbmOld;
    static int cxClient, cyClient;

    switch (msg) {
    case WM_CREATE: {
        GdiplusStartup(&gdiToken, &gdiInput, NULL);

        HMENU hMenu = CreateMenu();
        HMENU hFile = CreatePopupMenu();
        AppendMenu(hFile, MF_STRING, ID_ACTION_OPEN, L"Открыть изображение... (Ctrl+O)");
        AppendMenu(hFile, MF_STRING, ID_ACTION_SAVE, L"Сохранить как... (Ctrl+S)");

        // Логика чекбокса автозапуска при создании
        bool autoRunEnabled = IsAutorunEnabled();
        UINT flags = autoRunEnabled ? (MF_STRING | MF_CHECKED) : MF_STRING;
        AppendMenu(hFile, flags, ID_ACTION_AUTORUN, L"Запускать при входе в Windows");

        AppendMenu(hFile, MF_SEPARATOR, 0, NULL);
        AppendMenu(hFile, MF_STRING, ID_ACTION_CLEAR, L"Очистить (Ctrl+N)");
        AppendMenu(hMenu, MF_POPUP, (UINT_PTR)hFile, L"Файл");

        HMENU hTools = CreatePopupMenu();
        AppendMenu(hTools, MF_STRING, ID_TOOL_PEN, L"Кисть (Ctrl+P)");
        AppendMenu(hTools, MF_STRING, ID_TOOL_LINE, L"Линия (Ctrl+L)");
        AppendMenu(hTools, MF_STRING, ID_TOOL_RECT, L"Прямоугольник (Ctrl+R)");
        AppendMenu(hTools, MF_STRING, ID_TOOL_ELLIPSE, L"Эллипс (Ctrl+E)");
        AppendMenu(hTools, MF_STRING, ID_TOOL_TRIANGLE, L"Треугольник (Ctrl+T)");
        AppendMenu(hTools, MF_STRING, ID_TOOL_STAR, L"Звезда (Ctrl+Shift+S)");
        AppendMenu(hTools, MF_SEPARATOR, 0, NULL);
        AppendMenu(hTools, MF_STRING, ID_TOOL_FUNC, L"График функции (Ctrl+F)");

        HMENU hEraserSz = CreatePopupMenu();
        AppendMenu(hEraserSz, MF_STRING, ID_ERASER_XS, L"Очень маленький (5px)");
        AppendMenu(hEraserSz, MF_STRING, ID_ERASER_S, L"Маленький (10px)");
        AppendMenu(hEraserSz, MF_STRING, ID_ERASER_M, L"Средний (20px)");
        AppendMenu(hEraserSz, MF_STRING, ID_ERASER_L, L"Большой (40px)");
        AppendMenu(hEraserSz, MF_STRING, ID_ERASER_XL, L"Огромный (80px)");

        AppendMenu(hTools, MF_POPUP, (UINT_PTR)hEraserSz, L"Размер ластика");
        AppendMenu(hTools, MF_STRING, ID_TOOL_ERASER, L"Ластик (Ctrl+D)");

        AppendMenu(hMenu, MF_POPUP, (UINT_PTR)hTools, L"Инструменты");
        AppendMenu(hMenu, MF_STRING, ID_ACTION_COLOR, L"Цвет");
        SetMenu(hWnd, hMenu);

        CheckMenuRadioItem(hMenu, ID_ERASER_XS, ID_ERASER_XL, ID_ERASER_M, MF_BYCOMMAND);
        break;
    }

    case WM_SIZE: {
        cxClient = LOWORD(lParam);
        cyClient = HIWORD(lParam);

        if (hbmMem) DeleteObject(hbmMem);
        if (hdcMem) DeleteDC(hdcMem);

        HDC hdc = GetDC(hWnd);
        hdcMem = CreateCompatibleDC(hdc);
        hbmMem = CreateCompatibleBitmap(hdc, cxClient, cyClient);
        hbmOld = (HBITMAP)SelectObject(hdcMem, hbmMem);
        ReleaseDC(hWnd, hdc);
        break;
    }

    case WM_MOUSEWHEEL: {
        int zDelta = GET_WHEEL_DELTA_WPARAM(wParam);
        POINT pt; GetCursorPos(&pt); ScreenToClient(hWnd, &pt);

        float scaleFactor = (zDelta > 0) ? 1.1f : 0.9f;
        float oldZoom = appState.zoom;

        if (oldZoom * scaleFactor < 0.1f) scaleFactor = 1.0f;
        if (oldZoom * scaleFactor > 50.0f) scaleFactor = 1.0f;

        appState.zoom *= scaleFactor;
        appState.offsetX = (float)pt.x - ((float)pt.x - appState.offsetX) * scaleFactor;
        appState.offsetY = (float)pt.y - ((float)pt.y - appState.offsetY) * scaleFactor;

        InvalidateRect(hWnd, NULL, FALSE);
        break;
    }

    case WM_MBUTTONDOWN: {
        appState.isPanning = true;
        appState.lastMousePos.X = LOWORD(lParam);
        appState.lastMousePos.Y = HIWORD(lParam);
        SetCapture(hWnd);
        SetCursor(LoadCursor(NULL, IDC_SIZEALL));
        break;
    }

    case WM_MBUTTONUP:
        if (appState.isPanning) {
            appState.isPanning = false;
            ReleaseCapture();
            SetCursor(LoadCursor(NULL, IDC_ARROW));
        }
        break;

    case WM_COMMAND: {
        int id = LOWORD(wParam);

        if (id >= ID_ERASER_XS && id <= ID_ERASER_XL) {
            appState.eraserMenuID = id;
            CheckMenuRadioItem(GetMenu(hWnd), ID_ERASER_XS, ID_ERASER_XL, id, MF_BYCOMMAND);
        }

        switch (id) {
        case ID_TOOL_PEN: appState.currentTool = T_PEN; break;
        case ID_TOOL_LINE: appState.currentTool = T_LINE; break;
        case ID_TOOL_RECT: appState.currentTool = T_RECT; break;
        case ID_TOOL_ELLIPSE: appState.currentTool = T_ELLIPSE; break;
        case ID_TOOL_TRIANGLE: appState.currentTool = T_TRIANGLE; break;
        case ID_TOOL_STAR: appState.currentTool = T_STAR; break;
        case ID_TOOL_ERASER: appState.currentTool = T_ERASER; break;

        case ID_ERASER_XS: appState.eraserSize = 5.0f; break;
        case ID_ERASER_S: appState.eraserSize = 10.0f; break;
        case ID_ERASER_M: appState.eraserSize = 20.0f; break;
        case ID_ERASER_L: appState.eraserSize = 40.0f; break;
        case ID_ERASER_XL: appState.eraserSize = 80.0f; break;

        case ID_TOOL_FUNC: {
            g_FuncParams.expr[0] = 0;
            ShowFuncDialog(hWnd);
            if (g_FuncParams.resultOK && wcslen(g_FuncParams.expr) > 0) {
                char buf[256];
                WideCharToMultiByte(CP_ACP, 0, g_FuncParams.expr, -1, buf, 256, NULL, NULL);
                appState.funcExpr = string(buf);
                appState.funcStart = _wtof(g_FuncParams.rangeStart);
                appState.funcEnd = _wtof(g_FuncParams.rangeEnd);
                appState.funcShowAxes = (g_FuncParams.showAxes == BST_CHECKED);
                appState.funcClip = (g_FuncParams.clipRange == BST_CHECKED);
                appState.currentTool = T_FUNC_PLACE;
            }
            break;
        }
        case ID_ACTION_COLOR: SelectColor(hWnd); break;
        case ID_ACTION_CLEAR:
            for (auto s : appState.shapes) delete s;
            appState.shapes.clear();
            InvalidateRect(hWnd, NULL, FALSE);
            break;

        case ID_ACTION_OPEN: {
            OPENFILENAME ofn;
            WCHAR szFile[260] = { 0 };
            ZeroMemory(&ofn, sizeof(ofn));
            ofn.lStructSize = sizeof(ofn);
            ofn.hwndOwner = hWnd;
            ofn.lpstrFile = szFile;
            ofn.nMaxFile = sizeof(szFile);
            ofn.lpstrFilter = L"Images\0*.png;*.jpg;*.jpeg;*.bmp\0All\0*.*\0";
            ofn.nFilterIndex = 1;
            if (GetOpenFileName(&ofn) == TRUE) {
                appState.imagePath = szFile;
                appState.currentTool = T_IMAGE_PLACE;
            }
            break;
        }

        case ID_ACTION_SAVE: {
            OPENFILENAME ofn;
            WCHAR szFile[260] = { 0 };
            ZeroMemory(&ofn, sizeof(ofn));
            ofn.lStructSize = sizeof(ofn);
            ofn.hwndOwner = hWnd;
            ofn.lpstrFile = szFile;
            ofn.nMaxFile = sizeof(szFile);
            ofn.lpstrFilter = L"PNG Image\0*.png\0JPEG Image\0*.jpg\0Bitmap\0*.bmp\0All\0*.*\0";
            ofn.nFilterIndex = 1;
            ofn.lpstrDefExt = L"png";
            if (GetSaveFileName(&ofn) == TRUE) {
                CLSID clsid;
                if (wcsstr(szFile, L".jpg") || wcsstr(szFile, L".jpeg")) GetEncoderClsid(L"image/jpeg", &clsid);
                else if (wcsstr(szFile, L".bmp")) GetEncoderClsid(L"image/bmp", &clsid);
                else GetEncoderClsid(L"image/png", &clsid);

                Bitmap bmp(hbmMem, NULL);
                bmp.Save(szFile, &clsid, NULL);
                MessageBox(hWnd, L"Изображение сохранено.", L"Успех", MB_OK);
            }
            break;
        }
        case ID_ACTION_AUTORUN: {
            bool newState = !IsAutorunEnabled();
            SetAutorun(newState);
            // Обновляем галочку в меню
            CheckMenuItem(GetMenu(hWnd), ID_ACTION_AUTORUN, newState ? MF_CHECKED : MF_UNCHECKED);
            break;
        }
        }
        break;
    }

    case WM_LBUTTONDOWN: {
        PointF worldPos = ScreenToWorld((int)(short)LOWORD(lParam), (int)(short)HIWORD(lParam));
        appState.isDrawing = true;
        appState.startPoint = worldPos;
        appState.currentPoint = worldPos;
        SetCapture(hWnd);

        if (appState.currentTool == T_PEN || appState.currentTool == T_ERASER) {
            Color c = (appState.currentTool == T_ERASER) ? Color(255, 255, 255, 255) : appState.currentColor;
            float w = (appState.currentTool == T_ERASER) ? appState.eraserSize : appState.currentWidth;
            PenShape* p = new PenShape(c, w / appState.zoom);
            p->AddPoint(worldPos);
            appState.shapes.push_back(p);
        }
        else if (appState.currentTool == T_FUNC_PLACE) {
            appState.shapes.push_back(new FunctionShape(
                appState.funcExpr, appState.funcStart, appState.funcEnd,
                worldPos, appState.currentColor, 2.0f / appState.zoom,
                appState.funcShowAxes, appState.funcClip
            ));
            appState.currentTool = T_PEN;
            appState.isDrawing = false;
            ReleaseCapture();
            InvalidateRect(hWnd, NULL, FALSE);
        }
        break;
    }

    case WM_MOUSEMOVE: {
        int mx = (int)(short)LOWORD(lParam);
        int my = (int)(short)HIWORD(lParam);

        if (appState.isPanning) {
            float dx = (float)(mx - appState.lastMousePos.X);
            float dy = (float)(my - appState.lastMousePos.Y);
            appState.offsetX += dx;
            appState.offsetY += dy;
            appState.lastMousePos.X = mx;
            appState.lastMousePos.Y = my;
            InvalidateRect(hWnd, NULL, FALSE);
            return 0;
        }

        PointF worldPos = ScreenToWorld(mx, my);
        appState.currentPoint = worldPos;

        if (appState.currentTool == T_ERASER && !appState.isDrawing) {
            InvalidateRect(hWnd, NULL, FALSE);
        }

        if (appState.isDrawing) {
            if (appState.currentTool == T_PEN || appState.currentTool == T_ERASER) {
                Shape* s = appState.shapes.back();
                ((PenShape*)s)->AddPoint(worldPos);
            }
            InvalidateRect(hWnd, NULL, FALSE);
        }
        else if (appState.currentTool == T_FUNC_PLACE || appState.currentTool == T_IMAGE_PLACE) {
            InvalidateRect(hWnd, NULL, FALSE);
        }
        break;
    }

    case WM_LBUTTONUP: {
        if (appState.isDrawing) {
            appState.isDrawing = false;
            ReleaseCapture();

            Color c = appState.currentColor;
            float w = appState.currentWidth / appState.zoom;

            float l = min(appState.startPoint.X, appState.currentPoint.X);
            float t = min(appState.startPoint.Y, appState.currentPoint.Y);
            float rw = abs(appState.currentPoint.X - appState.startPoint.X);
            float rh = abs(appState.currentPoint.Y - appState.startPoint.Y);
            RectF r(l, t, rw, rh);

            // Обработка фигур
            if (appState.currentTool == T_LINE) {
                appState.shapes.push_back(new LineShape(appState.startPoint, appState.currentPoint, c, w));
            }
            else if (appState.currentTool == T_RECT) {
                appState.shapes.push_back(new RectShape(r, c, w));
            }
            else if (appState.currentTool == T_ELLIPSE) {
                appState.shapes.push_back(new EllipseShape(r, c, w));
            }
            else if (appState.currentTool == T_TRIANGLE) {
                appState.shapes.push_back(new TriangleShape(r, c, w));
            }
            else if (appState.currentTool == T_STAR) {
                appState.shapes.push_back(new StarShape(r, c, w));
            }
            else if (appState.currentTool == T_IMAGE_PLACE) {
                // Добавляем картинку
                appState.shapes.push_back(new ImageShape(appState.imagePath.c_str(), r));
                appState.currentTool = T_PEN; // Возврат к кисти
            }

            InvalidateRect(hWnd, NULL, FALSE);
        }
        break;
    }

    case WM_PAINT: {
        PAINTSTRUCT ps;
        HDC hdc = BeginPaint(hWnd, &ps);

        Graphics g(hdcMem);
        g.SetSmoothingMode(SmoothingModeAntiAlias);
        g.Clear(Color(255, 255, 255, 255));

        Matrix matrix;
        matrix.Translate(appState.offsetX, appState.offsetY);
        matrix.Scale(appState.zoom, appState.zoom);
        g.SetTransform(&matrix);

        // Рисуем все фигуры
        for (auto s : appState.shapes) s->Draw(g);

        Color previewColor = Color(128, 100, 100, 100);
        Pen previewPen(previewColor, 1.0f / appState.zoom);
        previewPen.SetDashStyle(DashStyleDot);

        // ПРЕДПРОСМОТР ЛАСТИКА
        if (appState.currentTool == T_ERASER) {
            float size = appState.eraserSize / appState.zoom;
            float x = appState.currentPoint.X - size / 2;
            float y = appState.currentPoint.Y - size / 2;
            Pen eraserPen(Color(150, 0, 0, 0), 1.0f / appState.zoom);
            g.DrawEllipse(&eraserPen, x, y, size, size);
        }

        if (appState.currentTool == T_FUNC_PLACE) {
            PointF origin = appState.currentPoint;
            float axLen = 1000.0f / appState.zoom;
            g.DrawLine(&previewPen, origin.X - axLen, origin.Y, origin.X + axLen, origin.Y);
            g.DrawLine(&previewPen, origin.X, origin.Y - axLen, origin.X, origin.Y + axLen);

            FunctionShape tmp(appState.funcExpr, appState.funcStart, appState.funcEnd, origin, Color(100, 0, 0, 200), 1.0f / appState.zoom, false, appState.funcClip);
            tmp.Draw(g);
        }
        else if (appState.isDrawing && appState.currentTool != T_PEN && appState.currentTool != T_ERASER) {
            if (appState.currentTool == T_LINE) {
                g.DrawLine(&previewPen, appState.startPoint, appState.currentPoint);
            }
            else {
                float l = min(appState.startPoint.X, appState.currentPoint.X);
                float t = min(appState.startPoint.Y, appState.currentPoint.Y);
                float w = abs(appState.currentPoint.X - appState.startPoint.X);
                float h = abs(appState.currentPoint.Y - appState.startPoint.Y);
                if (appState.currentTool == T_TRIANGLE || appState.currentTool == T_STAR) {
                    g.DrawRectangle(&previewPen, l, t, w, h); // Рамка для сложных фигур
                }
                else if (appState.currentTool == T_IMAGE_PLACE) {
                    g.DrawRectangle(&previewPen, l, t, w, h);
                    // Можно добавить текст "Image"
                }
                else if (appState.currentTool == T_RECT) {
                    g.DrawRectangle(&previewPen, l, t, w, h);
                }
                else if (appState.currentTool == T_ELLIPSE) {
                    g.DrawEllipse(&previewPen, l, t, w, h);
                }
            }
        }

        BitBlt(hdc, 0, 0, cxClient, cyClient, hdcMem, 0, 0, SRCCOPY);
        EndPaint(hWnd, &ps);
        break;
    }

    case WM_ERASEBKGND: return 1;

    case WM_DESTROY:
        SelectObject(hdcMem, hbmOld);
        DeleteObject(hbmMem);
        DeleteDC(hdcMem);
        GdiplusShutdown(gdiToken);
        for (auto s : appState.shapes) delete s;
        PostQuitMessage(0);
        break;

    default: return DefWindowProc(hWnd, msg, wParam, lParam);
    }
    return 0;
}

// -------------------------------------------------------------------------
// 8. Точка входа
// -------------------------------------------------------------------------
int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrev, LPSTR lpCmdLine, int nCmdShow) {
    HANDLE hMutex = CreateMutex(NULL, TRUE, MUTEX_NAME);
    if (GetLastError() == ERROR_ALREADY_EXISTS) return 0;

    WNDCLASSEX wc;
    wc.cbSize = sizeof(WNDCLASSEX);
    wc.style = CS_HREDRAW | CS_VREDRAW;
    wc.lpfnWndProc = WndProc;
    wc.cbClsExtra = 0;
    wc.cbWndExtra = 0;
    wc.hInstance = hInstance;
    wc.hIcon = LoadIcon(NULL, IDI_APPLICATION);
    wc.hCursor = LoadCursor(NULL, IDC_CROSS);
    wc.hbrBackground = (HBRUSH)GetStockObject(WHITE_BRUSH);
    wc.lpszMenuName = NULL;
    wc.lpszClassName = L"MyPaintClass";
    wc.hIconSm = LoadIcon(NULL, IDI_APPLICATION);

    RegisterClassEx(&wc);

    HWND hWnd = CreateWindow(L"MyPaintClass", L"Super Paint V5 (Import + Triangles)",
        WS_OVERLAPPEDWINDOW | WS_CLIPCHILDREN,
        CW_USEDEFAULT, CW_USEDEFAULT, 1200, 800, NULL, NULL, hInstance, NULL);

    ShowWindow(hWnd, nCmdShow);
    UpdateWindow(hWnd);

    ACCEL accels[] = {
        { FCONTROL | FVIRTKEY, 'N', ID_ACTION_CLEAR },
        { FCONTROL | FVIRTKEY, 'S', ID_ACTION_SAVE },
        { FCONTROL | FVIRTKEY, 'O', ID_ACTION_OPEN }, // Ctrl+O
        { FCONTROL | FVIRTKEY, 'P', ID_TOOL_PEN },
        { FCONTROL | FVIRTKEY, 'L', ID_TOOL_LINE },
        { FCONTROL | FVIRTKEY, 'R', ID_TOOL_RECT },
        { FCONTROL | FVIRTKEY, 'E', ID_TOOL_ELLIPSE },
        { FCONTROL | FVIRTKEY, 'T', ID_TOOL_TRIANGLE },
        { FCONTROL | FSHIFT | FVIRTKEY, 'S', ID_TOOL_STAR }, // Ctrl+Shift+S
        { FCONTROL | FVIRTKEY, 'D', ID_TOOL_ERASER },
        { FCONTROL | FVIRTKEY, 'F', ID_TOOL_FUNC }
    };
    HACCEL hAccel = CreateAcceleratorTable(accels, 11);

    MSG msg;
    while (GetMessage(&msg, NULL, 0, 0)) {
        if (!TranslateAccelerator(hWnd, hAccel, &msg)) {
            TranslateMessage(&msg);
            DispatchMessage(&msg);
        }
    }
    DestroyAcceleratorTable(hAccel);
    CloseHandle(hMutex);
    return (int)msg.wParam;
}