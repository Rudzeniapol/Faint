#define _USE_MATH_DEFINES
#include <windows.h>
#include <gdiplus.h>
#include <vector>
#include <string>
#include <cmath>
#include <sstream>
#include <stack>
#include <map>
#include <functional>
#include <algorithm>

// Подключение библиотек
#pragma comment(lib, "gdiplus.lib")
#pragma comment(lib, "user32.lib")
#pragma comment(lib, "gdi32.lib")
#pragma comment(lib, "advapi32.lib")

// Включение визуальных стилей (для красивых кнопок в диалогах)
#pragma comment(linker,"\"/manifestdependency:type='win32' \
name='Microsoft.Windows.Common-Controls' version='6.0.0.0' \
processorArchitecture='*' publicKeyToken='6595b64144ccf1df' language='*'\"")

using namespace Gdiplus;
using namespace std;

// -------------------------------------------------------------------------
// 1. Глобальные константы и идентификаторы
// -------------------------------------------------------------------------
#define ID_TOOL_PEN       1001
#define ID_TOOL_LINE      1002
#define ID_TOOL_RECT      1003
#define ID_TOOL_ELLIPSE   1004
#define ID_TOOL_FUNC      1005
#define ID_TOOL_ERASER    1006
#define ID_ACTION_CLEAR   1007
#define ID_ACTION_AUTORUN 1008
#define ID_BTN_OK         2001
#define ID_BTN_CANCEL     2002

// Мьютек для защиты от повторного запуска
const wchar_t* MUTEX_NAME = L"Global\\MyGDIPlusPaintMutex_v1";
const wchar_t* REG_PATH = L"Software\\Microsoft\\Windows\\CurrentVersion\\Run";
const wchar_t* APP_NAME = L"MyGDIPlusPaint";

// -------------------------------------------------------------------------
// 2. Математический парсер (Упрощенный)
// Поддерживает +, -, *, /, sin, cos, x, числа
// -------------------------------------------------------------------------
class MathParser {
public:
    static double Evaluate(string expr, double x) {
        // Удаляем пробелы
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
            if (op == '+') left += right;
            else left -= right;
        }
        return left;
    }

    static double ParseTerm(const string& expr, size_t& pos, double x) {
        double left = ParseFactor(expr, pos, x);
        while (pos < expr.length()) {
            char op = expr[pos];
            if (op != '*' && op != '/') break;
            pos++;
            double right = ParseFactor(expr, pos, x);
            if (op == '*') left *= right;
            else if (right != 0) left /= right;
        }
        return left;
    }

    static double ParseFactor(const string& expr, size_t& pos, double x) {
        if (pos >= expr.length()) return 0;

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
            if (pos < expr.length() && expr[pos] == '(') {
                pos++;
                double val = ParseExpression(expr, pos, x);
                if (pos < expr.length() && expr[pos] == ')') pos++;
                if (func == "sin") return sin(val);
                if (func == "cos") return cos(val);
            }
        }
        if (isdigit(expr[pos]) || expr[pos] == '.' || expr[pos] == '-') {
            string numStr;
            if (expr[pos] == '-') numStr += expr[pos++];
            while (pos < expr.length() && (isdigit(expr[pos]) || expr[pos] == '.')) {
                numStr += expr[pos++];
            }
            return stod(numStr);
        }
        return 0;
    }
};

// -------------------------------------------------------------------------
// 3. Классы фигур
// -------------------------------------------------------------------------
enum ShapeType { SHAPE_PEN, SHAPE_LINE, SHAPE_RECT, SHAPE_ELLIPSE, SHAPE_FUNC };

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
    std::vector<Point> points;
    PenShape(Color c, float w) : Shape(c, w) {}
    void AddPoint(Point p) { points.push_back(p); }
    void Draw(Graphics& g) override {
        if (points.size() < 2) return;
        Pen pen(color, width);
        pen.SetStartCap(LineCapRound);
        pen.SetEndCap(LineCapRound);
        g.DrawCurve(&pen, points.data(), (INT)points.size());
    }
};

