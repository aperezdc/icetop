/*
 * icetop.cc
 * Copyright (C) 2016 Adrian Perez <aperez@igalia.com>
 *
 * Distributed under terms of the GPLv2 license.
 */

#include "util/getenv.hh"

extern "C" {
#include <libdill.h>
}

#include <errno.h>
#include <icecc/comm.h>

#include <algorithm>
#include <cstdio>
#include <memory>
#include <sstream>
#include <string>
#include <unordered_map>
#include <vector>


using host_stats_map = std::unordered_map<std::string, std::string>;

struct host_info {
public:
    unsigned int id;
    unsigned int max_jobs;
    int          load;
    bool         offline;
    std::string  name;
    std::string  platform;

    host_info(unsigned int id_): id(id_) {}

    bool operator==(const host_info& rhs) const { return id == rhs.id; }
    bool operator!=(const host_info& rhs) const { return id != rhs.id; }

    void update_from_stats_map(const host_stats_map& stats) {
        auto item = stats.find("State");
        if (item != stats.end()) {
            offline = item->second == "Offline";
            return;
        }

        auto new_name = stats.at("Name");
        if (name != new_name) {
            name = new_name;
            platform = stats.at("Platform");
        }
        max_jobs = std::stoul(stats.at("MaxJobs"));
        load     = std::stoi(stats.at("Load"));
        offline  = false;
    }
};


using host_info_map = std::unordered_map<unsigned int, host_info>;


struct team_info {
public:
    const host_info* find(unsigned int id) const {
        auto item = host_infos.find(id);
        if (item != host_infos.end())
            return &item->second;
        return nullptr;
    }

    const std::string& name_for(unsigned int id) const {
        static const std::string unknown_host_string("<unknown>");
        auto host = find(id);
        return host ? host->name : unknown_host_string;
    }

    const unsigned int max_jobs_for(unsigned int id) const {
        auto host = find(id);
        return host ? host->max_jobs : 0;
    }

    host_info* check_host(unsigned int id, const host_stats_map& stats) {
        auto item = host_infos.find(id);
        if (item == host_infos.end()) {
            item = host_infos.emplace(id, host_info(id)).first;
        }
        item->second.update_from_stats_map(stats);
        return &item->second;
    }

private:
    host_info_map host_infos;
};


struct icecc_monitor;


struct job_info {
    enum job_state {
        WAITING,
        LOCAL,
        COMPILING,
        FINISHED,
        FAILED,
        IDLE,
    };

    unsigned int id;
    job_state    state;
    unsigned int client_id;
    unsigned int server_id;
    std::string  filename;
    unsigned int real_msec;
    unsigned int user_msec;
    unsigned int sys_msec;
    unsigned int page_faults;
    int          exit_code;

    const char* state_string() const {
        switch (state) {
            case WAITING: return "waiting";
            case LOCAL: return "local";
            case COMPILING: return "compiling";
            case FINISHED: return "finished";
            case FAILED: return "failed";
            case IDLE: return "idle";
            default: abort();
        }
    }

    const host_info* server() const;
    const host_info* client() const;

private:
    icecc_monitor& monitor;

    job_info(icecc_monitor& monitor_,
             unsigned int id_,
             unsigned int client_id_,
             const std::string& filename_)
        : id(id_), client_id(client_id_)
        , filename(filename_)
        , monitor(monitor_) { }

    friend struct icecc_monitor;
};


using job_info_map = std::unordered_map<unsigned int, job_info>;


#define MESSAGE_TYPES(F) \
    F (MON_LOCAL_JOB_BEGIN, MonLocalJobBeginMsg) \
    F (JOB_LOCAL_DONE,      JobLocalDoneMsg)     \
    F (MON_JOB_BEGIN,       MonJobBeginMsg)      \
    F (MON_JOB_DONE,        MonJobDoneMsg)       \
    F (MON_GET_CS,          MonGetCSMsg)         \
    F (MON_STATS,           MonStatsMsg)


struct icecc_monitor {
public:
    using host_updated_func = std::function<void(const host_info&)>;
    using job_updated_func  = std::function<void(const job_info&)>;

    enum monitor_state {
        OFFLINE,
    };

    coroutine void check_scheduler(bool deleteit=false) {
        if (deleteit) {
            discover = nullptr;
            scheduler = nullptr;
        }
        while (!scheduler) {
            msleep(now() + 1000);

            std::vector<std::string> names;
            if (!network_name.empty()) {
                names.push_back(network_name);
            } else {
                names.push_back("ICECREAM");
            }
            if (auto env_scheduler = util::getenv("USE_SCHEDULER")) {
                names.push_back(env_scheduler.value());
            }

            for (auto name: names) {
                if (!discover || discover->timed_out()) {
                    discover.reset(new DiscoverSched(name));
                }
                scheduler.reset(discover->try_get_scheduler());
                if (scheduler) {
                    network_name = discover->networkName();
                    scheduler_name = discover->schedulerName();
                    scheduler->setBulkTransfer();
                    discover.reset(nullptr);
                    return;
                }
            }
        }
    }

    coroutine void listen(int64_t deadline = -1) {
        if (!scheduler->send_msg(MonLoginMsg())) {
            // TODO: Recheck for the scheduler
            return;
        }
        while (true) {
            if (fdin(scheduler->fd, deadline)) {
                return;
            }
            while (!scheduler->read_a_bit() || scheduler->has_msg()) {
                if (!_handle_activity()) {
                    break;
                }
            }
        }
    }

    const host_info* find_host(unsigned int id) const { return team.find(id); }

