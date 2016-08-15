/*
 * icetop.cc
 * Copyright (C) 2016 Adrian Perez <aperez@igalia.com>
 *
 * Distributed under terms of the GPLv2 license.
 */

#include "util/getenv.hh"
#include "util/ti.hh"

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

    host_info(unsigned int id_): id(id_), name(), platform() {}
    host_info(const host_info&) = delete;
    host_info(host_info&&) = default;

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
        ONLINE,
    };

    icecc_monitor(host_updated_func on_host_updated_ = nullptr,
                  job_updated_func on_job_updated_ = nullptr)
        : on_host_updated(on_host_updated_)
        , on_job_updated(on_job_updated_)
        , state(OFFLINE)
    { }



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
                    state = ONLINE;
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

    std::string                    network_name;
    std::string                    scheduler_name;
    std::unique_ptr<MsgChannel>    scheduler;

private:
    host_updated_func              on_host_updated;
    job_updated_func               on_job_updated;
    monitor_state                  state;
    team_info                      team;
    job_info_map                   jobs;
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


struct host_layout {
    static ti::pen line_pens[2];

    host_layout(ti::window&& w, const host_info& host)
        : window(std::move(w)), hostname(host.name)
        , platform(host.platform)
        , filename()
        , state_string("idle")
    {
        window.on_expose([this](ti::window::expose_event& event) {
            on_expose(event);
            return true;
        });
    }

    host_layout(host_layout&&) = default;

    unsigned position() const {
        return window.top();
    }

    void move_up() {
        window.set_position(position() - 1, window.left());
        window.root().expose();
    }

    void on_expose(ti::window::expose_event& ev) {
        ev.render.set_pen(line_pens[position() % 2]).clear();
        ev.render.at(0, 1) << platform;
        ev.render.at(0, 9) << hostname;
        ev.render.at(0, 30) << filename;
        if (ev.columns >= (11 + origin.size())) {
            ev.render.at(0, ev.columns - 11 - origin.size()) << origin;
            ev.render.at(0, ev.columns - 10) << state_string;
        }
    }

    void host_info_updated(const host_info& host) {
        hostname = host.name;
        platform = host.platform;
        window.expose();
    }

    void job_info_updated(const job_info& job) {
        if (job.server()) {
            origin = job.client()->name;
        } else {
            origin = "";
        }
        state_string = job.state_string();
        filename = job.filename;
        window.expose();
    }

    ti::window window;
    std::string hostname;
    std::string platform;
    std::string filename;
    std::string origin;
    const char *state_string;
};

ti::pen host_layout::line_pens[2] = {
    { ti::pen::bg(235) },
    { ti::pen::bg(234) },
};


struct screen_layout {
    static ti::pen status_pen;

    screen_layout(ti::terminal& term)
        : root(ti::window(term))
        , status(root, root.lines() - 1, 0, 1, root.columns())
    {
        status.on_expose([this](ti::window::expose_event& ev) {
            char timestring[15];
            struct tm *t = localtime(&statustime);
            strftime(timestring, sizeof(timestring), "[%H:%M:%S] ", t);
            ev.render.set_pen(status_pen).clear().at(0, 1) << timestring << statusline;
            return true;
        });

        root.on_geometry_change([this, &term](ti::window::geometry_change_event& ev) {
            status.set_geometry(ev.lines - 1, 0, 1, ev.columns);
            term.clear();
            root.expose();
            return true;
        });
    }

    void host_info_updated(const host_info& host) {
        if (host.offline) {
            set_status("Host " + host.name + " went offline");
            auto index_item = hostid_to_index.find(host.id);
            if (index_item == hostid_to_index.end()) {
                // No line for it: do nothing.
                return;
            }
            auto index = index_item->second;
            host_layouts.erase(host_layouts.begin() + index);
            for (auto it = host_layouts.begin() + index; it != host_layouts.end(); ++it) {
                it->get()->move_up();
            }
            hostid_to_index.erase(index_item);
        } else {
            auto index_item = hostid_to_index.find(host.id);
            if (index_item == hostid_to_index.end()) {
                set_status("Host " + host.name + " (" + host.platform + ") came online");
                unsigned index = host_layouts.size();  // Add it at the end.
                ti::window w { root, index, 0, 1, root.columns() };
                host_layouts.emplace_back(std::make_unique<host_layout>(std::move(w), host));
                hostid_to_index[host.id] = index;
            } else {
                set_status("Host " + host.name + " (" + host.platform + ") is still online");
                host_layouts[index_item->second]->host_info_updated(host);
            }
        }
    }

    void job_info_updated(const job_info& job) {
        auto index_item = hostid_to_index.find(job.server() ? job.server_id : job.client_id);
        if (index_item == hostid_to_index.end())
            return;
        host_layouts[index_item->second]->job_info_updated(job);
    }

    void flush() {
        root.flush();
    }

    void set_status(const std::string& s) {
        statustime = time(nullptr);
        statusline = s;
        status.expose();
    }

    ti::window root;
    ti::window status;

    std::unordered_map<int, size_t> hostid_to_index;
    std::vector<std::unique_ptr<host_layout>> host_layouts;
    std::string statusline;
    time_t statustime;
};


ti::pen screen_layout::status_pen = { ti::pen::bg(4) };


#include <signal.h>

static bool running = true;
static void handle_sigint(int)
{
    running = false;
}


int main(int argc, char **argv)
{
    ti::terminal term { };
    term.wait_ready();

    screen_layout layout { term };

    icecc_monitor monitor {
        [&layout](const host_info& host) {
            layout.host_info_updated(host);
        },
        [&layout](const job_info& job) {
            layout.job_info_updated(job);
        }
    };

    term << "Waiting for scheduler...\n";
    go(monitor.check_scheduler());
    while (!monitor.scheduler) msleep(now() + 100);

    go(monitor.listen());

    term.set(ti::terminal::altscreen).clear();

    signal(SIGINT, handle_sigint);
    while (running) {
        layout.flush();
        term.wait_input(10);
        msleep(40);
    }
}