class LineShape : public Shape {
public:
    Point start, end;
    LineShape(Point s, Point e, Color c, float w) : Shape(c, w), start(s), end(e) {}
    void Draw(Graphics& g) override {
        Pen pen(color, width);
        g.DrawLine(&pen, start, end);
    }
};

class RectShape : public Shape {
public:
    Rect rect;
    RectShape(Rect r, Color c, float w) : Shape(c, w), rect(r) {}
    void Draw(Graphics& g) override {
        Pen pen(color, width);
        g.DrawRectangle(&pen, rect);
    }
};

class EllipseShape : public Shape {
public:
    Rect rect;
    EllipseShape(Rect r, Color c, float w) : Shape(c, w), rect(r) {}
    void Draw(Graphics& g) override {
        Pen pen(color, width);
        g.DrawEllipse(&pen, rect);
    }
};

class FunctionShape : public Shape {
public:
    string expression;
    double rangeStart, rangeEnd;
    Point origin;

    FunctionShape(string expr, double start, double end, Point org, Color c, float w)
        : Shape(c, w), expression(expr), rangeStart(start), rangeEnd(end), origin(org) {
    }

    void Draw(Graphics& g) override {
        Pen pen(color, width);

        // Рисуем оси (с прозрачностью)
        Pen axisPen(Color(100, 0, 0, 0), 1);
        g.DrawLine(&axisPen, Point(origin.X - 1000, origin.Y), Point(origin.X + 1000, origin.Y)); // X
        g.DrawLine(&axisPen, Point(origin.X, origin.Y - 1000), Point(origin.X, origin.Y + 1000)); // Y

        std::vector<PointF> pnts;
        // Шаг отрисовки
        double step = 1.0;
        for (double xPix = -500; xPix <= 500; xPix += step) {
            // Логический x относительно начала координат
            // Масштаб: 1 пиксель = 1 единица (можно усложнить)
            double xVal = xPix;

            if (xVal < rangeStart || xVal > rangeEnd) continue;

            double yVal = MathParser::Evaluate(expression, xVal);

            // Инвертируем Y, так как на экране Y растет вниз
            float screenX = (float)origin.X + (float)xPix;
            float screenY = (float)origin.Y - (float)yVal;

            pnts.push_back(PointF(screenX, screenY));
        }

        if (pnts.size() > 1) {
            g.DrawLines(&pen, pnts.data(), (INT)pnts.size());
        }
    }
};

// -------------------------------------------------------------------------
// 4. Состояние приложения
// -------------------------------------------------------------------------
enum Tool { T_PEN, T_LINE, T_RECT, T_ELLIPSE, T_ERASER, T_FUNC_PREPARE, T_FUNC_PLACE };

struct AppState {
    Tool currentTool = T_PEN;
    Color currentColor = Color(255, 0, 0, 0);
    float currentWidth = 2.0f;
    std::vector<Shape*> shapes;
    bool isDrawing = false;
    Point startPoint;
    Point currentPoint;

    // Для функции
    string funcExpr;
    double funcStart, funcEnd;

    // Undo/Redo можно реализовать через стеки, но для краткости опустим полный стек redo
} appState;

// -------------------------------------------------------------------------
// 5. Вспомогательные функции (Реестр, Диалог ввода)
// -------------------------------------------------------------------------

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

// Простая структура для передачи данных в диалог
struct FuncParams {
    wchar_t expr[256];
    wchar_t rangeStart[20];
    wchar_t rangeEnd[20];
};

FuncParams g_FuncParams;