    host_updated_func              on_host_updated;
    job_updated_func               on_job_updated;
    std::string                    network_name;
    std::string                    scheduler_name;
    std::unique_ptr<MsgChannel>    scheduler;

private:
    team_info                      team;
    job_info_map                   jobs;
    monitor_state                  state;
    std::unique_ptr<DiscoverSched> discover;

    bool _handle_activity();

#define MESSAGE_HANDLER(typecode, msgtype, msgvarname) \
    void icecc_monitor::_handle_ ## typecode(const msgtype & msgvarname)

#define DECLARE_MESSAGE_HANDLER(typecode, msgtype) \
    void _handle_ ## typecode(const msgtype & m);

    MESSAGE_TYPES (DECLARE_MESSAGE_HANDLER)

#undef DECLARE_MESSAGE_HANDLER
};


const host_info* job_info::server() const { return monitor.find_host(server_id); }
const host_info* job_info::client() const { return monitor.find_host(client_id); }


bool icecc_monitor::_handle_activity()
{
    std::unique_ptr<Msg> m(scheduler->get_msg());
    if (!m) {
        check_scheduler();
        state = OFFLINE;
        return false;
    }

#define SWITCH_MESSAGE_TYPE(typecode, msgtype)          \
    case M_ ## typecode: {                              \
        msgtype * mm = dynamic_cast<msgtype*>(m.get()); \
        if (mm) _handle_ ## typecode(*mm);              \
    } break;

    switch (m->type) {
        MESSAGE_TYPES (SWITCH_MESSAGE_TYPE)
        case M_END:
            check_scheduler(true);
            // fall-through
        default:
            break;
    }

#undef SWITCH_MESSAGE_TYPE

    return true;
}

static host_stats_map
parse_stats(const std::string& input)
{
    std::stringstream stream(input);
    std::string key, value;
    host_stats_map stats;
    while (std::getline(stream, key, ':') && std::getline(stream, value)) {
        stats.emplace(key, value);
    }
    return stats;
}

MESSAGE_HANDLER (MON_STATS, MonStatsMsg, m)
{
    auto stats = parse_stats(m.statmsg);
    auto host = team.check_host(m.hostid, stats);
    if (on_host_updated) on_host_updated(*host);
}

MESSAGE_HANDLER (MON_LOCAL_JOB_BEGIN, MonLocalJobBeginMsg, m)
{
    job_info& job = jobs.emplace(m.job_id, job_info(*this,
                                                    m.job_id,
                                                    m.hostid,
                                                    m.file)).first->second;
    job.state = job_info::LOCAL;
    if (on_job_updated) on_job_updated(job);
}

MESSAGE_HANDLER (JOB_LOCAL_DONE, JobLocalDoneMsg, m)
{
    auto item = jobs.find(m.job_id);
    if (item == jobs.end()) {
        return;  // Monitoring started after the job was created.
    }
    job_info& job = item->second;
    job.state = job_info::FINISHED;
    if (on_job_updated) on_job_updated(job);
}

MESSAGE_HANDLER (MON_GET_CS, MonGetCSMsg, m)
{
    job_info& job = jobs.emplace(m.job_id, job_info(*this,
                                                    m.job_id,
                                                    m.clientid,
                                                    m.filename)).first->second;
    job.state = job_info::WAITING;
    if (on_job_updated) on_job_updated(job);
}

MESSAGE_HANDLER (MON_JOB_BEGIN, MonJobBeginMsg, m)
{
    auto item = jobs.find(m.job_id);
    if (item == jobs.end()) {
        return;  // Monitoring started after the job was created.
    }
    job_info& job = item->second;
    job.server_id = m.hostid;
    job.state = job_info::COMPILING;
    if (on_job_updated) on_job_updated(job);
}

MESSAGE_HANDLER (MON_JOB_DONE, MonJobDoneMsg, m)
{
    auto item = jobs.find(m.job_id);
    if (item == jobs.end()) {
        return;  // Monitoring started after the job was created.
    }

    job_info& job = item->second;

    if (m.exitcode) {
        job.state       = job_info::FAILED;
        job.exit_code   = m.exitcode;
    } else {
        job.state       = job_info::FINISHED;
        job.real_msec   = m.real_msec;
        job.user_msec   = m.user_msec;
        job.sys_msec    = m.sys_msec;
        job.page_faults = m.pfaults;
    }

    if (on_job_updated) on_job_updated(job);

    jobs.erase(item);
}


static void show_job(const job_info& job)
{
    if (job.state != job_info::FINISHED && job.state != job_info::FAILED)
        return;
    const char *server = nullptr;
    const char *client = "?";
    if (job.server_id) {
        auto host = job.server();
        if (host) server = host->name.c_str();
    }
    if (job.client_id) {
        auto host = job.client();
        if (host) client = host->name.c_str();
    }
    if (server) {
        printf("Job %u [%s->%s] '%s' %s\n",
               job.id,
               client,
               server,
               job.filename.c_str(),
               job.state_string());
    } else {
        printf("Job %u [%s] '%s' %s\n",
               job.id,
               client,
               job.filename.c_str(),
               job.state_string());
    }
}

static void show_host(const host_info& host)
{
    printf("Host %u '%s' (%s, load %i, max %u) is %s\n",
           host.id,
           host.name.c_str(),
           host.platform.c_str(),
           host.load,
           host.max_jobs,
           host.offline ? "offline" : "online");
}


int main(int argc, char **argv)
{
    icecc_monitor monitor;
    monitor.on_host_updated = show_host;
    monitor.on_job_updated = show_job;

    printf("Waiting for scheduler...\n");
    go(monitor.check_scheduler());
    while (!monitor.scheduler) msleep(now() + 100);

    go(monitor.listen());
    while (true) {
        msleep(now() + 100);
    }
}
