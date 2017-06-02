/*
 * Copyright (C) 2017 Canonical, Ltd.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; version 3.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 *
 * Authored by: Alberto Aguirre <alberto.aguirre@canonical.com>
 *
 */

#include "daemon.h"
#include "daemon_config.h"

#include <multipass/platform.h>
#include <multipass/name_generator.h>
#include <multipass/virtual_machine_factory.h>
#include <multipass/vm_image_host.h>
#include <multipass/vm_image_vault.h>

#include <QCoreApplication>
#include <thread>

namespace
{
class AutoJoinThread
{
public:
    template <typename Callable, typename... Args>
    AutoJoinThread(Callable&& f, Args&&... args) : thread{std::forward<Callable>(f), std::forward<Args>(args)...}
    {
    }
    ~AutoJoinThread()
    {
        if (thread.joinable())
            thread.join();
    }

private:
    std::thread thread;
};

class DaemonRunner
{
public:
    DaemonRunner() : daemon{multipass::DaemonConfigBuilder{}.build()}, daemon_thread{[this] { daemon.run(); }} {}
    ~DaemonRunner() { daemon.shutdown(); }

private:
    multipass::Daemon daemon;
    AutoJoinThread daemon_thread;
};

#if !defined(_MSC_VER)
#include <signal.h>
#include <vector>

sigset_t make_and_block_signals(std::vector<int> sigs)
{
    sigset_t sigset;
    sigemptyset(&sigset);
    for (auto signal : sigs)
    {
        sigaddset(&sigset, signal);
    }
    sigprocmask(SIG_BLOCK, &sigset, nullptr);
    return sigset;
}

class UnixSignalHandler
{
public:
    UnixSignalHandler(QCoreApplication& app)
        : signal_handling_thread{
            [this, &app, sigs = make_and_block_signals({SIGTERM, SIGINT})] { monitor_signals(sigs, app); }}
    {
    }

    void monitor_signals(sigset_t sigset, QCoreApplication& app)
    {
        int sig = -1;
        sigwait(&sigset, &sig);
        std::cout << "Received signal: " << sig << "\n";
        app.quit();
    }

private:
    AutoJoinThread signal_handling_thread;
};
#endif // _MSC_VER
}

int main(int argc, char* argv[])
try
{
    QCoreApplication app(argc, argv);
#if !defined(_MSC_VER)
    UnixSignalHandler handler(app);
#endif
    DaemonRunner daemon_runner;

    app.exec();

    std::cout << "Goodbye!\n";
    return EXIT_SUCCESS;
}
catch (const std::exception& e)
{
    std::cerr << "Error: " << e.what() << std::endl;
}