// Процедура диалогового окна (создаем вручную без ресурсов .rc для портативности кода)
LRESULT CALLBACK InputDialogProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    static HWND hEditExpr, hEditStart, hEditEnd;
    switch (msg) {
    case WM_CREATE:
        CreateWindow(L"STATIC", L"f(x) =", WS_VISIBLE | WS_CHILD, 10, 10, 50, 20, hWnd, NULL, NULL, NULL);
        hEditExpr = CreateWindow(L"EDIT", L"sin(x/20)*50", WS_VISIBLE | WS_CHILD | WS_BORDER | ES_AUTOHSCROLL, 70, 10, 200, 20, hWnd, NULL, NULL, NULL);

        CreateWindow(L"STATIC", L"От:", WS_VISIBLE | WS_CHILD, 10, 40, 30, 20, hWnd, NULL, NULL, NULL);
        hEditStart = CreateWindow(L"EDIT", L"-300", WS_VISIBLE | WS_CHILD | WS_BORDER, 50, 40, 80, 20, hWnd, NULL, NULL, NULL);

        CreateWindow(L"STATIC", L"До:", WS_VISIBLE | WS_CHILD, 150, 40, 30, 20, hWnd, NULL, NULL, NULL);
        hEditEnd = CreateWindow(L"EDIT", L"300", WS_VISIBLE | WS_CHILD | WS_BORDER, 190, 40, 80, 20, hWnd, NULL, NULL, NULL);

        CreateWindow(L"BUTTON", L"OK", WS_VISIBLE | WS_CHILD | BS_DEFPUSHBUTTON, 50, 80, 80, 25, hWnd, (HMENU)ID_BTN_OK, NULL, NULL);
        CreateWindow(L"BUTTON", L"Отмена", WS_VISIBLE | WS_CHILD, 150, 80, 80, 25, hWnd, (HMENU)ID_BTN_CANCEL, NULL, NULL);
        break;
    case WM_COMMAND:
        if (LOWORD(wParam) == ID_BTN_OK) {
            GetWindowText(hEditExpr, g_FuncParams.expr, 256);
            GetWindowText(hEditStart, g_FuncParams.rangeStart, 20);
            GetWindowText(hEditEnd, g_FuncParams.rangeEnd, 20);
            PostMessage(hWnd, WM_CLOSE, 0, 0);
            EndDialog(hWnd, IDOK); // Эмуляция
            DestroyWindow(hWnd);
        }
        else if (LOWORD(wParam) == ID_BTN_CANCEL) {
            DestroyWindow(hWnd);
        }
        break;
    default:
        return DefWindowProc(hWnd, msg, wParam, lParam);
    }
    return 0;
}

void ShowFuncDialog(HWND hParent) {
    // Регистрируем класс для диалога
    WNDCLASS wc = { 0 };
    wc.lpfnWndProc = InputDialogProc;
    wc.hInstance = GetModuleHandle(NULL);
    wc.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1);
    wc.lpszClassName = L"FuncDialogClass";
    wc.hCursor = LoadCursor(NULL, IDC_ARROW);
    RegisterClass(&wc);

    HWND hDlg = CreateWindowEx(WS_EX_DLGMODALFRAME, L"FuncDialogClass", L"Параметры функции",
        WS_VISIBLE | WS_POPUP | WS_CAPTION | WS_SYSMENU,
        100, 100, 300, 150, hParent, NULL, GetModuleHandle(NULL), NULL);

    // Центрируем
    RECT rcParent, rcDlg;
    GetWindowRect(hParent, &rcParent);
    GetWindowRect(hDlg, &rcDlg);
    int x = rcParent.left + (rcParent.right - rcParent.left) / 2 - (rcDlg.right - rcDlg.left) / 2;
    int y = rcParent.top + (rcParent.bottom - rcParent.top) / 2 - (rcDlg.bottom - rcDlg.top) / 2;
    SetWindowPos(hDlg, NULL, x, y, 0, 0, SWP_NOSIZE | SWP_NOZORDER);

    // Модальный цикл
    EnableWindow(hParent, FALSE);
    MSG msg;
    while (IsWindow(hDlg) && GetMessage(&msg, NULL, 0, 0)) {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }
    EnableWindow(hParent, TRUE);
    SetForegroundWindow(hParent);

    // Парсим результат если окно закрылось успешно (тут упрощено, берем из глобальной структуры)
    if (lstrlen(g_FuncParams.expr) > 0) {
        // Конвертация WCHAR -> string
        char buf[256];
        WideCharToMultiByte(CP_ACP, 0, g_FuncParams.expr, -1, buf, 256, NULL, NULL);
        appState.funcExpr = string(buf);
        appState.funcStart = _wtof(g_FuncParams.rangeStart);
        appState.funcEnd = _wtof(g_FuncParams.rangeEnd);

        // Переход в режим выбора точки
        appState.currentTool = T_FUNC_PLACE;
    }
}

