#include <wchar.h>
#include <string>
#include "offset_scan.h"
#include "offsets/ExeFile.h"
#include "offsets/Injector.h"
#include "meter/logging.h"

#ifdef _MSC_VER
#include <windows.h>
#include <Shlwapi.h>
#include <Strsafe.h>
#pragma comment(lib, "shlwapi.lib")
#else
#define MAX_PATH 260
#endif

#ifdef __cplusplus
extern "C" {
#endif

logger_t s_logger;
static logger_filter_t  s_filter;
static logger_formatter_t s_formatter;
static logger_handler_t s_handler_scr;
static logger_handler_t s_handler_file;

#ifdef __cplusplus
}
#endif

static bool logger_create(const char* filename)
{
	static char logpath[MAX_PATH];
	FILE *logfile = NULL;

	memset(&s_logger, 0, sizeof(s_logger));
	s_logger.name = "bgdm-logger";

#ifdef _DEBUG
	s_filter.minlevel = 0;
#else
	s_filter.minlevel = 0;
#endif
	s_filter.maxlevel = LOGGING_MAX_LEVEL;
	s_filter.next = NULL;

	s_formatter.datefmt = "%Y-%m-%dT%H:%M:%SZ";
	s_formatter.fmt =
		"%(asctime)s	"
		"%(levelname)s	"
		"%(message)s";

	StringCbCopyA(logpath, sizeof(logpath), filename);
	PathRenameExtensionA(logpath, ".log");
	logfile = fopen(logpath, "w");

	s_handler_scr.name = "bgdm-scr";
	s_handler_scr.file = stdout;
	s_handler_scr.filter = &s_filter;
	s_handler_scr.formatter = &s_formatter;
	s_handler_scr.next = logfile ? &s_handler_file : NULL;

	s_handler_file.name = "bgdm-file";
	s_handler_file.file = logfile;
	s_handler_file.filter = &s_filter;
	s_handler_file.formatter = &s_formatter;
	s_handler_file.next = NULL;

	s_logger.filter = &s_filter;
	s_logger.handler = &s_handler_scr;

	logging_setlogger(&s_logger);

	if (!logfile)
		LOG_WARN("[client] Unable to open %s for writing.", logpath);

	return true;
}

int main(int argc, char* argv[], char *envp[])
{
	static char cwd[MAX_PATH];
	static char file[MAX_PATH];
	static const char gw2file[] = "Gw2-64.exe";
	static const char gw2dir[] = "C:\\Program Files\\Guild Wars 2";

	GetModuleFileNameA(NULL, cwd, MAX_PATH);
	PathRemoveFileSpecA(cwd);
	PathCombineA(file, cwd, argv[0]);

	logger_create(file);

	std::vector<int> pids = hl::GetPIDsByProcName(gw2file);
	if (pids.size() > 0) {
		std::string error;
		LOG_INFO("Process scan <%#x> %d:%s started...", pids[0], pids[0], gw2file);
		if (ScanOffsets(pids[0])) {
			LOG_INFO("Process scan <%#x> %d:%s succeeded.", pids[0], pids[0], gw2file);
		}
		else {
			LOG_ERR("Process scan <%#x> %d:%s failed, error: %s", pids[0], pids[0], gw2file, error.c_str());
		}
	}
}