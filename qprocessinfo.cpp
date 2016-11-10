#include "qprocessinfo.h"

#if defined(Q_OS_WIN32)

#include <tlhelp32.h>
#include <windows.h>

typedef DWORD(WINAPI *PFN_GETWINDOWTHREADPROCESSID)(HWND hWnd, LPDWORD lpdwProcessId);
typedef HWND(WINAPI *PFN_GETWINDOW)(HWND hWnd, UINT uCmd);
typedef BOOL(WINAPI *PFN_ISWINDOWVISIBLE)(HWND hWnd);
typedef int(WINAPI *PFN_GETWINDOWTEXTLENGTHW)(HWND hWnd);
typedef int(WINAPI *PFN_GETWINDOWTEXTW)(HWND hWnd, LPWSTR lpString, int nMaxCount);
typedef BOOL(WINAPI *PFN_ENUMWINDOWS)(WNDENUMPROC lpEnumFunc, LPARAM lParam);

namespace
{
struct callbackContext
{
  callbackContext(QList<QProcessInfo> &l) : list(l) {}
  QList<QProcessInfo> &list;

  PFN_GETWINDOWTHREADPROCESSID GetWindowThreadProcessId;
  PFN_GETWINDOW GetWindow;
  PFN_ISWINDOWVISIBLE IsWindowVisible;
  PFN_GETWINDOWTEXTLENGTHW GetWindowTextLengthW;
  PFN_GETWINDOWTEXTW GetWindowTextW;
  PFN_ENUMWINDOWS EnumWindows;
};
};

static BOOL CALLBACK fillWindowTitles(HWND hwnd, LPARAM lp)
{
  callbackContext *ctx = (callbackContext *)lp;

  DWORD pid = 0;
  ctx->GetWindowThreadProcessId(hwnd, &pid);

  HWND parent = ctx->GetWindow(hwnd, GW_OWNER);

  if(parent != 0)
    return TRUE;

  if(!ctx->IsWindowVisible(hwnd))
    return TRUE;

  for(QProcessInfo &info : ctx->list)
  {
    if(info.pid() == (uint32_t)pid)
    {
      int len = ctx->GetWindowTextLengthW(hwnd);
      wchar_t *buf = new wchar_t[len + 1];
      ctx->GetWindowTextW(hwnd, buf, len + 1);
      buf[len] = 0;
      info.setWindowTitle(QString::fromStdWString(std::wstring(buf)));
      return TRUE;
    }
  }

  return TRUE;
}

QList<QProcessInfo> QProcessInfo::enumerate()
{
  QList<QProcessInfo> ret;

  HANDLE h = NULL;
  PROCESSENTRY32 pe = {0};
  pe.dwSize = sizeof(PROCESSENTRY32);
  h = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
  if(Process32First(h, &pe))
  {
    do
    {
      QProcessInfo info;
      info.setPid((uint32_t)pe.th32ProcessID);
      info.setName(QString::fromStdWString(std::wstring(pe.szExeFile)));

      ret.push_back(info);
    } while(Process32Next(h, &pe));
  }
  CloseHandle(h);

  HMODULE user32 = LoadLibraryA("user32.dll");

  if(user32)
  {
    callbackContext ctx(ret);

    ctx.GetWindowThreadProcessId =
        (PFN_GETWINDOWTHREADPROCESSID)GetProcAddress(user32, "GetWindowThreadProcessId");
    ctx.GetWindow = (PFN_GETWINDOW)GetProcAddress(user32, "GetWindow");
    ctx.IsWindowVisible = (PFN_ISWINDOWVISIBLE)GetProcAddress(user32, "IsWindowVisible");
    ctx.GetWindowTextLengthW =
        (PFN_GETWINDOWTEXTLENGTHW)GetProcAddress(user32, "GetWindowTextLengthW");
    ctx.GetWindowTextW = (PFN_GETWINDOWTEXTW)GetProcAddress(user32, "GetWindowTextW");
    ctx.EnumWindows = (PFN_ENUMWINDOWS)GetProcAddress(user32, "EnumWindows");

    if(ctx.GetWindowThreadProcessId && ctx.GetWindow && ctx.IsWindowVisible &&
       ctx.GetWindowTextLengthW && ctx.GetWindowTextW && ctx.EnumWindows)
    {
      ctx.EnumWindows(fillWindowTitles, (LPARAM)&ctx);
    }

    FreeLibrary(user32);
  }

  return ret;
}

#elif defined(Q_OS_UNIX)

#include <QDir>
#include <QTextStream>

QList<QProcessInfo> QProcessInfo::enumerate()
{
  QList<QProcessInfo> ret;

  QDir proc("/proc");

  QStringList files = proc.entryList();

  for(const QString &f : files)
  {
    bool ok = false;
    uint32_t pid = f.toUInt(&ok);

    if(ok)
    {
      QProcessInfo info;
      info.setPid(pid);

      QDir processDir("/proc/" + f);

      QFile status(processDir.absoluteFilePath("status"));
      if(status.open(QIODevice::ReadOnly))
      {
        QByteArray contents = status.readAll();

        QTextStream in(&contents);
        while(!in.atEnd())
        {
          QString line = in.readLine();

          if(line.startsWith("Name:"))
          {
            line.remove(0, 5);
            info.setName(line.trimmed());
            break;
          }
        }
        status.close();
      }

      QFile cmdline(processDir.absoluteFilePath("cmdline"));
      if(cmdline.open(QIODevice::ReadOnly))
      {
        QByteArray contents = cmdline.readAll();

        int nullIdx = contents.indexOf('\0');

        if(nullIdx > 0)
        {
          QString firstparam = QString::fromUtf8(contents.data(), nullIdx);

          // if name is a truncated form of a filename, replace it
          if(firstparam.startsWith(info.name()) && QFileInfo::exists(firstparam))
            info.setName(firstparam);

          // if we don't have a name, replace it but with []s
          if(info.name() == "")
            info.setName(QString("[%1]").arg(firstparam));

          contents.replace('\0', " ");
        }

        info.setCommandLine(QString::fromUtf8(contents).trimmed());
      }

      ret.push_back(info);
    }
  }

  return ret;
}

#else

QList<QProcessInfo> QProcessInfo::enumerate()
{
  QList<QProcessInfo> ret;

  qWarning() << "Process enumeration not supported on this platform";

  return ret;
}

#endif

uint32_t QProcessInfo::pid() const
{
  return m_pid;
}

void QProcessInfo::setPid(uint32_t pid)
{
  m_pid = pid;
}

const QString &QProcessInfo::name() const
{
  return m_name;
}

void QProcessInfo::setName(const QString &name)
{
  m_name = name;
}

const QString &QProcessInfo::windowTitle() const
{
  return m_title;
}

void QProcessInfo::setWindowTitle(const QString &title)
{
  m_title = title;
}

const QString &QProcessInfo::commandLine() const
{
  return m_cmdLine;
}

void QProcessInfo::setCommandLine(const QString &cmd)
{
  m_cmdLine = cmd;
}