// -------------------------------------------------------------------------
// 6. Основное окно и отрисовка
// -------------------------------------------------------------------------

LRESULT CALLBACK WndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    static ULONG_PTR gdiplusToken;
    static GdiplusStartupInput gdiplusStartupInput;

    // Двойная буферизация
    static HDC hdcMem = NULL;
    static HBITMAP hbmMem = NULL;
    static HBITMAP hbmOld = NULL;
    static int cxClient, cyClient;

    switch (msg) {
    case WM_CREATE: {
        GdiplusStartup(&gdiplusToken, &gdiplusStartupInput, NULL);

        // Создание меню
        HMENU hMenu = CreateMenu();
        HMENU hFile = CreatePopupMenu();
        AppendMenu(hFile, MF_STRING, ID_ACTION_AUTORUN, L"Автозапуск вкл/выкл");
        AppendMenu(hFile, MF_STRING, ID_ACTION_CLEAR, L"Очистить (Ctrl+N)");
        AppendMenu(hMenu, MF_POPUP, (UINT_PTR)hFile, L"Файл");

        HMENU hTools = CreatePopupMenu();
        AppendMenu(hTools, MF_STRING, ID_TOOL_PEN, L"Кисть");
        AppendMenu(hTools, MF_STRING, ID_TOOL_LINE, L"Линия");
        AppendMenu(hTools, MF_STRING, ID_TOOL_RECT, L"Прямоугольник");
        AppendMenu(hTools, MF_STRING, ID_TOOL_ELLIPSE, L"Эллипс");
        AppendMenu(hTools, MF_STRING, ID_TOOL_ERASER, L"Ластик");
        AppendMenu(hTools, MF_SEPARATOR, 0, NULL);
        AppendMenu(hTools, MF_STRING, ID_TOOL_FUNC, L"График функции (Ctrl+F)");
        AppendMenu(hMenu, MF_POPUP, (UINT_PTR)hTools, L"Инструменты");

        SetMenu(hWnd, hMenu);
        break;
    }

    case WM_SIZE: {
        cxClient = LOWORD(lParam);
        cyClient = HIWORD(lParam);

        // Пересоздаем буфер
        if (hbmMem) DeleteObject(hbmMem);
        if (hdcMem) DeleteDC(hdcMem);

        HDC hdc = GetDC(hWnd);
        hdcMem = CreateCompatibleDC(hdc);
        hbmMem = CreateCompatibleBitmap(hdc, cxClient, cyClient);
        hbmOld = (HBITMAP)SelectObject(hdcMem, hbmMem);

        // Заливаем белым
        RECT rc = { 0, 0, cxClient, cyClient };
        FillRect(hdcMem, &rc, (HBRUSH)GetStockObject(WHITE_BRUSH));

        ReleaseDC(hWnd, hdc);
        break;
    }

    case WM_COMMAND: {
        int id = LOWORD(wParam);
        switch (id) {
        case ID_TOOL_PEN: appState.currentTool = T_PEN; break;
        case ID_TOOL_LINE: appState.currentTool = T_LINE; break;
        case ID_TOOL_RECT: appState.currentTool = T_RECT; break;
        case ID_TOOL_ELLIPSE: appState.currentTool = T_ELLIPSE; break;
        case ID_TOOL_ERASER: appState.currentTool = T_ERASER; break;
        case ID_TOOL_FUNC:
            g_FuncParams.expr[0] = 0; // Сброс
            ShowFuncDialog(hWnd);
            break;
        case ID_ACTION_CLEAR:
            for (auto s : appState.shapes) delete s;
            appState.shapes.clear();
            InvalidateRect(hWnd, NULL, FALSE);
            break;
        case ID_ACTION_AUTORUN: {
            // Простая переключалка (логика определения состояния опущена для краткости)
            static bool autoRun = false;
            autoRun = !autoRun;
            SetAutorun(autoRun);
            MessageBox(hWnd, autoRun ? L"Автозапуск включен" : L"Автозапуск выключен", L"Настройки", MB_OK);
            break;
        }
        }
        break;
    }

    case WM_LBUTTONDOWN: {
        appState.isDrawing = true;
        appState.startPoint = Point((int)(short)LOWORD(lParam), (int)(short)HIWORD(lParam));
        appState.currentPoint = appState.startPoint;
        SetCapture(hWnd);

        if (appState.currentTool == T_PEN || appState.currentTool == T_ERASER) {
            Color c = (appState.currentTool == T_ERASER) ? Color(255, 255, 255, 255) : appState.currentColor;
            float w = (appState.currentTool == T_ERASER) ? 10.0f : appState.currentWidth;
            PenShape* p = new PenShape(c, w);
            p->AddPoint(appState.startPoint);
            appState.shapes.push_back(p);
        }
        else if (appState.currentTool == T_FUNC_PLACE) {
            // Фиксация функции
            appState.shapes.push_back(new FunctionShape(
                appState.funcExpr,
                appState.funcStart,
                appState.funcEnd,
                appState.startPoint, // Origin
                Color(255, 0, 0, 255), // Синий цвет графика
                2.0f
            ));
            appState.currentTool = T_PEN; // Возврат к кисти
            appState.isDrawing = false;
            ReleaseCapture();
            InvalidateRect(hWnd, NULL, FALSE);
        }
        break;
    }

    case WM_MOUSEMOVE: {
        appState.currentPoint = Point((int)(short)LOWORD(lParam), (int)(short)HIWORD(lParam));

        if (appState.currentTool == T_FUNC_PLACE) {
            // Режим выбора начала координат - просто перерисовка для предпросмотра
            InvalidateRect(hWnd, NULL, FALSE);
        }
        else if (appState.isDrawing) {
            if (appState.currentTool == T_PEN || appState.currentTool == T_ERASER) {
                // Добавляем точки к последней фигуре (PenShape)
                Shape* s = appState.shapes.back();
                ((PenShape*)s)->AddPoint(appState.currentPoint);
            }
            // Для остальных фигур перерисовка для предпросмотра
            InvalidateRect(hWnd, NULL, FALSE);
        }
        break;
    }

    case WM_LBUTTONUP: {
        if (appState.isDrawing) {
            appState.isDrawing = false;
            ReleaseCapture();

            Color c = appState.currentColor;
            float w = appState.currentWidth;
            Rect r = Rect(
                min(appState.startPoint.X, appState.currentPoint.X),
                min(appState.startPoint.Y, appState.currentPoint.Y),
                abs(appState.currentPoint.X - appState.startPoint.X),
                abs(appState.currentPoint.Y - appState.startPoint.Y)
            );

            if (appState.currentTool == T_LINE) {
                appState.shapes.push_back(new LineShape(appState.startPoint, appState.currentPoint, c, w));
            }
            else if (appState.currentTool == T_RECT) {
                appState.shapes.push_back(new RectShape(r, c, w));
            }
            else if (appState.currentTool == T_ELLIPSE) {
                appState.shapes.push_back(new EllipseShape(r, c, w));
            }
            InvalidateRect(hWnd, NULL, FALSE);
        }
        break;
    }

    case WM_PAINT: {
        PAINTSTRUCT ps;
        HDC hdc = BeginPaint(hWnd, &ps);

        // 1. Очистка буфера
        Graphics g(hdcMem);
        g.Clear(Color(255, 255, 255, 255));
        g.SetSmoothingMode(SmoothingModeAntiAlias);

        // 2. Отрисовка сохраненных фигур
        for (auto s : appState.shapes) {
            s->Draw(g);
        }

        // 3. Отрисовка предпросмотра (текущая операция)
        Color previewColor = Color(128, 0, 0, 0); // Полупрозрачный
        Pen previewPen(previewColor, 1.0f);
        previewPen.SetDashStyle(DashStyleDot);

        if (appState.currentTool == T_FUNC_PLACE) {
            // Предпросмотр графика
            Point origin = appState.currentPoint;
            // Рисуем крестик осей
            g.DrawLine(&previewPen, origin.X - 50, origin.Y, origin.X + 50, origin.Y);
            g.DrawLine(&previewPen, origin.X, origin.Y - 50, origin.X, origin.Y + 50);

            // Рисуем призрачный график
            FunctionShape tempFunc(appState.funcExpr, appState.funcStart, appState.funcEnd, origin, Color(100, 0, 0, 255), 2.0f);
            tempFunc.Draw(g);
        }
        else if (appState.isDrawing) {
            if (appState.currentTool == T_LINE) {
                g.DrawLine(&previewPen, appState.startPoint, appState.currentPoint);
            }
            else if (appState.currentTool == T_RECT || appState.currentTool == T_ELLIPSE) {
                Rect r(
                    min(appState.startPoint.X, appState.currentPoint.X),
                    min(appState.startPoint.Y, appState.currentPoint.Y),
                    abs(appState.currentPoint.X - appState.startPoint.X),
                    abs(appState.currentPoint.Y - appState.startPoint.Y)
                );
                if (appState.currentTool == T_RECT) g.DrawRectangle(&previewPen, r);
                else g.DrawEllipse(&previewPen, r);
            }
        }

        // 4. Копирование буфера на экран
        BitBlt(hdc, 0, 0, cxClient, cyClient, hdcMem, 0, 0, SRCCOPY);

        EndPaint(hWnd, &ps);
        break;
    }

    case WM_ERASEBKGND:
        return 1; // Предотвращаем мерцание

    case WM_DESTROY:
        SelectObject(hdcMem, hbmOld);
        DeleteObject(hbmMem);
        DeleteDC(hdcMem);
        GdiplusShutdown(gdiplusToken);
        for (auto s : appState.shapes) delete s;
        PostQuitMessage(0);
        break;

    default:
        return DefWindowProc(hWnd, msg, wParam, lParam);
    }
    return 0;
}

