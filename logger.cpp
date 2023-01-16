#include <HalonMTA.h>
#include <string>
#include <cstring>
#include <mutex>
#include <map>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <errno.h>
#include <unistd.h>
#include <syslog.h>

extern char *__progname;

void log_open(const std::string& log, const std::string& path, const std::string& header, bool fsync, ssize_t mode);
void log_logger(const std::string& log, const std::string& msg);
void log_reopen(const std::string& log);

HALON_EXPORT
int Halon_version()
{
	return HALONMTA_PLUGIN_VERSION;
}

HALON_EXPORT
bool Halon_init(HalonInitContext* hic)
{
	HalonConfig* cfg;
	HalonMTA_init_getinfo(hic, HALONMTA_INIT_CONFIG, nullptr, 0, &cfg, nullptr);

	try {
		auto logs = HalonMTA_config_object_get(cfg, "logs");
		if (logs)
		{
			size_t l = 0;
			HalonConfig* log;
			while ((log = HalonMTA_config_array_get(logs, l++)))
			{
				const char* id = HalonMTA_config_string_get(HalonMTA_config_object_get(log, "id"), nullptr);
				const char* path = HalonMTA_config_string_get(HalonMTA_config_object_get(log, "path"), nullptr);
				const char* header = HalonMTA_config_string_get(HalonMTA_config_object_get(log, "header"), nullptr);
				const char* fsync = HalonMTA_config_string_get(HalonMTA_config_object_get(log, "fsync"), nullptr);
				const char* mode_ = HalonMTA_config_string_get(HalonMTA_config_object_get(log, "chmod"), nullptr);
				if (!id || !path)
					continue;
				ssize_t mode = -1;
				if (mode_)
					mode = strtol(mode_, nullptr, 8);
				log_open(id, path, header ? header : "", !fsync || strcmp(fsync, "false") != 0, mode);
			}
		}
		return true;
	} catch (const std::runtime_error& e) {
		syslog(LOG_CRIT, "%s", e.what());
		return false;
	}
}

HALON_EXPORT
bool Halon_plugin_command(const char* in, size_t len, char** out, size_t* olen)
{
	if (strncmp(in, "reopen:", 7) == 0)
	{
		try {
			log_reopen(in + 7);
			*out = strdup("OK");
			return true;
		} catch (const std::runtime_error& e) {
			*out = strdup(e.what());
			return false;
		}
	}
	*out = strdup("Unknown command");
	return false;
}

HALON_EXPORT
void logger(HalonHSLContext* hhc, HalonHSLArguments* args, HalonHSLValue* ret)
{
	HalonHSLValue* x;
	char* id = nullptr;
	char* text = nullptr;
	size_t textlen = 0;

	x = HalonMTA_hsl_argument_get(args, 0);
	if (x && HalonMTA_hsl_value_type(x) == HALONMTA_HSL_TYPE_STRING)
		HalonMTA_hsl_value_get(x, HALONMTA_HSL_TYPE_STRING, &id, nullptr);
	else
		return;

	x = HalonMTA_hsl_argument_get(args, 1);
	if (x && HalonMTA_hsl_value_type(x) == HALONMTA_HSL_TYPE_STRING)
		HalonMTA_hsl_value_get(x, HALONMTA_HSL_TYPE_STRING, &text, &textlen);
	else
		return;

	try {
		log_logger(id, std::string(text, textlen));
		bool t = true;
		HalonMTA_hsl_value_set(ret, HALONMTA_HSL_TYPE_BOOLEAN, &t, 0);
	} catch (const std::runtime_error& e) {
		syslog(LOG_CRIT, "%s", e.what());
	}
}

HALON_EXPORT
bool Halon_hsl_register(HalonHSLRegisterContext* ptr)
{
	HalonMTA_hsl_register_function(ptr, "logger", &logger);
	HalonMTA_hsl_module_register_function(ptr, "logger", &logger);
	return true;
}

struct log
{
	std::string path;
	std::string header;
	int fd = -1;
	bool fsync = false;
	std::mutex lock;
};

std::map<std::string, struct log> logs;

void log_open(const std::string& log, const std::string& path, const std::string& header, bool fsync, ssize_t mode)
{
	if (logs.find(log) != logs.end())
		throw std::runtime_error("Duplicate log id: " + log);

	int fd = open(path.c_str(), O_CREAT | O_APPEND | O_WRONLY, mode != -1 ? mode : 0666);
	if (fd < 0)
		throw std::runtime_error("Could not open log (" + path + "): " + std::string(strerror(errno)));

	if (!header.empty() && lseek(fd, 0, SEEK_END) == 0)
	{
		if (write(fd, header.c_str(), header.size()) != header.size())
			throw std::runtime_error("Could not write header (" + path + "): " + std::string(strerror(errno)));
		if (fsync && ::fsync(fd) != 0)
			throw std::runtime_error("Could not fsync header (" + path + "): " + std::string(strerror(errno)));
	}

	auto& h = logs[log];
	h.path = path;
	h.header = header;
	h.fd = fd;
	h.fsync = fsync;

	if (strcmp(__progname, "smtpd") == 0)
		syslog(LOG_INFO, "logger: open(%s) as %s%s", path.c_str(), log.c_str(), fsync ? " (fsync)" : "");
}

void log_logger(const std::string& log, const std::string& msg)
{
	auto l = logs.find(log);
	if (l == logs.end())
		throw std::runtime_error("No such log id: " + log);

	l->second.lock.lock();
	if (write(l->second.fd, msg.c_str(), msg.size()) != msg.size())
	{
		l->second.lock.unlock();
		throw std::runtime_error("Could not write log (" + l->second.path + "): " + std::string(strerror(errno)));
	}
	if (l->second.fsync && fsync(l->second.fd) != 0)
	{
		l->second.lock.unlock();
		throw std::runtime_error("Could not fsync log (" + l->second.path + "): " + std::string(strerror(errno)));
	}
	l->second.lock.unlock();
}

void log_reopen(const std::string& log)
{
	auto l = logs.find(log);
	if (l == logs.end())
		throw std::runtime_error("No such log id: " + log);

	int fd = open(l->second.path.c_str(), O_APPEND | O_WRONLY);
	if (fd < 0)
		throw std::runtime_error("Could not reopen log (" + l->second.path + "): " + std::string(strerror(errno)));

	if (!l->second.header.empty() && lseek(fd, 0, SEEK_END) == 0)
	{
		if (write(fd, l->second.header.c_str(), l->second.header.size()) != l->second.header.size())
			throw std::runtime_error("Could not write header log (" + l->second.path + "): " + std::string(strerror(errno)));
		if (l->second.fsync && fsync(fd) != 0)
			throw std::runtime_error("Could not fsync header log (" + l->second.path + "): " + std::string(strerror(errno)));
	}

	l->second.lock.lock();
	close(l->second.fd);
	l->second.fd = fd;
	l->second.lock.unlock();
}