// -------------------------------------------------------------------------
// 7. Точка входа
// -------------------------------------------------------------------------

int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nCmdShow) {
    // 1. Защита от повторного запуска
    HANDLE hMutex = CreateMutex(NULL, TRUE, MUTEX_NAME);
    if (GetLastError() == ERROR_ALREADY_EXISTS) {
        MessageBox(NULL, L"Приложение уже запущено!", L"Ошибка", MB_OK | MB_ICONERROR);
        return 0;
    }

    // 2. Регистрация класса окна
    WNDCLASSEX wc = { 0 };
    wc.cbSize = sizeof(WNDCLASSEX);
    wc.style = CS_HREDRAW | CS_VREDRAW;
    wc.lpfnWndProc = WndProc;
    wc.hInstance = hInstance;
    wc.hCursor = LoadCursor(NULL, IDC_CROSS);
    wc.hbrBackground = (HBRUSH)GetStockObject(WHITE_BRUSH);
    wc.lpszClassName = L"MyPaintClass";

    RegisterClassEx(&wc);

    // 3. Создание окна
    HWND hWnd = CreateWindow(L"MyPaintClass", L"WinAPI C++ Paint (GDI+ & Functions)",
        WS_OVERLAPPEDWINDOW, CW_USEDEFAULT, CW_USEDEFAULT, 1024, 768,
        NULL, NULL, hInstance, NULL);

    ShowWindow(hWnd, nCmdShow);
    UpdateWindow(hWnd);

    // 4. Таблица акселераторов (Горячие клавиши)
    ACCEL accels[] = {
        { FCONTROL | FVIRTKEY, 'N', ID_ACTION_CLEAR },
        { FCONTROL | FVIRTKEY, 'F', ID_TOOL_FUNC }
    };
    HACCEL hAccel = CreateAcceleratorTable(accels, 2);

    // 5. Цикл сообщений
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